#include "Character/FrontierBaseCharacter.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "AbilitySystemInterface.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"
#include "Components/FrontierCombatComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Components/FrontierLootComponent.h"
#include "Components/FrontierStorageComponent.h"
#include "Components/FrontierRaidInventoryComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/CapsuleComponent.h"
#include "Frontier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h"
#include "Game/FrontierPlayerState.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Loot/FrontierLootContainerActor.h"
#include "Loot/FrontierDeathLootContainerActor.h"
#include "Net/UnrealNetwork.h"
#include "Tags/FrontierGameplayTags.h"
#include "TimerManager.h"
#include "Weapons/FrontierWeaponDataAsset.h"

namespace
{
void CancelActiveAttackAbilities(AFrontierBaseCharacter* Character)
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
}

AFrontierBaseCharacter::AFrontierBaseCharacter()
{
	FRONTIER_LOG_FUNC();

	bReplicates = true;

	CombatComponent = CreateDefaultSubobject<UFrontierCombatComponent>(TEXT("CombatComponent"));
	LootComponent = CreateDefaultSubobject<UFrontierLootComponent>(TEXT("LootComponent"));
	DeathLootContainerClass = AFrontierDeathLootContainerActor::StaticClass();

	FFrontierGameplayTags::InitializeNativeGameplayTags();
}

UAbilitySystemComponent* AFrontierBaseCharacter::GetAbilitySystemComponent() const
{
	return GetFrontierAbilitySystemComponent();
}

void AFrontierBaseCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierBaseCharacter, bIsDead);
	DOREPLIFETIME(AFrontierBaseCharacter, bIsAiming);
	DOREPLIFETIME(AFrontierBaseCharacter, StatusMovementSpeedMultiplier);
}

void AFrontierBaseCharacter::PossessedBy(AController* NewController)
{
	FRONTIER_LOG_FUNC();
	Super::PossessedBy(NewController);
	InitializeAbilityActorInfo();
}

void AFrontierBaseCharacter::OnRep_Controller()
{
	FRONTIER_LOG_FUNC();
	Super::OnRep_Controller();
	InitializeAbilityActorInfo();
}
UFrontierAbilitySystemComponent* AFrontierBaseCharacter::GetFrontierAbilitySystemComponent() const
{
	if (AbilitySystemComponent)
	{
		return AbilitySystemComponent;
	}

	const APlayerState* CurrentPlayerState = GetPlayerState();
	const IAbilitySystemInterface* AbilitySystemInterface = Cast<IAbilitySystemInterface>(CurrentPlayerState);
	return AbilitySystemInterface
		? Cast<UFrontierAbilitySystemComponent>(AbilitySystemInterface->GetAbilitySystemComponent())
		: nullptr;
}

const UFrontierAttributeSet* AFrontierBaseCharacter::GetFrontierAttributeSet() const
{
	if (AttributeSet)
	{
		return AttributeSet;
	}

	const UFrontierAbilitySystemComponent* ResolvedAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	return ResolvedAbilitySystemComponent
		? ResolvedAbilitySystemComponent->GetSet<UFrontierAttributeSet>()
		: nullptr;
}

UFrontierAttributeSet* AFrontierBaseCharacter::GetMutableFrontierAttributeSet() const
{
	return const_cast<UFrontierAttributeSet*>(GetFrontierAttributeSet());
}

UFrontierCombatComponent* AFrontierBaseCharacter::GetCombatComponent() const
{
	return CombatComponent;
}

UFrontierStorageComponent* AFrontierBaseCharacter::GetStorageComponent() const
{
	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	return FrontierPlayerState ? FrontierPlayerState->GetStorageComponent() : nullptr;
}

UFrontierRaidInventoryComponent* AFrontierBaseCharacter::GetRaidInventoryComponent() const
{
	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	return FrontierPlayerState ? FrontierPlayerState->GetRaidInventoryComponent() : nullptr;
}

