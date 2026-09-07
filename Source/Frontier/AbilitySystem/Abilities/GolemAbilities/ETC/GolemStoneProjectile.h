#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GolemStoneProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UGameplayEffect;
class UAbilitySystemComponent;
class AFrontierChaosDeathActor;

UCLASS()
class FRONTIER_API AGolemStoneProjectile : public AActor
{
	GENERATED_BODY()

public:
	AGolemStoneProjectile();

	void InitProjectile(
		AActor* InOwnerActor,
		AActor* InInstigatorActor,
		TSubclassOf<UGameplayEffect> InDamageEffectClass,
		float InDamage,
		FGameplayTag InDamageTypeTag
	);

	void SetHeldProjectileState(bool bHeld);

	void LaunchToDirection(const FVector& Direction, float Speed);

	void LaunchToTarget(const FVector& TargetLocation, float Speed);

	void SetImpactWarningId(FGuid InWarningId);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> RockMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float LifeSeconds = 5.0f;

	/** Blueprint child with a Geometry Collection assigned to its component. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Impact")
	TSubclassOf<AFrontierChaosDeathActor> ImpactBreakActorClass;

	UPROPERTY()
	TObjectPtr<AActor> OwnerActor;

	UPROPERTY()
	TObjectPtr<AActor> InstigatorActor;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float DamageMultiplier = 1.0f;

	float Damage = 0.0f;
	FGameplayTag DamageTypeTag;
	FGuid ImpactWarningId;
	bool bImpactWarningHidden = false;

	void HideImpactWarning();

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	UFUNCTION()
	void OnSphereHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	UFUNCTION()
	void OnProjectileStop(const FHitResult& ImpactResult);

	void SpawnImpactBreakEffect(const FVector& ImpactLocation, const FVector& ImpactNormal);
	void HandleBlockingImpact(AActor* OtherActor, const FVector& ImpactLocation, const FVector& ImpactNormal);
	void EnableWorldStaticCollision();
	void DelayWorldStaticCollisionAfterLaunch();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastSpawnImpactBreakEffect(const FTransform& SpawnTransform, FVector ImpactPoint);

	bool bImpactHandled = false;
	FTimerHandle WorldStaticCollisionTimerHandle;
	FVector WorldStaticCollisionGraceStartLocation = FVector::ZeroVector;
};
