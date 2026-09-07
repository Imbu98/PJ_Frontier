#include "Components/FrontierCombatComponent.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "Character/FrontierBaseCharacter.h"
#include "Combat/FrontierDamageStatics.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierCombatComponent::UFrontierCombatComponent()
{
	SetIsReplicatedByDefault(true);
	PrimaryComponentTick.bCanEverTick = false;
}

void UFrontierCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UFrontierCombatComponent, AttackCounter);
}

void UFrontierCombatComponent::RequestPrimaryAttack()
{
	if (!CanRequestPrimaryAttack())
	{
		return;
	}

	OnAttackPredicted.Broadcast(AttackCounter + 1);

	if (GetOwnerRole() < ROLE_Authority)
	{
		Server_RequestPrimaryAttack();
		return;
	}

	HandlePrimaryAttackConfirmed(true);
}

void UFrontierCombatComponent::RequestPrimaryAttackWithNotify()
{
	

	if (!CanRequestPrimaryAttack())
	{
		return;
	}

	OnAttackPredicted.Broadcast(AttackCounter + 1);

	if (GetOwnerRole() < ROLE_Authority)
	{
		Server_RequestPrimaryAttackWithNotify();
		return;
	}

	HandlePrimaryAttackConfirmed(false);
}

bool UFrontierCombatComponent::CanRequestPrimaryAttack() const
{
	const UFrontierAbilitySystemComponent* AbilitySystemComponent = GetFrontierAbilitySystemComponent();
	if (!AbilitySystemComponent)
	{
		return false;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	return !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateDead)
		&& !AbilitySystemComponent->HasMatchingGameplayTag(Tags.StateCCStun);
}

FFrontierDamageResult UFrontierCombatComponent::ApplyDamageToTarget(AActor* TargetActor, const float BaseDamage, const FGameplayTag DamageTypeTag, const float DamageMultiplier, const EFrontierElementalType ElementalType, const FGameplayTag HitReactionTag, const EFrontierHitReactionLevel HitReactionLevel, const float StaggerDuration, const float KnockbackHorizontalStrength, const float KnockbackVerticalStrength, const FVector HitLocation)
{
	return ApplyDamageToTarget(TargetActor, BaseDamage, DamageTypeTag, nullptr, nullptr, DamageMultiplier, ElementalType, HitReactionTag, HitReactionLevel, StaggerDuration, KnockbackHorizontalStrength, KnockbackVerticalStrength, HitLocation);
}

FFrontierDamageResult UFrontierCombatComponent::ApplyDamageToTarget(
	AActor* TargetActor,
	const float BaseDamage,
	const FGameplayTag DamageTypeTag,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	const float DamageMultiplier,
	const EFrontierElementalType ElementalType,
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector HitLocation)
{
	return ApplyDamageToTarget(TargetActor, BaseDamage, DamageTypeTag, DamageEffectClass, nullptr, DamageMultiplier, ElementalType, HitReactionTag, HitReactionLevel, StaggerDuration, KnockbackHorizontalStrength, KnockbackVerticalStrength, HitLocation);
}

FFrontierDamageResult UFrontierCombatComponent::ApplyDamageToTarget(
	AActor* TargetActor,
	const float BaseDamage,
	const FGameplayTag DamageTypeTag,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses,
	const float DamageMultiplier,
	const EFrontierElementalType ElementalType,
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector HitLocation)
{
	

	FFrontierDamageRequest DamageRequest;
	DamageRequest.BaseDamage = BaseDamage;
	DamageRequest.DamageMultiplier = DamageMultiplier;
	DamageRequest.DamageTypeTag = DamageTypeTag;
	DamageRequest.HitReactionTag = HitReactionTag;
	DamageRequest.HitReactionLevel = HitReactionLevel;
	DamageRequest.StaggerDuration = StaggerDuration;
	DamageRequest.KnockbackHorizontalStrength = KnockbackHorizontalStrength;
	DamageRequest.KnockbackVerticalStrength = KnockbackVerticalStrength;
	DamageRequest.HitLocation = HitLocation;
	DamageRequest.ElementalType = ElementalType;
	DamageRequest.DamageEffectClass = DamageEffectClass;
	if (AdditionalEffectClasses)
	{
		DamageRequest.AdditionalEffectClasses = *AdditionalEffectClasses;
	}

	return UFrontierDamageStatics::ApplyDamage(GetOwner(), TargetActor, DamageRequest);
}