UFrontierLoadoutComponent* AFrontierBaseCharacter::GetLoadoutComponent() const
{
	const AFrontierPlayerState* FrontierPlayerState = GetPlayerState<AFrontierPlayerState>();
	return FrontierPlayerState ? FrontierPlayerState->GetLoadoutComponent() : nullptr;
}

EFrontierTeam AFrontierBaseCharacter::GetTeam() const
{
	return Team;
}

void AFrontierBaseCharacter::SetTeam(const EFrontierTeam NewTeam)
{
	FRONTIER_LOG(Log, TEXT("Character team changed from %d to %d"), static_cast<uint8>(Team), static_cast<uint8>(NewTeam));
	Team = NewTeam;
}

EFrontierTeam AFrontierBaseCharacter::ResolvePlayerTeam(const int32 TeamId)
{
	switch (TeamId)
	{
	case static_cast<int32>(EFrontierTeam::TeamA):
		return EFrontierTeam::TeamA;
	case static_cast<int32>(EFrontierTeam::TeamB):
		return EFrontierTeam::TeamB;
	case static_cast<int32>(EFrontierTeam::TeamC):
		return EFrontierTeam::TeamC;
	default:
		return EFrontierTeam::None;
	}
}

bool AFrontierBaseCharacter::IsDead() const
{
	return bIsDead;
}

void AFrontierBaseCharacter::RevealCombatOverheadLocally(const float HealthPercent)
{
	// Character subclasses opt into a local-only overhead combat presentation.
}

float AFrontierBaseCharacter::GetEffectiveMovementSpeedMultiplier() const
{
	const UFrontierAttributeSet* Attributes = GetFrontierAttributeSet();
	const float AttributeMultiplier = FMath::Max(0.0f, 1.0f + (Attributes ? Attributes->GetMoveSpeedBonus() : 0.0f));
	return AttributeMultiplier * FMath::Clamp(StatusMovementSpeedMultiplier, 0.0f, 1.0f);
}

void AFrontierBaseCharacter::SetMovementSpeedModifier(const FName SourceId, const float Multiplier)
{
	if (!HasAuthority() || SourceId.IsNone())
	{
		return;
	}

	MovementSpeedModifiers.FindOrAdd(SourceId) = FMath::Clamp(Multiplier, 0.0f, 1.0f);
	float StrongestSlow = 1.0f;
	for (const TPair<FName, float>& Entry : MovementSpeedModifiers)
	{
		StrongestSlow = FMath::Min(StrongestSlow, Entry.Value);
	}
	StatusMovementSpeedMultiplier = StrongestSlow;
	HandleMovementSpeedModifiersChanged();
}

void AFrontierBaseCharacter::RemoveMovementSpeedModifier(const FName SourceId)
{
	if (!HasAuthority() || SourceId.IsNone() || MovementSpeedModifiers.Remove(SourceId) == 0)
	{
		return;
	}

	float StrongestSlow = 1.0f;
	for (const TPair<FName, float>& Entry : MovementSpeedModifiers)
	{
		StrongestSlow = FMath::Min(StrongestSlow, Entry.Value);
	}
	StatusMovementSpeedMultiplier = StrongestSlow;
	HandleMovementSpeedModifiersChanged();
}

void AFrontierBaseCharacter::SetDesiredMaxWalkSpeed(const float NewBaseSpeed)
{
	DesiredBaseMaxWalkSpeed = FMath::Max(0.0f, NewBaseSpeed);
	HandleMovementSpeedModifiersChanged();
}

void AFrontierBaseCharacter::HandleMovementSpeedModifiersChanged()
{
	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->MaxWalkSpeed = DesiredBaseMaxWalkSpeed * GetEffectiveMovementSpeedMultiplier();
	}
}

void AFrontierBaseCharacter::OnRep_StatusMovementSpeedMultiplier()
{
	HandleMovementSpeedModifiersChanged();
}

void AFrontierBaseCharacter::SetAiming(const bool bNewIsAiming)
{
	if (bIsAiming == bNewIsAiming)
	{
		return;
	}

	bIsAiming = bNewIsAiming;
	HandleAimingStateChanged();
}

