#include "UI/FrontierPlayerStatusWidget.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Blueprint/WidgetTree.h"
#include "Character/FrontierBaseCharacter.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameState.h"
#include "Game/FrontierPlayerState.h"
#include "UI/FrontierSkillBarWidget.h"
#include "UI/FrontierQuickSlotBarWidget.h"
#include "Components/FrontierQuickSlotComponent.h"
#include "UI/FrontierCrossbowWidget.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "TimerManager.h"
#include "Components/Overlay.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Styling/SlateBrush.h"
#include "UObject/UnrealType.h"
#include "../Game/FrontierSteamSubsystem.h"

void UFrontierPlayerStatusWidget::NativeConstruct()
{
	
	Super::NativeConstruct();

	BindToOwningPawnAttributes();
	BindSkillBarWidget();
	BindQuickSlotWidget();
	BindWeaponAttackWidget();
	BuildRaidTimerImageSlots();
	HideInteractionProgress();
	HideInteractionPrompt();
	HideDamageIndicator();
	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->OnClicked.AddDynamic(this, &UFrontierPlayerStatusWidget::HandlePrevSpectatorClicked);
	}
	if (NextSpectatorButton)
	{
		NextSpectatorButton->OnClicked.AddDynamic(this, &UFrontierPlayerStatusWidget::HandleNextSpectatorClicked);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ReadyCountRefreshTimerHandle,
			this,
			&UFrontierPlayerStatusWidget::HandleReadyCountRefreshTick,
			0.25f,
			true);
	}
	
	
}

void UFrontierPlayerStatusWidget::NativeDestruct()
{
	
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningClearTimerHandle);
		World->GetTimerManager().ClearTimer(ReadyCountRefreshTimerHandle);
	}
	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->OnClicked.RemoveAll(this);
	}
	if (NextSpectatorButton)
	{
		NextSpectatorButton->OnClicked.RemoveAll(this);
	}
	UnbindFromOwningPawnAttributes();
	if (SkillBarWidget)
	{
		SkillBarWidget->SetSkillComponent(nullptr);
	}
	if (QuickSlotBarWidget)
	{
		QuickSlotBarWidget->SetQuickSlotComponent(nullptr);
	}
	CachedSkillComponent = nullptr;
	CachedEquipmentComponent = nullptr;
	RaidTimerImageSlots.Reset();
	Super::NativeDestruct();
}

void UFrontierPlayerStatusWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bDamageIndicatorActive || !DamageIndicatorImage)
	{
		return;
	}

	DamageIndicatorElapsedTime += FMath::Max(0.0f, InDeltaTime);
	const float SafeFadeDuration = FMath::Max(0.01f, DamageIndicatorFadeDuration);
	const float FadeAlpha = 1.0f - FMath::Clamp(DamageIndicatorElapsedTime / SafeFadeDuration, 0.0f, 1.0f);
	DamageIndicatorImage->SetRenderOpacity(FadeAlpha * FMath::Clamp(DamageIndicatorPeakOpacity, 0.0f, 1.0f));

	if (DamageIndicatorElapsedTime >= SafeFadeDuration)
	{
		HideDamageIndicator();
	}
}

void UFrontierPlayerStatusWidget::RebindToOwningPawnAttributes()
{
	
	BindToOwningPawnAttributes();
	BindSkillBarWidget();
	BindQuickSlotWidget();
	BindWeaponAttackWidget();
}

void UFrontierPlayerStatusWidget::RefreshSkillBarWidget()
{
	BindSkillBarWidget();
	if (SkillBarWidget)
	{
		SkillBarWidget->RefreshSlots();
	}
}

void UFrontierPlayerStatusWidget::AttachLobbyStatusWidget(UWidget* InWidget)
{
	if (!InWidget || !LobbyStatusHost)
	{
		return;
	}

	InWidget->RemoveFromParent();
	if (UVerticalBoxSlot* AddedSlot = LobbyStatusHost->AddChildToVerticalBox(InWidget))
	{
		AddedSlot->SetHorizontalAlignment(HAlign_Fill);
		AddedSlot->SetVerticalAlignment(VAlign_Top);
	}
}

