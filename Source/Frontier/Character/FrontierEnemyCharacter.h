#pragma once

#include "CoreMinimal.h"
#include "Character/FrontierBaseCharacter.h"
#include "GameplayTagContainer.h"
#include "FrontierEnemyCharacter.generated.h"

class AActor;
class AFrontierChaosDeathActor;
class UAnimMontage;
class UGameplayAbility;
class USplineComponent;
class UWidgetComponent;

UENUM(BlueprintType)
enum class EFrontierEnemyState : uint8
{
	Idle,
	Patrolling,
	Chasing,
	Attacking,
	Returning
};

UENUM(BlueprintType)
enum class EFrontierEnemyMoveMode : uint8
{
	Patrol,
	Chase
};

UENUM(BlueprintType)
enum class EFrontierPatrolCenterMode : uint8
{
	SpawnLocation,
	CurrentLocation,
	ExplicitLocation
};

UENUM(BlueprintType)
enum class EFrontierPatrolRouteMode : uint8
{
	RandomAroundCenter,
	Spline
};

USTRUCT(BlueprintType)
struct FFrontierEnemyAttackDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FName StartSocketName = TEXT("startSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FName EndSocketName = TEXT("endSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.1"))
	float TraceRadius = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack", meta=(ClampMin="0.0"))
	float BaseDamage = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FGameplayTag DamageTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::Light;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|HitReaction", meta=(ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack|HitReaction", meta=(ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	EFrontierHitReactionLevel HitReactionResistance = EFrontierHitReactionLevel::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	bool bDrawDebugTrace = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FFrontierEnemyStateChangedSignature, EFrontierEnemyState, PreviousState, EFrontierEnemyState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierEnemyDiedSignature, AFrontierEnemyCharacter*, Enemy);

UCLASS()
class FRONTIER_API AFrontierEnemyCharacter : public AFrontierBaseCharacter
{
	GENERATED_BODY()

public:
	AFrontierEnemyCharacter();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual EFrontierHitReactionLevel GetCurrentHitReactionResistance() const override;
	virtual bool IsDamageReactionBlocked() const override;

	UFUNCTION(BlueprintPure, Category="Frontier|HitReaction")
	bool IsEnemyDamageReactionBlocked() const { return bBlockDamageReaction; }

	UFUNCTION(BlueprintCallable, Category="Frontier|HitReaction")
	void SetBlockDamageReaction(bool bNewBlockDamageReaction);

	UFUNCTION(BlueprintCallable, Category="Frontier|AI")
	void SetEnemyState(EFrontierEnemyState NewState);

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	EFrontierEnemyState GetEnemyState() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|AI")
	void SetCombatTarget(AActor* NewCombatTarget);

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	AActor* GetCombatTarget() const;

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	bool HasCombatTarget() const;

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	bool IsCombatTargetInAttackRange() const;

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	FVector GetDesiredMoveLocation() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|AI")
	bool RefreshDesiredMoveLocation(EFrontierEnemyMoveMode MoveMode);

	UFUNCTION(BlueprintCallable, Category="Frontier|AI")
	bool TryPerformAttack();

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	float GetLastAttackMontageDuration() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|AI|Patrol")
	void SetRandomPatrolArea(FVector InHomeLocation, FVector InPatrolCenter, float InPatrolRadius);

	UFUNCTION(BlueprintCallable, Category="Frontier|AI|Patrol")
	void SetSplinePatrolRoute(FVector InHomeLocation, USplineComponent* InPatrolSpline);

	bool PrepareRandomAttackDefinitionForAbility();
	UAnimMontage* GetCurrentAttackMontage() const;
	bool HasValidAttackDefinition() const;

	void BeginCurrentAttackTrace();
	void TickCurrentAttackTrace();
	void EndCurrentAttackTrace();

	UPROPERTY(BlueprintAssignable, Category="Frontier|AI")
	FFrontierEnemyStateChangedSignature OnEnemyStateChanged;

	UPROPERTY(BlueprintAssignable, Category="Frontier|AI")
	FFrontierEnemyDiedSignature OnEnemyDied;

protected:
	virtual void BeginPlay() override;
	virtual void HandleDeathStateChanged(bool bWasDead) override;
	virtual bool UsesAlternativeDeathPresentation() const override;
	virtual void ActivateAlternativeDeathPresentation() override;
	virtual void CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const override;
	virtual EFrontierLootContainerSourceType GetDeathLootContainerSourceType() const override;

	UFUNCTION()
	void OnRep_EnemyState(EFrontierEnemyState PreviousState);

	UFUNCTION()
	void OnRep_CombatTarget(AActor* PreviousCombatTarget);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackMontage(UAnimMontage* AttackMontage);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSpawnChaosDeathActor(FTransform SpawnTransform, FVector ImpactPoint, bool bHasImpactPoint);

	virtual bool SelectPatrolLocation(FVector& OutLocation);
	virtual bool SelectChaseLocation(AActor* TargetActor, FVector& OutLocation);
	virtual bool CanUseAttackOnTarget(AActor* TargetActor) const;
	virtual bool ExecuteAttackBehavior(AActor* TargetActor);
	virtual void HandleEnemyStateChanged(EFrontierEnemyState PreviousState, EFrontierEnemyState NewState);
	void GrantStartupAbilities();

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category="Frontier|AI|Patrol")
	TArray<TObjectPtr<AActor>> PatrolPoints;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|AI|Patrol")
	EFrontierPatrolCenterMode PatrolCenterMode = EFrontierPatrolCenterMode::SpawnLocation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|AI|Patrol")
	EFrontierPatrolRouteMode PatrolRouteMode = EFrontierPatrolRouteMode::RandomAroundCenter;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|AI|Patrol", meta=(ClampMin="0.0"))
	float PatrolRadius = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|AI|Patrol", meta=(ClampMin="0.0"))
	float MinPatrolDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|AI|Combat", meta=(ClampMin="0.0"))
	float AttackRange = 200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|AI|Combat")
	TArray<FFrontierEnemyAttackDefinition> AttackDefinitions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|AbilitySystem")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** Allows Chaos death when a player lands the killing blow with a Knockback hit reaction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos")
	bool bUseChaosDeath = false;

	/** Blocks hit reaction montages, stagger, knockback, and knockdown-style reactions while enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|HitReaction")
	bool bBlockDamageReaction = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(EditCondition="bUseChaosDeath"))
	TSubclassOf<AFrontierChaosDeathActor> ChaosDeathActorClass;

	/** Applied relative to the skeletal mesh transform when spawning the destruction actor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos", meta=(EditCondition="bUseChaosDeath"))
	FTransform ChaosDeathRelativeTransform = FTransform::Identity;

	/** Server-selected death presentation, replicated so clients do not also show the ragdoll. */
	UPROPERTY(Replicated, Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Death|Chaos")
	bool bUseChaosDeathForCurrentDeath = false;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI|Combat")
	float LastAttackMontageDuration = 0.0f;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI|Combat")
	int32 CurrentAttackDefinitionIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing=OnRep_EnemyState, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI")
	EFrontierEnemyState EnemyState = EFrontierEnemyState::Idle;

	UPROPERTY(ReplicatedUsing=OnRep_CombatTarget, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI")
	TObjectPtr<AActor> CombatTarget = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI")
	FVector DesiredMoveLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI")
	FVector HomeLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI")
	FVector PatrolCenterLocation = FVector::ZeroVector;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI")
	TObjectPtr<USplineComponent> PatrolSplineComponent = nullptr;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|AI")
	int32 PatrolPointIndex = INDEX_NONE;
};