void AFrontierBaseCharacter::Die()
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	const bool bWasDead = bIsDead;
	bIsDead = true;
	HandleDeathStateChanged(bWasDead);
}

void AFrontierBaseCharacter::HandleDamageReceived(const float DamageAmount, const FGameplayTag DamageTypeTag)

{
	HandleDamageReceivedWithReaction(
		DamageAmount,
		DamageTypeTag,
		FGameplayTag(),
		EFrontierHitReactionLevel::None,
		0.35f,
		650.0f,
		150.0f,
		GetActorLocation());
}

void AFrontierBaseCharacter::HandleDamageReceivedWithReaction(
	const float DamageAmount,
	const FGameplayTag DamageTypeTag,
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector DamageOrigin)
{
	if (!HasAuthority() || bIsDead || DamageAmount <= 0.0f)
	{
		return;
	}

	if (IsDamageReactionBlocked())
	{
		return;
	}

	RecordLastDamageReaction(
		HitReactionTag,
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength,
		DamageOrigin);
	LastDamageReceivedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : LastDamageReceivedTime;
	if (HitReactionLevel == EFrontierHitReactionLevel::None)
	{
		return;
	}

	const EFrontierHitReactionLevel ResistanceLevel = GetCurrentHitReactionResistance();
	if (static_cast<uint8>(ResistanceLevel) >= static_cast<uint8>(HitReactionLevel))
	{
		return;
	}

	const bool bInterruptAttack = static_cast<uint8>(HitReactionLevel)
		>= static_cast<uint8>(EFrontierHitReactionLevel::Stagger);
	if (bInterruptAttack)
	{
		CancelActiveAttackAbilities(this);
		if (AFrontierPlayerCharacter* FrontierPlayerCharacter = Cast<AFrontierPlayerCharacter>(this))
		{
			FrontierPlayerCharacter->CancelRollImmediately();
			FrontierPlayerCharacter->ApplyTemporaryDamageMoveSlow();
		}
	}

	if (UAnimMontage* HitReactMontage = SelectHitReactionMontage(HitReactionTag, DamageTypeTag))
	{
		MulticastPlayHitReactMontage(HitReactMontage);
	}

	ApplyHitReactionMovement(
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength,
		DamageOrigin);
}

EFrontierHitReactionLevel AFrontierBaseCharacter::GetCurrentHitReactionResistance() const
{
	const UAnimInstance* AnimInstance = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr;
	if (AnimInstance && ResistanceMontage && AnimInstance->Montage_IsPlaying(ResistanceMontage))
	{
		return MontageHitReactionResistance;
	}

	return EFrontierHitReactionLevel::None;
}

bool AFrontierBaseCharacter::IsDamageReactionBlocked() const
{
	return false;
}

void AFrontierBaseCharacter::SetMontageHitReactionResistance(
	UAnimMontage* Montage,
	const EFrontierHitReactionLevel ResistanceLevel)
{
	ResistanceMontage = Montage;
	MontageHitReactionResistance = Montage ? ResistanceLevel : EFrontierHitReactionLevel::None;
}

void AFrontierBaseCharacter::ClearMontageHitReactionResistance(UAnimMontage* ExpectedMontage)
{
	if (ExpectedMontage && ResistanceMontage != ExpectedMontage)
	{
		return;
	}

	ResistanceMontage = nullptr;
	MontageHitReactionResistance = EFrontierHitReactionLevel::None;
}

void AFrontierBaseCharacter::PlayCosmeticHitReaction(const FGameplayTag DamageTypeTag)
{
	if (!HasAuthority() || bIsDead || IsDamageReactionBlocked())
	{
		return;
	}

	if (UAnimMontage* HitReactMontage = SelectHitReactMontage(DamageTypeTag))
	{
		MulticastPlayHitReactMontage(HitReactMontage);
	}
}

float AFrontierBaseCharacter::GetLastDamageReceivedTime() const
{
	return LastDamageReceivedTime;
}

