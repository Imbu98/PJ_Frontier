#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/FrontierMeleeTraceAbilityInterface.h"
#include "AbilitySystem/Abilities/FrontierGameplayAbility_WeaponAttack.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "FrontierGameplayAbility_PlayerAttack.generated.h"

class AFrontierBaseCharacter;
class AFrontierWeaponBase;
class AActor;
class UAnimMontage;
class UGameplayEffect;

UCLASS()
class FRONTIER_API UFrontierGameplayAbility_PlayerAttack
	: public UFrontierGameplayAbility_WeaponAttack
	, public IFrontierMeleeTraceAbilityInterface
{
	GENERATED_BODY()

public:
	UFrontierGameplayAbility_PlayerAttack();

	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual void BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides) override;
	virtual void EndAttackTraceWindow() override;
	void RequestComboAttackInput();
	/** Applies a combo transition requested by the owning client on the server. */
	void RequestComboAttackFromServer(int32 RequestedComboIndex);
	virtual void HandleAttackInputPressed() override;
	/** Applies a hit detected by the owning client after resolving the server-side attack state. */
	void HandleClientReportedAttackHit(AActor* TargetActor, const FVector& HitLocation, const FFrontierMeleeTraceOverrides& TraceOverrides);
	bool CanAcceptComboInput() const;
	void OpenComboAttackWindowFromNotify();
	void ResetAttackStateFromNotify();

protected:
	UFUNCTION()
	void HandleMontageCompleted();

	UFUNCTION()
	void HandleMontageCancelled();

	UFUNCTION()
	void HandleMontageBlendOut();

	bool ResolveAttackData(AFrontierBaseCharacter* SourceCharacter);
	bool ResolveAttackDataForNextCombo();
	bool HasEnoughStaminaForCachedAttack() const;
	void ConsumeCachedAttackStamina();
	float ResolveAttackPlayRate() const;
	bool PlayCachedAttackMontage();
	bool TryStartBufferedComboAttack(bool bNotifyServer = true);
	void TryStartHeldComboAttack();
	void FinishComboMontageSwitch();
	void ExecuteAttackTrace(const FFrontierMeleeTraceOverrides& TraceOverrides);
	void ApplyAttackDamageToTarget(AActor* TargetActor, const FVector& HitLocation, const FFrontierMeleeTraceOverrides& TraceOverrides);
	bool ResolveAttackActionData();
	void LockCharacterMovementIfNeeded();
	void UnlockCharacterMovementIfNeeded();

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBaseCharacter> CachedSourceCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierWeaponBase> CachedWeapon;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierWeaponDataAsset> CachedWeaponData;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> LoadedAttackMontage;

	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> LoadedDamageEffectClass;

	UPROPERTY(Transient)
	TArray<TSubclassOf<UGameplayEffect>> LoadedAdditionalHitEffectClasses;

	FFrontierAttackActionData CachedAttackActionData;

	bool bHasCachedAttackActionData = false;
	bool bMovementLocked = false;
	bool bComboInputBuffered = false;
	bool bComboWindowOpen = false;
	bool bSwitchingComboMontage = false;
	TSet<TObjectPtr<AActor>> ReportedHitActorsThisAttack;
	float AttackActivationTimeSeconds = 0.0f;
	float CachedAttackPlayRate = 1.0f;
	TEnumAsByte<EMovementMode> CachedMovementMode = MOVE_Walking;
	uint8 CachedCustomMovementMode = 0;
	float CachedAttackMovementSpeedMultiplier = 1.0f;
	EFrontierAttackAnimationMode CachedAttackAnimationMode = EFrontierAttackAnimationMode::UpperBodyOnly;
	EFrontierElementalType CachedWeaponElementalType = EFrontierElementalType::Normal;

	UPROPERTY(Transient)
	int32 NextComboIndex = 0;

	UPROPERTY(Transient)
	int32 CurrentComboAttackIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FTimerHandle AttackAbilityTimeoutTimerHandle;

	UPROPERTY(Transient)
	FTimerHandle AutoComboHoldTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category="Attack|Combo", meta=(ClampMin="0.0"))
	float MinimumAutoComboHoldSeconds = 0.18f;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="0.0"))
	float MontageTimeoutBuffer = 0.25f;

	UPROPERTY(EditDefaultsOnly, Category="Attack", meta=(ClampMin="1.0"))
	float MaxAttackPlayRate = 2.0f;
};
