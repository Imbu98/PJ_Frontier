#include "Character/FrontierPlayerCharacter.h"

#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Character/FrontierCharacterSelectionSubsystem.h"
#include "Camera/CameraComponent.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_AreaSkill.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "Components/FrontierCombatComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameplayEffect.h"
#include "Game/FrontierPlayerState.h"
#include "InputActionValue.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Net/UnrealNetwork.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_WeaponAttack.h"
#include "AbilitySystem/Effects/FrontierHealingGameplayEffect.h"
#include "AbilitySystem/Effects/FrontierStaminaGameplayEffect.h"
#include "TimerManager.h"
#include "Tags/FrontierGameplayTags.h"
#include "Team/FrontierTeamVisualDataAsset.h"
#include "Kismet/GameplayStatics.h"
#include "UI/FrontierCharacterPreviewActor.h"
#include "UI/FrontierOverheadPlayerNameWidget.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "Engine/GameInstance.h"
#include "Settings/FrontierUserSettingsSubsystem.h"

namespace
{
void CancelAttackAbilitiesForCharacter(AFrontierPlayerCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	if (UFrontierAbilitySystemComponent* AbilitySystemComponent = Character->GetFrontierAbilitySystemComponent())
	{
		FGameplayTagContainer CancelTags;
		CancelTags.AddTag(FFrontierGameplayTags::Get().AbilityAttackPrimary);
		AbilitySystemComponent->CancelAbilities(&CancelTags);
	}
}

FGameplayAbilitySpec* ResolveEquippedAttackAbilitySpec(AFrontierPlayerCharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	UFrontierAbilitySystemComponent* ASC = Character->GetFrontierAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}

	const UFrontierEquipmentComponent* EquipmentComponent = Character->GetEquipmentComponent();
	const FGameplayAbilitySpecHandle ManagedHandle = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponAttackAbilityHandle()
		: FGameplayAbilitySpecHandle();
	if (ManagedHandle.IsValid())
	{
		if (FGameplayAbilitySpec* ManagedSpec = ASC->FindAbilitySpecFromHandle(ManagedHandle))
		{
			return ManagedSpec;
		}

		// A replicated/stale handle can briefly outlive the spec while the
		// equipment component is rebuilding the current weapon ability. Fall back
		// to the already preloaded class instead of dropping the first input.
	}

	// Attack abilities are granted by the equipment component after its async
	// preload completes. Do not synchronously load an ability class from input.
	const TSubclassOf<UGameplayAbility> AttackAbilityClass = EquipmentComponent
		? EquipmentComponent->GetPreloadedAttackAbilityClass()
		: nullptr;
	return AttackAbilityClass ? ASC->FindAbilitySpecFromClass(AttackAbilityClass) : nullptr;
}

UFrontierGameplayAbility_WeaponAttack* ResolveActiveWeaponAttackAbility(AFrontierPlayerCharacter* Character)
{
	const FGameplayAbilitySpec* AbilitySpec = ResolveEquippedAttackAbilitySpec(Character);
	if (AbilitySpec && AbilitySpec->IsActive())
	{
		return Cast<UFrontierGameplayAbility_WeaponAttack>(AbilitySpec->GetPrimaryInstance());
	}

	return nullptr;
}

UFrontierGameplayAbility_AreaSkill* ResolveActiveAreaSkillAbility(AFrontierPlayerCharacter* Character)
{
	if (!Character)
	{
		return nullptr;
	}

	UFrontierAbilitySystemComponent* ASC = Character->GetFrontierAbilitySystemComponent();
	if (!ASC)
	{
		return nullptr;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (!AbilitySpec.IsActive())
		{
			continue;
		}

		if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = Cast<UFrontierGameplayAbility_AreaSkill>(AbilitySpec.GetPrimaryInstance()))
		{
			if (AreaSkillAbility->IsAwaitingAreaConfirm())
			{
				return AreaSkillAbility;
			}
		}
	}

	return nullptr;
}

void ApplyPotionHealthRestore(UAbilitySystemComponent* AbilitySystemComponent, const float RestoreAmount)
{
	if (!AbilitySystemComponent || RestoreAmount <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle EffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		UFrontierHealingGameplayEffect::StaticClass(),
		1.0f,
		AbilitySystemComponent->MakeEffectContext());
	if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
	{
		return;
	}

	EffectSpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataHealthDelta, RestoreAmount);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
}

void ApplyPotionStaminaRestore(UAbilitySystemComponent* AbilitySystemComponent, const float RestoreAmount)
{
	if (!AbilitySystemComponent || RestoreAmount <= 0.0f)
	{
		return;
	}

	FGameplayEffectSpecHandle EffectSpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		UFrontierStaminaGameplayEffect::StaticClass(),
		1.0f,
		AbilitySystemComponent->MakeEffectContext());
	if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid())
	{
		return;
	}

	EffectSpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataStaminaDelta, RestoreAmount);
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());
}
}

AFrontierPlayerCharacter::AFrontierPlayerCharacter()
{
	

	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = BaseJumpZVelocity;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.0f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.0f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;

	EquipmentComponent = CreateDefaultSubobject<UFrontierEquipmentComponent>(TEXT("EquipmentComponent"));

	// TEMP: The original full-character Mesh is visible while modular rendering is paused.
	GetMesh()->SetVisibility(true, false);
	GetMesh()->SetHiddenInGame(false, false);
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	ArmorMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmorMesh"));
	ArmorMeshComponent->SetupAttachment(GetMesh());

	GloveMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GloveMesh"));
	GloveMeshComponent->SetupAttachment(GetMesh());

	GreavesMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GreavesMesh"));
	GreavesMeshComponent->SetupAttachment(GetMesh());

	HeadMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMeshComponent->SetupAttachment(GetMesh());

#if 0 // TEMP: Preserved modular-character leader-pose setup. Do not delete.
	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		ModularMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ModularMesh->SetGenerateOverlapEvents(false);
		ModularMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		ModularMesh->SetLeaderPoseComponent(GetMesh(), false, true);
	}
#endif

	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		ModularMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ModularMesh->SetGenerateOverlapEvents(false);
		ModularMesh->SetVisibility(false, true);
		ModularMesh->SetHiddenInGame(true, true);
	}

	NockedArrowMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("NockedArrowMeshComponent"));
	NockedArrowMeshComponent->SetupAttachment(GetMesh());
	NockedArrowMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NockedArrowMeshComponent->SetGenerateOverlapEvents(false);
	NockedArrowMeshComponent->SetHiddenInGame(true);
	NockedArrowMeshComponent->SetVisibility(false, true);

	PotionMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PotionMeshComponent"));
	PotionMeshComponent->SetupAttachment(GetMesh());
	PotionMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PotionMeshComponent->SetGenerateOverlapEvents(false);
	PotionMeshComponent->SetHiddenInGame(true);
	PotionMeshComponent->SetVisibility(false, true);

	OverheadNameWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("OverheadNameWidgetComponent"));
	OverheadNameWidgetComponent->SetupAttachment(RootComponent);
	OverheadNameWidgetComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 120.0f));
	OverheadNameWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
	OverheadNameWidgetComponent->SetDrawSize(FVector2D(200.0f, 48.0f));
	OverheadNameWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OverheadNameWidgetComponent->SetWidgetClass(UFrontierOverheadPlayerNameWidget::StaticClass());
	CurrentAttackAnimationMode = EFrontierAttackAnimationMode::UpperBodyOnly;
}

bool AFrontierPlayerCharacter::BeginConsumableUse(const FFrontierItemInstance& ItemInstance)
{
	if (!HasAuthority() || IsDead() || bConsumableUsePending)
	{
		return false;
	}

	const float HealthRestoreAmount = ItemInstance.GetHealthRestoreAmount();
	const float StaminaRestoreAmount = ItemInstance.GetStaminaRestoreAmount();
	UAnimMontage* UseMontage = ItemInstance.GetUseMontage().LoadSynchronous();
	const bool bHasDirectRestoreAmount = HealthRestoreAmount > 0.0f || StaminaRestoreAmount > 0.0f;
	TSubclassOf<UGameplayEffect> ConsumeEffectClass;
	if (!bHasDirectRestoreAmount)
	{
		ConsumeEffectClass = ItemInstance.GetConsumeEffectClass().LoadSynchronous();
	}
	if (!UseMontage || (!bHasDirectRestoreAmount && !ConsumeEffectClass))
	{
		return false;
	}

	PendingConsumableEffectClass = ConsumeEffectClass;
	UStaticMesh* PotionMesh = ItemInstance.GetPotionMesh().LoadSynchronous();
	MulticastPlayConsumableUse(
		UseMontage,
		PotionMesh,
		ItemInstance.GetPotionSocketName(),
		ItemInstance.GetPotionMeshRelativeTransform(),
		HealthRestoreAmount,
		StaminaRestoreAmount);
	if (!bConsumableUsePending)
	{
		PendingConsumableEffectClass = nullptr;
		return false;
	}
	return true;
}

void AFrontierPlayerCharacter::HandleConsumableUseNotify()
{
	if (!bConsumableUsePending)
	{
		return;
	}

	if (HasAuthority())
	{
		const float HealthRestoreAmount = PendingConsumableHealthRestore;
		const float StaminaRestoreAmount = PendingConsumableStaminaRestore;
		const TSubclassOf<UGameplayEffect> ConsumeEffectClass = PendingConsumableEffectClass;
		bConsumableUsePending = false;
		PendingConsumableHealthRestore = 0.0f;
		PendingConsumableStaminaRestore = 0.0f;
		PendingConsumableEffectClass = nullptr;
		ClearConsumablePotionVisual();

		if (UFrontierAbilitySystemComponent* PotionAbilitySystemComponent = GetFrontierAbilitySystemComponent())
		{
			if (const UGameplayEffect* EffectCDO = ConsumeEffectClass
				? ConsumeEffectClass->GetDefaultObject<UGameplayEffect>()
				: nullptr)
			{
				PotionAbilitySystemComponent->ApplyGameplayEffectToSelf(
					EffectCDO,
					1.0f,
					PotionAbilitySystemComponent->MakeEffectContext());
			}
			else
			{
				ApplyPotionHealthRestore(PotionAbilitySystemComponent, HealthRestoreAmount);
				ApplyPotionStaminaRestore(PotionAbilitySystemComponent, StaminaRestoreAmount);
			}
		}
		MulticastFinishConsumableUse();
		return;
	}

	ClearConsumablePotionVisual();
	bConsumableUsePending = false;
	PendingConsumableEffectClass = nullptr;
	if (IsLocallyControlled())
	{
		ServerConsumeConsumableFromNotify();
	}
}

