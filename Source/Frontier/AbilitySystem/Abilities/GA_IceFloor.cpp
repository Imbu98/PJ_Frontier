#include "AbilitySystem/Abilities/GA_IceFloor.h"

#include "AbilitySystem/Abilities/FrontierIceFloorActor.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Skill/FrontierSkillDataSubsystem.h"
#include "Tags/FrontierGameplayTags.h"

UGA_IceFloor::UGA_IceFloor()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
	IceFloorActorClass = AFrontierIceFloorActor::StaticClass();

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	ActivationOwnedTags.AddTag(Tags.StateCombatSkill);
	ActivationBlockedTags.AddTag(Tags.StateDead);
	ActivationBlockedTags.AddTag(Tags.StateCCStun);
	ActivationBlockedTags.AddTag(Tags.StateActionAttacking);
	ActivationBlockedTags.AddTag(Tags.StateCombatSkill);
}

bool UGA_IceFloor::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AFrontierPlayerCharacter* Player = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const UFrontierEquipmentComponent* Equipment = Player ? Player->GetEquipmentComponent() : nullptr;
	const UFrontierWeaponDataAsset* WeaponData = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
	return Player
		&& Player->HasAuthority()
		&& !Player->IsDead()
		&& CastMontage
		&& IceFloorActorClass
		&& WeaponData
		&& WeaponData->WeaponTypeTag.MatchesTag(FFrontierGameplayTags::Get().WeaponTypeAxe);
}

void UGA_IceFloor::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
	CachedPlayerCharacter = ActorInfo ? Cast<AFrontierPlayerCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!CachedPlayerCharacter || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const int32 SkillLevel = FMath::Max(1, GetAbilityLevel(Handle, ActorInfo));
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	TMap<FGameplayTag, float> Parameters;
	Parameters.Add(Tags.SkillParameterRadius, BaseRadius);
	Parameters.Add(Tags.SkillParameterDuration, BaseDuration);
	Parameters.Add(Tags.SkillParameterDamageMultiplier, BaseDamageMultiplier);
	Parameters.Add(Tags.SkillParameterDamageInterval, DamageInterval);
	Parameters.Add(Tags.SkillParameterSlowMultiplier, SlowMultiplier);
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UFrontierSkillDataSubsystem* SkillData = GameInstance->GetSubsystem<UFrontierSkillDataSubsystem>())
		{
			SkillData->ApplySkillBalance(GetClass(), SkillLevel, Parameters);
		}
	}

	ResolvedRadius = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterRadius));
	ResolvedDuration = FMath::Max(0.1f, Parameters.FindRef(Tags.SkillParameterDuration));
	ResolvedDamageMultiplier = FMath::Max(0.0f, Parameters.FindRef(Tags.SkillParameterDamageMultiplier));
	ResolvedDamageInterval = FMath::Max(0.05f, Parameters.FindRef(Tags.SkillParameterDamageInterval));
	ResolvedSlowMultiplier = FMath::Clamp(Parameters.FindRef(Tags.SkillParameterSlowMultiplier), 0.0f, 1.0f);
	bIceFloorSpawned = false;

	CachedPlayerCharacter->SetMontageHitReactionResistance(CastMontage, HitReactionResistance);
	CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(AttackAnimationMode);
	CachedPlayerCharacter->MulticastPlaySkillMontage(CastMontage, AttackAnimationMode);
	if (UAnimInstance* AnimInstance = CachedPlayerCharacter->GetMesh() ? CachedPlayerCharacter->GetMesh()->GetAnimInstance() : nullptr)
	{
		FOnMontageEnded EndDelegate;
		EndDelegate.BindUObject(this, &UGA_IceFloor::HandleCastMontageEnded);
		AnimInstance->Montage_SetEndDelegate(EndDelegate, CastMontage);
	}
	else
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

void UGA_IceFloor::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (CachedPlayerCharacter)
	{
		CachedPlayerCharacter->ClearMontageHitReactionResistance(CastMontage);
		CachedPlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}
	CachedPlayerCharacter = nullptr;
	ResolvedRadius = 0.0f;
	ResolvedDuration = 0.0f;
	ResolvedDamageMultiplier = 0.0f;
	ResolvedDamageInterval = 0.0f;
	ResolvedSlowMultiplier = 1.0f;
	bIceFloorSpawned = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UGA_IceFloor::HandleCastMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (!IsActive() || Montage != CastMontage || !CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}
	if (bInterrupted)
	{
		K2_CancelAbility();
		return;
	}
	K2_EndAbility();
}

void UGA_IceFloor::SpawnIceFloorFromNotify()
{
	if (!IsActive() || bIceFloorSpawned || !CachedPlayerCharacter || !CachedPlayerCharacter->HasAuthority())
	{
		return;
	}

	bIceFloorSpawned = true;
	SpawnIceFloor();
}

void UGA_IceFloor::SpawnIceFloor()
{
	if (!CachedPlayerCharacter || !IceFloorActorClass || !GetWorld())
	{
		return;
	}
	if (ActiveIceFloor.IsValid())
	{
		ActiveIceFloor->Destroy();
	}

	const FTransform SpawnTransform(FRotator::ZeroRotator, ResolveFloorLocation());
	AFrontierIceFloorActor* Floor = GetWorld()->SpawnActorDeferred<AFrontierIceFloorActor>(
		IceFloorActorClass,
		SpawnTransform,
		CachedPlayerCharacter,
		CachedPlayerCharacter,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Floor)
	{
		return;
	}

	FFrontierDamageRequest Request;
	Request.DamageTypeTag = FFrontierGameplayTags::Get().DamageTypeIce;
	Request.ElementalType = EFrontierElementalType::Ice;
	Request.DamageEffectClass = DamageEffectClass;
	Request.HitReactionLevel = EFrontierHitReactionLevel::None;
	Floor->InitializeIceFloor(
		CachedPlayerCharacter,
		ResolvedRadius,
		ResolvedDuration,
		ResolvedDamageInterval,
		ResolvedDamageMultiplier,
		ResolvedSlowMultiplier,
		Request);
	UGameplayStatics::FinishSpawningActor(Floor, SpawnTransform);
	ActiveIceFloor = Floor;
}

FVector UGA_IceFloor::ResolveFloorLocation() const
{
	if (!CachedPlayerCharacter || !GetWorld())
	{
		return FVector::ZeroVector;
	}

	const FVector Start = CachedPlayerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 100.0f);
	const FVector End = Start - FVector(0.0f, 0.0f, 500.0f);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(IceFloorGroundTrace), false, CachedPlayerCharacter);
	return GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params)
		? Hit.ImpactPoint
		: CachedPlayerCharacter->GetActorLocation();
}