void UFrontierPlayerStatusWidget::ShowTransientWarning(const FText& WarningText)
{
	if (!CenterWarningText)
	{
		return;
	}

	CenterWarningText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)));
	CenterWarningText->SetText(WarningText);
	CenterWarningText->SetVisibility(ESlateVisibility::HitTestInvisible);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WarningClearTimerHandle);
		World->GetTimerManager().SetTimer(
			WarningClearTimerHandle,
			this,
			&UFrontierPlayerStatusWidget::ClearTransientWarning,
			3.0f,
			false);
	}
}

void UFrontierPlayerStatusWidget::SetExtractionProgress(const float InProgress)
{
	CachedExtractionProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);

	const bool bShowProgress = !IsLobbyWorld() && CachedExtractionProgress > 0.0f && CachedExtractionProgress < 1.0f;
	if (ExtractionProgressBar)
	{
		ExtractionProgressBar->SetPercent(CachedExtractionProgress);
		ExtractionOverlay->SetVisibility(bShowProgress ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ExtractionProgressText)
	{
		ExtractionProgressText->SetText(FText::FromString(FString::Printf(TEXT("Extracting %d%%"), FMath::RoundToInt(CachedExtractionProgress * 100.0f))));
		ExtractionProgressText->SetVisibility(bShowProgress ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UFrontierPlayerStatusWidget::ShowInteractionProgress()
{
	if (WBP_CircleProgressBar)
	{
		ApplyInteractionProgressPercent(0.0f);
		WBP_CircleProgressBar->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (InteractionProgressPercentText)
	{
		InteractionProgressPercentText->SetText(FText::FromString(TEXT("0%")));
		InteractionProgressPercentText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UFrontierPlayerStatusWidget::SetInteractionProgress(const float InProgress)
{
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	ApplyInteractionProgressPercent(ClampedProgress);
	if (InteractionProgressPercentText)
	{
		InteractionProgressPercentText->SetText(FText::FromString(FString::Printf(
			TEXT("%d%%"),
			FMath::RoundToInt(ClampedProgress * 100.0f))));
	}
}

void UFrontierPlayerStatusWidget::HideInteractionProgress()
{
	if (WBP_CircleProgressBar)
	{
		ApplyInteractionProgressPercent(0.0f);
		WBP_CircleProgressBar->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InteractionProgressPercentText)
	{
		InteractionProgressPercentText->SetText(FText::FromString(TEXT("0%")));
		InteractionProgressPercentText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierPlayerStatusWidget::ShowInteractionPrompt(
	const FText& TargetDisplayName,
	const FText& ActionText)
{
	if (InteractionPromptContainer)
	{
		InteractionPromptContainer->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (InteractionKeyText)
	{
		InteractionKeyText->SetText(FText::FromString(TEXT("[ F ] 키로")));
		InteractionKeyText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (InteractionTargetNameText)
	{
		InteractionTargetNameText->SetText(FText::Format(
			NSLOCTEXT("FrontierInteraction", "TargetActionPrompt", "\"{0}\" {1}"),
			TargetDisplayName,
			ActionText));
		InteractionTargetNameText->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UFrontierPlayerStatusWidget::HideInteractionPrompt()
{
	if (InteractionPromptContainer)
	{
		InteractionPromptContainer->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InteractionKeyText)
	{
		InteractionKeyText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InteractionTargetNameText)
	{
		InteractionTargetNameText->SetText(FText::GetEmpty());
		InteractionTargetNameText->SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UFrontierPlayerStatusWidget::ApplyInteractionProgressPercent(const float InProgress)
{
	if (!WBP_CircleProgressBar)
	{
		return false;
	}

	UFunction* SetPercentFunction = WBP_CircleProgressBar->FindFunction(TEXT("SetPercent"));
	FProperty* PercentParameter = nullptr;
	int32 InputParameterCount = 0;
	if (SetPercentFunction)
	{
		for (TFieldIterator<FProperty> PropertyIterator(SetPercentFunction); PropertyIterator; ++PropertyIterator)
		{
			FProperty* Property = *PropertyIterator;
			if (!Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				continue;
			}

			++InputParameterCount;
			PercentParameter = Property;
		}
	}

	FFloatProperty* FloatParameter = CastField<FFloatProperty>(PercentParameter);
	FDoubleProperty* DoubleParameter = CastField<FDoubleProperty>(PercentParameter);
	if (!SetPercentFunction || InputParameterCount != 1 || (!FloatParameter && !DoubleParameter))
	{
		if (!bLoggedInvalidInteractionProgressContract)
		{
			bLoggedInvalidInteractionProgressContract = true;
			FRONTIER_LOG(Warning,
				TEXT("Bound WBP_CircleProgressBar must implement SetPercent with one float/real input. WidgetClass=%s"),
				*GetNameSafe(WBP_CircleProgressBar->GetClass()));
		}
		return false;
	}

	uint8* Parameters = static_cast<uint8*>(FMemory_Alloca(SetPercentFunction->ParmsSize));
	FMemory::Memzero(Parameters, SetPercentFunction->ParmsSize);
	const float ClampedProgress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	if (FloatParameter)
	{
		FloatParameter->SetPropertyValue_InContainer(Parameters, ClampedProgress);
	}
	else
	{
		DoubleParameter->SetPropertyValue_InContainer(Parameters, static_cast<double>(ClampedProgress));
	}

	WBP_CircleProgressBar->ProcessEvent(SetPercentFunction, Parameters);

	// The current round-progress material exposes the legacy misspelled scalar
	// parameter "Precent", while the Blueprint function uses "Percent". Update
	// the generated MID directly as a compatibility bridge so the authored WBP
	// visibly fills without changing or replacing the user's asset.
	const FObjectProperty* DynamicMaterialProperty = FindFProperty<FObjectProperty>(
		WBP_CircleProgressBar->GetClass(),
		TEXT("RoundProgressbarInst"));
	if (DynamicMaterialProperty)
	{
		if (UMaterialInstanceDynamic* DynamicMaterial = Cast<UMaterialInstanceDynamic>(
			DynamicMaterialProperty->GetObjectPropertyValue_InContainer(WBP_CircleProgressBar)))
		{
			DynamicMaterial->SetScalarParameterValue(TEXT("Precent"), ClampedProgress);
		}
	}

	return true;
}

void UFrontierPlayerStatusWidget::SetWeaponAttackCharge(const float ChargeProgress)
{
	if (CrossbowWidget)
	{
		CrossbowWidget->SetChargeProgress(ChargeProgress);
	}
}

void UFrontierPlayerStatusWidget::BindWeaponAttackWidget()
{
	UFrontierEquipmentComponent* NewEquipmentComponent = GetOwningPlayerPawn()
		? GetOwningPlayerPawn()->FindComponentByClass<UFrontierEquipmentComponent>()
		: nullptr;
	if (CachedEquipmentComponent != NewEquipmentComponent)
	{
		if (CachedEquipmentComponent)
		{
			CachedEquipmentComponent->OnCurrentWeaponChanged.RemoveAll(this);
		}

		CachedEquipmentComponent = NewEquipmentComponent;
		if (CachedEquipmentComponent)
		{
			CachedEquipmentComponent->OnCurrentWeaponChanged.AddUniqueDynamic(
				this,
				&UFrontierPlayerStatusWidget::HandleCurrentWeaponChanged);
		}
	}

	RefreshWeaponAttackWidgetVisibility();
}

void UFrontierPlayerStatusWidget::RefreshWeaponAttackWidgetVisibility()
{
	if (!CrossbowWidget)
	{
		return;
	}

	const UFrontierWeaponDataAsset* WeaponData = CachedEquipmentComponent
		? CachedEquipmentComponent->GetCurrentWeaponData()
		: nullptr;
	const bool bBowEquipped = WeaponData && !WeaponData->BowAttack.ArrowProjectileClass.IsNull();
	CrossbowWidget->SetChargeProgress(0.0f);
	CrossbowWidget->SetVisibility(
		bBowEquipped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UFrontierPlayerStatusWidget::HandleCurrentWeaponChanged(AActor* NewWeapon, const int32 NewWeaponIndex)
{
	RefreshWeaponAttackWidgetVisibility();
}

void UFrontierPlayerStatusWidget::BindToOwningPawnAttributes()
{
	
	UnbindFromOwningPawnAttributes();

	CachedCharacter = Cast<AFrontierBaseCharacter>(GetOwningPlayerPawn());
	CachedAbilitySystemComponent = CachedCharacter ? CachedCharacter->GetFrontierAbilitySystemComponent() : nullptr;
	CachedAttributeSet = CachedCharacter
		? const_cast<UFrontierAttributeSet*>(CachedCharacter->GetFrontierAttributeSet())
		: nullptr;
	RefreshObservedPlayerState();
	BindQuickSlotWidget();

	if (!CachedAbilitySystemComponent || !CachedAttributeSet)
	{
		if (!bLoggedAttributeBindPending)
		{
			FRONTIER_LOG(
				Log,
				TEXT("Player status widget is waiting for the approved pawn, ASC, and AttributeSet."));
			bLoggedAttributeBindPending = true;
		}
		return;
	}

	const bool bRecoveredFromPendingBind = bLoggedAttributeBindPending;
	bLoggedAttributeBindPending = false;
	HealthChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute())
		.AddUObject(this, &UFrontierPlayerStatusWidget::HandleHealthChanged);
	MaxHealthChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute())
		.AddUObject(this, &UFrontierPlayerStatusWidget::HandleMaxHealthChanged);
	StaminaChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetStaminaAttribute())
		.AddUObject(this, &UFrontierPlayerStatusWidget::HandleStaminaChanged);
	MaxStaminaChangedHandle = CachedAbilitySystemComponent
		->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxStaminaAttribute())
		.AddUObject(this, &UFrontierPlayerStatusWidget::HandleMaxStaminaChanged);

	FRONTIER_LOG(Log, TEXT("Player status widget bound to Health/MaxHealth/Stamina/MaxStamina delegates. Character=%s"), *GetNameSafe(CachedCharacter));
	if (bRecoveredFromPendingBind)
	{
		FRONTIER_LOG(
			Log,
			TEXT("Player status widget initialized after pawn approval. Character=%s PlayerState=%s"),
			*GetNameSafe(CachedCharacter),
			*GetNameSafe(CachedPlayerState));
	}
	RefreshFromCachedAttributes();
}

void UFrontierPlayerStatusWidget::UnbindFromOwningPawnAttributes()
{
	if (CachedEquipmentComponent)
	{
		CachedEquipmentComponent->OnCurrentWeaponChanged.RemoveAll(this);
		CachedEquipmentComponent = nullptr;
	}

	if (CachedAbilitySystemComponent)
	{
		if (HealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetHealthAttribute())
				.Remove(HealthChangedHandle);
		}

		if (MaxHealthChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxHealthAttribute())
				.Remove(MaxHealthChangedHandle);
		}

		if (StaminaChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetStaminaAttribute())
				.Remove(StaminaChangedHandle);
		}

		if (MaxStaminaChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent
				->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMaxStaminaAttribute())
				.Remove(MaxStaminaChangedHandle);
		}
	}

	HealthChangedHandle.Reset();
	MaxHealthChangedHandle.Reset();
	StaminaChangedHandle.Reset();
	MaxStaminaChangedHandle.Reset();
	CachedCharacter = nullptr;
	CachedAbilitySystemComponent = nullptr;
	CachedAttributeSet = nullptr;

	if (CachedPlayerState)
	{
		CachedPlayerState->OnTeamIdChanged.RemoveAll(this);
		CachedPlayerState->OnReadyChanged.RemoveAll(this);
	}

	if (SkillBarWidget)
	{
		SkillBarWidget->SetSkillComponent(nullptr);
	}

	CachedPlayerState = nullptr;
	CachedSkillComponent = nullptr;
}

void UFrontierPlayerStatusWidget::RefreshFromCachedAttributes()
{
	

	if (!HPBar || !CachedCharacter || !CachedAttributeSet)
	{
		FRONTIER_LOG(Warning, TEXT("Player HP UI refresh skipped. HPBar=%s Character=%s AttributeSet=%s"),
			*GetNameSafe(HPBar),
			*GetNameSafe(CachedCharacter),
			*GetNameSafe(CachedAttributeSet));
		return;
	}

	const float Health = CachedAttributeSet->GetHealth();
	const float MaxHealth = CachedAttributeSet->GetMaxHealth();
	const float SafeMaxHealth = FMath::Max(MaxHealth, 1.0f);
	const float HealthPercent = FMath::Clamp(Health / SafeMaxHealth, 0.0f, 1.0f);

	HPBar->SetPercent(HealthPercent);
	if (HealthText && (CachedCharacter->IsDead() || Health <= 0.0f))
	{
		HealthText->SetText(FText::FromString(TEXT("DEAD")));
	}
	else if (HealthText)
	{
		HealthText->SetText(FText::FromString(FString::Printf(TEXT("HP %.0f / %.0f"), Health, MaxHealth)));
	}

	const float Stamina = CachedAttributeSet->GetStamina();
	const float MaxStamina = CachedAttributeSet->GetMaxStamina();
	const float SafeMaxStamina = FMath::Max(MaxStamina, 1.0f);
	const float StaminaPercent = FMath::Clamp(Stamina / SafeMaxStamina, 0.0f, 1.0f);

	if (StaminaBar)
	{
		StaminaBar->SetPercent(StaminaPercent);
	}

	if (StaminaText)
	{
		StaminaText->SetText(FText::FromString(FString::Printf(TEXT("SP %.0f / %.0f"), Stamina, MaxStamina)));
	}

	RefreshTeamAndReadyTexts();
	RefreshRaidTimerDisplay();
	SetExtractionProgress(CachedExtractionProgress);
	RefreshSpectatorControls();

	//FRONTIER_LOG(Log, TEXT("Player status UI refreshed. Health=%.2f/%.2f Stamina=%.2f/%.2f"), Health, MaxHealth, Stamina, MaxStamina);
}

void UFrontierPlayerStatusWidget::BindSkillBarWidget()
{
	UFrontierEquipmentSkillComponent* NewSkillComponent = CachedPlayerState
		? CachedPlayerState->GetEquipmentSkillComponent()
		: nullptr;

	if (!NewSkillComponent)
	{
		if (AFrontierPlayerState* OwningPlayerState = GetOwningPlayer()
			? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
			: nullptr)
		{
			NewSkillComponent = OwningPlayerState->GetEquipmentSkillComponent();
		}
	}

	if (CachedSkillComponent == NewSkillComponent)
	{
		if (SkillBarWidget)
		{
			SkillBarWidget->RefreshSlots();
		}
		return;
	}

	CachedSkillComponent = NewSkillComponent;
	if (SkillBarWidget)
	{
		SkillBarWidget->SetSkillComponent(CachedSkillComponent);
	}
}

void UFrontierPlayerStatusWidget::BindQuickSlotWidget()
{
	if (!QuickSlotBarWidget)
	{
		return;
	}

	AFrontierPlayerState* PlayerState = CachedPlayerState;
	if (!PlayerState && GetOwningPlayer())
	{
		PlayerState = GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>();
	}

	QuickSlotBarWidget->SetEditMode(false);
	QuickSlotBarWidget->SetQuickSlotComponent(PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr);
}

void UFrontierPlayerStatusWidget::RefreshObservedPlayerState()
{
	AFrontierPlayerState* NewPlayerState = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>() : nullptr;
	if (!NewPlayerState && CachedCharacter)
	{
		NewPlayerState = CachedCharacter->GetPlayerState<AFrontierPlayerState>();
	}

	if (CachedPlayerState == NewPlayerState)
	{
		return;
	}

	if (CachedPlayerState)
	{
		CachedPlayerState->OnTeamIdChanged.RemoveAll(this);
		CachedPlayerState->OnReadyChanged.RemoveAll(this);
	}

	CachedPlayerState = NewPlayerState;

	if (CachedPlayerState)
	{
		CachedPlayerState->OnTeamIdChanged.AddUObject(this, &UFrontierPlayerStatusWidget::HandleObservedTeamChanged);
		CachedPlayerState->OnReadyChanged.AddUObject(this, &UFrontierPlayerStatusWidget::HandleObservedReadyChanged);
	}

	BindSkillBarWidget();
}

void UFrontierPlayerStatusWidget::RefreshTeamAndReadyTexts()
{
	RefreshObservedPlayerState();

	if (TeamText && CachedPlayerState)
	{
		const int32 TeamId = CachedPlayerState->GetTeamId();
		TeamText->SetText(FText::FromString(TeamId > 0
			? FString::Printf(TEXT("Team : %d"), TeamId)
			: TEXT("Team : none")));
	}
	else if (TeamText)
	{
		TeamText->SetText(FText::FromString(TEXT("Team : none")));
	}

	if (ReadyText && CachedPlayerState)
	{
		ReadyText->SetVisibility(IsLobbyWorld() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ReadyText->SetText(FText::FromString(CachedPlayerState->IsRaidReady() ? TEXT("READY") : TEXT("NOT READY")));
	}
	else if (ReadyText)
	{
		ReadyText->SetVisibility(IsLobbyWorld() ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		ReadyText->SetText(FText::FromString(TEXT("NOT READY")));
	}

	RefreshReadyCountText();
}

bool UFrontierPlayerStatusWidget::IsLobbyWorld() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FString WorldPackageName = World->GetOutermost()->GetName();
	return WorldPackageName.Contains(TEXT("Lobby"), ESearchCase::IgnoreCase);
}

void UFrontierPlayerStatusWidget::RefreshReadyCountText()
{
	if (!ReadyCountText)
	{
		return;
	}

	if (!IsLobbyWorld())
	{
		ReadyCountText->SetText(FText::GetEmpty());
		ReadyCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	const AFrontierGameState* FrontierGameState = GetWorld() ? GetWorld()->GetGameState<AFrontierGameState>() : nullptr;
	if (!FrontierGameState)
	{
		ReadyCountText->SetText(FText::GetEmpty());
		ReadyCountText->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	ReadyCountText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	ReadyCountText->SetText(FText::FromString(FString::Printf(
		TEXT("%d / %d"),
		FrontierGameState->GetReadyPlayerCount(),
		FrontierGameState->GetConnectedPlayerCount())));
}

void UFrontierPlayerStatusWidget::BuildRaidTimerImageSlots()
{
	RaidTimerImageSlots.Reset();
	if (!RaidTimerImageContainer || !WidgetTree)
	{
		return;
	}

	RaidTimerImageContainer->ClearChildren();
	static const TCHAR* SlotNames[] =
	{
		TEXT("RaidTimerMinutesTens"),
		TEXT("RaidTimerMinutesOnes"),
		TEXT("RaidTimerSeparator"),
		TEXT("RaidTimerSecondsTens"),
		TEXT("RaidTimerSecondsOnes")
	};

	for (const TCHAR* SlotName : SlotNames)
	{
		UImage* Image = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), FName(SlotName));
		if (!Image)
		{
			continue;
		}

		Image->SetVisibility(ESlateVisibility::Collapsed);
		Image->SetDesiredSizeOverride(RaidTimerDigitSize);
		if (UHorizontalBoxSlot* ImageSlot = RaidTimerImageContainer->AddChildToHorizontalBox(Image))
		{
			ImageSlot->SetPadding(RaidTimerDigitPadding);
		}
		RaidTimerImageSlots.Add(Image);
	}
}

void UFrontierPlayerStatusWidget::HideRaidTimerImageSlots()
{
	for (UImage* Image : RaidTimerImageSlots)
	{
		if (Image)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (RaidTimerImageContainer)
	{
		RaidTimerImageContainer->SetVisibility(ESlateVisibility::Collapsed);
	}

	CachedRaidTimerSeconds = INDEX_NONE;
}

bool UFrontierPlayerStatusWidget::SetRaidTimerAtlasImage(UImage* Image, const int32 AtlasIndex, const FLinearColor& Color) const
{
	if (!Image || !RaidTimerDigitAtlas || RaidTimerAtlasColumns <= 0 || RaidTimerAtlasRows <= 0)
	{
		if (Image)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
		}
		return false;
	}

	const int32 AtlasCellCount = RaidTimerAtlasColumns * RaidTimerAtlasRows;
	if (AtlasIndex < 0 || AtlasIndex >= AtlasCellCount)
	{
		Image->SetVisibility(ESlateVisibility::Collapsed);
		return false;
	}

	const int32 Column = AtlasIndex % RaidTimerAtlasColumns;
	const int32 Row = AtlasIndex / RaidTimerAtlasColumns;
	const float MinU = static_cast<float>(Column) / static_cast<float>(RaidTimerAtlasColumns);
	const float MinV = static_cast<float>(Row) / static_cast<float>(RaidTimerAtlasRows);
	const float MaxU = static_cast<float>(Column + 1) / static_cast<float>(RaidTimerAtlasColumns);
	const float MaxV = static_cast<float>(Row + 1) / static_cast<float>(RaidTimerAtlasRows);

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(RaidTimerDigitAtlas);
	Brush.ImageSize = RaidTimerDigitSize;
	Brush.SetUVRegion(FBox2f(FVector2f(MinU, MinV), FVector2f(MaxU, MaxV)));
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(Color);
	Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

bool UFrontierPlayerStatusWidget::SetRaidTimerSeparatorImage(UImage* Image, const FLinearColor& Color) const
{
	if (RaidTimerSeparatorAtlasIndex >= 0)
	{
		return SetRaidTimerAtlasImage(Image, RaidTimerSeparatorAtlasIndex, Color);
	}

	if (!Image || !RaidTimerSeparatorTexture)
	{
		if (Image)
		{
			Image->SetVisibility(ESlateVisibility::Collapsed);
		}
		return false;
	}

	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Image;
	Brush.SetResourceObject(RaidTimerSeparatorTexture);
	Brush.ImageSize = RaidTimerDigitSize;
	Image->SetBrush(Brush);
	Image->SetColorAndOpacity(Color);
	Image->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	return true;
}

void UFrontierPlayerStatusWidget::RefreshRaidTimerDisplay()
{
	const AFrontierGameState* FrontierGameState = GetWorld() ? GetWorld()->GetGameState<AFrontierGameState>() : nullptr;
	if (!FrontierGameState || !FrontierGameState->IsRaidTimerActive() || IsLobbyWorld())
	{
		HideRaidTimerImageSlots();
		SetExtractionProgress(0.0f);
		return;
	}

	if (!RaidTimerImageContainer || RaidTimerImageSlots.Num() != 5 || !RaidTimerDigitAtlas)
	{
		return;
	}

	const int32 RemainingTimeSeconds = FMath::Max(0, FrontierGameState->GetRaidRemainingTimeSeconds());
	if (CachedRaidTimerSeconds == RemainingTimeSeconds)
	{
		return;
	}

	const int32 Minutes = FMath::Clamp(RemainingTimeSeconds / 60, 0, 99);
	const int32 Seconds = RemainingTimeSeconds % 60;
	const FLinearColor TimerColor = RemainingTimeSeconds <= 60
		? FLinearColor(1.0f, 0.2f, 0.2f, 1.0f)
		: FLinearColor::White;

	RaidTimerImageContainer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	SetRaidTimerAtlasImage(RaidTimerImageSlots[0], Minutes / 10, TimerColor);
	SetRaidTimerAtlasImage(RaidTimerImageSlots[1], Minutes % 10, TimerColor);
	SetRaidTimerSeparatorImage(RaidTimerImageSlots[2], TimerColor);
	SetRaidTimerAtlasImage(RaidTimerImageSlots[3], Seconds / 10, TimerColor);
	SetRaidTimerAtlasImage(RaidTimerImageSlots[4], Seconds % 10, TimerColor);
	CachedRaidTimerSeconds = RemainingTimeSeconds;
}

void UFrontierPlayerStatusWidget::RefreshSpectatorControls()
{
	AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>();
	const bool bIsSpectator = CachedPlayerState && CachedPlayerState->IsOnlyASpectator();
	const bool bCanCycle = FrontierPlayerController && FrontierPlayerController->CanCycleSpectatorTargets();

	if (PrevSpectatorButton)
	{
		PrevSpectatorButton->SetVisibility(bIsSpectator ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		PrevSpectatorButton->SetIsEnabled(bCanCycle);
	}

	if (NextSpectatorButton)
	{
		NextSpectatorButton->SetVisibility(bIsSpectator ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		NextSpectatorButton->SetIsEnabled(bCanCycle);
	}

	if (SpectatorTargetText)
	{
		SpectatorTargetText->SetVisibility(bIsSpectator ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
		SpectatorTargetText->SetText(FrontierPlayerController ? FrontierPlayerController->GetCurrentSpectatorTargetDisplayText() : FText::GetEmpty());
	}
}

void UFrontierPlayerStatusWidget::ClearTransientWarning()
{
	if (!CenterWarningText)
	{
		return;
	}

	CenterWarningText->SetText(FText::GetEmpty());
	CenterWarningText->SetVisibility(ESlateVisibility::Collapsed);
}

void UFrontierPlayerStatusWidget::HandleReadyCountRefreshTick()
{
	RefreshReadyCountText();
	RefreshRaidTimerDisplay();
	RefreshSpectatorControls();
}

void UFrontierPlayerStatusWidget::HandlePrevSpectatorClicked()
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->CycleSpectatorTarget(-1);
		RefreshSpectatorControls();
	}
}

void UFrontierPlayerStatusWidget::HandleNextSpectatorClicked()
{
	if (AFrontierPlayerController* FrontierPlayerController = GetOwningPlayer<AFrontierPlayerController>())
	{
		FrontierPlayerController->CycleSpectatorTarget(1);
		RefreshSpectatorControls();
	}
}

void UFrontierPlayerStatusWidget::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	if (ChangeData.NewValue < ChangeData.OldValue - KINDA_SMALL_NUMBER)
	{
		ShowDamageIndicator();
	}
	RefreshFromCachedAttributes();
}

void UFrontierPlayerStatusWidget::ShowDamageIndicator()
{
	if (!DamageIndicatorImage)
	{
		return;
	}

	DamageIndicatorElapsedTime = 0.0f;
	bDamageIndicatorActive = true;
	DamageIndicatorImage->SetRenderOpacity(FMath::Clamp(DamageIndicatorPeakOpacity, 0.0f, 1.0f));
	DamageIndicatorImage->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFrontierPlayerStatusWidget::HideDamageIndicator()
{
	DamageIndicatorElapsedTime = 0.0f;
	bDamageIndicatorActive = false;
	if (DamageIndicatorImage)
	{
		DamageIndicatorImage->SetRenderOpacity(0.0f);
		DamageIndicatorImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierPlayerStatusWidget::HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFromCachedAttributes();
}

void UFrontierPlayerStatusWidget::HandleStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFromCachedAttributes();
}

void UFrontierPlayerStatusWidget::HandleMaxStaminaChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshFromCachedAttributes();
}

void UFrontierPlayerStatusWidget::HandleObservedTeamChanged(AFrontierPlayerState* PlayerState, int32 TeamId)
{
	RefreshTeamAndReadyTexts();
}

void UFrontierPlayerStatusWidget::HandleObservedReadyChanged(AFrontierPlayerState* PlayerState)
{
	RefreshTeamAndReadyTexts();
}