void AFrontierPlayerCharacter::ApplyConsumablePotionVisual(
	UStaticMesh* PotionMesh,
	const FName PotionSocketName,
	const FTransform& RelativeTransform)
{
	if (!PotionMeshComponent || !GetMesh() || !PotionMesh || PotionSocketName.IsNone())
	{
		return;
	}

	if (!GetMesh()->DoesSocketExist(PotionSocketName))
	{
		FRONTIER_LOG(Warning, TEXT("Potion socket does not exist. Character=%s Socket=%s"), *GetNameSafe(this), *PotionSocketName.ToString());
		return;
	}

	PotionMeshComponent->SetStaticMesh(PotionMesh);
	PotionMeshComponent->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetNotIncludingScale, PotionSocketName);
	PotionMeshComponent->SetRelativeTransform(RelativeTransform);
	PotionMeshComponent->SetHiddenInGame(false);
	PotionMeshComponent->SetVisibility(true, true);
}

void AFrontierPlayerCharacter::ClearConsumablePotionVisual()
{
	if (PotionMeshComponent)
	{
		PotionMeshComponent->SetHiddenInGame(true);
		PotionMeshComponent->SetVisibility(false, true);
	}
}

void AFrontierPlayerCharacter::HandleConsumableMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActiveConsumableMontage)
	{
		return;
	}

	ActiveConsumableMontage = nullptr;
	if (bInterrupted && bConsumableUsePending)
	{
		bConsumableUsePending = false;
		PendingConsumableHealthRestore = 0.0f;
		PendingConsumableStaminaRestore = 0.0f;
		PendingConsumableEffectClass = nullptr;
		ClearConsumablePotionVisual();
		if (HasAuthority())
		{
			MulticastCancelConsumableUse();
		}
		else if (IsLocallyControlled())
		{
			ServerCancelConsumableUse();
		}
	}
}

void AFrontierPlayerCharacter::PossessedBy(AController* NewController)
{
	
	Super::PossessedBy(NewController);
	BindToFrontierPlayerState();
	if (const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>())
	{
		SetTeam(ResolvePlayerTeam(FrontierPlayerState->GetTeamId()));
	}
	ApplyTeamMaterialFromPlayerState();
	RefreshOverheadNameWidget();
	BindAttackStateTagDelegate();
	BindPassiveMovementAttributes();

	if (EquipmentComponent)
	{
		EquipmentComponent->RefreshFromLoadout();
	}

	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::OnRep_PlayerState()
{
	
	Super::OnRep_PlayerState();
	InitializeAbilityActorInfo();
	BindToFrontierPlayerState();
	if (const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>())
	{
		SetTeam(ResolvePlayerTeam(FrontierPlayerState->GetTeamId()));
	}
	ApplyTeamMaterialFromPlayerState();
	RefreshOverheadNameWidget();
	BindAttackStateTagDelegate();
	BindPassiveMovementAttributes();

	if (EquipmentComponent)
	{
		EquipmentComponent->RefreshFromLoadout();
	}

	if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		FrontierPlayerController->RefreshPlayerStatusWidgetBinding();
		FrontierPlayerController->TryNotifyClientGameplayReady();
	}

	RefreshMovementSpeed();
}

EFrontierHitReactionLevel AFrontierPlayerCharacter::GetCurrentHitReactionResistance() const
{
	EFrontierHitReactionLevel HighestResistance = Super::GetCurrentHitReactionResistance();
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	const UFrontierWeaponDataAsset* WeaponData = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponData()
		: nullptr;
	if (!AnimInstance || !WeaponData)
	{
		return HighestResistance;
	}

	const auto AccumulateResistance = [&HighestResistance, AnimInstance](
		const TSoftObjectPtr<UAnimMontage>& MontageAsset,
		const EFrontierHitReactionLevel Resistance)
	{
		const UAnimMontage* Montage = MontageAsset.Get();
		if (Montage && AnimInstance->Montage_IsPlaying(Montage)
			&& static_cast<uint8>(Resistance) > static_cast<uint8>(HighestResistance))
		{
			HighestResistance = Resistance;
		}
	};

	for (const FFrontierAttackActionData& AttackData : WeaponData->BasicComboAttacks)
	{
		AccumulateResistance(AttackData.AttackMontage, AttackData.HitReactionResistance);
	}
	AccumulateResistance(WeaponData->BowAttack.DrawMontage, WeaponData->BowAttack.HitReactionResistance);
	AccumulateResistance(WeaponData->BowAttack.ReleaseMontage, WeaponData->BowAttack.HitReactionResistance);

	return HighestResistance;
}

void AFrontierPlayerCharacter::BeginPlay()
{
	
	Super::BeginPlay();
	// RefreshModularMeshLeaderPose(); // TEMP: Modular mode is preserved but disabled.
	ApplyTemporarySingleMeshMode();
	if (CameraBoom)
	{
		DefaultCameraSocketOffset = CameraBoom->SocketOffset;
	}
	HandleAimingStateChanged();
	if (GetNetMode() != NM_DedicatedServer)
	{
		ApplyCharacterType(SelectedCharacterType);
	}
	BindToFrontierPlayerState();
	ApplyTeamMaterialFromPlayerState();
	RefreshOverheadNameWidget();

	if (GetNetMode() != NM_DedicatedServer && IsLocallyControlled() && CharacterPreviewActorClass)
	{
		GetOrCreateCharacterPreviewActor(CharacterPreviewActorClass, GetActorLocation() + CharacterPreviewWorldOffset);
	}
}

void AFrontierPlayerCharacter::HandleAimingStateChanged()
{
	Super::HandleAimingStateChanged();

	if (!IsLocallyControlled())
	{
		return;
	}

	AimingStartedAtSeconds = IsAiming() && GetWorld()
		? GetWorld()->GetTimeSeconds()
		: -1.0f;
}

void AFrontierPlayerCharacter::RefreshOverheadNameWidget()
{
	if (GetNetMode() == NM_DedicatedServer || !OverheadNameWidgetComponent)
	{
		return;
	}

	OverheadNameWidgetComponent->InitWidget();
	if (UFrontierOverheadPlayerNameWidget* NameWidget = Cast<UFrontierOverheadPlayerNameWidget>(OverheadNameWidgetComponent->GetUserWidgetObject()))
	{
		NameWidget->SetObservedPlayerState(GetPlayerState<AFrontierPlayerState>());
	}
}

void AFrontierPlayerCharacter::RevealCombatOverheadLocally(const float HealthPercent)
{
	if (GetNetMode() == NM_DedicatedServer || IsDead() || !GetWorld())
	{
		return;
	}

	EnemyOverheadVisibleUntilSeconds = GetWorld()->GetTimeSeconds() + EnemyOverheadRevealDuration;
	RefreshOverheadNameWidget();
	if (UFrontierOverheadPlayerNameWidget* NameWidget = Cast<UFrontierOverheadPlayerNameWidget>(OverheadNameWidgetComponent->GetUserWidgetObject()))
	{
		NameWidget->SetEnemyPresentation(true);
		NameWidget->SetHealthPercent(HealthPercent);
	}
	UpdateOverheadNameVisibility();
}

void AFrontierPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindPassiveMovementAttributes();
	UnbindFromFrontierPlayerState();
	if (CharacterPreviewActor)
	{
		CharacterPreviewActor->ShutdownPreview();
		CharacterPreviewActor = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void AFrontierPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateAimingCamera(DeltaSeconds);
	UpdateInteractionFacing(DeltaSeconds);

	if (bIsRolling)
	{
		UpdateRollCamera(DeltaSeconds);
	}
	else if (bReturningFreeLookCamera)
	{
		UpdateFreeLookCameraReturn(DeltaSeconds);
	}

	UpdateOverheadNameVisibility();

	if (HasAuthority())
	{
		if (bIsRolling && bRollMovementActive)
		{
			UpdateRollMovement();
		}

		UpdateStamina(DeltaSeconds);
	}
}

void AFrontierPlayerCharacter::BeginInteractionFacing(const FVector& TargetWorldLocation)
{
	FVector DirectionToTarget = TargetWorldLocation - GetActorLocation();
	DirectionToTarget.Z = 0.0f;
	if (DirectionToTarget.IsNearlyZero())
	{
		return;
	}

	InteractionTargetYaw = DirectionToTarget.Rotation().Yaw;
	bInteractionRotationLocked = true;
	bUseControllerRotationYaw = false;
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->bOrientRotationToMovement = false;
	}
}

