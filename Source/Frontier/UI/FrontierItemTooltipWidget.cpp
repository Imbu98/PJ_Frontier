#include "UI/FrontierItemTooltipWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Game/FrontierPlayerState.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Progression/FrontierUpgradeBalanceSubsystem.h"
#include "Progression/FrontierUpgradeSettings.h"
#include "Skill/FrontierSkillDataSubsystem.h"
#include "Skill/FrontierSkillDataAsset.h"
#include "Skill/FrontierWeaponSkillGenerationDataAsset.h"
#include "UI/FrontierItemRarityBorderWidget.h"
#include "Weapons/FrontierWeaponDataAsset.h"

void UFrontierItemTooltipWidget::SetItemInstance(const FFrontierItemInstance& InItemInstance)
{
	CurrentItemInstance = InItemInstance;
	RefreshFromItemInstance();
}

void UFrontierItemTooltipWidget::ClearItemInstance()
{
	CurrentItemInstance = FFrontierItemInstance();
	RefreshFromItemInstance();
	// For Merge
}

void UFrontierItemTooltipWidget::SetTooltipScreenPosition(const FVector2D& ScreenPosition)
{
	// 레이아웃 갱신
	ForceLayoutPrepass();

	FVector2D DesiredSize = ResolveTooltipDesiredSize();

	// DesiredSize가 아직 제대로 안 잡힌 경우 대비
	if (DesiredSize.IsNearlyZero())
	{
		DesiredSize = GetDesiredSize();
	}

	// 그래도 0이면 기본값 사용
	if (DesiredSize.IsNearlyZero())
	{
		DesiredSize = FVector2D(300.f, 150.f);
	}

	// 내부 Root 정렬
	if (TooltipContentRoot)
	{
		if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(TooltipContentRoot->Slot))
		{
			CanvasSlot->SetAutoSize(true);
			CanvasSlot->SetAnchors(FAnchors(0.0f, 0.0f));
			CanvasSlot->SetAlignment(FVector2D(0.0f, 0.0f));
			CanvasSlot->SetPosition(FVector2D::ZeroVector);
		}
		else
		{
			TooltipContentRoot->SetRenderTranslation(FVector2D::ZeroVector);
		}
	}

	FVector2D ViewportSize = FVector2D::ZeroVector;

	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	if (ViewportSize.IsNearlyZero())
	{
		SetDesiredSizeInViewport(DesiredSize);
		SetAlignmentInViewport(FVector2D(0.0f, 0.0f));
		SetPositionInViewport(ScreenPosition, false);
		return;
	}

	// 마우스와 툴팁이 너무 붙지 않도록 오프셋
	const FVector2D TooltipOffset(12.f, 12.f);

	FVector2D FinalPosition = ScreenPosition + TooltipOffset;

	// 화면 오른쪽을 넘어가면 왼쪽으로 띄우기
	if (FinalPosition.X + DesiredSize.X > ViewportSize.X)
	{
		FinalPosition.X = ScreenPosition.X - DesiredSize.X - TooltipOffset.X;
	}

	// 화면 아래쪽을 넘어가면 위쪽으로 띄우기
	if (FinalPosition.Y + DesiredSize.Y > ViewportSize.Y)
	{
		FinalPosition.Y = ScreenPosition.Y - DesiredSize.Y - TooltipOffset.Y;
	}

	// 그래도 화면 밖으로 나가지 않게 최종 Clamp
	FinalPosition.X = FMath::Clamp(FinalPosition.X, 0.f, FMath::Max(0.f, ViewportSize.X - DesiredSize.X));
	FinalPosition.Y = FMath::Clamp(FinalPosition.Y, 0.f, FMath::Max(0.f, ViewportSize.Y - DesiredSize.Y));

	SetDesiredSizeInViewport(DesiredSize);
	SetAlignmentInViewport(FVector2D(0.0f, 0.0f));

	// ScreenPosition이 이미 DPI 적용된 좌표라면 false
	SetPositionInViewport(FinalPosition, false);
}

void UFrontierItemTooltipWidget::SetTooltipViewportPositionExact(const FVector2D& ViewportPosition)
{
	ForceLayoutPrepass();

	FVector2D DesiredSize = ResolveTooltipDesiredSize();
	if (DesiredSize.IsNearlyZero())
	{
		DesiredSize = FVector2D(300.0f, 150.0f);
	}

	SetDesiredSizeInViewport(DesiredSize);
	SetAlignmentInViewport(FVector2D::ZeroVector);
	SetPositionInViewport(ViewportPosition, false);
}

FVector2D UFrontierItemTooltipWidget::GetTooltipContentDesiredSize() const
{
	return ResolveTooltipDesiredSize();
}