TArray<FFrontierDamageResult> UFrontierCombatComponent::ApplySphereTraceDamageFromSockets(
	USceneComponent* TraceComponent,
	const FName StartSocketName,
	const FName EndSocketName,
	const float TraceRadius,
	const float BaseDamage,
	const FGameplayTag DamageTypeTag,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses,
	const float DamageMultiplier,
	const bool bDrawDebugTrace,
	const EFrontierElementalType ElementalType,
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector HitLocation)
{

	TArray<FFrontierDamageResult> DamageResults;

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return DamageResults;
	}

	if (!TraceComponent)
	{
		FRONTIER_LOG(Warning, TEXT("Sphere trace damage failed because TraceComponent is null."));
		return DamageResults;
	}

	const FVector TraceStart = (!StartSocketName.IsNone() && TraceComponent->DoesSocketExist(StartSocketName))
		? TraceComponent->GetSocketLocation(StartSocketName)
		: TraceComponent->GetComponentLocation();
	const FVector TraceEnd = (!EndSocketName.IsNone() && TraceComponent->DoesSocketExist(EndSocketName))
		? TraceComponent->GetSocketLocation(EndSocketName)
		: TraceComponent->GetComponentLocation();

	return ApplySphereTraceDamageBetweenPoints(
		TraceStart,
		TraceEnd,
		TraceRadius,
		BaseDamage,
		DamageTypeTag,
		DamageEffectClass,
		AdditionalEffectClasses,
		DamageMultiplier,
		bDrawDebugTrace,
		ElementalType,
		HitReactionTag,
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength,
		HitLocation);
}

TArray<FFrontierDamageResult> UFrontierCombatComponent::ApplySphereTraceDamageBetweenPoints(
	const FVector& TraceStart,
	const FVector& TraceEnd,
	const float TraceRadius,
	const float BaseDamage,
	const FGameplayTag DamageTypeTag,
	TSubclassOf<UGameplayEffect> DamageEffectClass,
	const TArray<TSubclassOf<UGameplayEffect>>* AdditionalEffectClasses,
	const float DamageMultiplier,
	const bool bDrawDebugTrace,
	const EFrontierElementalType ElementalType,
	const FGameplayTag HitReactionTag,
	const EFrontierHitReactionLevel HitReactionLevel,
	const float StaggerDuration,
	const float KnockbackHorizontalStrength,
	const float KnockbackVerticalStrength,
	const FVector HitLocation)
{
	TArray<FFrontierDamageResult> DamageResults;

	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("Point-to-point sphere trace damage ignored on non-authority owner."));
		return DamageResults;
	}

	TArray<FHitResult> HitResults;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(GetOwner());
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel2)); // Enemy
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel3)); // Player
	

	const bool bHit = UKismetSystemLibrary::SphereTraceMultiForObjects(
		this,
		TraceStart,
		TraceEnd,
		TraceRadius,
		ObjectTypes,
		false,
		ActorsToIgnore,
		bDrawDebugTrace ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		HitResults,
		true);

	if (!bHit)
	{
		return DamageResults;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* HitActor = HitResult.GetActor();
		if (!IsValid(HitActor) || HitActorsThisAttack.Contains(HitActor))
		{
			continue;
		}

		HitActorsThisAttack.Add(HitActor);
		DamageResults.Add(ApplyDamageToTarget(HitActor, BaseDamage, DamageTypeTag, DamageEffectClass, AdditionalEffectClasses, DamageMultiplier, ElementalType, HitReactionTag, HitReactionLevel, StaggerDuration, KnockbackHorizontalStrength, KnockbackVerticalStrength, HitResult.ImpactPoint));
	}

	return DamageResults;
}

void UFrontierCombatComponent::ResetHitActorsThisAttack()
{
	
	HitActorsThisAttack.Reset();
}

void UFrontierCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFrontierCombatComponent::Server_RequestPrimaryAttack_Implementation()
{
	if (!CanRequestPrimaryAttack())
	{
		return;
	}

	HandlePrimaryAttackConfirmed(true);
}

void UFrontierCombatComponent::Server_RequestPrimaryAttackWithNotify_Implementation()
{
	

	if (!CanRequestPrimaryAttack())
	{
		return;
	}

	HandlePrimaryAttackConfirmed(false);
}

void UFrontierCombatComponent::OnRep_AttackCounter()
{
	OnAttackConfirmed.Broadcast(AttackCounter);
}

void UFrontierCombatComponent::HandlePrimaryAttackConfirmed(const bool bExecuteDefaultAttack)
{
	++AttackCounter;
	OnAttackConfirmed.Broadcast(AttackCounter);

	if (bExecuteDefaultAttack && GetOwner() && GetOwner()->HasAuthority())
	{
		PerformDefaultPrimaryAttack();
	}
}

UFrontierAbilitySystemComponent* UFrontierCombatComponent::GetFrontierAbilitySystemComponent() const
{
	

	const AFrontierBaseCharacter* OwnerCharacter = Cast<AFrontierBaseCharacter>(GetOwner());
	return OwnerCharacter ? OwnerCharacter->GetFrontierAbilitySystemComponent() : nullptr;
}

void UFrontierCombatComponent::PerformDefaultPrimaryAttack()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	if (PrimaryAttackDamage <= 0.0f || PrimaryAttackRange <= 0.0f || PrimaryAttackRadius <= 0.0f)
	{
		FRONTIER_LOG(Warning, TEXT("Default primary attack skipped because attack tuning values are invalid."));
		return;
	}

	ResetHitActorsThisAttack();

	const FVector Forward = OwnerCharacter->GetActorForwardVector().GetSafeNormal();
	const FVector TraceStart = OwnerCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	const FVector TraceEnd = TraceStart + (Forward * PrimaryAttackRange);

	ApplySphereTraceDamageBetweenPoints(
		TraceStart,
		TraceEnd,
		PrimaryAttackRadius,
		PrimaryAttackDamage,
		FFrontierGameplayTags::Get().DamageTypePhysical);
}