void AFrontierPlayerCharacter::EndInteractionFacing()
{
	if (!bInteractionRotationLocked)
	{
		return;
	}

	bInteractionRotationLocked = false;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::UpdateInteractionFacing(const float DeltaSeconds)
{
	if (!bInteractionRotationLocked)
	{
		return;
	}

	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation(0.0f, InteractionTargetYaw, 0.0f);
	SetActorRotation(FMath::RInterpTo(
		CurrentRotation,
		TargetRotation,
		DeltaSeconds,
		InteractionFacingInterpSpeed));
}

void AFrontierPlayerCharacter::UpdateOverheadNameVisibility()
{
	if (GetNetMode() == NM_DedicatedServer || !OverheadNameWidgetComponent)
	{
		return;
	}

	AFrontierPlayerController* LocalPlayerController = Cast<AFrontierPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AFrontierPlayerCharacter* LocalPlayerCharacter = LocalPlayerController ? Cast<AFrontierPlayerCharacter>(LocalPlayerController->GetPawn()) : nullptr;
	AFrontierPlayerState* LocalPlayerState = LocalPlayerCharacter ? LocalPlayerCharacter->GetPlayerState<AFrontierPlayerState>() : nullptr;
	AFrontierPlayerState* TargetPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (!LocalPlayerController || !LocalPlayerState || !TargetPlayerState || IsDead())
	{
		OverheadNameWidgetComponent->SetVisibility(false, true);
		return;
	}

	const bool bInLobby = LocalPlayerController->IsInLobbyContext();
	const bool bSameTeam = LocalPlayerState->GetTeamId() > 0
		&& LocalPlayerState->GetTeamId() == TargetPlayerState->GetTeamId();
	const bool bEnemyRevealActive = !bInLobby
		&& LocalPlayerCharacter != this
		&& !bSameTeam
		&& GetWorld()
		&& GetWorld()->GetTimeSeconds() < EnemyOverheadVisibleUntilSeconds;
	if (UFrontierOverheadPlayerNameWidget* NameWidget = Cast<UFrontierOverheadPlayerNameWidget>(OverheadNameWidgetComponent->GetUserWidgetObject()))
	{
		NameWidget->SetEnemyPresentation(bEnemyRevealActive);
	}

	bool bShouldShow = false;
	if (bInLobby)
	{
		bShouldShow = true;
	}
	else
	{
		bShouldShow = LocalPlayerCharacter != this && (bSameTeam || bEnemyRevealActive);
	}

	if (bShouldShow)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		LocalPlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

		const FVector TargetLocation = GetActorLocation() + FVector(0.0f, 0.0f, 120.0f);
		FHitResult HitResult;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(FrontierOverheadNameVisibility), true);
		QueryParams.AddIgnoredActor(LocalPlayerCharacter);
		QueryParams.AddIgnoredActor(this);

		if (GetWorld()->LineTraceSingleByChannel(HitResult, ViewLocation, TargetLocation, ECC_Visibility, QueryParams))
		{
			bShouldShow = HitResult.GetActor() == this;
		}
	}

	OverheadNameWidgetComponent->SetVisibility(bShouldShow, true);
}

void AFrontierPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierPlayerCharacter, bIsSprinting);
	DOREPLIFETIME(AFrontierPlayerCharacter, bIsRolling);
	DOREPLIFETIME(AFrontierPlayerCharacter, SelectedCharacterType);
}

AFrontierWeaponBase* AFrontierPlayerCharacter::GetCurrentWeapon() const
{
	return EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
}

UFrontierEquipmentComponent* AFrontierPlayerCharacter::GetEquipmentComponent() const
{
	return EquipmentComponent;
}

USkeletalMeshComponent* AFrontierPlayerCharacter::GetArmorMesh() const
{
	return ArmorMeshComponent;
}

USkeletalMeshComponent* AFrontierPlayerCharacter::GetGloveMesh() const
{
	return GloveMeshComponent;
}

USkeletalMeshComponent* AFrontierPlayerCharacter::GetGreavesMesh() const
{
	return GreavesMeshComponent;
}

USkeletalMeshComponent* AFrontierPlayerCharacter::GetHeadMesh() const
{
	return HeadMeshComponent;
}

void AFrontierPlayerCharacter::ApplyTemporarySingleMeshMode()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	CharacterMesh->SetVisibility(true, false);
	CharacterMesh->SetHiddenInGame(false, false);
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		if (ModularMesh)
		{
			ModularMesh->SetVisibility(false, true);
			ModularMesh->SetHiddenInGame(true, true);
		}
	}
}

#if 0 // TEMP: Preserved modular-character implementation. Do not delete.
void AFrontierPlayerCharacter::RefreshModularMeshLeaderPose()
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		return;
	}

	CharacterMesh->SetVisibility(false, false);
	CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		if (ModularMesh)
		{
			ModularMesh->SetLeaderPoseComponent(CharacterMesh, false, true);
		}
	}
}
#endif

AFrontierCharacterPreviewActor* AFrontierPlayerCharacter::GetOrCreateCharacterPreviewActor(
	TSubclassOf<AFrontierCharacterPreviewActor> OverridePreviewActorClass,
	const FVector& PreviewLocation)
{
	if (CharacterPreviewActor && !CharacterPreviewActor->IsActorBeingDestroyed())
	{
		CharacterPreviewActor->SetActorLocation(PreviewLocation);
		return CharacterPreviewActor;
	}

	UWorld* World = GetWorld();
	TSubclassOf<AFrontierCharacterPreviewActor> SpawnClass = OverridePreviewActorClass ? OverridePreviewActorClass : CharacterPreviewActorClass;
	if (!World || !SpawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParameters.ObjectFlags |= RF_Transient;

	CharacterPreviewActor = World->SpawnActor<AFrontierCharacterPreviewActor>(
		SpawnClass,
		PreviewLocation,
		FRotator::ZeroRotator,
		SpawnParameters);

	return CharacterPreviewActor;
}

AFrontierCharacterPreviewActor* AFrontierPlayerCharacter::GetCharacterPreviewActor() const
{
	return CharacterPreviewActor;
}

bool AFrontierPlayerCharacter::ApplyCharacterAppearance(
	const FFrontierCharacterAppearanceData& AppearanceData)
{
	USkeletalMeshComponent* CharacterMesh = GetMesh();
	if (!CharacterMesh)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot apply character appearance because the mesh component is null."));
		return false;
	}

	// Keep loading behind this application function so it can move to StreamableManager later.
	USkeletalMesh* LoadedMesh = AppearanceData.SkeletalMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Failed to load in-game character mesh. CharacterType=%s Asset=%s"),
			*UEnum::GetValueAsString(AppearanceData.CharacterType),
			*AppearanceData.SkeletalMesh.ToSoftObjectPath().ToString());
		return false;
	}

	UClass* LoadedAnimationClass = AppearanceData.AnimationClass.LoadSynchronous();
	if (!AppearanceData.AnimationClass.IsNull() && !LoadedAnimationClass)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Failed to load in-game animation class. CharacterType=%s Asset=%s"),
			*UEnum::GetValueAsString(AppearanceData.CharacterType),
			*AppearanceData.AnimationClass.ToSoftObjectPath().ToString());
		return false;
	}

	CharacterMesh->EmptyOverrideMaterials();
	CharacterMesh->SetSkeletalMesh(LoadedMesh);
	// RefreshModularMeshLeaderPose(); // TEMP: Modular mode is preserved but disabled.
	ApplyTemporarySingleMeshMode();
	if (LoadedAnimationClass)
	{
		CharacterMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
		CharacterMesh->SetAnimInstanceClass(LoadedAnimationClass);
	}

	ApplyTeamMaterialFromPlayerState();
	if (EquipmentComponent)
	{
		EquipmentComponent->RefreshArmorVisuals();
		EquipmentComponent->RefreshWeaponAnimationLayer();
	}

	FRONTIER_LOG(
		Log,
		TEXT("In-game character appearance applied. CharacterType=%s Mesh=%s"),
		*UEnum::GetValueAsString(AppearanceData.CharacterType),
		*LoadedMesh->GetPathName());
	return true;
}

bool AFrontierPlayerCharacter::ApplyCharacterType(
	const EFrontierCharacterType CharacterType)
{
	UGameInstance* GameInstance = GetGameInstance();
	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	const FFrontierCharacterAppearanceData* Appearance = SelectionSubsystem
		? SelectionSubsystem->FindAppearance(CharacterType)
		: nullptr;
	if (!Appearance)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("No in-game character appearance mapping found. CharacterType=%s"),
			*UEnum::GetValueAsString(CharacterType));
		return false;
	}

	return ApplyCharacterAppearance(*Appearance);
}

void AFrontierPlayerCharacter::RequestCharacterType(
	const EFrontierCharacterType CharacterType)
{
	if (!IsValidFrontierCharacterType(CharacterType) || !IsLocallyControlled())
	{
		return;
	}

	// Apply immediately for responsive local presentation, then let the server
	// replicate the authoritative enum to every connection.
	SelectedCharacterType = CharacterType;
	ApplyCharacterType(CharacterType);

	if (HasAuthority())
	{
		ForceNetUpdate();
		return;
	}

	ServerSetCharacterType(CharacterType);
}

void AFrontierPlayerCharacter::ServerSetCharacterType_Implementation(
	const EFrontierCharacterType NewCharacterType)
{
	if (!IsValidFrontierCharacterType(NewCharacterType))
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Rejected invalid character type. Character=%s Value=%d"),
			*GetNameSafe(this),
			static_cast<int32>(NewCharacterType));
		return;
	}

	SelectedCharacterType = NewCharacterType;
	if (GetNetMode() != NM_DedicatedServer)
	{
		ApplyCharacterType(SelectedCharacterType);
	}
	else if (EquipmentComponent)
	{
		// Dedicated servers do not create render components, but they must rebuild
		// the public armor snapshot for remote clients.
		EquipmentComponent->RefreshArmorVisuals();
	}
	ForceNetUpdate();
}

void AFrontierPlayerCharacter::OnRep_SelectedCharacterType()
{
	if (!IsValidFrontierCharacterType(SelectedCharacterType))
	{
		SelectedCharacterType = EFrontierCharacterType::DarkKnight;
	}

	ApplyCharacterType(SelectedCharacterType);
}

bool AFrontierPlayerCharacter::IsRolling() const
{
	return bIsRolling;
}

bool AFrontierPlayerCharacter::IsAttackAnimationActive() const
{
	const UFrontierAbilitySystemComponent* FrontierASC = GetFrontierAbilitySystemComponent();
	return FrontierASC && FrontierASC->HasMatchingGameplayTag(FFrontierGameplayTags::Get().StateActionAttacking);
}

