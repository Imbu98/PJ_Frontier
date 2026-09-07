#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Skill/FrontierSkillTypes.h"
#include "FrontierSkillDataSubsystem.generated.h"

class UDataTable;

UCLASS()
class FRONTIER_API UFrontierSkillDataSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UFUNCTION(BlueprintCallable, Category="Frontier|Skill")
	void RebuildSkillDataMap();

	const FFrontierSkillTableRow* FindSkillData(const FGameplayTag& SkillTag) const;
	const FFrontierSkillTableRow* FindSkillDataById(FName SkillId) const;
	const FFrontierSkillTableRow* FindSkillDataByIdString(const FString& SkillId) const;
	FName FindSkillId(const FGameplayTag& SkillTag) const;
	FName FindSkillIdByAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const;

	/** Applies base and unlocked milestone modifiers to caller-provided default parameters. */
	bool ApplySkillBalance(
		TSubclassOf<UGameplayAbility> AbilityClass,
		int32 SkillLevel,
		TMap<FGameplayTag, float>& InOutParameters) const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetSkillDataTableForTesting(UDataTable* InSkillDataTable);
#endif

private:
	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedSkillDataTable;

	UPROPERTY(Transient)
	TObjectPtr<UDataTable> LoadedSkillBalanceDataTable;

	TMap<FGameplayTag, const FFrontierSkillTableRow*> SkillDataMap;
	TMap<FName, const FFrontierSkillTableRow*> SkillDataByIdMap;
	TMap<FGameplayTag, FName> SkillIdByTagMap;
	TMap<TSubclassOf<UGameplayAbility>, FName> SkillIdByAbilityClassMap;
	TMap<FName, const FFrontierSkillBalanceTableRow*> SkillBalanceByIdMap;
};
