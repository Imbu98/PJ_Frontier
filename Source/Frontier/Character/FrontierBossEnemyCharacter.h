#pragma once

#include "CoreMinimal.h"
#include "Character/FrontierEnemyCharacter.h"
#include "TimerManager.h"
#include "FrontierBossEnemyCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFrontierBossArcMoveFinishedSignature, bool, bInterrupted);

UENUM(BlueprintType)
enum class EFrontierBossDistanceRange : uint8
{
	None,
	Close,
	Mid,
	Far,
	OutOfCombat
};

UENUM(BlueprintType)
enum class EFrontierBossCombatAction : uint8
{
	None,
	MeleeAttack,
	MeleeSkill,
	DashSkill,
	RangedSkill,
	Reposition,
	ReturnHome
};

USTRUCT(BlueprintType)
struct FFrontierBossSkillDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill")
	EFrontierBossCombatAction Action = EFrontierBossCombatAction::None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill")
	FGameplayTag AbilityTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill", meta=(ClampMin="0.0"))
	float MinRange = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill", meta=(ClampMin="0.0"))
	float MaxRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill", meta=(ClampMin="0.0"))
	float Cooldown = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill", meta=(ClampMin="0.0"))
	float Weight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Boss|Skill")
	bool bRequiresLineOfSight = true;
};

UCLASS()
class FRONTIER_API AFrontierBossEnemyCharacter : public AFrontierEnemyCharacter
{
	GENERATED_BODY()

public:
	AFrontierBossEnemyCharacter();

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Combat")
	float GetDistanceToCombatTarget() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Combat")
	EFrontierBossDistanceRange GetDistanceRangeToCombatTarget() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Combat")
	bool IsBossActionPlaying() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Combat")
	void CancelBossActions();

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Skill")
	bool IsSkillReady(FGameplayTag AbilityTag) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Skill")
	bool CanUseSkill(FGameplayTag AbilityTag) const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Skill")
	bool TryActivateBossAbility(FGameplayTag AbilityTag);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Skill")
	bool TryActivateWeightedSkillForRange(EFrontierBossDistanceRange DistanceRange);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Action")
	bool SelectWeightedCombatAction();

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Action")
	EFrontierBossCombatAction GetSelectedCombatAction() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Action")
	FGameplayTag GetSelectedCombatActionAbilityTag() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Action")
	void ClearSelectedCombatAction();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Action")
	bool TryActivateSelectedCombatActionAbility();

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Targeting")
	bool CanReselectCombatTarget() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Targeting")
	bool TryFindRandomCombatTargetInRange(bool bKeepExistingTargetIfNoCandidate = true);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool RefreshMoveLocationForSkillRange(FGameplayTag AbilityTag);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool RefreshRepositionLocation();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool RefreshReturnHomeLocation();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Combat")
	void FaceCombatTarget();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Combat")
	void AbandonCombat();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool StartArcMoveToLocation(FVector StartLocation, FVector ImpactLocation, float JumpHeight, float Duration);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	void StopArcMove(bool bInterrupted);

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool CompleteArcMoveFromNotify();

	UFUNCTION(BlueprintCallable, Category="Frontier|Boss|Movement")
	bool PrepareArcMoveLandingFromNotify();

	UFUNCTION(BlueprintPure, Category="Frontier|Boss|Movement")
	bool IsArcMoveReadyForLandingNotify() const;

	UPROPERTY(BlueprintAssignable, Category="Frontier|Boss|Movement")
	FFrontierBossArcMoveFinishedSignature OnArcMoveFinished;

protected:
	virtual void BeginPlay() override;
	virtual EFrontierLootContainerSourceType GetDeathLootContainerSourceType() const override;
	virtual bool CanUseAttackOnTarget(AActor* TargetActor) const override;

	void EnsurePrimaryAttackAbility();
	const FFrontierBossSkillDefinition* FindSkillDefinition(FGameplayTag AbilityTag) const;
	bool HasLineOfSightToCombatTarget() const;
	bool GetWeightedSkillForRange(EFrontierBossDistanceRange DistanceRange, FGameplayTag& OutAbilityTag) const;
	bool IsSkillInDistanceRange(const FFrontierBossSkillDefinition& SkillDefinition, float Distance) const;
	bool CanSelectCombatAction(const FFrontierBossSkillDefinition& SkillDefinition) const;
	void SetDesiredBossMoveLocation(const FVector& NewLocation);
	void UpdateArcMove();
	void FinishArcMove(bool bInterrupted);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Range", meta=(ClampMin="0.0"))
	float CloseRange = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Range", meta=(ClampMin="0.0"))
	float MidRange = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Range", meta=(ClampMin="0.0"))
	float FarRange = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Movement", meta=(ClampMin="0.0"))
	float RepositionRadius = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Targeting", meta=(ClampMin="0.0"))
	float TargetSearchRadius = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Targeting", meta=(ClampMin="0.0"))
	float TargetReselectCooldown = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Targeting")
	bool bExcludeStunnedTargetsFromReselect = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Targeting")
	bool bRequireLineOfSightForTargetReselect = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Targeting", meta=(ClampMin="1"))
	int32 MaxTargetCandidates = 16;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Boss|Skill")
	TArray<FFrontierBossSkillDefinition> BossSkills;

	UPROPERTY(Transient)
	TMap<FGameplayTag, float> LastSkillUseTimes;

	UPROPERTY(Transient)
	float LastTargetReselectTime = -TNumericLimits<float>::Max();

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Boss|Action")
	EFrontierBossCombatAction SelectedCombatAction = EFrontierBossCombatAction::None;

	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category="Frontier|Boss|Action")
	FGameplayTag SelectedCombatActionAbilityTag;

	UPROPERTY(Transient)
	FTimerHandle ArcMoveTimerHandle;

	UPROPERTY(Transient)
	FVector ArcMoveStartLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ArcMoveImpactLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float ArcMoveJumpHeight = 0.0f;

	UPROPERTY(Transient)
	float ArcMoveDuration = 0.0f;

	UPROPERTY(Transient)
	float ArcMoveElapsedTime = 0.0f;

	UPROPERTY(Transient)
	bool bArcMoveActive = false;

	UPROPERTY(Transient)
	bool bArcMoveAwaitingLandingNotify = false;

	static float SmoothArcAlpha(float Alpha);

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> SavedArcMoveMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	uint8 SavedArcMoveCustomMovementMode = 0;
};