EFrontierElementalType AFrontierBaseCharacter::GetElementalType() const
{
	return ElementalType;
}

void AFrontierBaseCharacter::BeginPlay()
{
	Super::BeginPlay();
	DesiredBaseMaxWalkSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 0.0f;
	InitializeAbilityActorInfo();
}

void AFrontierBaseCharacter::InitializeAbilityActorInfo()
{
	FRONTIER_LOG_FUNC();

	UFrontierAbilitySystemComponent* ResolvedAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!ResolvedAbilitySystemComponent)
	{
		FRONTIER_LOG(Verbose, TEXT("Ability actor info initialization deferred because ASC is unavailable. Character=%s"), *GetNameSafe(this));
		return;
	}

	AActor* OwnerActor = this;
	if (!AbilitySystemComponent)
	{
		OwnerActor = GetPlayerState();
	}

	if (!OwnerActor)
	{
		FRONTIER_LOG(Verbose, TEXT("Ability actor info initialization deferred because owner actor is unavailable. Character=%s"), *GetNameSafe(this));
		return;
	}

	FRONTIER_LOG(Log, TEXT("Initializing ability actor info. Owner=%s Avatar=%s"), *GetNameSafe(OwnerActor), *GetNameSafe(this));
	ResolvedAbilitySystemComponent->InitializeAbilityActorInfo(OwnerActor, this);
	ApplyDefaultHealthAttributes();

	const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();
	ResolvedAbilitySystemComponent->SetLooseGameplayTagCount(FrontierTags.StateAlive, bIsDead ? 0 : 1);
	ResolvedAbilitySystemComponent->SetLooseGameplayTagCount(FrontierTags.StateDead, bIsDead ? 1 : 0);
}

void AFrontierBaseCharacter::OnRep_IsDead()
{
	HandleDeathStateChanged(!bIsDead);
}

void AFrontierBaseCharacter::OnRep_IsAiming()
{
	HandleAimingStateChanged();
}

void AFrontierBaseCharacter::HandleAimingStateChanged()
{
}

void AFrontierBaseCharacter::MulticastPlayHitReactMontage_Implementation(UAnimMontage* HitReactMontage)
{
	if (!HitReactMontage || bIsDead)
	{
		return;
	}

	PlayAnimMontage(HitReactMontage);
}

void AFrontierBaseCharacter::MulticastApplyDeathRagdollImpulse_Implementation(const FVector Impulse)
{
	EnableDeathRagdoll();
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->AddImpulseToAllBodiesBelow(Impulse, NAME_None, true, true);
	}
}

UAnimMontage* AFrontierBaseCharacter::SelectHitReactMontage(const FGameplayTag DamageTypeTag) const
{
	if (const UFrontierEquipmentComponent* EquipmentComponent = FindComponentByClass<UFrontierEquipmentComponent>())
	{
		if (const UFrontierWeaponDataAsset* CurrentWeaponData = EquipmentComponent->GetCurrentWeaponData())
		{
			for (const FFrontierWeaponHitReactMontageEntry& Entry : CurrentWeaponData->HitReactMontagesByDamageType)
			{
				if (Entry.HitReactMontage.IsNull() || !DamageTypeTag.IsValid())
				{
					continue;
				}

				if (Entry.DamageTypeTag.MatchesTagExact(DamageTypeTag))
				{
					return Entry.HitReactMontage.LoadSynchronous();
				}
			}

			for (const FFrontierWeaponHitReactMontageEntry& Entry : CurrentWeaponData->HitReactMontagesByDamageType)
			{
				if (Entry.HitReactMontage.IsNull() || !DamageTypeTag.IsValid())
				{
					continue;
				}

				if (DamageTypeTag.MatchesTag(Entry.DamageTypeTag))
				{
					return Entry.HitReactMontage.LoadSynchronous();
				}
			}

			if (!CurrentWeaponData->DefaultHitReactMontage.IsNull())
			{
				return CurrentWeaponData->DefaultHitReactMontage.LoadSynchronous();
			}
		}
	}

	for (const FFrontierHitReactMontageEntry& Entry : HitReactMontagesByDamageType)
	{
		if (Entry.Montage && DamageTypeTag.IsValid() && Entry.DamageTypeTag.MatchesTagExact(DamageTypeTag))
		{
			return Entry.Montage;
		}
	}

	return DefaultHitReactMontage;
}

