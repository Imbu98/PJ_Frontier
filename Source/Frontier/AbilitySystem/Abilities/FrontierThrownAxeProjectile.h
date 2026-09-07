#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "FrontierThrownAxeProjectile.generated.h"

class UGameplayEffect;
class UMaterialInterface;
class UProjectileMovementComponent;
class USkeletalMesh;
class USkeletalMeshComponent;
class USphereComponent;

UCLASS()
class FRONTIER_API AFrontierThrownAxeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AFrontierThrownAxeProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void Tick(float DeltaSeconds) override;

	void InitProjectile(
		AActor* InSourceActor,
		USkeletalMesh* InWeaponMesh,
		const TArray<UMaterialInterface*>& InWeaponMaterials,
		float InBaseDamage,
		float InDamageMultiplier,
		FGameplayTag InDamageTypeTag,
		EFrontierElementalType InElementalType,
		TSubclassOf<UGameplayEffect> InDamageEffectClass);

	void LaunchInDirection(const FVector& Direction, float Speed);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USphereComponent> SphereCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<USkeletalMeshComponent> AxeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Projectile")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditDefaultsOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float LifeSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, Category="Projectile")
	FRotator MeshRelativeRotation = FRotator(0.0f, 90.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, Category="Projectile")
	bool bSpinMeshOnPitch = true;

	UPROPERTY(EditDefaultsOnly, Category="Projectile", meta=(ClampMin="0.0"))
	float MeshPitchSpinSpeed = 1080.0f;

	UPROPERTY(ReplicatedUsing=OnRep_ReplicatedWeaponMesh)
	TObjectPtr<USkeletalMesh> ReplicatedWeaponMesh = nullptr;

	UPROPERTY()
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY()
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	float BaseDamage = 0.0f;
	float DamageMultiplier = 1.0f;
	FGameplayTag DamageTypeTag;
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;
	TSet<TWeakObjectPtr<AActor>> DamagedActors;

	UFUNCTION()
	void OnRep_ReplicatedWeaponMesh();

	UFUNCTION()
	void OnSphereBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void ApplyReplicatedWeaponMesh();
};
