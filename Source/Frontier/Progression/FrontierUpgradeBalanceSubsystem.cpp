#include "Progression/FrontierUpgradeBalanceSubsystem.h"

#include "Engine/DataTable.h"
#include "Progression/FrontierUpgradeSettings.h"

void UFrontierUpgradeBalanceSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ReloadFromDataTables();
}

const FFrontierUpgradeLevelData* UFrontierUpgradeBalanceSubsystem::FindUpgradeLevel(
	const FName ItemTemplateId,
	const int32 TargetEnhancementLevel) const
{
	const FFrontierUpgradeLevelCollection* LevelCollection = UpgradeLevelsByItem.Find(ItemTemplateId);
	if (!LevelCollection)
	{
		return nullptr;
	}

	for (const FFrontierUpgradeLevelData& Level : LevelCollection->Levels)
	{
		if (Level.TargetEnhancementLevel == TargetEnhancementLevel)
		{
			return &Level;
		}
	}
	return nullptr;
}

bool UFrontierUpgradeBalanceSubsystem::ReloadFromDataTables()
{
	UpgradeLevelsByItem.Reset();
	EquipmentScoreEnhancementRates.Reset();

	const UFrontierUpgradeSettings* Settings = GetDefault<UFrontierUpgradeSettings>();
	if (!Settings)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade settings are unavailable."));
		return false;
	}

	UDataTable* LevelTable = Settings->LevelTableDataTable.LoadSynchronous();
	UDataTable* MaterialTable = Settings->MaterialTableDataTable.LoadSynchronous();
	if (!LevelTable || !MaterialTable)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade DataTables are not configured."));
		return false;
	}

	if (LevelTable->GetRowStruct() != FFrontierUpgradeLevelTableRow::StaticStruct()
		|| MaterialTable->GetRowStruct() != FFrontierUpgradeMaterialTableRow::StaticStruct())
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade DataTables use an unexpected row struct."));
		return false;
	}

	bool bSuccess = true;
	LevelTable->ForeachRow<FFrontierUpgradeLevelTableRow>(
		TEXT("LoadFrontierUpgradeLevels"),
		[this, &bSuccess](const FName& RowName, const FFrontierUpgradeLevelTableRow& Row)
		{
			bSuccess &= AddLevelRow(RowName, Row);
		});

	if (!bSuccess)
	{
		UpgradeLevelsByItem.Reset();
		return false;
	}

	MaterialTable->ForeachRow<FFrontierUpgradeMaterialTableRow>(
		TEXT("LoadFrontierUpgradeMaterials"),
		[this, &bSuccess](const FName& RowName, const FFrontierUpgradeMaterialTableRow& Row)
		{
			bSuccess &= AddMaterialRow(RowName, Row);
		});

	if (!bSuccess)
	{
		UpgradeLevelsByItem.Reset();
	}

	if (bSuccess)
	{
		if (UDataTable* EquipmentScoreTable = Settings->EquipmentScoreEnhancementRateDataTable.LoadSynchronous())
		{
			if (EquipmentScoreTable->GetRowStruct() != FFrontierEquipmentScoreEnhancementRateTableRow::StaticStruct())
			{
				UE_LOG(LogTemp, Error, TEXT("Frontier equipment score DataTable uses an unexpected row struct."));
				UpgradeLevelsByItem.Reset();
				EquipmentScoreEnhancementRates.Reset();
				return false;
			}

			EquipmentScoreTable->ForeachRow<FFrontierEquipmentScoreEnhancementRateTableRow>(
				TEXT("LoadFrontierEquipmentScoreEnhancementRates"),
				[this, &bSuccess](const FName& RowName, const FFrontierEquipmentScoreEnhancementRateTableRow& Row)
				{
					bSuccess &= AddEquipmentScoreEnhancementRateRow(RowName, Row);
				});
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Frontier equipment score enhancement rate DataTable is not configured. EnhancementRate defaults to 0."));
		}
	}

	if (!bSuccess)
	{
		UpgradeLevelsByItem.Reset();
		EquipmentScoreEnhancementRates.Reset();
	}
	return bSuccess;
}