float AFrontierPlayerCharacter::GetAttackMovementSpeedMultiplier() const
{
	return AttackMovementSpeedMultiplier;
}

EFrontierAttackAnimationMode AFrontierPlayerCharacter::GetCurrentAttackAnimationMode() const
{
	return CurrentAttackAnimationMode;
}

void AFrontierPlayerCharacter::ApplyTeamMaterialFromPlayerState()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (!FrontierPlayerState || !TeamVisualDataAsset || !GetMesh())
	{
		return;
	}

	UMaterialInterface* TeamMaterial = TeamVisualDataAsset->GetMaterialForTeam(FrontierPlayerState->GetTeamId());
	if (!TeamMaterial)
	{
		return;
	}

	const int32 MaterialCount = GetMesh()->GetNumMaterials();
	for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		GetMesh()->SetMaterial(MaterialIndex, TeamMaterial);
	}

	FRONTIER_LOG(Log, TEXT("Applied team material. Character=%s TeamId=%d Material=%s"),
		*GetNameSafe(this),
		FrontierPlayerState->GetTeamId(),
		*GetNameSafe(TeamMaterial));
}

void AFrontierPlayerCharacter::BindToFrontierPlayerState()
{
	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	if (BoundFrontierPlayerState.Get() == FrontierPlayerState)
	{
		return;
	}

	UnbindFromFrontierPlayerState();

	BoundFrontierPlayerState = FrontierPlayerState;
	if (FrontierPlayerState)
	{
		FrontierPlayerState->OnTeamIdChanged.AddUObject(this, &AFrontierPlayerCharacter::HandleTeamIdChanged);
	}
}

void AFrontierPlayerCharacter::UnbindFromFrontierPlayerState()
{
	if (AFrontierPlayerState* FrontierPlayerState = BoundFrontierPlayerState.Get())
	{
		FrontierPlayerState->OnTeamIdChanged.RemoveAll(this);
	}

	BoundFrontierPlayerState.Reset();
}

void AFrontierPlayerCharacter::HandleTeamIdChanged(AFrontierPlayerState* ChangedPlayerState, const int32 NewTeamId)
{
	if (ChangedPlayerState == GetPlayerState<AFrontierPlayerState>())
	{
		SetTeam(ResolvePlayerTeam(NewTeamId));
		ApplyTeamMaterialFromPlayerState();
	}
}

AFrontierLootContainerActor* AFrontierPlayerCharacter::SpawnDeathLootContainer()
{
	

	if (!HasAuthority())
	{
		return nullptr;
	}

	TArray<FFrontierInventorySlot> DroppedLootSlots;
	CreateDeathLootSlots(DroppedLootSlots);

	AFrontierLootContainerActor* LootContainer = Super::SpawnDeathLootContainer();
	if (LootContainer)
	{
		HandleItemsDroppedOnDeath(DroppedLootSlots);
	}

	return LootContainer;
}

void AFrontierPlayerCharacter::CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const
{
	

	OutLootSlots.Reset();

	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	const UFrontierRaidInventoryComponent* RaidInventoryComponent = FrontierPlayerState ? FrontierPlayerState->GetRaidInventoryComponent() : nullptr;

	if (RaidInventoryComponent)
	{
		for (const FFrontierInventorySlot& RaidSlot : RaidInventoryComponent->GetSlots())
		{
			FFrontierInventorySlot LootSlot = RaidSlot;
			if (!LootSlot.bOccupied || !LootSlot.ItemInstance.IsValid())
			{
				LootSlot.bOccupied = false;
				LootSlot.ItemInstance = FFrontierItemInstance();
			}

			OutLootSlots.Add(LootSlot);
		}
	}
}

void AFrontierPlayerCharacter::CreateDeathLoadoutSlots(TArray<FFrontierLoadoutSlot>& OutLoadoutSlots) const
{
	

	OutLoadoutSlots.Reset();

	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	const UFrontierLoadoutComponent* LoadoutComponent = FrontierPlayerState ? FrontierPlayerState->GetLoadoutComponent() : nullptr;

	if (LoadoutComponent)
	{
		for (const FFrontierLoadoutSlot& LoadoutSlot : LoadoutComponent->GetLoadoutSlots())
		{
			OutLoadoutSlots.Add(LoadoutSlot);
		}
	}
}

EFrontierLootContainerSourceType AFrontierPlayerCharacter::GetDeathLootContainerSourceType() const
{
	return EFrontierLootContainerSourceType::PlayerDeath;
}

void AFrontierPlayerCharacter::HandleItemsDroppedOnDeath(const TArray<FFrontierInventorySlot>& DroppedLootSlots)
{
	FRONTIER_LOG(Log, TEXT("Player death dropped %d loot slots and will clear raid/loadout ownership."), DroppedLootSlots.Num());

	if (AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>())
	{
		FrontierPlayerState->LoseRaidItemsOnDeath();

		if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
		{
			if (AFrontierGameMode* FrontierGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<AFrontierGameMode>() : nullptr)
			{
				FrontierGameMode->HandlePlayerDeathReturnToLobby(FrontierPlayerController);
			}
		}
	}
}

void AFrontierPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (JumpAction)
		{
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_JumpStarted);
			EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AFrontierPlayerCharacter::Input_JumpEnded);
		}

		if (MoveAction)
		{
			EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AFrontierPlayerCharacter::Move);
		}

		if (LookAction)
		{
			EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AFrontierPlayerCharacter::Look);
		}
		
		if (RightClickAction)
		{
			EnhancedInputComponent->BindAction(RightClickAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::OnRightClick);
		}

		if (AttackAction)
		{
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_AttackStarted);
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Completed, this, &AFrontierPlayerCharacter::Input_AttackReleased);
			EnhancedInputComponent->BindAction(AttackAction, ETriggerEvent::Canceled, this, &AFrontierPlayerCharacter::Input_AttackReleased);
		}

		if (InteractAction)
		{
			EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_Interact);
		}

		if (InventoryAction)
		{
			EnhancedInputComponent->BindAction(InventoryAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_Inventory);
		}

		

		if (SprintAction)
		{
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_SprintStarted);
			EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AFrontierPlayerCharacter::Input_SprintEnded);
		}

		if (RollAction)
		{
			EnhancedInputComponent->BindAction(RollAction, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_Roll);
		}

		if (UseSkillSlot1Action)
		{
			EnhancedInputComponent->BindAction(UseSkillSlot1Action, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_UseSkillSlot1);
		}

		if (UseSkillSlot2Action)
		{
			EnhancedInputComponent->BindAction(UseSkillSlot2Action, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_UseSkillSlot2);
		}

		if (UseSkillSlot3Action)
		{
			EnhancedInputComponent->BindAction(UseSkillSlot3Action, ETriggerEvent::Started, this, &AFrontierPlayerCharacter::Input_UseSkillSlot3);
		}
	}

	PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &AFrontierPlayerCharacter::Input_FreeLookStarted);
	PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Released, this, &AFrontierPlayerCharacter::Input_FreeLookEnded);
	PlayerInputComponent->BindKey(EKeys::RightAlt, IE_Pressed, this, &AFrontierPlayerCharacter::Input_FreeLookStarted);
	PlayerInputComponent->BindKey(EKeys::RightAlt, IE_Released, this, &AFrontierPlayerCharacter::Input_FreeLookEnded);
}

void AFrontierPlayerCharacter::Move(const FInputActionValue& Value)
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			if (AFrontierPlayerController* MutableFrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
			{
				MutableFrontierPlayerController->ToggleInventoryUI();
			}
		}
	}

	const FVector2D MovementVector = Value.Get<FVector2D>();
	if (!MovementVector.IsNearlyZero())
	{
		if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
		{
			FrontierPlayerController->CancelInteractionChannelForLocalAction();
		}
	}

	if (bIsRolling)
	{
		return;
	}

	if (!GetController())
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
	const FVector DesiredMovementDirection = ((ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X)).GetSafeNormal2D();
	CachedDesiredMovementDirection = DesiredMovementDirection;

	AddMovementInput(ForwardDirection, MovementVector.Y);
	AddMovementInput(RightDirection, MovementVector.X);
}

void AFrontierPlayerCharacter::Look(const FInputActionValue& Value)
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (!FrontierPlayerController->ShouldAcceptLookInput())
		{
			return;
		}

		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	float MouseSensitivityScale = 1.0f;
	if (const UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UFrontierUserSettingsSubsystem* Settings =
			GameInstance->GetSubsystem<UFrontierUserSettingsSubsystem>())
		{
			MouseSensitivityScale = Settings->GetMouseSensitivityScale();
		}
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>() * MouseSensitivityScale;
	if (TryAdjustAreaSkillTargetDistance(LookAxisVector.Y))
	{
		AddControllerYawInput(LookAxisVector.X);
		return;
	}

	if (bIsFreeLooking && CameraBoom)
	{
		FRotator FreeLookRotation = CameraBoom->GetRelativeRotation();
		FreeLookRotation.Yaw += LookAxisVector.X;
		FreeLookRotation.Pitch = FMath::ClampAngle(FreeLookRotation.Pitch + LookAxisVector.Y, FreeLookMinPitch, FreeLookMaxPitch);
		FreeLookRotation.Roll = 0.0f;
		CameraBoom->SetRelativeRotation(FreeLookRotation);
		return;
	}

	AddControllerYawInput(LookAxisVector.X);
	AddControllerPitchInput(LookAxisVector.Y);
}

void AFrontierPlayerCharacter::Input_FreeLookStarted()
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	SetFreeLooking(true);
}

void AFrontierPlayerCharacter::Input_FreeLookEnded()
{
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (PlayerController->IsInputKeyDown(EKeys::LeftAlt) || PlayerController->IsInputKeyDown(EKeys::RightAlt))
		{
			return;
		}
	}

	SetFreeLooking(false);
}

