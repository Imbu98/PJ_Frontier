#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierUpgradeBalanceSubsystem.generated.h"

USTRUCT(BlueprintType)
struct FFrontierUpgradeMaterialRequirement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	FName ItemTemplateId;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	int32 RequiredAmount = 1;
};

USTRUCT(BlueprintType)
struct FFrontierUpgradeCurrencyRequirement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	FString CurrencyCode;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	int64 RequiredAmount = 0;
};

USTRUCT(BlueprintType)
struct FFrontierUpgradeLevelData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	int32 TargetEnhancementLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	float SuccessProbability = 100.0f;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	TArray<FFrontierUpgradeMaterialRequirement> Materials;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	TArray<FFrontierUpgradeCurrencyRequirement> Currencies;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	float FirstRandomStatIncrease = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Upgrade")
	int32 SkillLevelIncrease = 0;
};

USTRUCT(BlueprintType)
struct FFrontierUpgradeLevelTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FName ItemTemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 TargetEnhancementLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	float SuccessProbability = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FString RequiredCurrencyCode;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int64 RequiredCurrencyAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	float FirstRandomStatIncrease = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 SkillLevelIncrease = 0;
};

USTRUCT(BlueprintType)
struct FFrontierUpgradeMaterialTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FName ItemTemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 TargetEnhancementLevel = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	FName MaterialItemTemplateId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Upgrade")
	int32 RequiredAmount = 1;
};

USTRUCT(BlueprintType)
struct FFrontierEquipmentScoreEnhancementRateTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment Score", meta=(ClampMin="0", ClampMax="9"))
	int32 EnhancementLevel = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Equipment Score", meta=(ClampMin="0.0"))
	float EnhancementRate = 0.0f;
};

USTRUCT()
struct FFrontierUpgradeLevelCollection
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FFrontierUpgradeLevelData> Levels;
};

UCLASS()
class FRONTIER_API UFrontierUpgradeBalanceSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	const FFrontierUpgradeLevelData* FindUpgradeLevel(FName ItemTemplateId, int32 TargetEnhancementLevel) const;
	bool ReloadFromDataTables();

	float CalculateNormalizedEquipmentScore(const FFrontierItemInstance& ItemInstance) const;
	float CalculateEquipmentScore(const FFrontierItemInstance& ItemInstance) const;
	void RecalculateEquipmentScore(FFrontierItemInstance& ItemInstance) const;
	float GetEquipmentEnhancementRate(int32 EnhancementLevel) const;

private:
	bool AddLevelRow(const FName& RowName, const FFrontierUpgradeLevelTableRow& SourceRow);
	bool AddMaterialRow(const FName& RowName, const FFrontierUpgradeMaterialTableRow& SourceRow);
	bool AddEquipmentScoreEnhancementRateRow(const FName& RowName, const FFrontierEquipmentScoreEnhancementRateTableRow& SourceRow);

	TMap<FName, FFrontierUpgradeLevelCollection> UpgradeLevelsByItem;
	TMap<int32, float> EquipmentScoreEnhancementRates;
};