const FFrontierHitReactMontageEntry* AFrontierBaseCharacter::FindHitReactMontageEntry(
	const FGameplayTag HitReactionTag) const
{
	if (!HitReactionTag.IsValid())
	{
		return nullptr;
	}

	for (const FFrontierHitReactMontageEntry& Definition : HitReactMontagesByDamageType)
	{
		if (Definition.HitReactionTag.MatchesTagExact(HitReactionTag))
		{
			return &Definition;
		}
	}

	for (const FFrontierHitReactMontageEntry& Definition : HitReactMontagesByDamageType)
	{
		if (Definition.HitReactionTag.IsValid() && HitReactionTag.MatchesTag(Definition.HitReactionTag))
		{
			return &Definition;
		}
	}

	return nullptr;
}

UAnimMontage* AFrontierBaseCharacter::SelectHitReactionMontage(
	const FGameplayTag HitReactionTag,
	const FGameplayTag DamageTypeTag) const
{
	if (const FFrontierHitReactMontageEntry* Definition = FindHitReactMontageEntry(HitReactionTag))
	{
		if (Definition->Montage)
		{
			return Definition->Montage;
		}
	}

	return SelectHitReactMontage(DamageTypeTag);
}

void AFrontierBaseCharacter::ApplyHitReactionMovement(
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector& DamageOrigin)
{
	if (!HasAuthority() || bIsDead)
	{
		return;
	}

	if (HitReactionLevel == EFrontierHitReactionLevel::Stagger)
	{
		if (UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent())
		{
			ASC->SetLooseGameplayTagCount(FFrontierGameplayTags::Get().StateCCStun, 1);
		}

		if (StaggerDuration > 0.0f)
		{
			GetWorldTimerManager().SetTimer(
				HitReactionStunTimerHandle,
				this,
				&AFrontierBaseCharacter::ClearHitReactionStun,
				StaggerDuration,
				false);
		}
		else
		{
			ClearHitReactionStun();
		}
	}
	else if (HitReactionLevel == EFrontierHitReactionLevel::KnockBack)
	{
		FVector AwayDirection = GetActorLocation() - DamageOrigin;
		AwayDirection.Z = 0.0f;
		if (!AwayDirection.Normalize())
		{
			AwayDirection = -GetActorForwardVector().GetSafeNormal2D();
		}

		const FVector LaunchVelocity =
			(AwayDirection * KnockbackHorizontalStrength)
			+ (FVector::UpVector * KnockbackVerticalStrength);
		LaunchCharacter(LaunchVelocity, true, true);
	}
}

void AFrontierBaseCharacter::RecordLastDamageReaction(
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector& DamageOrigin)
{
	if (!HasAuthority() || IsDamageReactionBlocked())
	{
		return;
	}

	LastDamageHitReactionTag = HitReactionTag;
	LastDamageHitReactionLevel = HitReactionLevel;
	LastDamageStaggerDuration = StaggerDuration;
	LastDamageKnockbackHorizontalStrength = KnockbackHorizontalStrength;
	LastDamageKnockbackVerticalStrength = KnockbackVerticalStrength;
	LastDamageOrigin = DamageOrigin;
	bHasLastDamageReaction = true;
}

void AFrontierBaseCharacter::RecordLastDamageImpactPoint(
	const FVector& DamageImpactPoint,
	const bool bIsValidImpactPoint)
{
	if (!HasAuthority())
	{
		return;
	}

	LastDamageImpactPoint = bIsValidImpactPoint ? DamageImpactPoint : FVector::ZeroVector;
	bHasLastDamageImpactPoint = bIsValidImpactPoint;
}

