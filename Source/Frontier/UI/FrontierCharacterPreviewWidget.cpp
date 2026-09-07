#include "UI/FrontierCharacterPreviewWidget.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Components/Image.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierQuickSlotComponent.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Character/FrontierCharacterSelectionSubsystem.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Skill/FrontierSkillTypes.h"
#include "Tags/FrontierGameplayTags.h"
#include "UI/FrontierCharacterPreviewActor.h"
#include "UI/FrontierQuickSlotBarWidget.h"
#include "UI/FrontierSkillBarWidget.h"

void UFrontierCharacterPreviewWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (Button_DarkKnight)
	{
		Button_DarkKnight->OnClicked.AddDynamic(
			this,
			&UFrontierCharacterPreviewWidget::HandleDarkKnightClicked);
	}
	if (Button_DarkLady)
	{
		Button_DarkLady->OnClicked.AddDynamic(
			this,
			&UFrontierCharacterPreviewWidget::HandleDarkLadyClicked);
	}
}

void UFrontierCharacterPreviewWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UFrontierCharacterSelectionSubsystem* SelectionSubsystem =
		GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
			: nullptr)
	{
		SelectionSubsystem->InitializeCharacterSelection();
	}

	if (CharacterInfoPanel)
	{
		CharacterInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (CharacterInfoButton)
	{
		CharacterInfoButton->OnHovered.AddDynamic(this, &UFrontierCharacterPreviewWidget::ShowCharacterInfoPanel);
		CharacterInfoButton->OnUnhovered.AddDynamic(this, &UFrontierCharacterPreviewWidget::HideCharacterInfoPanel);
	}

	RefreshCharacterSelectionUI();
	RefreshPreviewCharacter();
	BindCharacterInfoSources();
	BindQuickSlotWidget();
	BindSkillBarWidget();
}

void UFrontierCharacterPreviewWidget::NativeDestruct()
{
	if (CharacterInfoButton)
	{
		CharacterInfoButton->OnHovered.RemoveDynamic(this, &UFrontierCharacterPreviewWidget::ShowCharacterInfoPanel);
		CharacterInfoButton->OnUnhovered.RemoveDynamic(this, &UFrontierCharacterPreviewWidget::HideCharacterInfoPanel);
	}

	ClearPreview();
	Super::NativeDestruct();
}

