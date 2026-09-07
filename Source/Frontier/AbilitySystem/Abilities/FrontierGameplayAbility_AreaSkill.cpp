#include "AbilitySystem/Abilities/FrontierGameplayAbility_AreaSkill.h"

#include "Components/CapsuleComponent.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Frontier.h"
#include "GameFramework/Character.h"
#include "NavigationSystem.h"

namespace
{
FVector LimitAreaTargetByObstacle(
	const UWorld& World,
	const ACharacter& SourceCharacter,
	const FVector& DesiredGroundLocation,
	const float AreaRadius,
	const bool bCanOver,
	const float ObstacleFrontClearance,
	const ECollisionChannel TraceObjectType,
	const FName TraceTag)
{
	(void)AreaRadius;
	(void)bCanOver;

	const FVector SourceActorLocation = SourceCharacter.GetActorLocation();
	FVector TraceEnd = DesiredGroundLocation;
	TraceEnd.Z = SourceActorLocation.Z;

	if (FVector::DistSquared2D(SourceActorLocation, TraceEnd) <= FMath::Square(KINDA_SMALL_NUMBER))
	{
		return DesiredGroundLocation;
	}

	FCollisionQueryParams QueryParams(TraceTag, false, &SourceCharacter);
	QueryParams.AddIgnoredActor(&SourceCharacter);
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(TraceObjectType);

	FHitResult ObstacleHit;
	if (!World.LineTraceSingleByObjectType(ObstacleHit, SourceActorLocation, TraceEnd, ObjectQueryParams, QueryParams)
		|| !ObstacleHit.bBlockingHit)
	{
		return DesiredGroundLocation;
	}

	const FVector Direction = (TraceEnd - SourceActorLocation).GetSafeNormal2D();
	if (Direction.IsNearlyZero())
	{
		return DesiredGroundLocation;
	}

	const float HitDistance = FVector::Dist2D(SourceActorLocation, ObstacleHit.ImpactPoint);
	const float SafeFrontDistance = FMath::Max(0.0f, HitDistance - FMath::Max(0.0f, ObstacleFrontClearance));

	FVector LimitedGroundLocation = SourceActorLocation + Direction * SafeFrontDistance;
	LimitedGroundLocation.Z = DesiredGroundLocation.Z;
	return LimitedGroundLocation;
}
}

UFrontierGameplayAbility_AreaSkill::UFrontierGameplayAbility_AreaSkill()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UFrontierGameplayAbility_AreaSkill::ConfirmAreaSkill()
{
	if (!CanConfirmAreaSkill())
	{
		return;
	}

	if (!CanCommitAreaSkillCostAndCooldown())
	{
		FRONTIER_LOG(Warning, TEXT("Area skill confirm failed because equipment skill cost or cooldown rejected it. Ability=%s"), *GetNameSafe(this));
		K2_CancelAbility();
		return;
	}

	if (!CommitAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Area skill confirm failed because CommitAbility was rejected. Ability=%s"), *GetNameSafe(this));
		K2_CancelAbility();
		return;
	}

	if (!CommitAreaSkillCostAndCooldown())
	{
		FRONTIER_LOG(Warning, TEXT("Area skill confirm failed because equipment skill commit was rejected. Ability=%s"), *GetNameSafe(this));
		K2_CancelAbility();
		return;
	}

	bAreaSkillConfirmed = true;
	OnAreaSkillConfirmed();
}

void UFrontierGameplayAbility_AreaSkill::CancelAreaSkill()
{
	if (bAreaSkillConfirmed)
	{
		return;
	}

	OnAreaSkillCancelled();
	K2_CancelAbility();
}

void UFrontierGameplayAbility_AreaSkill::AdjustAreaTargetDistanceInput(const float InputAxis)
{
}

bool UFrontierGameplayAbility_AreaSkill::IsAwaitingAreaConfirm() const
{
	return IsActive() && !bAreaSkillConfirmed;
}

