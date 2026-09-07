#pragma once

#include "CoreMinimal.h"
#include "Combat/FrontierHitReactionTypes.h"
#include "GameplayTagContainer.h"
#include "Inventory/FrontierItemSharedTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FrontierDamageStatics.generated.h"

class AActor;
class UGameplayEffect;
class UFrontierAbilitySystemComponent;
class UFrontierAttributeSet;

USTRUCT(BlueprintType)
struct FFrontierDamageRequest
{
	GENERATED_BODY()

	/** Stable server event ID. Reusing it prevents duplicate raid contribution processing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	FGuid DamageEventId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage", meta=(ClampMin="0.0"))
	float BaseDamage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage", meta=(ClampMin="0.0"))
	float DamageMultiplier = 1.0f;

	/** A negative value inherits DamageMultiplier for backwards-compatible attacks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	float ElementDamageMultiplier = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	FGameplayTag DamageTypeTag;

	/** Selects the target-side reaction definition independently from damage type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	FGameplayTag HitReactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	EFrontierHitReactionLevel HitReactionLevel = EFrontierHitReactionLevel::None;

	/** Attack-owned reaction tuning. The target never selects these values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage", meta=(ClampMin="0.0"))
	float StaggerDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage", meta=(ClampMin="0.0"))
	float KnockbackHorizontalStrength = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage", meta=(ClampMin="0.0"))
	float KnockbackVerticalStrength = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	EFrontierElementalType ElementalType = EFrontierElementalType::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	TArray<TSubclassOf<UGameplayEffect>> AdditionalEffectClasses;

	/** Optional server hit point used for attacker feedback and impact-driven death effects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Damage")
	FVector HitLocation = FVector::ZeroVector;
};

USTRUCT(BlueprintType)
struct FFrontierDamageResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	bool bApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	bool bHit = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float AppliedDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float RawDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float SourceAttackPower = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float SourceElementAttackPower = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float SourceWeaponTypeAttackPower = 0.0f;

	/** Skill-tree bonus matching both the current weapon family and attack element. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float SourceWeaponElementAttackPower = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float TargetDefense = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float TargetElementResistance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float DefenseMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float ResistanceMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	float AffinityMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Damage")
	bool bTargetDied = false;
};

USTRUCT(BlueprintType)
struct FFrontierHealingRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Healing", meta=(ClampMin="0.0"))
	float BaseHealing = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Healing", meta=(ClampMin="0.0"))
	float HealingMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Healing")
	FGameplayTag HealingTypeTag;
};

USTRUCT(BlueprintType)
struct FFrontierHealingResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Healing")
	bool bApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Healing")
	float AppliedHealing = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Healing")
	bool bFullyHealed = false;
};

UCLASS()
class FRONTIER_API UFrontierDamageStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool CanActorsDamageEachOther(const AActor* SourceActor, const AActor* TargetActor);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static float GetDamageTypeMultiplier(const AActor* TargetActor, EFrontierElementalType ElementalType);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool IsDamageTypeWeakness(const AActor* TargetActor, EFrontierElementalType ElementalType);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool IsElementalAdvantage(EFrontierElementalType AttackElementalType, EFrontierElementalType TargetElementalType);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool IsElementalDisadvantage(EFrontierElementalType AttackElementalType, EFrontierElementalType TargetElementalType);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool AreElementsOpposed(EFrontierElementalType FirstElementalType, EFrontierElementalType SecondElementalType);

	/** Converts defense/resistance rating to a smooth damage multiplier. */
	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static float CalculateRatingDamageMultiplier(float Rating, float RatingConstant = 100.0f);

	static float GetElementAttackPower(const UFrontierAttributeSet* Attributes, EFrontierElementalType ElementalType);
	static float GetElementResistance(const UFrontierAttributeSet* Attributes, EFrontierElementalType ElementalType);

	UFUNCTION(BlueprintCallable, Category="Frontier|Damage")
	static FFrontierDamageResult ApplyDamage(AActor* SourceActor, AActor* TargetActor, const FFrontierDamageRequest& DamageRequest);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static bool IsHeavyHitReaction(EFrontierHitReactionLevel HitReactionLevel);

	UFUNCTION(BlueprintPure, Category="Frontier|Damage")
	static FGameplayTag GetHitGameplayCueTagForElementalType(EFrontierElementalType ElementalType);

private:
	static void ExecuteDamageHitGameplayCue(
		AActor* SourceActor,
		AActor* TargetActor,
		UFrontierAbilitySystemComponent* TargetASC,
		EFrontierElementalType ElementalType,
		FGameplayTag DamageTypeTag,
		float FinalDamage,
		const FVector& HitLocation);
};