void AFrontierPlayerCharacter::Input_AttackStarted()
{
	if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}

		FrontierPlayerController->CancelInteractionChannelForLocalAction();
	}

	if (bIsRolling)
	{
		return;
	}

	if (TryConfirmAreaSkillInput())
	{
		return;
	}

	SetAttackInputState(true);
	if (!HasAuthority())
	{
		ServerSetAttackInputState(true);
	}
}

void AFrontierPlayerCharacter::UpdateAimingCamera(const float DeltaSeconds)
{
	if (!CameraBoom || !IsLocallyControlled())
	{
		return;
	}

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bActivationDelayElapsed = IsAiming()
		&& AimingStartedAtSeconds >= 0.0f
		&& CurrentTime - AimingStartedAtSeconds >= AimingCameraActivationDelay;
	const FVector TargetOffset = bActivationDelayElapsed
		? AimingCameraSocketOffset
		: DefaultCameraSocketOffset;

	CameraBoom->SocketOffset = FMath::VInterpTo(
		CameraBoom->SocketOffset,
		TargetOffset,
		DeltaSeconds,
		AimingCameraInterpSpeed);
	if (CameraBoom->SocketOffset.Equals(TargetOffset, 0.01f))
	{
		CameraBoom->SocketOffset = TargetOffset;
	}
}

void AFrontierPlayerCharacter::SetNockedArrowVisual(
	const bool bVisible,
	UStaticMesh* ArrowMesh,
	const FName CharacterSocketName,
	const FTransform& RelativeTransform)
{
	if (HasAuthority())
	{
		MulticastSetNockedArrowVisual(bVisible, ArrowMesh, CharacterSocketName, RelativeTransform);
		return;
	}

	if (IsLocallyControlled())
	{
		ApplyNockedArrowVisual(bVisible, ArrowMesh, CharacterSocketName, RelativeTransform);
	}
}

void AFrontierPlayerCharacter::MulticastSetNockedArrowVisual_Implementation(
	const bool bVisible,
	UStaticMesh* ArrowMesh,
	const FName CharacterSocketName,
	const FTransform RelativeTransform)
{
	ApplyNockedArrowVisual(bVisible, ArrowMesh, CharacterSocketName, RelativeTransform);
}

void AFrontierPlayerCharacter::ApplyNockedArrowVisual(
	const bool bVisible,
	UStaticMesh* ArrowMesh,
	const FName CharacterSocketName,
	const FTransform& RelativeTransform)
{
	if (!NockedArrowMeshComponent || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	if (!bVisible || !ArrowMesh || CharacterSocketName.IsNone())
	{
		NockedArrowMeshComponent->SetVisibility(false, true);
		NockedArrowMeshComponent->SetHiddenInGame(true);
		return;
	}

	NockedArrowMeshComponent->SetStaticMesh(ArrowMesh);
	NockedArrowMeshComponent->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::SnapToTargetNotIncludingScale,
		CharacterSocketName);
	NockedArrowMeshComponent->SetRelativeTransform(RelativeTransform);
	NockedArrowMeshComponent->SetHiddenInGame(false);
	NockedArrowMeshComponent->SetVisibility(true, true);
}

void AFrontierPlayerCharacter::Input_AttackReleased()
{
	SetAttackInputState(false);
	if (!HasAuthority())
	{
		ServerSetAttackInputState(false);
	}
}

void AFrontierPlayerCharacter::ReportClientAttackHit(
	AActor* TargetActor,
	const FVector_NetQuantize HitLocation,
	const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (!HasAuthority() && IsLocallyControlled() && IsValid(TargetActor))
	{
		ServerReportAttackHit(TargetActor, HitLocation, TraceOverrides);
	}
}

void AFrontierPlayerCharacter::SetAttackInputState(const bool bPressed)
{
	bAttackInputHeld = bPressed;
	if (!bPressed)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HeldAttackRetryTimerHandle);
		}
	}

	UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	FGameplayAbilitySpec* AbilitySpec = ResolveEquippedAttackAbilitySpec(this);
	if (!ASC || !AbilitySpec)
	{
		if (bPressed)
		{
			// The server grants the weapon ability after the async equipment
			// preload. Keep a held input until the replicated spec is available.
			ScheduleHeldAttackRetry(0.03f);
		}
		return;
	}

	if (UFrontierGameplayAbility_WeaponAttack* ActiveAttackAbility = ResolveActiveWeaponAttackAbility(this))
	{
		if (bPressed)
		{
			ActiveAttackAbility->HandleAttackInputPressed();
		}
		else
		{
			ActiveAttackAbility->HandleAttackInputReleased();
		}
		return;
	}

	if (bPressed)
	{
		TryActivateHeldAttack();
	}
}

void AFrontierPlayerCharacter::TryActivateHeldAttack()
{
	if (!bAttackInputHeld || bIsRolling)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (IsAttackDelayActive())
	{
		const float ElapsedSeconds = World ? World->GetTimeSeconds() - LastAttackResetTime : 0.0f;
		ScheduleHeldAttackRetry(FMath::Max(ResolvedAttackDelay - ElapsedSeconds, 0.01f));
		return;
	}

	UFrontierEquipmentComponent* CurrentEquipmentComponent = GetEquipmentComponent();
	if (CurrentEquipmentComponent
		&& CurrentEquipmentComponent->GetCurrentWeaponData()
		&& !CurrentEquipmentComponent->AreCurrentWeaponAttackAssetsReady())
	{
		// Equipment preload is asynchronous. Keep the held input and retry after
		// the preload callback grants the attack ability.
		ScheduleHeldAttackRetry(0.03f);
		return;
	}

	UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	FGameplayAbilitySpec* AbilitySpec = ResolveEquippedAttackAbilitySpec(this);
	if (!ASC || !AbilitySpec)
	{
		ScheduleHeldAttackRetry(0.03f);
		return;
	}

	if (ResolveActiveWeaponAttackAbility(this))
	{
		return;
	}

	const bool bActivated = ASC->TryActivateAbility(AbilitySpec->Handle);
	if (!bActivated && bAttackInputHeld)
	{
		// A predicted activation can be rejected while the server is still
		// finishing possession, movement initialization, or ability replication.
		// Do not lose the first held input in that short synchronization window.
		ScheduleHeldAttackRetry(0.03f);
	}
}

void AFrontierPlayerCharacter::ScheduleHeldAttackRetry(const float DelaySeconds)
{
	UWorld* World = GetWorld();
	if (!World || !bAttackInputHeld)
	{
		return;
	}

	World->GetTimerManager().ClearTimer(HeldAttackRetryTimerHandle);
	World->GetTimerManager().SetTimer(
		HeldAttackRetryTimerHandle,
		this,
		&AFrontierPlayerCharacter::TryActivateHeldAttack,
		FMath::Max(DelaySeconds, 0.01f),
		false);
}

void AFrontierPlayerCharacter::OnRightClick()
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	TryCancelAreaSkillInput();
}


bool AFrontierPlayerCharacter::TryConfirmAreaSkillInput()
{
	

	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->ConfirmAreaSkill();
		return true;
	}

	const UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	const bool bCombatSkillActive = ASC && ASC->HasMatchingGameplayTag(FFrontierGameplayTags::Get().StateCombatSkill);
	if (!bCombatSkillActive)
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerConfirmAreaSkillInput();
	}

	return true;
}

bool AFrontierPlayerCharacter::TryCancelAreaSkillInput()
{
	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->CancelAreaSkill();
		return true;
	}

	const UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	const bool bCombatSkillActive = ASC && ASC->HasMatchingGameplayTag(FFrontierGameplayTags::Get().StateCombatSkill);
	if (!bCombatSkillActive)
	{
		return false;
	}

	if (!HasAuthority())
	{
		ServerCancelAreaSkillInput();
	}

	return true;
}

bool AFrontierPlayerCharacter::TryAdjustAreaSkillTargetDistance(const float LookPitchAxis)
{
	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->AdjustAreaTargetDistanceInput(LookPitchAxis);
		return true;
	}

	const UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	const bool bCombatSkillActive = ASC && ASC->HasMatchingGameplayTag(FFrontierGameplayTags::Get().StateCombatSkill);
	if (!bCombatSkillActive)
	{
		return false;
	}

	if (!HasAuthority() && !FMath::IsNearlyZero(LookPitchAxis))
	{
		ServerAdjustAreaSkillTargetDistance(LookPitchAxis);
	}

	return true;
}

void AFrontierPlayerCharacter::Input_Interact()
{
	

	if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsLootUIOpen())
		{
			FrontierPlayerController->RequestInteraction();
			return;
		}

		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}

		FrontierPlayerController->RequestInteraction();
	}
}

void AFrontierPlayerCharacter::Input_Inventory()
{
	

	if (AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		FrontierPlayerController->ToggleInventoryUI();
	}
}

void AFrontierPlayerCharacter::Input_JumpStarted()
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	if (bIsRolling)
	{
		return;
	}

	Jump();
}

void AFrontierPlayerCharacter::Input_JumpEnded()
{
	StopJumping();
}

void AFrontierPlayerCharacter::Input_SprintStarted()
{
	if (bIsRolling)
	{
		return;
	}

	if (!CanStartSprinting())
	{
		return;
	}

	SetSprintingInternal(true);

	if (!HasAuthority())
	{
		ServerSetSprinting(true);
	}
}

void AFrontierPlayerCharacter::Input_SprintEnded()
{
	SetSprintingInternal(false);

	if (!HasAuthority())
	{
		ServerSetSprinting(false);
	}
}

void AFrontierPlayerCharacter::Input_Roll()
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	if (!CanRoll())
	{
		return;
	}

	CancelActiveAttackForRoll();

	const FVector RollDirection = GetDesiredRollDirection();
	if (HasAuthority())
	{
		BeginRoll(RollDirection);
		return;
	}

	bRollRequestPending = true;
	ApplyRollDirectionRotation(RollDirection);
	ServerRequestRoll(RollDirection);
}