void UFrontierGameplayAbility_AreaSkill::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	if (!bAreaSkillConfirmed)
	{
		ClearPendingEquipmentAreaSkill();
	}

	bAreaSkillConfirmed = false;
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UFrontierGameplayAbility_AreaSkill::CanConfirmAreaSkill()
{
	return IsActive() && !bAreaSkillConfirmed;
}

void UFrontierGameplayAbility_AreaSkill::OnAreaSkillConfirmed()
{
}

void UFrontierGameplayAbility_AreaSkill::OnAreaSkillCancelled()
{
}

bool UFrontierGameplayAbility_AreaSkill::ResolveAreaSkillTargetLocation(
	const FFrontierAreaSkillTargetLocationParams& Params,
	FVector& OutActorLocation,
	FVector& OutGroundLocation)
{
	const ACharacter* SourceCharacter = Params.SourceCharacter;
	if (!SourceCharacter || !SourceCharacter->GetWorld())
	{
		return false;
	}

	UWorld* World = SourceCharacter->GetWorld();

	const UCapsuleComponent* CapsuleComponent = SourceCharacter->GetCapsuleComponent();
	const float CapsuleHalfHeight =
		CapsuleComponent
			? CapsuleComponent->GetScaledCapsuleHalfHeight()
			: 0.0f;

	const FVector SourceActorLocation = SourceCharacter->GetActorLocation();
	const float SourceGroundZ = SourceActorLocation.Z - CapsuleHalfHeight;

	// 캐릭터가 바라보는 수평 방향
	const FVector Forward = SourceCharacter->GetActorForwardVector().GetSafeNormal2D();
	if (Forward.IsNearlyZero())
	{
		return false;
	}

	// 타겟 거리 제한
	const float SafeMaxDistance =
		FMath::Max(Params.MinTargetDistance, Params.MaxTargetDistance);

	const float ResolvedTargetDistance = FMath::Clamp(
		Params.TargetDistance,
		Params.MinTargetDistance,
		SafeMaxDistance);

	// 우선 캐릭터 전방의 목표 위치 계산
	FVector DesiredGroundLocation =
		SourceActorLocation + Forward * ResolvedTargetDistance;

	DesiredGroundLocation.Z = SourceGroundZ;

	// 벽이나 큰 장애물 때문에 영역이 관통하지 않도록 제한
	DesiredGroundLocation = LimitAreaTargetByObstacle(
		*World,
		*SourceCharacter,
		DesiredGroundLocation,
		Params.AreaRadius,
		Params.bCanOver,
		Params.ObstacleFrontClearance,
		Params.ObstacleTraceObjectType,
		Params.TraceTag);

	// 필요할 경우 NavMesh 위로 위치 보정
	if (Params.bUseNavigationProjection)
	{
		if (UNavigationSystemV1* NavigationSystem =
			FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
		{
			FNavLocation ProjectedLocation;

			if (NavigationSystem->ProjectPointToNavigation(
				DesiredGroundLocation,
				ProjectedLocation,
				Params.NavigationProjectionExtent))
			{
				const float HeightDifference = FMath::Abs(
					ProjectedLocation.Location.Z - SourceGroundZ);

				// 플레이어와 지나치게 높이 차이가 나는 Nav 위치는 사용하지 않음
				if (HeightDifference <= Params.MaxAllowedHeightDifference)
				{
					DesiredGroundLocation = ProjectedLocation.Location;

					// NavMesh Projection으로 위치가 변경됐으므로
					// 장애물 제한을 한 번 더 수행
					DesiredGroundLocation = LimitAreaTargetByObstacle(
						*World,
						*SourceCharacter,
						DesiredGroundLocation,
						Params.AreaRadius,
						Params.bCanOver,
						Params.ObstacleFrontClearance,
						Params.ObstacleTraceObjectType,
						Params.TraceTag);
				}
			}
		}
	}

	// 최종 위치에서 실제 바닥 탐색
	const FVector TraceStart =
		DesiredGroundLocation +
		FVector(0.0f, 0.0f, Params.FloorTraceUpDistance);

	const FVector TraceEnd =
		DesiredGroundLocation -
		FVector(0.0f, 0.0f, Params.FloorTraceDownDistance);

	FCollisionQueryParams QueryParams(
		Params.TraceTag,
		false,
		SourceCharacter);

	QueryParams.AddIgnoredActor(SourceCharacter);

	FCollisionObjectQueryParams FloorObjectQueryParams;
	FloorObjectQueryParams.AddObjectTypesToQuery(
		Params.FloorTraceObjectType);

	TArray<FHitResult> FloorHits;

	const bool bHitSomething = World->LineTraceMultiByObjectType(
		FloorHits,
		TraceStart,
		TraceEnd,
		FloorObjectQueryParams,
		QueryParams);

	if (!bHitSomething)
	{
		FRONTIER_LOG(
			Verbose,
			TEXT("Area skill target floor trace found no surface. Character=%s Location=%s"),
			*GetNameSafe(SourceCharacter),
			*DesiredGroundLocation.ToCompactString());

		return false;
	}

	bool bFoundUsableFloor = false;

	for (const FHitResult& FloorHit : FloorHits)
	{
		if (!FloorHit.bBlockingHit)
		{
			continue;
		}

		// 벽이나 지나치게 가파른 면 제외
		if (FloorHit.ImpactNormal.Z < Params.MinFloorNormalZ)
		{
			continue;
		}

		const FVector FloorLocation = FloorHit.ImpactPoint;

		const float FloorHeightDifference =
			FMath::Abs(FloorLocation.Z - SourceGroundZ);

		// 해당 Hit만 무시하고 다음 Hit 검사
		// 작은 상자나 오브젝트 위를 먼저 맞았다고 해서
		// 스킬 전체를 실패시키지 않음
		if (FloorHeightDifference > Params.MaxAllowedHeightDifference)
		{
			continue;
		}

		DesiredGroundLocation = FloorLocation;
		bFoundUsableFloor = true;

		break;
	}

	if (!bFoundUsableFloor)
	{
		FRONTIER_LOG(
			Verbose,
			TEXT("Area skill target floor trace found no usable floor. Character=%s Location=%s"),
			*GetNameSafe(SourceCharacter),
			*DesiredGroundLocation.ToCompactString());

		return false;
	}

	OutGroundLocation = DesiredGroundLocation;

	OutActorLocation =
		DesiredGroundLocation +
		FVector(0.0f, 0.0f, CapsuleHalfHeight);

	return true;
}

bool UFrontierGameplayAbility_AreaSkill::CanCommitAreaSkillCostAndCooldown() const
{
	const UFrontierEquipmentSkillComponent* SkillComponent = Cast<UFrontierEquipmentSkillComponent>(
		GetSourceObject(CurrentSpecHandle, CurrentActorInfo));
	return !SkillComponent || SkillComponent->CanCommitPendingAreaSkill(GetClass());
}

bool UFrontierGameplayAbility_AreaSkill::CommitAreaSkillCostAndCooldown()
{
	UFrontierEquipmentSkillComponent* SkillComponent = Cast<UFrontierEquipmentSkillComponent>(
		GetSourceObject(CurrentSpecHandle, CurrentActorInfo));
	return !SkillComponent || SkillComponent->CommitPendingAreaSkill(GetClass());
}

void UFrontierGameplayAbility_AreaSkill::ClearPendingEquipmentAreaSkill()
{
	if (UFrontierEquipmentSkillComponent* SkillComponent = Cast<UFrontierEquipmentSkillComponent>(
		GetSourceObject(CurrentSpecHandle, CurrentActorInfo)))
	{
		SkillComponent->ClearPendingAreaSkill(GetClass());
	}
}