float UFrontierUpgradeBalanceSubsystem::CalculateNormalizedEquipmentScore(const FFrontierItemInstance& ItemInstance) const
{
	const EFrontierItemCategory Category = ItemInstance.GetCategory();
	if (Category != EFrontierItemCategory::Weapon
		&& Category != EFrontierItemCategory::Armor
		&& Category != EFrontierItemCategory::Accessory)
	{
		return 0.0f;
	}

	if (ItemInstance.RuntimeGeneratedStats.IsEmpty())
	{
		return 0.0f;
	}

	float NormalizedValueSum = 0.0f;
	int32 NormalizedStatCount = 0;
	for (const FFrontierRuntimeStatData& RuntimeStat : ItemInstance.RuntimeGeneratedStats)
	{
		if (!RuntimeStat.StatTag.IsValid() || !RuntimeStat.bHasNormalizedValue)
		{
			continue;
		}

		NormalizedValueSum += RuntimeStat.NormalizedValue;
		++NormalizedStatCount;
	}

	// The backend supplies normalizedValue per generated option. This only aggregates
	// those supplied values; it never derives normalizedValue from item stat ranges.
	return NormalizedStatCount > 0
		? NormalizedValueSum / static_cast<float>(NormalizedStatCount)
		: 0.0f;
}

float UFrontierUpgradeBalanceSubsystem::GetEquipmentEnhancementRate(const int32 EnhancementLevel) const
{
	const int32 ClampedLevel = FMath::Clamp(EnhancementLevel, 0, 9);
	if (const float* Rate = EquipmentScoreEnhancementRates.Find(ClampedLevel))
	{
		return FMath::Max(0.0f, *Rate);
	}

	return 0.0f;
}

float UFrontierUpgradeBalanceSubsystem::CalculateEquipmentScore(const FFrontierItemInstance& ItemInstance) const
{
	const EFrontierItemCategory Category = ItemInstance.GetCategory();
	if (Category != EFrontierItemCategory::Weapon
		&& Category != EFrontierItemCategory::Armor
		&& Category != EFrontierItemCategory::Accessory)
	{
		return 0.0f;
	}

	const float BaseValue = FMath::Max(0.0f, ItemInstance.ItemTemplateData.EquipmentBaseValue);
	const float NormalizedScore = CalculateNormalizedEquipmentScore(ItemInstance);
	const float EnhancementRate = GetEquipmentEnhancementRate(ItemInstance.EnhancementLevel);
	return BaseValue * (NormalizedScore + EnhancementRate);
}

void UFrontierUpgradeBalanceSubsystem::RecalculateEquipmentScore(FFrontierItemInstance& ItemInstance) const
{
	ItemInstance.NormalizedScore = CalculateNormalizedEquipmentScore(ItemInstance);
	ItemInstance.EquipmentScore = CalculateEquipmentScore(ItemInstance);
}