void AFrontierBaseCharacter::RecordLastDamageSource(const AActor* DamageSource)
{
	if (!HasAuthority())
	{
		return;
	}

	bLastDamageWasFromPlayer = IsValid(DamageSource)
		&& DamageSource->IsA<AFrontierPlayerCharacter>();
}

void AFrontierBaseCharacter::ClearHitReactionStun()
{
	if (UFrontierAbilitySystemComponent* ASC = GetFrontierAbilitySystemComponent())
	{
		ASC->SetLooseGameplayTagCount(FFrontierGameplayTags::Get().StateCCStun, 0);
	}
}

void AFrontierBaseCharacter::EnableDeathRagdoll()
{
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		CharacterMesh->SetSimulatePhysics(true);
		CharacterMesh->WakeAllRigidBodies();
		CharacterMesh->bBlendPhysics = true;
		CharacterMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		CharacterMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (UCapsuleComponent* CharacterCapsuleComponent = GetCapsuleComponent())
	{
		CharacterCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

FVector AFrontierBaseCharacter::CalculateDeathRagdollImpulse() const
{
	if (IsDamageReactionBlocked() || !bHasLastDamageReaction)
	{
		return FVector::ZeroVector;
	}

	FVector AwayDirection = GetActorLocation() - LastDamageOrigin;
	AwayDirection.Z = 0.0f;
	if (!AwayDirection.Normalize())
	{
		AwayDirection = -GetActorForwardVector().GetSafeNormal2D();
	}

	float ReactionScale = 0.35f;
	switch (LastDamageHitReactionLevel)
	{
	case EFrontierHitReactionLevel::Light:
		ReactionScale = 0.5f;
		break;
	case EFrontierHitReactionLevel::Stagger:
		ReactionScale = 0.75f;
		break;
	case EFrontierHitReactionLevel::KnockBack:
		ReactionScale = 1.0f;
		break;
	default:
		break;
	}

	const float SafeWeight = FMath::Max(RagdollImpactWeight, 0.1f);
	return ((AwayDirection * LastDamageKnockbackHorizontalStrength)
		+ (FVector::UpVector * LastDamageKnockbackVerticalStrength))
		* (ReactionScale / SafeWeight);
}

bool AFrontierBaseCharacter::UsesAlternativeDeathPresentation() const
{
	return false;
}

void AFrontierBaseCharacter::ActivateAlternativeDeathPresentation()
{
}

void AFrontierBaseCharacter::HideCharacterForDeathReplacement()
{
	if (USkeletalMeshComponent* CharacterMesh = GetMesh())
	{
		CharacterMesh->SetSimulatePhysics(false);
		CharacterMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		CharacterMesh->SetVisibility(false, true);
	}

	if (UCapsuleComponent* CharacterCapsuleComponent = GetCapsuleComponent())
	{
		CharacterCapsuleComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
}

void AFrontierBaseCharacter::HandleDeathStateChanged(const bool bWasDead)
{
	if (bWasDead == bIsDead)
	{
		return;
	}

	FRONTIER_LOG(Log, TEXT("Character death state changed. Character=%s bIsDead=%d"), *GetNameSafe(this), bIsDead ? 1 : 0);

	if (UFrontierAbilitySystemComponent* ResolvedAbilitySystemComponent = GetFrontierAbilitySystemComponent())
	{
		const FFrontierGameplayTags& FrontierTags = FFrontierGameplayTags::Get();
		ResolvedAbilitySystemComponent->SetLooseGameplayTagCount(FrontierTags.StateAlive, bIsDead ? 0 : 1);
		ResolvedAbilitySystemComponent->SetLooseGameplayTagCount(FrontierTags.StateDead, bIsDead ? 1 : 0);
	}

	if (bIsDead)
	{
		GetWorldTimerManager().ClearTimer(HitReactionStunTimerHandle);
		ClearHitReactionStun();
		SetAiming(false);

		if (HasAuthority())
		{
			SpawnDeathLootContainer();
		}

		if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
		{
			MovementComponent->DisableMovement();
		}

		if (UsesAlternativeDeathPresentation())
		{
			HideCharacterForDeathReplacement();
			if (HasAuthority())
			{
				ActivateAlternativeDeathPresentation();
			}
		}
		else
		{
			EnableDeathRagdoll();
			if (HasAuthority())
			{
				MulticastApplyDeathRagdollImpulse(CalculateDeathRagdollImpulse());
			}
		}

		if (HasAuthority() && DeathDestroyDelay > 0.0f)
		{
			SetLifeSpan(DeathDestroyDelay);
		}
	}
}

AFrontierLootContainerActor* AFrontierBaseCharacter::SpawnDeathLootContainer()
{
	FRONTIER_LOG_FUNC();

	if (!HasAuthority() || !DeathLootContainerClass)
	{
		return nullptr;
	}

	TArray<FFrontierInventorySlot> LootSlots;
	CreateDeathLootSlots(LootSlots);
	TArray<FFrontierLoadoutSlot> LoadoutSlots;
	CreateDeathLoadoutSlots(LoadoutSlots);
	if (LootSlots.IsEmpty() && LoadoutSlots.IsEmpty())
	{
		FRONTIER_LOG(Log, TEXT("Death loot container skipped because no loot data was generated. Character=%s"), *GetNameSafe(this));
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FTransform SpawnTransform(GetActorRotation(), GetActorLocation());
	AFrontierLootContainerActor* LootContainer = World->SpawnActorDeferred<AFrontierLootContainerActor>(
		DeathLootContainerClass,
		SpawnTransform,
		nullptr,
		this,
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!LootContainer)
	{
		return nullptr;
	}

	LootContainer->InitializeFromLootSlots(LootSlots, GetDeathLootContainerSourceType());
	if (GetDeathLootContainerSourceType() == EFrontierLootContainerSourceType::PlayerDeath)
	{
		LootContainer->InitializeDeadPlayerLootData(LootSlots, LoadoutSlots);
	}
	UGameplayStatics::FinishSpawningActor(LootContainer, SpawnTransform);
	return LootContainer;
}

void AFrontierBaseCharacter::CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const
{
	OutLootSlots.Reset();
}

void AFrontierBaseCharacter::CreateDeathLoadoutSlots(TArray<FFrontierLoadoutSlot>& OutLoadoutSlots) const
{
	OutLoadoutSlots.Reset();
}

void AFrontierBaseCharacter::ApplyDefaultHealthAttributes()
{
	if (!HasAuthority() || bDefaultHealthAttributesApplied || bIsDead)
	{
		return;
	}

	UFrontierAbilitySystemComponent* ResolvedAbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!ResolvedAbilitySystemComponent || !GetFrontierAttributeSet())
	{
		return;
	}

	const float ResolvedMaxHealth = FMath::Max(DefaultMaxHealth, 1.0f);
	const float ResolvedHealth = FMath::Clamp(DefaultHealth, 0.0f, ResolvedMaxHealth);

	// Set the base values so later GameplayEffect aggregation (equipment/passives)
	// does not rebuild these attributes from the AttributeSet constructor defaults.
	ResolvedAbilitySystemComponent->SetNumericAttributeBase(
		UFrontierAttributeSet::GetMaxHealthAttribute(),
		ResolvedMaxHealth);
	ResolvedAbilitySystemComponent->SetNumericAttributeBase(
		UFrontierAttributeSet::GetHealthAttribute(),
		ResolvedHealth);
	bDefaultHealthAttributesApplied = true;

	FRONTIER_LOG(Log, TEXT("Applied default health attributes. Character=%s Health=%.2f MaxHealth=%.2f"),
		*GetNameSafe(this),
		ResolvedHealth,
		ResolvedMaxHealth);
}

EFrontierLootContainerSourceType AFrontierBaseCharacter::GetDeathLootContainerSourceType() const
{
	return EFrontierLootContainerSourceType::WorldLoot;
}