void AFrontierPlayerCharacter::Input_UseSkillSlot1()
{
	RequestUseSkillSlot(0);
}

void AFrontierPlayerCharacter::Input_UseSkillSlot2()
{
	RequestUseSkillSlot(1);
}

void AFrontierPlayerCharacter::Input_UseSkillSlot3()
{
	RequestUseSkillSlot(2);
}

void AFrontierPlayerCharacter::RequestUseSkillSlot(const int32 SlotIndex)
{
	if (const AFrontierPlayerController* FrontierPlayerController = Cast<AFrontierPlayerController>(GetController()))
	{
		if (FrontierPlayerController->IsInventoryUIOpen())
		{
			return;
		}
	}

	if (bIsRolling)
	{
		return;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (MovementComponent && MovementComponent->IsFalling())
	{
		return;
	}

	AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	UFrontierEquipmentSkillComponent* EquipmentSkillComponent = FrontierPlayerState ? FrontierPlayerState->GetEquipmentSkillComponent() : nullptr;
	if (!EquipmentSkillComponent)
	{
		FRONTIER_LOG(Warning, TEXT("Equipment skill request ignored because EquipmentSkillComponent is missing."));
		return;
	}

	EquipmentSkillComponent->RequestUseSkillSlot(SlotIndex);
}

bool AFrontierPlayerCharacter::CanStartSprinting() const
{
	if (IsDead())
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UFrontierAttributeSet* FrontierAttributeSet = GetFrontierAttributeSet();
	return MovementComponent
		&& MovementComponent->IsMovingOnGround()
		&& FrontierAttributeSet
		&& FrontierAttributeSet->GetStamina() >= MinStaminaToStartSprint;
}

bool AFrontierPlayerCharacter::CanRoll() const
{
	if (IsDead() || bIsRolling || bRollRequestPending)
	{
		return false;
	}

	const UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	const UFrontierAttributeSet* FrontierAttributeSet = GetFrontierAttributeSet();
	const UFrontierAbilitySystemComponent* FrontierASC = GetFrontierAbilitySystemComponent();
	return MovementComponent
		&& MovementComponent->IsMovingOnGround()
		&& FrontierAttributeSet
		&& (!FrontierASC || !FrontierASC->HasMatchingGameplayTag(FFrontierGameplayTags::Get().StateCCStun))
		&& FrontierAttributeSet->GetStamina() >= RollStaminaCost;
}

void AFrontierPlayerCharacter::CancelActiveAttackForRoll()
{
	CancelAttackAbilitiesForCharacter(this);
}

bool AFrontierPlayerCharacter::CanJumpInternal_Implementation() const
{
	if (!Super::CanJumpInternal_Implementation())
	{
		return false;
	}

	if (bIsRolling)
	{
		return false;
	}

	const UFrontierAttributeSet* FrontierAttributeSet = GetFrontierAttributeSet();
	return FrontierAttributeSet && FrontierAttributeSet->GetStamina() >= JumpStaminaCost;
}

void AFrontierPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	if (!HasAuthority())
	{
		return;
	}

	if (UFrontierAttributeSet* FrontierAttributeSet = GetMutableFrontierAttributeSet())
	{
		const bool bAppliedThroughGas = GetFrontierAbilitySystemComponent()
			&& GetFrontierAbilitySystemComponent()->ApplyStaminaDelta(-JumpStaminaCost);
		if (!bAppliedThroughGas)
		{
			const float NewStamina = FMath::Max(0.0f, FrontierAttributeSet->GetStamina() - JumpStaminaCost);
			FrontierAttributeSet->SetStamina(NewStamina);
		}
		LastStaminaConsumeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastStaminaConsumeTime;
	}
}

void AFrontierPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	if (bIsSprinting)
	{
		RefreshMovementSpeed();
	}
}

void AFrontierPlayerCharacter::RefreshMovementSpeed()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		const UFrontierAttributeSet* Attributes = GetFrontierAttributeSet();
		const float MoveSpeedMultiplier = GetEffectiveMovementSpeedMultiplier();
		const float JumpPowerMultiplier = FMath::Max(0.0f, 1.0f + (Attributes ? Attributes->GetJumpPowerBonus() : 0.0f));
		MovementComponent->JumpZVelocity = BaseJumpZVelocity * JumpPowerMultiplier;
		const bool bAttackAnimationActive = IsAttackAnimationActive();
		bUseControllerRotationYaw = !bInteractionRotationLocked
			&& !bIsRolling
			&& (!bAttackAnimationActive || bAttackUseControllerRotation);
		MovementComponent->bOrientRotationToMovement = false;

		if (bIsRolling)
		{
			MovementComponent->MaxWalkSpeed = 0.0f;
			return;
		}

		float NewMaxWalkSpeed = (bIsSprinting ? SprintSpeed : WalkSpeed) * MoveSpeedMultiplier;
		if (bAttackAnimationActive)
		{
			NewMaxWalkSpeed *= FMath::Clamp(AttackMovementSpeedMultiplier, 0.0f, 1.0f);
		}
		if (bDamageMoveSlowActive)
		{
			NewMaxWalkSpeed *= FMath::Clamp(DamageMoveSlowMultiplier, 0.0f, 1.0f);
		}

		MovementComponent->MaxWalkSpeed = NewMaxWalkSpeed;
	}
}

void AFrontierPlayerCharacter::HandleMovementSpeedModifiersChanged()
{
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::BindPassiveMovementAttributes()
{
	UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent();
	if (BoundPassiveMovementASC.Get() == ASC && MoveSpeedBonusChangedHandle.IsValid() && JumpPowerBonusChangedHandle.IsValid())
	{
		RefreshMovementSpeed();
		return;
	}

	UnbindPassiveMovementAttributes();
	BoundPassiveMovementASC = ASC;
	if (!ASC)
	{
		return;
	}

	MoveSpeedBonusChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UFrontierAttributeSet::GetMoveSpeedBonusAttribute()).AddUObject(
			this, &AFrontierPlayerCharacter::HandlePassiveMovementAttributeChanged);
	JumpPowerBonusChangedHandle = ASC->GetGameplayAttributeValueChangeDelegate(
		UFrontierAttributeSet::GetJumpPowerBonusAttribute()).AddUObject(
			this, &AFrontierPlayerCharacter::HandlePassiveMovementAttributeChanged);
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::UnbindPassiveMovementAttributes()
{
	if (UFrontierAbilitySystemComponent* ASC = BoundPassiveMovementASC.Get())
	{
		if (MoveSpeedBonusChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetMoveSpeedBonusAttribute())
				.Remove(MoveSpeedBonusChangedHandle);
		}
		if (JumpPowerBonusChangedHandle.IsValid())
		{
			ASC->GetGameplayAttributeValueChangeDelegate(UFrontierAttributeSet::GetJumpPowerBonusAttribute())
				.Remove(JumpPowerBonusChangedHandle);
		}
	}

	MoveSpeedBonusChangedHandle.Reset();
	JumpPowerBonusChangedHandle.Reset();
	BoundPassiveMovementASC.Reset();
}