void UFrontierCharacterPreviewWidget::SetPreviewCharacter(ACharacter* InCharacter)
{
	if (PreviewSourceCharacter != InCharacter)
	{
		UnbindEquipmentComponent();
	}

	PreviewSourceCharacter = InCharacter;
	PreviewPlayerState = InCharacter ? InCharacter->GetPlayerState<AFrontierPlayerState>() : nullptr;
	BindEquipmentComponent();
	BindLoadoutComponent();
	BindCharacterInfoSources();
	BindQuickSlotWidget();
	BindSkillBarWidget();
	RefreshPreview();
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::SetPreviewPlayerState(AFrontierPlayerState* InPlayerState)
{
	if (PreviewSourceCharacter)
	{
		UnbindEquipmentComponent();
		PreviewSourceCharacter = nullptr;
	}

	PreviewPlayerState = InPlayerState;
	BindLoadoutComponent();
	BindCharacterInfoSources();
	BindQuickSlotWidget();
	BindSkillBarWidget();
	RefreshPreview();
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::ClearPreview()
{
	UnbindEquipmentComponent();
	UnbindLoadoutComponent();
	UnbindCharacterInfoSources();
	if (QuickSlotBarWidget)
	{
		QuickSlotBarWidget->SetQuickSlotComponent(nullptr);
	}
	if (WBP_Skillbarwidget)
	{
		WBP_Skillbarwidget->SetSkillComponent(nullptr);
	}

	if (PreviewActor)
	{
		if (bOwnsPreviewActor)
		{
			PreviewActor->ShutdownPreview();
		}
		else
		{
			PreviewActor->StopPreviewCapture();
		}
	}

	PreviewActor = nullptr;
	PreviewSourceCharacter = nullptr;
	PreviewPlayerState = nullptr;
	bOwnsPreviewActor = false;

	if (CharacterPreviewImage)
	{
		CharacterPreviewImage->SetVisibility(ESlateVisibility::Hidden);
	}
}

void UFrontierCharacterPreviewWidget::BindQuickSlotWidget()
{
	if (!QuickSlotBarWidget)
	{
		return;
	}

	AFrontierPlayerState* PlayerState = PreviewPlayerState;
	if (!PlayerState && PreviewSourceCharacter)
	{
		PlayerState = PreviewSourceCharacter->GetPlayerState<AFrontierPlayerState>();
	}
	if (!PlayerState && GetOwningPlayer())
	{
		PlayerState = GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>();
	}

	QuickSlotBarWidget->SetEditMode(true);
	QuickSlotBarWidget->SetQuickSlotComponent(PlayerState ? PlayerState->GetQuickSlotComponent() : nullptr);
}

void UFrontierCharacterPreviewWidget::BindSkillBarWidget()
{
	if (!WBP_Skillbarwidget)
	{
		return;
	}

	if (!CachedSkillComponent)
	{
		BindCharacterInfoSources();
	}

	WBP_Skillbarwidget->SetSkillComponent(CachedSkillComponent);
}

void UFrontierCharacterPreviewWidget::RefreshPreview()
{
	if ((!PreviewSourceCharacter && !PreviewPlayerState) || !GetWorld())
	{
		return;
	}

	EnsurePreviewActor();
	if (PreviewActor)
	{
		if (PreviewSourceCharacter)
		{
			PreviewActor->InitializeFromCharacter(PreviewSourceCharacter);
		}
		else
		{
			PreviewActor->InitializeFromPlayerState(PreviewPlayerState);
		}

		if (const UFrontierCharacterSelectionSubsystem* SelectionSubsystem =
			GetGameInstance()
				? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
				: nullptr)
		{
			ApplyPreviewAppearance(SelectionSubsystem->GetSelectedCharacterType());
		}
	}

	if (CharacterPreviewImage)
	{
		CharacterPreviewImage->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrontierCharacterPreviewWidget::HandleDarkKnightClicked()
{
	SelectCharacter(EFrontierCharacterType::DarkKnight);
}

void UFrontierCharacterPreviewWidget::HandleDarkLadyClicked()
{
	SelectCharacter(EFrontierCharacterType::DarkLady);
}

void UFrontierCharacterPreviewWidget::SelectCharacter(
	const EFrontierCharacterType CharacterType)
{
	UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	if (!SelectionSubsystem)
	{
		return;
	}

	const bool bSelectionChanged =
		SelectionSubsystem->GetSelectedCharacterType() != CharacterType;
	if (bSelectionChanged)
	{
		SelectionSubsystem->SetSelectedCharacterType(CharacterType);
	}

	// Keep the possessed in-game character in sync with the preview selection.
	// RequestCharacterType applies locally first, then uses the existing reliable
	// server RPC and replicated SelectedCharacterType for the other clients.
	AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(GetOwningPlayerPawn());
	if (!PlayerCharacter)
	{
		PlayerCharacter = Cast<AFrontierPlayerCharacter>(PreviewSourceCharacter.Get());
	}
	if (PlayerCharacter)
	{
		PlayerCharacter->RequestCharacterType(CharacterType);
	}

	RefreshPreviewCharacter();
	RefreshCharacterSelectionUI();
}

void UFrontierCharacterPreviewWidget::RefreshCharacterSelectionUI()
{
	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	const EFrontierCharacterType SelectedType = SelectionSubsystem
		? SelectionSubsystem->GetSelectedCharacterType()
		: EFrontierCharacterType::DarkKnight;
	const bool bDarkKnightSelected =
		SelectedType == EFrontierCharacterType::DarkKnight;

	if (Button_DarkKnight)
	{
		Button_DarkKnight->SetIsEnabled(!bDarkKnightSelected);
	}
	if (Button_DarkLady)
	{
		Button_DarkLady->SetIsEnabled(bDarkKnightSelected);
	}
}

void UFrontierCharacterPreviewWidget::RefreshPreviewCharacter()
{
	if (!PreviewActor)
	{
		RefreshPreview();
		return;
	}

	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	if (SelectionSubsystem)
	{
		ApplyPreviewAppearance(SelectionSubsystem->GetSelectedCharacterType());
	}
}

bool UFrontierCharacterPreviewWidget::ApplyPreviewAppearance(
	const EFrontierCharacterType CharacterType)
{
	if (!PreviewActor)
	{
		return false;
	}

	const UFrontierCharacterAppearanceDataAsset* AppearanceDataAsset =
		CharacterAppearanceData.LoadSynchronous();
	const FFrontierCharacterAppearanceData* Appearance = AppearanceDataAsset
		? AppearanceDataAsset->FindAppearance(CharacterType)
		: nullptr;

	if (!Appearance)
	{
		const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
			: nullptr;
		Appearance = SelectionSubsystem
			? SelectionSubsystem->FindAppearance(CharacterType)
			: nullptr;
	}

	return Appearance && PreviewActor->ApplyCharacterAppearance(*Appearance);
}

void UFrontierCharacterPreviewWidget::EnsurePreviewActor()
{
	if (PreviewActor && !PreviewActor->IsActorBeingDestroyed())
	{
		PreviewActor->SetActorLocation(ResolvePreviewActorLocation());
		return;
	}

	if (AFrontierPlayerCharacter* PreviewPlayerCharacter = Cast<AFrontierPlayerCharacter>(PreviewSourceCharacter))
	{
		PreviewActor = PreviewPlayerCharacter->GetOrCreateCharacterPreviewActor(PreviewActorClass, ResolvePreviewActorLocation());
		bOwnsPreviewActor = false;
		return;
	}

	if (PreviewPlayerState && PreviewActorClass && GetWorld())
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwningPlayer();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;
		PreviewActor = GetWorld()->SpawnActor<AFrontierCharacterPreviewActor>(
			PreviewActorClass,
			ResolvePreviewActorLocation(),
			FRotator::ZeroRotator,
			SpawnParameters);
		bOwnsPreviewActor = PreviewActor != nullptr;
	}
}

FVector UFrontierCharacterPreviewWidget::ResolvePreviewActorLocation() const
{
	const APlayerController* PlayerController = GetOwningPlayer();
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	return Pawn ? Pawn->GetActorLocation() + PreviewWorldOffset : PreviewWorldOffset;
}

void UFrontierCharacterPreviewWidget::BindEquipmentComponent()
{
	UFrontierEquipmentComponent* EquipmentComponent = PreviewSourceCharacter
		? PreviewSourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>()
		: nullptr;

	if (BoundEquipmentComponent == EquipmentComponent)
	{
		return;
	}

	UnbindEquipmentComponent();
	BoundEquipmentComponent = EquipmentComponent;

	if (BoundEquipmentComponent)
	{
		BoundEquipmentComponent->OnCurrentWeaponChanged.AddDynamic(this, &UFrontierCharacterPreviewWidget::HandleCurrentWeaponChanged);
	}
}

void UFrontierCharacterPreviewWidget::UnbindEquipmentComponent()
{
	if (BoundEquipmentComponent)
	{
		BoundEquipmentComponent->OnCurrentWeaponChanged.RemoveDynamic(this, &UFrontierCharacterPreviewWidget::HandleCurrentWeaponChanged);
		BoundEquipmentComponent = nullptr;
	}
}

void UFrontierCharacterPreviewWidget::BindLoadoutComponent()
{
	UFrontierLoadoutComponent* LoadoutComponent = PreviewPlayerState
		? PreviewPlayerState->GetLoadoutComponent()
		: nullptr;
	if (BoundLoadoutComponent == LoadoutComponent)
	{
		return;
	}

	UnbindLoadoutComponent();
	BoundLoadoutComponent = LoadoutComponent;
	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.AddDynamic(this, &UFrontierCharacterPreviewWidget::HandleLoadoutChanged);
	}
}

void UFrontierCharacterPreviewWidget::UnbindLoadoutComponent()
{
	if (BoundLoadoutComponent)
	{
		BoundLoadoutComponent->OnLoadoutChanged.RemoveDynamic(this, &UFrontierCharacterPreviewWidget::HandleLoadoutChanged);
		BoundLoadoutComponent = nullptr;
	}
}

void UFrontierCharacterPreviewWidget::HandleCurrentWeaponChanged(AActor* NewWeapon, int32 NewWeaponIndex)
{
	RefreshPreview();
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& Slots)
{
	RefreshPreview();
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::ShowCharacterInfoPanel()
{
	RefreshCharacterInfoPanel();

	if (CharacterInfoPanel)
	{
		CharacterInfoPanel->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrontierCharacterPreviewWidget::HideCharacterInfoPanel()
{
	if (CharacterInfoPanel)
	{
		CharacterInfoPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UFrontierCharacterPreviewWidget::HandleCharacterAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::HandleEquipmentSkillsChanged()
{
	if (WBP_Skillbarwidget)
	{
		WBP_Skillbarwidget->RefreshSlots();
	}
	RefreshCharacterInfoPanel();
}

void UFrontierCharacterPreviewWidget::BindCharacterInfoSources()
{
	AFrontierPlayerState* NewPlayerState = PreviewPlayerState;
	if (const APawn* PreviewPawn = Cast<APawn>(PreviewSourceCharacter))
	{
		NewPlayerState = PreviewPawn->GetPlayerState<AFrontierPlayerState>();
	}
	if (!NewPlayerState && GetOwningPlayer())
	{
		NewPlayerState = GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>();
	}

	if (ObservedPlayerState == NewPlayerState && CachedAbilitySystemComponent && CachedAttributeSet)
	{
		return;
	}

	UnbindCharacterInfoSources();
	ObservedPlayerState = NewPlayerState;

	CachedAbilitySystemComponent = ObservedPlayerState ? ObservedPlayerState->GetFrontierAbilitySystemComponent() : nullptr;
	CachedAttributeSet = ObservedPlayerState ? const_cast<UFrontierAttributeSet*>(ObservedPlayerState->GetFrontierAttributeSet()) : nullptr;
	CachedSkillComponent = ObservedPlayerState ? ObservedPlayerState->GetEquipmentSkillComponent() : nullptr;

	if (CachedAbilitySystemComponent && CachedAttributeSet)
	{
		AttackPowerChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UFrontierAttributeSet::GetAttackPowerAttribute()).AddUObject(
			this,
			&UFrontierCharacterPreviewWidget::HandleCharacterAttributeChanged);
		DefenseChangedHandle = CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			UFrontierAttributeSet::GetDefenseAttribute()).AddUObject(
			this,
			&UFrontierCharacterPreviewWidget::HandleCharacterAttributeChanged);
	}

	if (CachedSkillComponent)
	{
		CachedSkillComponent->OnEquipmentSkillsChanged.AddUObject(this, &UFrontierCharacterPreviewWidget::HandleEquipmentSkillsChanged);
	}
}

void UFrontierCharacterPreviewWidget::UnbindCharacterInfoSources()
{
	if (CachedAbilitySystemComponent)
	{
		if (AttackPowerChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
				UFrontierAttributeSet::GetAttackPowerAttribute()).Remove(AttackPowerChangedHandle);
		}
		if (DefenseChangedHandle.IsValid())
		{
			CachedAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
				UFrontierAttributeSet::GetDefenseAttribute()).Remove(DefenseChangedHandle);
		}
	}

	if (CachedSkillComponent)
	{
		CachedSkillComponent->OnEquipmentSkillsChanged.RemoveAll(this);
	}

	AttackPowerChangedHandle.Reset();
	DefenseChangedHandle.Reset();
	ObservedPlayerState = nullptr;
	CachedAbilitySystemComponent = nullptr;
	CachedAttributeSet = nullptr;
	CachedSkillComponent = nullptr;
}

void UFrontierCharacterPreviewWidget::RefreshCharacterInfoPanel()
{
	if (!CachedAttributeSet || !CachedSkillComponent)
	{
		BindCharacterInfoSources();
	}

	if (Text_CharacterAttackPower)
	{
		const bool bHasAttackPower = CachedAttributeSet != nullptr || BoundLoadoutComponent != nullptr;
		const FString AttackText = bHasAttackPower
			? FString::Printf(TEXT("캐릭터 공격력: %.0f"), ResolvePreviewAttackPower())
			: TEXT("캐릭터 공격력: -");
		Text_CharacterAttackPower->SetText(FText::FromString(AttackText));
	}

	if (Text_CharacterDefense)
	{
		const bool bHasDefense = CachedAttributeSet != nullptr || BoundLoadoutComponent != nullptr;
		const FString DefenseText = bHasDefense
			? FString::Printf(TEXT("방어력: %.0f"), ResolvePreviewDefense())
			: TEXT("방어력: -");
		Text_CharacterDefense->SetText(FText::FromString(DefenseText));
	}

	if (Text_EquipmentScore)
	{
		const float EquipmentScore = BoundLoadoutComponent
			? BoundLoadoutComponent->GetTotalEquipmentScore()
			: 0.0f;
		Text_EquipmentScore->SetText(FText::FromString(
			FString::Printf(TEXT("%.1f"), FMath::Max(0.0f, EquipmentScore))));
	}

	if (Text_CharacterSkill1)
	{
		Text_CharacterSkill1->SetText(FText::FromString(BuildSkillLine(0)));
	}
	if (Text_CharacterSkill2)
	{
		Text_CharacterSkill2->SetText(FText::FromString(BuildSkillLine(1)));
	}
	if (Text_CharacterSkill3)
	{
		Text_CharacterSkill3->SetText(FText::FromString(BuildSkillLine(2)));
	}
}

float UFrontierCharacterPreviewWidget::ResolvePreviewAttackPower() const
{
	float AttackPower = CachedAttributeSet ? CachedAttributeSet->GetAttackPower() : 0.0f;

	// A pawn-free lobby preview has the replicated Loadout but does not run the
	// server-side equipment GameplayEffect. Add the main weapon's stat only in
	// that path; a live character's ASC value already contains it.
	if (PreviewSourceCharacter || !BoundLoadoutComponent)
	{
		return AttackPower;
	}

	for (const FFrontierLoadoutSlot& Slots : BoundLoadoutComponent->GetLoadoutSlots())
	{
		if (Slots.SlotType != EFrontierEquipmentSlot::MainWeapon
			|| !Slots.bOccupied
			|| !Slots.ItemInstance.IsValid()
			|| Slots.ItemInstance.GetCategory() != EFrontierItemCategory::Weapon)
		{
			continue;
		}

		AttackPower += Slots.ItemInstance.GetCurrentFinalStatValue(
			FFrontierGameplayTags::Get().StatAttackPower,
			0.0f);
		break;
	}

	return AttackPower;
}

float UFrontierCharacterPreviewWidget::ResolvePreviewDefense() const
{
	float Defense = CachedAttributeSet ? CachedAttributeSet->GetDefense() : 0.0f;

	// A pawn-free lobby preview has the replicated Loadout but does not run the
	// server-side equipment GameplayEffect. Add defensive gear stats only in
	// that path; a live character's ASC value already contains them.
	if (PreviewSourceCharacter || !BoundLoadoutComponent)
	{
		return Defense;
	}

	const EFrontierEquipmentSlot DefensiveSlots[] = {
		EFrontierEquipmentSlot::Helmet,
		EFrontierEquipmentSlot::Chest,
		EFrontierEquipmentSlot::Gloves,
		EFrontierEquipmentSlot::Boots,
		EFrontierEquipmentSlot::Necklace,
		EFrontierEquipmentSlot::Ring
	};

	for (const EFrontierEquipmentSlot SlotType : DefensiveSlots)
	{
		FFrontierLoadoutSlot LoadoutSlot;
		if (!BoundLoadoutComponent->FindLoadoutSlot(SlotType, LoadoutSlot)
			|| !LoadoutSlot.bOccupied
			|| !LoadoutSlot.ItemInstance.IsValid())
		{
			continue;
		}

		Defense += LoadoutSlot.ItemInstance.GetCurrentFinalStatValue(
			FFrontierGameplayTags::Get().StatDefense,
			0.0f);
	}

	return Defense;
}

FString UFrontierCharacterPreviewWidget::BuildSkillLine(const int32 SlotIndex) const
{
	if (!CachedSkillComponent)
	{
		return FString::Printf(TEXT("스킬%d: 비어 있음"), SlotIndex + 1);
	}

	FFrontierGeneratedWeaponSkill GeneratedSkill;
	if (!CachedSkillComponent->GetGeneratedSkillAtSlot(SlotIndex, GeneratedSkill) || !GeneratedSkill.SkillTag.IsValid())
	{
		return FString::Printf(TEXT("스킬%d: 비어 있음"), SlotIndex + 1);
	}

	const FFrontierSkillInfo* SkillInfo = CachedSkillComponent->FindSkillInfo(GeneratedSkill.SkillTag);
	const FString SkillName = SkillInfo && !SkillInfo->DisplayName.IsEmpty()
		? SkillInfo->DisplayName.ToString()
		: GeneratedSkill.SkillTag.GetTagName().ToString();
	const int32 SkillLevel = CachedSkillComponent->GetSkillLevel(GeneratedSkill.SkillTag);

	return FString::Printf(TEXT("스킬%d: %s Lv.%d"), SlotIndex + 1, *SkillName, SkillLevel);
}