bool UFrontierUpgradeBalanceSubsystem::AddLevelRow(
	const FName& RowName,
	const FFrontierUpgradeLevelTableRow& SourceRow)
{
	if (SourceRow.ItemTemplateId.IsNone()
		|| SourceRow.TargetEnhancementLevel <= 0
		|| SourceRow.SuccessProbability < 0.0f
		|| SourceRow.SuccessProbability > 100.0f
		|| SourceRow.RequiredCurrencyAmount < 0
		|| (SourceRow.RequiredCurrencyCode.IsEmpty() && SourceRow.RequiredCurrencyAmount > 0)
		|| (!SourceRow.RequiredCurrencyCode.IsEmpty() && SourceRow.RequiredCurrencyAmount <= 0)
		|| SourceRow.FirstRandomStatIncrease < 0.0f
		|| SourceRow.SkillLevelIncrease < 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade level row %s contains invalid data."), *RowName.ToString());
		return false;
	}

	FFrontierUpgradeLevelCollection& LevelCollection = UpgradeLevelsByItem.FindOrAdd(SourceRow.ItemTemplateId);
	if (LevelCollection.Levels.ContainsByPredicate([&SourceRow](const FFrontierUpgradeLevelData& ExistingLevel)
		{
			return ExistingLevel.TargetEnhancementLevel == SourceRow.TargetEnhancementLevel;
		}))
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade level row %s duplicates %s +%d."),
			*RowName.ToString(),
			*SourceRow.ItemTemplateId.ToString(),
			SourceRow.TargetEnhancementLevel);
		return false;
	}

	FFrontierUpgradeLevelData& Level = LevelCollection.Levels.AddDefaulted_GetRef();
	Level.TargetEnhancementLevel = SourceRow.TargetEnhancementLevel;
	Level.SuccessProbability = SourceRow.SuccessProbability;
	Level.FirstRandomStatIncrease = SourceRow.FirstRandomStatIncrease;
	Level.SkillLevelIncrease = SourceRow.SkillLevelIncrease;

	if (!SourceRow.RequiredCurrencyCode.IsEmpty())
	{
		FFrontierUpgradeCurrencyRequirement& Currency = Level.Currencies.AddDefaulted_GetRef();
		Currency.CurrencyCode = SourceRow.RequiredCurrencyCode;
		Currency.RequiredAmount = SourceRow.RequiredCurrencyAmount;
	}

	return true;
}

bool UFrontierUpgradeBalanceSubsystem::AddMaterialRow(
	const FName& RowName,
	const FFrontierUpgradeMaterialTableRow& SourceRow)
{
	if (SourceRow.ItemTemplateId.IsNone()
		|| SourceRow.TargetEnhancementLevel <= 0
		|| SourceRow.MaterialItemTemplateId.IsNone()
		|| SourceRow.RequiredAmount <= 0)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade material row %s contains invalid data."), *RowName.ToString());
		return false;
	}

	FFrontierUpgradeLevelData* Level = nullptr;
	if (FFrontierUpgradeLevelCollection* LevelCollection = UpgradeLevelsByItem.Find(SourceRow.ItemTemplateId))
	{
		for (FFrontierUpgradeLevelData& Candidate : LevelCollection->Levels)
		{
			if (Candidate.TargetEnhancementLevel == SourceRow.TargetEnhancementLevel)
			{
				Level = &Candidate;
				break;
			}
		}
	}

	if (!Level)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier upgrade material row %s references an unknown item level: %s +%d."),
			*RowName.ToString(),
			*SourceRow.ItemTemplateId.ToString(),
			SourceRow.TargetEnhancementLevel);
		return false;
	}

	FFrontierUpgradeMaterialRequirement& Material = Level->Materials.AddDefaulted_GetRef();
	Material.ItemTemplateId = SourceRow.MaterialItemTemplateId;
	Material.RequiredAmount = SourceRow.RequiredAmount;
	return true;
}

bool UFrontierUpgradeBalanceSubsystem::AddEquipmentScoreEnhancementRateRow(
	const FName& RowName,
	const FFrontierEquipmentScoreEnhancementRateTableRow& SourceRow)
{
	if (SourceRow.EnhancementLevel < 0
		|| SourceRow.EnhancementLevel > 9
		|| SourceRow.EnhancementRate < 0.0f)
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier equipment score enhancement row %s contains invalid data."), *RowName.ToString());
		return false;
	}

	if (EquipmentScoreEnhancementRates.Contains(SourceRow.EnhancementLevel))
	{
		UE_LOG(LogTemp, Error, TEXT("Frontier equipment score enhancement row %s duplicates enhancement level %d."),
			*RowName.ToString(),
			SourceRow.EnhancementLevel);
		return false;
	}

	EquipmentScoreEnhancementRates.Add(SourceRow.EnhancementLevel, SourceRow.EnhancementRate);
	return true;
}
