#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierDamageStatics.h"
#include "GameFramework/Actor.h"
#include "FrontierArrowProjectile.generated.h"

class UBoxComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UProjectileMovementComponent;

UCLASS()
class FRONTIER_API AFrontierArrowProjectile : public AActor
{
	GENERATED_BODY()

public:
	AFrontierArrowProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	void InitializeArrow(
		AActor* InDamageSource,
		UStaticMesh* InArrowMesh,
		const FFrontierDamageRequest& InDamageRequest,
		float InGravityScale);

	void Launch(const FVector& Direction, float Speed, float MaximumRange);

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	void ArmCollision();
	void DestroyArrow(const TCHAR* Reason);

	UFUNCTION()
	void HandleOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComponent,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UFUNCTION()
	void OnRep_ArrowMesh();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arrow")
	TObjectPtr<UBoxComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arrow")
	TObjectPtr<UStaticMeshComponent> ArrowMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Arrow")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(ReplicatedUsing=OnRep_ArrowMesh)
	TObjectPtr<UStaticMesh> ReplicatedArrowMesh;

private:
	UPROPERTY(Transient)
	TObjectPtr<AActor> DamageSource;

	FFrontierDamageRequest DamageRequest;
	bool bHasLaunched = false;
	FString DestroyReason;
};