void AFrontierPlayerCharacter::HandlePassiveMovementAttributeChanged(const FOnAttributeChangeData& ChangeData)
{
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::SetSprintingInternal(const bool bNewSprinting)
{
	if (bIsSprinting == bNewSprinting)
	{
		return;
	}

	bIsSprinting = bNewSprinting;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::SetAttackMovementSpeedMultiplierInternal(const float NewMultiplier)
{
	const float ClampedMultiplier = FMath::Clamp(NewMultiplier, 0.0f, 1.0f);
	if (FMath::IsNearlyEqual(AttackMovementSpeedMultiplier, ClampedMultiplier))
	{
		return;
	}

	AttackMovementSpeedMultiplier = ClampedMultiplier;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::SetCurrentAttackAnimationModeInternal(const EFrontierAttackAnimationMode NewMode)
{
	CurrentAttackAnimationMode = NewMode;
}

void AFrontierPlayerCharacter::SetAttackUseControllerRotationInternal(const bool bNewUseControllerRotation)
{
	if (bAttackUseControllerRotation == bNewUseControllerRotation)
	{
		return;
	}

	bAttackUseControllerRotation = bNewUseControllerRotation;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::SetRollingInternal(const bool bNewRolling)
{
	if (bIsRolling == bNewRolling)
	{
		return;
	}

	bIsRolling = bNewRolling;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::UpdateStamina(const float DeltaSeconds)
{
	UFrontierAttributeSet* FrontierAttributeSet = GetMutableFrontierAttributeSet();
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!FrontierAttributeSet || !MovementComponent || IsDead())
	{
		return;
	}

	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const bool bCanActuallySprint = MovementComponent->IsMovingOnGround() && MovementComponent->Velocity.SizeSquared2D() > KINDA_SMALL_NUMBER;

	if (bIsSprinting)
	{
		if (!CanStartSprinting())
		{
			SetSprintingInternal(false);
			return;
		}

		if (bCanActuallySprint)
		{
			const float NewStamina = FMath::Max(0.0f, FrontierAttributeSet->GetStamina() - (SprintStaminaDrainPerSecond * DeltaSeconds));
			FrontierAttributeSet->SetStamina(NewStamina);
			LastStaminaConsumeTime = CurrentTimeSeconds;

			if (NewStamina <= 0.0f)
			{
				SetSprintingInternal(false);
			}
		}

		return;
	}

	if ((CurrentTimeSeconds - LastStaminaConsumeTime) < StaminaRecoveryDelay)
	{
		return;
	}

	if (FrontierAttributeSet->GetStamina() < FrontierAttributeSet->GetMaxStamina())
	{
		const float NewStamina = FMath::Min(
			FrontierAttributeSet->GetMaxStamina(),
			FrontierAttributeSet->GetStamina() + (StaminaRecoveryPerSecond * DeltaSeconds));
		FrontierAttributeSet->SetStamina(NewStamina);
	}
}

void AFrontierPlayerCharacter::OnRep_IsSprinting()
{
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::OnRep_IsRolling()
{
	bRollRequestPending = false;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::BindAttackStateTagDelegate()
{
	UFrontierAbilitySystemComponent* FrontierASC = GetFrontierAbilitySystemComponent();
	if (!FrontierASC)
	{
		return;
	}

	if (BoundAttackStateASC.Get() == FrontierASC && AttackStateTagChangedHandle.IsValid())
	{
		return;
	}

	if (BoundAttackStateASC.IsValid() && AttackStateTagChangedHandle.IsValid())
	{
		BoundAttackStateASC->RegisterGameplayTagEvent(FFrontierGameplayTags::Get().StateActionAttacking, EGameplayTagEventType::NewOrRemoved)
			.Remove(AttackStateTagChangedHandle);
		AttackStateTagChangedHandle.Reset();
	}

	BoundAttackStateASC = FrontierASC;
	AttackStateTagChangedHandle = FrontierASC->RegisterGameplayTagEvent(
		FFrontierGameplayTags::Get().StateActionAttacking,
		EGameplayTagEventType::NewOrRemoved).AddUObject(this, &AFrontierPlayerCharacter::HandleAttackStateTagChanged);

	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::HandleAttackStateTagChanged(const FGameplayTag CallbackTag, const int32 NewCount)
{
	RefreshMovementSpeed();
	if (NewCount == 0 && bAttackInputHeld)
	{
		// Defer activation until the previous ability has fully left the ASC activation scope.
		ScheduleHeldAttackRetry(0.01f);
	}
}

void AFrontierPlayerCharacter::BeginRoll(const FVector& RollDirection)
{
	if (!HasAuthority() || !CanRoll())
	{
		return;
	}

	SetSprintingInternal(false);
	SetRollingInternal(true);
	bRollRequestPending = false;
	bRollMovementActive = false;
	ApplyRollDirectionRotation(RollDirection);

	if (UFrontierAttributeSet* FrontierAttributeSet = GetMutableFrontierAttributeSet())
	{
		const bool bAppliedThroughGas = GetFrontierAbilitySystemComponent()
			&& GetFrontierAbilitySystemComponent()->ApplyStaminaDelta(-RollStaminaCost);
		if (!bAppliedThroughGas)
		{
			FrontierAttributeSet->SetStamina(FMath::Max(0.0f, FrontierAttributeSet->GetStamina() - RollStaminaCost));
		}
		LastStaminaConsumeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastStaminaConsumeTime;
	}

	if (RollMontage)
	{
		MulticastPlayRollMontage(CachedRollDirection);
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (!bCachedRollMovementSettings)
		{
			CachedGroundFriction = MovementComponent->GroundFriction;
			CachedBrakingDecelerationWalking = MovementComponent->BrakingDecelerationWalking;
			CachedBrakingFrictionFactor = MovementComponent->BrakingFrictionFactor;
			bCachedRollMovementSettings = true;
		}

		MovementComponent->GroundFriction = 0.0f;
		MovementComponent->BrakingDecelerationWalking = 0.0f;
		MovementComponent->BrakingFrictionFactor = 0.0f;
		MovementComponent->Velocity = FVector::ZeroVector;
	}

}

void AFrontierPlayerCharacter::FinishRoll()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		if (bCachedRollMovementSettings)
		{
			MovementComponent->GroundFriction = CachedGroundFriction;
			MovementComponent->BrakingDecelerationWalking = CachedBrakingDecelerationWalking;
			MovementComponent->BrakingFrictionFactor = CachedBrakingFrictionFactor;
			bCachedRollMovementSettings = false;
		}

		MovementComponent->StopMovementImmediately();
	}

	SetRollingInternal(false);
	bRollRequestPending = false;
	bRollMovementActive = false;
}

FVector AFrontierPlayerCharacter::GetDesiredRollDirection() const
{
	if (!CachedDesiredMovementDirection.IsNearlyZero())
	{
		return CachedDesiredMovementDirection.GetSafeNormal2D();
	}

	return GetActorForwardVector().GetSafeNormal2D();
}

void AFrontierPlayerCharacter::ApplyRollDirectionRotation(const FVector& RollDirection)
{
	CachedRollDirection = RollDirection.GetSafeNormal2D();
	if (CachedRollDirection.IsNearlyZero())
	{
		CachedRollDirection = GetActorForwardVector().GetSafeNormal2D();
	}

	SetActorRotation(CachedRollDirection.Rotation());
}

void AFrontierPlayerCharacter::NotifyStaminaConsumptionFinished()
{
	LastStaminaConsumeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastStaminaConsumeTime;
}

void AFrontierPlayerCharacter::NotifyAttackStaminaConsumed()
{
	LastStaminaConsumeTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastStaminaConsumeTime;
}

void AFrontierPlayerCharacter::StartAttackDelay(const float AttackPlayRate)
{
	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : LastAttackResetTime;
	LastAttackResetTime = CurrentTimeSeconds;
	ResolvedAttackDelay = AttackDelay / FMath::Max(AttackPlayRate, 0.1f);
	LastStaminaConsumeTime = CurrentTimeSeconds;
}

bool AFrontierPlayerCharacter::IsAttackDelayActive() const
{
	if (ResolvedAttackDelay <= 0.0f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return (World->GetTimeSeconds() - LastAttackResetTime) < ResolvedAttackDelay;
}

float AFrontierPlayerCharacter::GetMovementDirection() const
{
	const FVector MovementDirection = GetVelocity().GetSafeNormal2D();
	if (MovementDirection.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector CharacterForward = GetActorForwardVector().GetSafeNormal2D();
	const FVector CharacterRight = GetActorRightVector().GetSafeNormal2D();
	return FMath::RadiansToDegrees(FMath::Atan2(
		FVector::DotProduct(CharacterRight, MovementDirection),
		FVector::DotProduct(CharacterForward, MovementDirection)));
}

void AFrontierPlayerCharacter::UpdateRollMovement()
{
	UCharacterMovementComponent* MovementComponent = GetCharacterMovement();
	if (!MovementComponent)
	{
		return;
	}

	const FVector RollVelocity = CachedRollDirection.GetSafeNormal2D() * RollMoveSpeed;
	MovementComponent->Velocity = FVector(RollVelocity.X, RollVelocity.Y, MovementComponent->Velocity.Z);
}

void AFrontierPlayerCharacter::BeginRollMovementWindow()
{
	if (!bIsRolling)
	{
		return;
	}

	bRollMovementActive = true;
}

void AFrontierPlayerCharacter::EndRollMovementWindow()
{
	

	bRollMovementActive = false;

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->Velocity = FVector(0.0f, 0.0f, MovementComponent->Velocity.Z);
	}

	FinishRoll();
}

void AFrontierPlayerCharacter::HandleRollMoveStartNotify()
{
	

	if (HasAuthority())
	{
		BeginRollMovementWindow();
		return;
	}

	if (IsLocallyControlled())
	{
		BeginRollMovementWindow();
		ServerBeginRollMovementWindow();
	}
}

void AFrontierPlayerCharacter::HandleRollMoveEndNotify()
{
	

	if (HasAuthority())
	{
		EndRollMovementWindow();
		return;
	}

	if (IsLocallyControlled())
	{
		EndRollMovementWindow();
		ServerEndRollMovementWindow();
	}
}

void AFrontierPlayerCharacter::CancelRollImmediately()
{
	if (!bIsRolling)
	{
		return;
	}

	FinishRoll();
}

void AFrontierPlayerCharacter::ApplyTemporaryDamageMoveSlow()
{
	if (DamageMoveSlowDuration <= 0.0f)
	{
		return;
	}

	bDamageMoveSlowActive = true;
	RefreshMovementSpeed();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DamageMoveSlowTimerHandle);
		World->GetTimerManager().SetTimer(DamageMoveSlowTimerHandle, this, &AFrontierPlayerCharacter::ClearDamageMoveSlow, DamageMoveSlowDuration, false);
	}
}

void AFrontierPlayerCharacter::ClearDamageMoveSlow()
{
	bDamageMoveSlowActive = false;
	RefreshMovementSpeed();
}

void AFrontierPlayerCharacter::UpdateRollCamera(const float DeltaSeconds)
{
	(void)DeltaSeconds;
}

void AFrontierPlayerCharacter::UpdateFreeLookCameraReturn(const float DeltaSeconds)
{
	if (!CameraBoom)
	{
		bReturningFreeLookCamera = false;
		return;
	}

	const FRotator CurrentRotation = CameraBoom->GetRelativeRotation();
	const FRotator TargetRotation = FRotator::ZeroRotator;
	const FRotator NewRotation = FMath::RInterpTo(
		CurrentRotation,
		TargetRotation,
		DeltaSeconds,
		FreeLookCameraReturnInterpSpeed);

	CameraBoom->SetRelativeRotation(NewRotation);

	if (NewRotation.Equals(TargetRotation, 0.5f))
	{
		CameraBoom->SetRelativeRotation(TargetRotation);
		bReturningFreeLookCamera = false;

		if (AController* CurrentController = GetController())
		{
			const FRotator CurrentControlRotation = CurrentController->GetControlRotation();
			CurrentController->SetControlRotation(FRotator(CurrentControlRotation.Pitch, GetActorRotation().Yaw, 0.0f));
		}

		CameraBoom->bUsePawnControlRotation = true;
	}
}

void AFrontierPlayerCharacter::SetFreeLooking(const bool bNewFreeLooking)
{
	if (!bEnableAltFreeLook || !IsLocallyControlled() || !CameraBoom)
	{
		return;
	}

	if (bIsFreeLooking == bNewFreeLooking)
	{
		return;
	}

	bIsFreeLooking = bNewFreeLooking;

	if (bIsFreeLooking)
	{
		

		if (!bReturningFreeLookCamera)
		{
			FRotator InitialFreeLookRotation = FRotator::ZeroRotator;
			if (const AController* CurrentController = GetController())
			{
				InitialFreeLookRotation = CurrentController->GetControlRotation() - GetActorRotation();
				InitialFreeLookRotation.Normalize();
				InitialFreeLookRotation.Pitch = FMath::ClampAngle(InitialFreeLookRotation.Pitch, FreeLookMinPitch, FreeLookMaxPitch);
				InitialFreeLookRotation.Roll = 0.0f;
			}

			CameraBoom->SetRelativeRotation(InitialFreeLookRotation);
		}

		bReturningFreeLookCamera = false;
		CameraBoom->bUsePawnControlRotation = false;
		return;
	}

	
	bReturningFreeLookCamera = true;
	CameraBoom->bUsePawnControlRotation = false;
}

void AFrontierPlayerCharacter::ServerRequestRoll_Implementation(FVector_NetQuantizeNormal RollDirection)
{
	const FVector ResolvedRollDirection = RollDirection.IsNearlyZero()
		? GetDesiredRollDirection()
		: FVector(RollDirection);

	CancelActiveAttackForRoll();

	if (!CanRoll())
	{
		bRollRequestPending = false;
		return;
	}

	BeginRoll(ResolvedRollDirection);
}

void AFrontierPlayerCharacter::ServerSetAttackInputState_Implementation(const bool bPressed)
{
	// Keep the server-side held state in sync for auto-combo decisions. The
	// locally predicted ability activation remains the primary path; only start
	// it here when the server has not received the activation yet.
	bAttackInputHeld = bPressed;
	if (!bPressed)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(HeldAttackRetryTimerHandle);
		}
		return;
	}

	if (!ResolveActiveWeaponAttackAbility(this))
	{
		TryActivateHeldAttack();
	}
}

void AFrontierPlayerCharacter::	ServerReportAttackHit_Implementation(
	AActor* TargetActor,
	const FVector_NetQuantize HitLocation,
	const FFrontierMeleeTraceOverrides TraceOverrides)
{
	if (!IsValid(TargetActor) || TargetActor == this)
	{
		FRONTIER_LOG(Warning, TEXT("Rejected client attack hit report because the target is invalid. Source=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		return;
	}

	UFrontierGameplayAbility_PlayerAttack* AttackAbility = Cast<UFrontierGameplayAbility_PlayerAttack>(ResolveActiveWeaponAttackAbility(this));
	if (!AttackAbility)
	{
		FRONTIER_LOG(Warning, TEXT("Rejected client attack hit report because the server attack ability is not active. Source=%s Target=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor));
		return;
	}

	FRONTIER_LOG(VeryVerbose, TEXT("Client attack hit report received. Source=%s Target=%s Location=%s"), *GetNameSafe(this), *GetNameSafe(TargetActor), *FVector(HitLocation).ToString());
	AttackAbility->HandleClientReportedAttackHit(TargetActor, FVector(HitLocation), TraceOverrides);
}

void AFrontierPlayerCharacter::RequestComboAttackToServer(const int32 RequestedComboIndex)
{
	if (!HasAuthority())
	{
		ServerRequestComboAttack(RequestedComboIndex);
	}
}

void AFrontierPlayerCharacter::ServerRequestComboAttack_Implementation(const int32 RequestedComboIndex)
{
	if (UFrontierGameplayAbility_PlayerAttack* AttackAbility = Cast<UFrontierGameplayAbility_PlayerAttack>(ResolveActiveWeaponAttackAbility(this)))
	{
		AttackAbility->RequestComboAttackFromServer(RequestedComboIndex);
	}
}

void AFrontierPlayerCharacter::ServerConfirmAreaSkillInput_Implementation()
{
	

	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->ConfirmAreaSkill();
	}
}

void AFrontierPlayerCharacter::ServerCancelAreaSkillInput_Implementation()
{
	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->CancelAreaSkill();
	}
}

void AFrontierPlayerCharacter::ServerAdjustAreaSkillTargetDistance_Implementation(const float LookPitchAxis)
{
	if (UFrontierGameplayAbility_AreaSkill* AreaSkillAbility = ResolveActiveAreaSkillAbility(this))
	{
		AreaSkillAbility->AdjustAreaTargetDistanceInput(LookPitchAxis);
	}
}

void AFrontierPlayerCharacter::ServerBeginRollMovementWindow_Implementation()
{
	BeginRollMovementWindow();
}

void AFrontierPlayerCharacter::ServerEndRollMovementWindow_Implementation()
{
	EndRollMovementWindow();
}

void AFrontierPlayerCharacter::MulticastPlayRollMontage_Implementation(FVector_NetQuantizeNormal RollDirection)
{
	if (!IsDead() && RollMontage)
	{
		ApplyRollDirectionRotation(FVector(RollDirection));
		PlayAnimMontage(RollMontage);
	}
}

void AFrontierPlayerCharacter::MulticastPlaySkillMontage_Implementation(UAnimMontage* Montage, const EFrontierAttackAnimationMode AnimationMode)
{
	if (!IsDead() && Montage)
	{
		SetCurrentAttackAnimationModeInternal(AnimationMode);
		const float MontageDuration = PlayAnimMontage(Montage);
		if (AnimationMode != EFrontierAttackAnimationMode::UpperBodyOnly && MontageDuration > 0.0f)
		{
			GetWorldTimerManager().ClearTimer(SkillMontageAnimationModeResetTimerHandle);
			GetWorldTimerManager().SetTimer(
				SkillMontageAnimationModeResetTimerHandle,
				this,
				&AFrontierPlayerCharacter::ResetSkillMontageAnimationMode,
				MontageDuration,
				false);
		}
	}
}

void AFrontierPlayerCharacter::MulticastPlayConsumableUse_Implementation(
	UAnimMontage* Montage,
	UStaticMesh* PotionMesh,
	const FName PotionSocketName,
	const FTransform PotionMeshRelativeTransform,
	const float HealthRestoreAmount,
	const float StaminaRestoreAmount)
{
	if (IsDead() || !Montage || bConsumableUsePending)
	{
		return;
	}

	SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	const float MontageDuration = PlayAnimMontage(Montage);
	if (MontageDuration <= 0.0f)
	{
		FRONTIER_LOG(Warning, TEXT("Consumable montage failed to play. Character=%s Montage=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Montage));
		return;
	}

	bConsumableUsePending = true;
	PendingConsumableHealthRestore = FMath::Max(0.0f, HealthRestoreAmount);
	PendingConsumableStaminaRestore = FMath::Max(0.0f, StaminaRestoreAmount);
	ActiveConsumableMontage = Montage;
	ApplyConsumablePotionVisual(PotionMesh, PotionSocketName, PotionMeshRelativeTransform);

	if (UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &AFrontierPlayerCharacter::HandleConsumableMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, Montage);
	}
}

void AFrontierPlayerCharacter::MulticastFinishConsumableUse_Implementation()
{
	bConsumableUsePending = false;
	PendingConsumableHealthRestore = 0.0f;
	PendingConsumableStaminaRestore = 0.0f;
	PendingConsumableEffectClass = nullptr;
	ActiveConsumableMontage = nullptr;
	ClearConsumablePotionVisual();
}

void AFrontierPlayerCharacter::MulticastCancelConsumableUse_Implementation()
{
	bConsumableUsePending = false;
	PendingConsumableHealthRestore = 0.0f;
	PendingConsumableStaminaRestore = 0.0f;
	PendingConsumableEffectClass = nullptr;
	ActiveConsumableMontage = nullptr;
	ClearConsumablePotionVisual();
}

void AFrontierPlayerCharacter::ServerConsumeConsumableFromNotify_Implementation()
{
	if (!bConsumableUsePending)
	{
		return;
	}

	const float HealthRestoreAmount = PendingConsumableHealthRestore;
	const float StaminaRestoreAmount = PendingConsumableStaminaRestore;
	const TSubclassOf<UGameplayEffect> ConsumeEffectClass = PendingConsumableEffectClass;
	bConsumableUsePending = false;
	PendingConsumableHealthRestore = 0.0f;
	PendingConsumableStaminaRestore = 0.0f;
	PendingConsumableEffectClass = nullptr;
	ClearConsumablePotionVisual();

	if (UFrontierAbilitySystemComponent* PotionAbilitySystemComponent = GetFrontierAbilitySystemComponent())
	{
		if (const UGameplayEffect* EffectCDO = ConsumeEffectClass
			? ConsumeEffectClass->GetDefaultObject<UGameplayEffect>()
			: nullptr)
		{
			PotionAbilitySystemComponent->ApplyGameplayEffectToSelf(
				EffectCDO,
				1.0f,
				PotionAbilitySystemComponent->MakeEffectContext());
		}
		else
		{
			ApplyPotionHealthRestore(PotionAbilitySystemComponent, HealthRestoreAmount);
			ApplyPotionStaminaRestore(PotionAbilitySystemComponent, StaminaRestoreAmount);
		}
	}
	MulticastFinishConsumableUse();
}

void AFrontierPlayerCharacter::ServerCancelConsumableUse_Implementation()
{
	if (!bConsumableUsePending)
	{
		return;
	}

	bConsumableUsePending = false;
	PendingConsumableHealthRestore = 0.0f;
	PendingConsumableStaminaRestore = 0.0f;
	PendingConsumableEffectClass = nullptr;
	ClearConsumablePotionVisual();
	MulticastCancelConsumableUse();
}

void AFrontierPlayerCharacter::ResetSkillMontageAnimationMode()
{
	SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
}

void AFrontierPlayerCharacter::ServerSetSprinting_Implementation(const bool bNewSprinting)
{
	if (bIsRolling)
	{
		SetSprintingInternal(false);
		return;
	}

	if (bNewSprinting)
	{
		if (!CanStartSprinting())
		{
			SetSprintingInternal(false);
			return;
		}

		SetSprintingInternal(true);
		return;
	}

	SetSprintingInternal(false);
}