void UFrontierItemTooltipWidget::RefreshFromItemInstance()
{
	if (ItemIconImage)
	{
		const TSoftObjectPtr<UTexture2D> Icon = CurrentItemInstance.IsValid()
			? CurrentItemInstance.GetIcon()
			: TSoftObjectPtr<UTexture2D>();
		UTexture2D* IconTexture = !Icon.IsNull()
			? Icon.LoadSynchronous()
			: nullptr;
		ItemIconImage->SetBrushFromTexture(IconTexture, true);
		ItemIconImage->SetVisibility(IconTexture ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ItemRarityBorderWidget)
	{
		if (CurrentItemInstance.IsValid())
		{
			ItemRarityBorderWidget->SetItemRarity(CurrentItemInstance.GetDisplayRarity());
		}
		else
		{
			ItemRarityBorderWidget->ClearRarity();
		}
	}

	if (ItemNameText)
	{
		ItemNameText->SetText(CurrentItemInstance.IsValid() ? CurrentItemInstance.GetDisplayNameText() : FText::GetEmpty());
	}

	if (ItemEnhancementText)
	{
		const EFrontierItemCategory Category = CurrentItemInstance.GetCategory();
		const bool bShowEnhancement = CurrentItemInstance.IsValid()
			&& (Category == EFrontierItemCategory::Weapon
				|| Category == EFrontierItemCategory::Armor
				|| Category == EFrontierItemCategory::Accessory);
		ItemEnhancementText->SetText(
			bShowEnhancement
				? FText::Format(
					NSLOCTEXT("FrontierTooltip", "EnhancementLevel", "+{0}"),
					FText::AsNumber(FMath::Max(0, CurrentItemInstance.EnhancementLevel)))
				: FText::GetEmpty());
		ItemEnhancementText->SetVisibility(
			bShowEnhancement
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}

	if (ItemDescriptionText)
	{
		ItemDescriptionText->SetText(CurrentItemInstance.IsValid() ? CurrentItemInstance.GetDescriptionText() : FText::GetEmpty());
	}

	if (ItemTypeText)
	{
		ItemTypeText->SetText(CurrentItemInstance.IsValid()
			? BuildItemTypeText(CurrentItemInstance.GetCategory())
			: FText::GetEmpty());
	}

	RefreshEquipmentScore(CurrentItemInstance);
	RefreshStatsSwitcher(CurrentItemInstance);
}

FText UFrontierItemTooltipWidget::BuildItemTypeText(const EFrontierItemCategory Category) const
{
	switch (Category)
	{
	case EFrontierItemCategory::Weapon:
		return FText::FromString(TEXT("Weapon"));
	case EFrontierItemCategory::Armor:
		return FText::FromString(TEXT("Armor"));
	case EFrontierItemCategory::Accessory:
		return FText::FromString(TEXT("Accessory"));
	case EFrontierItemCategory::Consumable:
		return FText::FromString(TEXT("Consumable"));
	case EFrontierItemCategory::Material:
		return FText::FromString(TEXT("Material"));
	default:
		return FText::FromString(TEXT("Item"));
	}
}

void UFrontierItemTooltipWidget::RefreshStatsSwitcher(const FFrontierItemInstance& ItemInstance)
{
	RefreshCommonStats(ItemInstance);

	const EFrontierItemCategory ItemCategory = ItemInstance.GetCategory();
	if (ItemCategory == EFrontierItemCategory::Weapon
		|| ItemCategory == EFrontierItemCategory::Accessory)
	{
		SetStatsSwitcherIndex(WeaponStatsWidgetIndex);
		RefreshWeaponStats(ItemInstance);
		return;
	}

	if (ItemCategory == EFrontierItemCategory::Armor)
	{
		SetStatsSwitcherIndex(ArmorStatsWidgetIndex);
		RefreshArmorStats(ItemInstance);
		return;
	}

	if (ItemCategory == EFrontierItemCategory::Consumable)
	{
		SetStatsSwitcherIndex(ConsumableStatsWidgetIndex);
		RefreshConsumableStats(ItemInstance);
		return;
	}

	SetStatsSwitcherIndex(MiscStatsWidgetIndex);
	RefreshMiscStats();
}

void UFrontierItemTooltipWidget::RefreshWeaponStats(const FFrontierItemInstance& ItemInstance)
{
	const FString InstanceStatsText = BuildInstanceStatsText(ItemInstance);
	SetTextBlockValue(WeaponAttackPowerText, InstanceStatsText, !InstanceStatsText.IsEmpty());

	const FString GeneratedSkillsText = BuildGeneratedSkillsText(ItemInstance);
	SetTextBlockValue(WeaponSkillsText, GeneratedSkillsText, !GeneratedSkillsText.IsEmpty());
}

void UFrontierItemTooltipWidget::RefreshArmorStats(const FFrontierItemInstance& ItemInstance)
{
	const FString InstanceStatsText = BuildInstanceStatsText(ItemInstance);
	SetTextBlockValue(ArmorDefenseText, InstanceStatsText, !InstanceStatsText.IsEmpty());

	const FString GeneratedSkillsText = BuildGeneratedSkillsText(ItemInstance);
	SetTextBlockValue(ArmorSkillsText, GeneratedSkillsText, !GeneratedSkillsText.IsEmpty());
}

void UFrontierItemTooltipWidget::RefreshConsumableStats(const FFrontierItemInstance& ItemInstance)
{
	const float HealthRestoreAmount = ItemInstance.GetHealthRestoreAmount();
	SetTextBlockValue(
		ConsumableHealthRestoreText,
		HealthRestoreAmount > 0.0f ? FString::Printf(TEXT("Heal %.0f"), HealthRestoreAmount) : FString(),
		HealthRestoreAmount > 0.0f);
}

void UFrontierItemTooltipWidget::RefreshMiscStats()
{
}

void UFrontierItemTooltipWidget::RefreshCommonStats(const FFrontierItemInstance& ItemInstance)
{
	if (!ItemInstance.IsValid())
	{
		SetTextBlockValue(QuantityText, FString(), false);
		SetTextBlockValue(WeightText, FString(), false);
		SetTextBlockValue(SellPriceText, FString(), false);
		SetTextBlockValue(RarityText, FString(), false);
		return;
	}
	
	const bool bShowQuantity = ItemInstance.IsStackable() && ItemInstance.Quantity > 0;
	SetTextBlockValue(
		QuantityText,
		bShowQuantity ? FString::Printf(TEXT("%d개"), ItemInstance.Quantity) : FString(),
		bShowQuantity);

	const float DisplayWeight = ItemInstance.GetItemWeight();
	const bool bShowWeight = DisplayWeight > 0.0f;
	SetTextBlockValue(
		WeightText,
		bShowWeight ? FString::Printf(TEXT("%.1f kg"), DisplayWeight) : FString(),
		bShowWeight);

	const int32 DisplaySellPrice = ItemInstance.GetSellPriceGold();
	const bool bShowSellPrice = DisplaySellPrice > 0;
	SetTextBlockValue(
		SellPriceText,
		bShowSellPrice ? FString::Printf(TEXT("%d 원"), DisplaySellPrice) : FString(),
		bShowSellPrice);

	const FString RarityName = BuildItemRarityDisplayName(ItemInstance.GetDisplayRarity());
	SetTextBlockValue(RarityText, RarityName, !RarityName.IsEmpty());
}

void UFrontierItemTooltipWidget::RefreshEquipmentScore(FFrontierItemInstance& ItemInstance)
{
	const EFrontierItemCategory Category = ItemInstance.GetCategory();
	const bool bIsEquipment = ItemInstance.IsValid()
		&& (Category == EFrontierItemCategory::Weapon
			|| Category == EFrontierItemCategory::Armor
			|| Category == EFrontierItemCategory::Accessory)
		&& ItemInstance.GetEquipSlot() != EFrontierEquipmentSlot::None;

	if (!bIsEquipment)
	{
		SetTextBlockValue(Text_EquipmentScore, FString(), false);
		return;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierUpgradeBalanceSubsystem* UpgradeBalance = GameInstance
		? GameInstance->GetSubsystem<UFrontierUpgradeBalanceSubsystem>()
		: nullptr;
	if (UpgradeBalance)
	{
		UpgradeBalance->RecalculateEquipmentScore(ItemInstance);
	}

	const float EquipmentScore = FMath::Max(0.0f, ItemInstance.EquipmentScore);
	SetTextBlockValue(
		Text_EquipmentScore,
		FString::Printf(TEXT("%.1f"), EquipmentScore),
		bIsEquipment);
}

FString UFrontierItemTooltipWidget::BuildInstanceStatsText(const FFrontierItemInstance& ItemInstance) const
{
	FString StatsText;
	for (const FFrontierRuntimeStatData& InstanceStat : ItemInstance.RuntimeGeneratedStats)
	{
		const FString DisplayName = InstanceStat.StatTag.IsValid()
			? ResolveStatDisplayName(InstanceStat.StatTag).ToString()
			: InstanceStat.OptionId;
		if (DisplayName.IsEmpty())
		{
			continue;
		}

		if (!StatsText.IsEmpty())
		{
			StatsText += TEXT("\n");
		}

		StatsText += FString::Printf(
			TEXT("%s +%.0f%s"),
			*DisplayName,
			InstanceStat.FinalValue,
			InstanceStat.Unit.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" %s"), *InstanceStat.Unit));
	}

	return StatsText;
}

FText UFrontierItemTooltipWidget::ResolveStatDisplayName(const FGameplayTag StatTag) const
{
	if (!StatTag.IsValid())
	{
		return FText::FromString(StatTag.GetTagName().ToString());
	}

	const UFrontierUpgradeSettings* UpgradeSettings = GetDefault<UFrontierUpgradeSettings>();
	UDataTable* DisplayTable = UpgradeSettings
		? UpgradeSettings->ItemStatDisplayDefinitionDataTable.LoadSynchronous()
		: nullptr;

	if (!DisplayTable)
	{
		return FText::FromString(StatTag.GetTagName().ToString());
	}

	const FString Context = TEXT("ItemStatDisplayName");
	const FName TagRowName = StatTag.GetTagName();
	const FFrontierItemStatDisplayNameRow* Row = DisplayTable->FindRow<FFrontierItemStatDisplayNameRow>(TagRowName, Context, false);

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

	if (!Row)
	{
		return FText::FromString(StatTag.GetTagName().ToString());
	}

	return !Row->DisplayText.IsEmpty()
		? Row->DisplayText
		: FText::FromString(StatTag.GetTagName().ToString());
}

FString UFrontierItemTooltipWidget::BuildGeneratedSkillsText(const FFrontierItemInstance& ItemInstance) const
{
	if (ItemInstance.RuntimeGeneratedSkills.IsEmpty())
	{
		return FString();
	}

	TArray<FFrontierRuntimeSkillData> SortedRuntimeSkills = ItemInstance.RuntimeGeneratedSkills;
	SortedRuntimeSkills.Sort([](const FFrontierRuntimeSkillData& Left, const FFrontierRuntimeSkillData& Right)
	{
		return Left.SlotIndex < Right.SlotIndex;
	});

	TArray<FString> SkillLines;
	for (const FFrontierRuntimeSkillData& RuntimeSkill : SortedRuntimeSkills)
	{
		if (!RuntimeSkill.SkillTag.IsValid() && RuntimeSkill.SkillTemplateId.IsEmpty())
		{
			continue;
		}

		const FFrontierSkillInfo* SkillInfo = FindSkillInfo(ItemInstance, RuntimeSkill.SkillTag, RuntimeSkill.SkillTemplateId);
		const FString SkillName = SkillInfo && !SkillInfo->DisplayName.IsEmpty()
			? SkillInfo->DisplayName.ToString()
			: (!RuntimeSkill.SkillTemplateId.IsEmpty() ? RuntimeSkill.SkillTemplateId : RuntimeSkill.SkillTag.GetTagName().ToString());
		SkillLines.Add(FString::Printf(
			TEXT("%d. %s Lv.%d"),
			RuntimeSkill.SlotIndex + 1,
			*SkillName,
			RuntimeSkill.SkillLevel));
	}

	return FString::Join(SkillLines, TEXT("\n"));
}

FString UFrontierItemTooltipWidget::BuildSkillRarityDisplayName(const EFrontierItemRarity SkillRarity) const
{
	switch (SkillRarity)
	{
	case EFrontierItemRarity::Rare:
		return TEXT("Rare");
	case EFrontierItemRarity::Epic:
		return TEXT("Epic");
	case EFrontierItemRarity::Legendary:
		return TEXT("Legendary");
	case EFrontierItemRarity::Common:
	default:
		return TEXT("Common");
	}
}

FString UFrontierItemTooltipWidget::BuildItemRarityDisplayName(const EFrontierItemRarity ItemRarity) const
{
	switch (ItemRarity)
	{
	case EFrontierItemRarity::Rare:
		return TEXT("희귀");
	case EFrontierItemRarity::Epic:
		return TEXT("에픽");
	case EFrontierItemRarity::Legendary:
		return TEXT("레전더리");
	case EFrontierItemRarity::Common:
	default:
		return TEXT("커먼");
	}
}

const FFrontierSkillInfo* UFrontierItemTooltipWidget::FindSkillInfo(
	const FFrontierItemInstance& ItemInstance,
	const FGameplayTag SkillTag,
	const FString& SkillId) const
{
	if (!SkillTag.IsValid() && SkillId.IsEmpty())
	{
		return nullptr;
	}

	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UFrontierSkillDataSubsystem* SkillDataSubsystem = GameInstance ? GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>() : nullptr;
	const FFrontierSkillTableRow* SkillRow = nullptr;
	if (SkillDataSubsystem)
	{
		SkillRow = !SkillId.IsEmpty() ? SkillDataSubsystem->FindSkillDataByIdString(SkillId) : nullptr;
		if (!SkillRow && SkillTag.IsValid())
		{
			SkillRow = SkillDataSubsystem->FindSkillData(SkillTag);
		}
	}
	if (SkillRow)
	{
		ResolvedSkillInfoScratch = FFrontierSkillInfo();
		ResolvedSkillInfoScratch.SkillId = !SkillId.IsEmpty()
			? FName(*SkillId)
			: SkillDataSubsystem->FindSkillId(SkillTag);
		ResolvedSkillInfoScratch.SkillTag = SkillRow->SkillTag;
		ResolvedSkillInfoScratch.DisplayName = SkillRow->DisplayName;
		ResolvedSkillInfoScratch.Description = SkillRow->Description;
		ResolvedSkillInfoScratch.DescriptionParameters = SkillRow->DescriptionParameters;
		ResolvedSkillInfoScratch.Icon = SkillRow->Icon.LoadSynchronous();
		ResolvedSkillInfoScratch.AbilityClass = SkillRow->AbilityClass;
		ResolvedSkillInfoScratch.Cooldown = SkillRow->Cooldown;
		ResolvedSkillInfoScratch.CooldownTag = SkillRow->CooldownTag;
		ResolvedSkillInfoScratch.StaminaCost = SkillRow->StaminaCost;
		ResolvedSkillInfoScratch.MaxLevel = SkillRow->MaxLevel;
		return &ResolvedSkillInfoScratch;
	}

	const UFrontierWeaponDataAsset* WeaponData = ItemInstance.GetWeaponData().LoadSynchronous();
	const UFrontierWeaponSkillGenerationDataAsset* SkillGenerationData = ItemInstance.GetSkillGenerationData().LoadSynchronous();

	const UFrontierSkillDataAsset* WeaponSkillDataAsset = (WeaponData && SkillGenerationData)
		? SkillGenerationData->FindSkillDataAssetForWeaponType(WeaponData->WeaponTypeTag)
		: nullptr;
	if (WeaponSkillDataAsset)
	{
		if (const FFrontierSkillInfo* SkillInfo = WeaponSkillDataAsset->FindSkillInfo(SkillTag))
		{
			return SkillInfo;
		}
	}

	const APlayerController* OwningPlayer = GetOwningPlayer();
	const AFrontierPlayerState* PlayerState = OwningPlayer ? OwningPlayer->GetPlayerState<AFrontierPlayerState>() : nullptr;
	const UFrontierEquipmentSkillComponent* EquipmentSkillComponent = PlayerState ? PlayerState->GetEquipmentSkillComponent() : nullptr;
	return EquipmentSkillComponent ? EquipmentSkillComponent->FindSkillInfo(SkillTag) : nullptr;
}

void UFrontierItemTooltipWidget::SetStatsSwitcherIndex(const int32 WidgetIndex)
{
	if (!ItemStatsSwitcher)
	{
		return;
	}

	const int32 ClampedIndex = FMath::Clamp(WidgetIndex, 0, FMath::Max(0, ItemStatsSwitcher->GetNumWidgets() - 1));
	ItemStatsSwitcher->SetActiveWidgetIndex(ClampedIndex);
}

void UFrontierItemTooltipWidget::SetTextBlockValue(UTextBlock* TextBlock, const FString& Text, const bool bVisible)
{
	if (!TextBlock)
	{
		return;
	}

	TextBlock->SetText(bVisible ? FText::FromString(Text) : FText::GetEmpty());
	TextBlock->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

FVector2D UFrontierItemTooltipWidget::ResolveTooltipDesiredSize() const
{
	auto SanitizeDesiredSize = [](const FVector2D& InSize) -> FVector2D
	{
		const float SafeX = FMath::Max(1.0f, InSize.X);
		const float SafeY = FMath::Max(1.0f, InSize.Y);
		return FVector2D(SafeX, SafeY);
	};

	if (TooltipContentRoot)
	{
		const FVector2D CachedDesiredSize = TooltipContentRoot->GetDesiredSize();
		if (CachedDesiredSize.X > 1.0f && CachedDesiredSize.Y > 1.0f)
		{
			return SanitizeDesiredSize(CachedDesiredSize);
		}
	}

	const FVector2D WidgetDesiredSize = GetDesiredSize();
	return SanitizeDesiredSize(WidgetDesiredSize);
}
