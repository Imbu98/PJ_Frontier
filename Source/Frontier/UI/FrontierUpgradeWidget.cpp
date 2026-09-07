#include "FrontierUpgradeWidget.h"

//#include "Components/FrontierInventoryComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Game/FrontierPlayerState.h"
#include "Game/FrontierLobbyPlayerController.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"
#include "Progression/FrontierUpgradeBalanceSubsystem.h"
#include "Progression/FrontierUpgradeSettings.h"
#include "UI/FrontierInventoryDragDropOperation.h"
#include "UI/FrontierItemStatDisplayDefinition.h"
#include "UI/FrontierItemTooltipWidget.h"
#include "UI/FrontierInventorySlotWidget.h"
#include "UI/FrontierUpgradeCurrencySlot.h"
#include "UI/FrontierUpgradeEffectSlot.h"
#include "UI/FrontierUpgradeIngredientSlot.h"
#include "UI/FrontierUpgradeResultWidget.h"
#include "UI/FrontierPopupSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"

void UFrontierUpgradeWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (WBP_InfoSlot)
	{
		WBP_InfoSlot->OnSlotDoubleClicked.RemoveAll(this);
		WBP_InfoSlot->OnSlotDropped.RemoveAll(this);
		WBP_InfoSlot->OnSlotDoubleClicked.AddDynamic(this, &UFrontierUpgradeWidget::HandleInfoSlotDoubleClicked);
		WBP_InfoSlot->OnSlotDropped.AddDynamic(this, &UFrontierUpgradeWidget::HandleInfoSlotDropped);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->OnClicked.RemoveAll(this);
		Button_Upgrade->OnClicked.AddDynamic(this, &UFrontierUpgradeWidget::HandleUpgradeClicked);
	}
	if (AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer()))
	{
		BackendProtocolComponent = Controller->GetBackendProtocolComponent();
		if (BackendProtocolComponent)
		{
			BackendProtocolComponent->OnItemUpgradeCompleted.RemoveAll(this);
			BackendProtocolComponent->OnItemUpgradeCompleted.AddDynamic(
				this,
				&UFrontierUpgradeWidget::HandleItemUpgradeCompleted);
		}
	}

	if (const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr)
	{
		if (UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
		{
			Inventory->OnInventoryChanged.AddUniqueDynamic(
				this,
				&UFrontierUpgradeWidget::HandleInventoryChanged);
		}
		if (UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
		{
			Storage->OnInventoryChanged.AddUniqueDynamic(
				this,
				&UFrontierUpgradeWidget::HandleInventoryChanged);
		}
	}

	ClearSelectedUpgradeItem();
}

void UFrontierUpgradeWidget::NativeDestruct()
{
	if (ActiveUpgradeResultWidget)
	{
		ActiveUpgradeResultWidget->RemoveFromParent();
		ActiveUpgradeResultWidget = nullptr;
	}
	if (WBP_InfoSlot)
	{
		WBP_InfoSlot->OnSlotDoubleClicked.RemoveAll(this);
		WBP_InfoSlot->OnSlotDropped.RemoveAll(this);
	}
	if (Button_Upgrade)
	{
		Button_Upgrade->OnClicked.RemoveAll(this);
	}
	if (BackendProtocolComponent)
	{
		BackendProtocolComponent->OnItemUpgradeCompleted.RemoveAll(this);
		BackendProtocolComponent = nullptr;
	}
	if (const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr)
	{
		if (UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
		{
			Inventory->OnInventoryChanged.RemoveDynamic(
				this,
				&UFrontierUpgradeWidget::HandleInventoryChanged);
		}
		if (UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
		{
			Storage->OnInventoryChanged.RemoveDynamic(
				this,
				&UFrontierUpgradeWidget::HandleInventoryChanged);
		}
	}

	Super::NativeDestruct();
}

void UFrontierUpgradeWidget::SetSelectedUpgradeItem(const FFrontierItemInstance& ItemInstance)
{
	if (bUpgradeRequestInFlight)
	{
		return;
	}
	if (!IsEquipmentItemInstance(ItemInstance))
	{
		return;
	}

	SelectedUpgradeItem = ItemInstance;
	RefreshInfoSlot();
	RefreshUpgradeInfoVisibility();
	RefreshUpgradePreview();
}

void UFrontierUpgradeWidget::ClearSelectedUpgradeItem()
{
	if (bUpgradeRequestInFlight)
	{
		return;
	}
	SelectedUpgradeItem = FFrontierItemInstance();
	RefreshInfoSlot();
	RefreshUpgradeInfoVisibility();
	RefreshUpgradePreview();
}

bool UFrontierUpgradeWidget::HasSelectedUpgradeItem() const
{
	return SelectedUpgradeItem.IsValid();
}

void UFrontierUpgradeWidget::RefreshUpgradePreview()
{
	RefreshEquipmentHeader();
	ClearPreviewSlots();
	if (!SelectedUpgradeItem.IsValid())
	{
		if (Text_UpgradeProbability)
		{
			Text_UpgradeProbability->SetText(FText::GetEmpty());
		}
		RefreshUpgradeButtonState();
		return;
	}

	const FFrontierUpgradeLevelData* LevelData = ResolveNextUpgradeLevel();
	if (Text_UpgradeProbability)
	{
		const float Probability = LevelData ? LevelData->SuccessProbability : 0.0f;
		Text_UpgradeProbability->SetText(FText::Format(
			NSLOCTEXT("FrontierUpgrade", "Probability", "{0}"),
			FText::AsNumber(Probability)));
	}
	RefreshIngredientSlots(LevelData);
	RefreshCurrencySlot(LevelData);
	RefreshEffectSlots(LevelData);
	RefreshUpgradeButtonState();
}

void UFrontierUpgradeWidget::HandleUpgradeClicked()
{
	if (bUpgradeRequestInFlight
		|| !SelectedUpgradeItem.IsValid()
		|| !SelectedUpgradeItem.ItemInstanceId.IsValid()
		|| !ResolveNextUpgradeLevel())
	{
		RefreshUpgradeButtonState();
		return;
	}

	AFrontierLobbyPlayerController* Controller = Cast<AFrontierLobbyPlayerController>(GetOwningPlayer());
	if (!Controller)
	{
		FFrontierItemUpgradeResult Result;
		Result.Message = TEXT("Lobby player controller is unavailable.");
		ShowUpgradeResult(Result);
		return;
	}

	const int32 ExpectedLevel = FMath::Max(0, SelectedUpgradeItem.EnhancementLevel);
	const bool bCanReusePendingKey = PendingUpgradeItemId == SelectedUpgradeItem.ItemInstanceId
		&& PendingExpectedEnhancementLevel == ExpectedLevel
		&& !PendingUpgradeIdempotencyKey.IsEmpty();
	if (!bCanReusePendingKey)
	{
		PendingUpgradeItemId = SelectedUpgradeItem.ItemInstanceId;
		PendingExpectedEnhancementLevel = ExpectedLevel;
		PendingUpgradeIdempotencyKey = FString::Printf(
			TEXT("upgrade-%s-%d-%s"),
			*PendingUpgradeItemId.ToString(EGuidFormats::DigitsWithHyphensLower),
			ExpectedLevel,
			*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	}

	bUpgradeRequestInFlight = true;
	RefreshUpgradeButtonState();
	Controller->ServerRequestUpgradeItem(
		PendingUpgradeItemId,
		PendingExpectedEnhancementLevel,
		PendingUpgradeIdempotencyKey);
}

void UFrontierUpgradeWidget::HandleItemUpgradeCompleted(const FFrontierItemUpgradeResult& Result)
{
	bUpgradeRequestInFlight = false;
	if (Result.bRequestSucceeded)
	{
		if (Result.Item.IsValid())
		{
			SelectedUpgradeItem = Result.Item;
		}
		PendingUpgradeItemId.Invalidate();
		PendingExpectedEnhancementLevel = INDEX_NONE;
		PendingUpgradeIdempotencyKey.Reset();
		RefreshInfoSlot();
		RefreshUpgradeInfoVisibility();
		RefreshUpgradePreview();
	}
	else
	{
		if (!Result.bRetryable)
		{
			PendingUpgradeItemId.Invalidate();
			PendingExpectedEnhancementLevel = INDEX_NONE;
			PendingUpgradeIdempotencyKey.Reset();
		}
		RefreshUpgradeButtonState();
	}
	ShowUpgradeResult(Result);
}

void UFrontierUpgradeWidget::HandleInventoryChanged(
	const TArray<FFrontierInventorySlot>& Slots)
{
	if (!bUpgradeRequestInFlight && SelectedUpgradeItem.IsValid())
	{
		RefreshUpgradePreview();
	}
}

void UFrontierUpgradeWidget::RefreshUpgradeButtonState()
{
	if (Button_Upgrade)
	{
		Button_Upgrade->SetIsEnabled(
			!bUpgradeRequestInFlight
			&& SelectedUpgradeItem.IsValid()
			&& SelectedUpgradeItem.ItemInstanceId.IsValid()
			&& ResolveNextUpgradeLevel() != nullptr);
	}
}

void UFrontierUpgradeWidget::ShowUpgradeResult(const FFrontierItemUpgradeResult& Result)
{
	if (!Result.bRequestSucceeded)
	{
		if (UFrontierPopupSubsystem* Popup = UFrontierPopupSubsystem::Get(this))
		{
			Popup->ShowMessage(
				EFrontierPopupType::Error,
				NSLOCTEXT("FrontierUpgrade", "UpgradeRequestFailedTitle", "강화 요청 실패"),
				Result.Message.IsEmpty()
					? NSLOCTEXT("FrontierUpgrade", "UpgradeRequestFailedMessage", "강화 요청을 처리하지 못했습니다.")
					: FText::FromString(Result.Message));
		}
		return;
	}
	ShowUpgradeResultPresentation(Result);
}

void UFrontierUpgradeWidget::ShowUpgradeResultPresentation(const FFrontierItemUpgradeResult& Result)
{
	if (ActiveUpgradeResultWidget)
	{
		ActiveUpgradeResultWidget->RemoveFromParent();
		ActiveUpgradeResultWidget = nullptr;
	}

	TSubclassOf<UFrontierUpgradeResultWidget> WidgetClass = UpgradeResultWidgetClass;
	if (!WidgetClass)
	{
		WidgetClass = UFrontierUpgradeResultWidget::StaticClass();
	}
	ActiveUpgradeResultWidget = CreateWidget<UFrontierUpgradeResultWidget>(GetOwningPlayer(), WidgetClass);
	if (!ActiveUpgradeResultWidget)
	{
		return;
	}

	const FFrontierItemInstance& PresentedItem = Result.Item.IsValid() ? Result.Item : SelectedUpgradeItem;
	UTexture2D* ItemIcon = PresentedItem.IsValid() ? PresentedItem.GetIcon().LoadSynchronous() : nullptr;
	const FText ItemName = PresentedItem.IsValid()
		? PresentedItem.GetDisplayNameText()
		: NSLOCTEXT("FrontierUpgrade", "UnknownUpgradeItem", "강화 장비");

	ActiveUpgradeResultWidget->AddToViewport(1100);
	ActiveUpgradeResultWidget->PresentResult(
		Result.bUpgradeSucceeded,
		Result.PreviousEnhancementLevel,
		Result.CurrentEnhancementLevel,
		ItemName,
		ItemIcon);
}

void UFrontierUpgradeWidget::RefreshEquipmentHeader()
{
	if (!SelectedUpgradeItem.IsValid())
	{
		if (Text_EquipmentName)
		{
			Text_EquipmentName->SetText(FText::GetEmpty());
		}
		if (Text_CurStep)
		{
			Text_CurStep->SetText(FText::GetEmpty());
		}
		if (Text_NextStep)
		{
			Text_NextStep->SetText(FText::GetEmpty());
		}
		return;
	}

	const int32 CurrentLevel = FMath::Max(0, SelectedUpgradeItem.EnhancementLevel);
	if (Text_EquipmentName)
	{
		Text_EquipmentName->SetText(SelectedUpgradeItem.GetDisplayNameText());
	}
	if (Text_CurStep)
	{
		Text_CurStep->SetText(FText::Format(
			NSLOCTEXT("FrontierUpgrade", "CurrentStep", "+{0}"),
			FText::AsNumber(CurrentLevel)));
	}
	if (Text_NextStep)
	{
		const bool bHasNextUpgrade = ResolveNextUpgradeLevel() != nullptr;
		Text_NextStep->SetText(bHasNextUpgrade
			? FText::Format(
				NSLOCTEXT("FrontierUpgrade", "NextStep", "+{0}"),
				FText::AsNumber(CurrentLevel + 1))
			: FText::GetEmpty());
	}
}

void UFrontierUpgradeWidget::HandleInfoSlotDoubleClicked(UFrontierInventorySlotWidget* SlotWidget, const int32 SlotIndex)
{
	ClearSelectedUpgradeItem();
}

void UFrontierUpgradeWidget::HandleInfoSlotDropped(
	UFrontierInventorySlotWidget* SlotWidget,
	const int32 SlotIndex,
	UFrontierInventoryDragDropOperation* DragOperation)
{
	if (!DragOperation)
	{
		return;
	}

	SetSelectedUpgradeItem(DragOperation->DraggedItemInstance);
}

void UFrontierUpgradeWidget::RefreshInfoSlot()
{
	if (!WBP_InfoSlot)
	{
		return;
	}

	FFrontierInventorySlot InfoSlotData;
	InfoSlotData.SlotIndex = 0;
	InfoSlotData.bOccupied = SelectedUpgradeItem.IsValid();
	InfoSlotData.ItemInstance = SelectedUpgradeItem;
	WBP_InfoSlot->SetSlotData(0, InfoSlotData);
	WBP_InfoSlot->SetSelected(SelectedUpgradeItem.IsValid());
}

void UFrontierUpgradeWidget::RefreshUpgradeInfoVisibility()
{
	if (Box_UpgradeInfo)
	{
		Box_UpgradeInfo->SetVisibility(SelectedUpgradeItem.IsValid()
			? ESlateVisibility::Visible
			: ESlateVisibility::Hidden);
	}
}

void UFrontierUpgradeWidget::ClearPreviewSlots()
{
	UVerticalBox* IngredientWrapper = VerticalBox_IngredientSlotWrapper
		? VerticalBox_IngredientSlotWrapper.Get()
		: nullptr;
	if (IngredientWrapper)
	{
		IngredientWrapper->ClearChildren();
	}
	if (VerticalBox_EffectSlotWrapper)
	{
		VerticalBox_EffectSlotWrapper->ClearChildren();
	}
	if (WBP_UpgradeCurrencySlot)
	{
		WBP_UpgradeCurrencySlot->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierUpgradeWidget::RefreshIngredientSlots(const FFrontierUpgradeLevelData* LevelData)
{
	if (!LevelData || !UpgradeIngredientSlotClass)
	{
		return;
	}

	UVerticalBox* Wrapper = VerticalBox_IngredientSlotWrapper
		? VerticalBox_IngredientSlotWrapper.Get()
		: nullptr;
	if (!Wrapper)
	{
		return;
	}

	for (const FFrontierUpgradeMaterialRequirement& Requirement : LevelData->Materials)
	{
		if (Requirement.ItemTemplateId.IsNone() || Requirement.RequiredAmount <= 0)
		{
			continue;
		}

		UFrontierUpgradeIngredientSlot* ingredientSlot = CreateWidget<UFrontierUpgradeIngredientSlot>(
			GetWorld(),
			UpgradeIngredientSlotClass);
		if (!ingredientSlot)
		{
			continue;
		}

		ingredientSlot->SetIngredient(
			Requirement.ItemTemplateId,
			ResolveItemIcon(Requirement.ItemTemplateId),
			CountOwnedItem(Requirement.ItemTemplateId),
			Requirement.RequiredAmount);
		Wrapper->AddChild(ingredientSlot);
	}
}

void UFrontierUpgradeWidget::RefreshCurrencySlot(const FFrontierUpgradeLevelData* LevelData)
{
	if (!LevelData || !WBP_UpgradeCurrencySlot || LevelData->Currencies.IsEmpty())
	{
		return;
	}

	const FFrontierUpgradeCurrencyRequirement& Requirement = LevelData->Currencies[0];
	const TObjectPtr<UTexture2D>* Icon = CurrencyIconMap.Find(Requirement.CurrencyCode);
	WBP_UpgradeCurrencySlot->SetCurrency(
		Requirement.CurrencyCode,
		Icon ? Icon->Get() : nullptr,
		Requirement.RequiredAmount);
	WBP_UpgradeCurrencySlot->SetVisibility(ESlateVisibility::Visible);
}

void UFrontierUpgradeWidget::RefreshEffectSlots(const FFrontierUpgradeLevelData* LevelData)
{
	if (!LevelData || !UpgradeEffectSlotClass || !VerticalBox_EffectSlotWrapper)
	{
		return;
	}

	if (SelectedUpgradeItem.RuntimeGeneratedStats.Num() > 0
		&& !FMath::IsNearlyZero(LevelData->FirstRandomStatIncrease))
	{
		const FFrontierRuntimeStatData& Stat = SelectedUpgradeItem.RuntimeGeneratedStats[0];
		const FString FallbackName = !Stat.OptionId.IsEmpty() ? Stat.OptionId : Stat.StatTag.ToString();
		const FText EffectName = ResolveStatDisplayName(Stat.StatTag, FallbackName);
		UFrontierUpgradeEffectSlot* EffectSlot = CreateWidget<UFrontierUpgradeEffectSlot>(GetWorld(), UpgradeEffectSlotClass);
		if (EffectSlot)
		{
			EffectSlot->SetEffect(
				EffectName,
				FText::Format(NSLOCTEXT("FrontierUpgrade", "StatIncrease", "+{0}"),
					FText::AsNumber(LevelData->FirstRandomStatIncrease)));
			VerticalBox_EffectSlotWrapper->AddChild(EffectSlot);
		}
	}

	if (LevelData->SkillLevelIncrease > 0)
	{
		for (const FFrontierRuntimeSkillData& Skill : SelectedUpgradeItem.RuntimeGeneratedSkills)
		{
			UFrontierUpgradeEffectSlot* EffectSlot = CreateWidget<UFrontierUpgradeEffectSlot>(GetWorld(), UpgradeEffectSlotClass);
			if (!EffectSlot)
			{
				continue;
			}
			EffectSlot->SetEffect(
				FText::FromString(Skill.SkillTemplateId),
				NSLOCTEXT("FrontierUpgrade", "SkillLevelIncrease", "+1 Level"));
			VerticalBox_EffectSlotWrapper->AddChild(EffectSlot);
		}
	}
}

FText UFrontierUpgradeWidget::ResolveStatDisplayName(
	const FGameplayTag StatTag,
	const FString& FallbackName) const
{
	if (!StatTag.IsValid())
	{
		return FText::FromString(FallbackName);
	}

	const UFrontierUpgradeSettings* Settings = GetDefault<UFrontierUpgradeSettings>();
	UDataTable* DisplayTable = Settings
		? Settings->ItemStatDisplayDefinitionDataTable.LoadSynchronous()
		: nullptr;
	if (!DisplayTable)
	{
		return FText::FromString(FallbackName);
	}

	const FString Context = TEXT("FrontierUpgradeStatDisplay");
	const FName RowName = StatTag.GetTagName();
	if (DisplayTable->GetRowStruct() == FFrontierItemStatDisplayNameRow::StaticStruct())
	{
		const FFrontierItemStatDisplayNameRow* Row =
			DisplayTable->FindRow<FFrontierItemStatDisplayNameRow>(RowName, Context, false);
		if (!Row)
		{
			TArray<FFrontierItemStatDisplayNameRow*> Rows;
			DisplayTable->GetAllRows(Context, Rows);
			for (const FFrontierItemStatDisplayNameRow* CandidateRow : Rows)
			{
				if (CandidateRow && CandidateRow->StatTag == StatTag)
				{
					Row = CandidateRow;
					break;
				}
			}
		}
		return Row && !Row->DisplayText.IsEmpty()
			? Row->DisplayText
			: FText::FromString(FallbackName);
	}

	return FText::FromString(FallbackName);
}

int32 UFrontierUpgradeWidget::CountOwnedItem(const FName ItemTemplateId) const
{
	const AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;
	if (!PlayerState)
	{
		return 0;
	}

	int32 OwnedAmount = 0;
	if (const UFrontierRaidInventoryComponent* Inventory = PlayerState->GetRaidInventoryComponent())
	{
		for (const FFrontierInventorySlot& InventorySlot : Inventory->GetSlots())
		{
			if (InventorySlot.bOccupied && InventorySlot.ItemInstance.GetTemplateId() == ItemTemplateId)
			{
				OwnedAmount += InventorySlot.ItemInstance.Quantity;
			}
		}
	}
	if (const UFrontierStorageComponent* Storage = PlayerState->GetStorageComponent())
	{
		for (const FFrontierInventorySlot& InvtorySlot : Storage->GetSlots())
		{
			if (InvtorySlot.bOccupied && InvtorySlot.ItemInstance.GetTemplateId() == ItemTemplateId)
			{
				OwnedAmount += InvtorySlot.ItemInstance.Quantity;
			}
		}
	}
	return OwnedAmount;
}

UTexture2D* UFrontierUpgradeWidget::ResolveItemIcon(const FName ItemTemplateId) const
{
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UFrontierItemCatalogSubsystem* Catalog = GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>())
		{
			if (const FFrontierResolvedItemTemplateData* TemplateData = Catalog->ResolveItemTemplateData(ItemTemplateId))
			{
				return TemplateData->Common.Icon.LoadSynchronous();
			}
		}
	}
	return nullptr;
}

const FFrontierUpgradeLevelData* UFrontierUpgradeWidget::ResolveNextUpgradeLevel() const
{
	if (!SelectedUpgradeItem.IsValid())
	{
		return nullptr;
	}

	const int32 TargetLevel = SelectedUpgradeItem.EnhancementLevel + 1;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UFrontierUpgradeBalanceSubsystem* Balance = GameInstance->GetSubsystem<UFrontierUpgradeBalanceSubsystem>())
		{
			return Balance->FindUpgradeLevel(SelectedUpgradeItem.GetTemplateId(), TargetLevel);
		}
	}
	return nullptr;
}

bool UFrontierUpgradeWidget::IsEquipmentItemInstance(const FFrontierItemInstance& ItemInstance) const
{
	if (!ItemInstance.IsValid())
	{
		return false;
	}

	const EFrontierItemCategory Category = ItemInstance.GetCategory();
	return Category == EFrontierItemCategory::Weapon
		|| Category == EFrontierItemCategory::Armor
		|| Category == EFrontierItemCategory::Accessory;
}
