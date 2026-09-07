#include "Skill/FrontierSkillDataSubsystem.h"

#include "Abilities/GameplayAbility.h"
#include "Engine/DataTable.h"
#include "Frontier.h"
#include "Skill/FrontierSkillSettings.h"

namespace
{
void ApplyModifiers(const TArray<FFrontierSkillModifier>& Modifiers, TMap<FGameplayTag, float>& InOutParameters)
{
	for (const FFrontierSkillModifier& Modifier : Modifiers)
	{
		if (!Modifier.ParameterTag.IsValid())
		{
			continue;
		}

		float* ExistingValue = InOutParameters.Find(Modifier.ParameterTag);
		if (!ExistingValue)
		{
			const float InitialValue = Modifier.Operation == EFrontierSkillModifierOperation::Multiply ? 1.0f : 0.0f;
			ExistingValue = &InOutParameters.Add(Modifier.ParameterTag, InitialValue);
		}

		switch (Modifier.Operation)
		{
		case EFrontierSkillModifierOperation::Override:
			*ExistingValue = Modifier.Value;
			break;
		case EFrontierSkillModifierOperation::Add:
			*ExistingValue += Modifier.Value;
			break;
		case EFrontierSkillModifierOperation::Multiply:
			*ExistingValue *= Modifier.Value;
			break;
		default:
			break;
		}
	}
}
}

void UFrontierSkillDataSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RebuildSkillDataMap();
}

void UFrontierSkillDataSubsystem::RebuildSkillDataMap()
{
	SkillDataMap.Reset();
	SkillDataByIdMap.Reset();
	SkillIdByTagMap.Reset();
	SkillIdByAbilityClassMap.Reset();
	SkillBalanceByIdMap.Reset();
	LoadedSkillBalanceDataTable = nullptr;
	const UFrontierSkillSettings* Settings = GetDefault<UFrontierSkillSettings>();
	LoadedSkillDataTable = Settings ? Settings->SkillDataTable.LoadSynchronous() : nullptr;
	if (!LoadedSkillDataTable)
	{
		FRONTIER_LOG(Error, TEXT("SkillDataTable is not configured in Project Settings > Game > Frontier Skill Data."));
		return;
	}

	if (LoadedSkillDataTable->GetRowStruct() != FFrontierSkillTableRow::StaticStruct())
	{
		FRONTIER_LOG(Error, TEXT("Skill DataTable row struct mismatch. Table=%s Expected=%s Actual=%s"),
			*GetNameSafe(LoadedSkillDataTable),
			*GetNameSafe(FFrontierSkillTableRow::StaticStruct()),
			*GetNameSafe(LoadedSkillDataTable->GetRowStruct()));
		return;
	}

	for (const TPair<FName, uint8*>& RowPair : LoadedSkillDataTable->GetRowMap())
	{
		const FName RowName = RowPair.Key;
		const FFrontierSkillTableRow* Row = reinterpret_cast<const FFrontierSkillTableRow*>(RowPair.Value);
		if (!Row || !Row->SkillTag.IsValid())
		{
			FRONTIER_LOG(Error, TEXT("Skill DataTable has row with invalid SkillTag. Table=%s RowName=%s"),
				*GetNameSafe(LoadedSkillDataTable),
				*RowName.ToString());
			continue;
		}

		if (RowName.IsNone())
		{
			FRONTIER_LOG(Error, TEXT("Skill DataTable has row with empty RowName. Table=%s SkillTag=%s"),
				*GetNameSafe(LoadedSkillDataTable),
				*Row->SkillTag.ToString());
			continue;
		}
		if (SkillDataByIdMap.Contains(RowName))
		{
			FRONTIER_LOG(Error, TEXT("Skill DataTable contains duplicate SkillId. Table=%s SkillId=%s SkillTag=%s"),
				*GetNameSafe(LoadedSkillDataTable),
				*RowName.ToString(),
				*Row->SkillTag.ToString());
			continue;
		}
		SkillDataByIdMap.Add(RowName, Row);

		if (SkillDataMap.Contains(Row->SkillTag))
		{
			FRONTIER_LOG(Error, TEXT("Skill DataTable contains duplicate SkillTag. Table=%s SkillTag=%s"),
				*GetNameSafe(LoadedSkillDataTable),
				*Row->SkillTag.ToString());
			continue;
		}

		SkillDataMap.Add(Row->SkillTag, Row);
		SkillIdByTagMap.Add(Row->SkillTag, RowName);

		if (Row->AbilityClass)
		{
			if (const FName* ExistingSkillId = SkillIdByAbilityClassMap.Find(Row->AbilityClass))
			{
				FRONTIER_LOG(Error, TEXT("Skill DataTable maps one AbilityClass to multiple SkillIds. AbilityClass=%s ExistingSkillId=%s DuplicateSkillId=%s"),
					*GetNameSafe(Row->AbilityClass),
					*ExistingSkillId->ToString(),
					*RowName.ToString());
			}
			else
			{
				SkillIdByAbilityClassMap.Add(Row->AbilityClass, RowName);
			}
		}
	}

	LoadedSkillBalanceDataTable = Settings ? Settings->SkillBalanceDataTable.LoadSynchronous() : nullptr;
	if (!LoadedSkillBalanceDataTable)
	{
		FRONTIER_LOG(Warning, TEXT("SkillBalanceDataTable is not configured in Project Settings > Game > Frontier Skill Data. Ability defaults will be used."));
		return;
	}

	if (LoadedSkillBalanceDataTable->GetRowStruct() != FFrontierSkillBalanceTableRow::StaticStruct())
	{
		FRONTIER_LOG(Error, TEXT("Skill balance DataTable row struct mismatch. Table=%s Expected=%s Actual=%s"),
			*GetNameSafe(LoadedSkillBalanceDataTable),
			*GetNameSafe(FFrontierSkillBalanceTableRow::StaticStruct()),
			*GetNameSafe(LoadedSkillBalanceDataTable->GetRowStruct()));
		return;
	}

	for (const TPair<FName, uint8*>& RowPair : LoadedSkillBalanceDataTable->GetRowMap())
	{
		const FName SkillId = RowPair.Key;
		const FFrontierSkillBalanceTableRow* Row = reinterpret_cast<const FFrontierSkillBalanceTableRow*>(RowPair.Value);
		if (!Row || SkillId.IsNone())
		{
			continue;
		}
		if (!SkillDataByIdMap.Contains(SkillId))
		{
			FRONTIER_LOG(Warning, TEXT("Skill balance row has no matching skill template row. SkillId=%s Table=%s"),
				*SkillId.ToString(),
				*GetNameSafe(LoadedSkillBalanceDataTable));
		}
		SkillBalanceByIdMap.Add(SkillId, Row);
	}
}

const FFrontierSkillTableRow* UFrontierSkillDataSubsystem::FindSkillData(const FGameplayTag& SkillTag) const
{
	if (!SkillTag.IsValid())
	{
		return nullptr;
	}

	const FFrontierSkillTableRow* const* FoundSkillData = SkillDataMap.Find(SkillTag);
	return FoundSkillData ? *FoundSkillData : nullptr;
}

const FFrontierSkillTableRow* UFrontierSkillDataSubsystem::FindSkillDataById(const FName SkillId) const
{
	if (SkillId.IsNone())
	{
		return nullptr;
	}

	const FFrontierSkillTableRow* const* FoundSkillData = SkillDataByIdMap.Find(SkillId);
	return FoundSkillData ? *FoundSkillData : nullptr;
}

const FFrontierSkillTableRow* UFrontierSkillDataSubsystem::FindSkillDataByIdString(const FString& SkillId) const
{
	return SkillId.IsEmpty() ? nullptr : FindSkillDataById(FName(*SkillId));
}

FName UFrontierSkillDataSubsystem::FindSkillId(const FGameplayTag& SkillTag) const
{
	const FName* FoundSkillId = SkillIdByTagMap.Find(SkillTag);
	return FoundSkillId ? *FoundSkillId : NAME_None;
}

FName UFrontierSkillDataSubsystem::FindSkillIdByAbilityClass(const TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const FName* FoundSkillId = AbilityClass ? SkillIdByAbilityClassMap.Find(AbilityClass) : nullptr;
	return FoundSkillId ? *FoundSkillId : NAME_None;
}

bool UFrontierSkillDataSubsystem::ApplySkillBalance(
	const TSubclassOf<UGameplayAbility> AbilityClass,
	const int32 SkillLevel,
	TMap<FGameplayTag, float>& InOutParameters) const
{
	const FName SkillId = FindSkillIdByAbilityClass(AbilityClass);
	const FFrontierSkillBalanceTableRow* const* FoundRow = SkillBalanceByIdMap.Find(SkillId);
	if (!FoundRow || !*FoundRow)
	{
		return false;
	}

	const FFrontierSkillBalanceTableRow& Row = **FoundRow;
	ApplyModifiers(Row.BaseEffects, InOutParameters);
	if (SkillLevel >= 4)
	{
		ApplyModifiers(Row.Level3Effects, InOutParameters);
	}
	if (SkillLevel >= 8)
	{
		ApplyModifiers(Row.Level6Effects, InOutParameters);
	}
	if (SkillLevel >= 12)
	{
		ApplyModifiers(Row.Level9Effects, InOutParameters);
	}
	return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UFrontierSkillDataSubsystem::SetSkillDataTableForTesting(UDataTable* InSkillDataTable)
{
	LoadedSkillDataTable = InSkillDataTable;
	SkillDataMap.Reset();
	SkillDataByIdMap.Reset();
	SkillIdByTagMap.Reset();
	SkillIdByAbilityClassMap.Reset();

	if (!LoadedSkillDataTable)
	{
		return;
	}

	for (const TPair<FName, uint8*>& RowPair : LoadedSkillDataTable->GetRowMap())
	{
		const FName RowName = RowPair.Key;
		const FFrontierSkillTableRow* Row = reinterpret_cast<const FFrontierSkillTableRow*>(RowPair.Value);
		if (Row && Row->SkillTag.IsValid() && !SkillDataMap.Contains(Row->SkillTag))
		{
			SkillDataMap.Add(Row->SkillTag, Row);
			SkillIdByTagMap.Add(Row->SkillTag, RowName);
		}
		if (Row && !RowName.IsNone())
		{
			if (!SkillDataByIdMap.Contains(RowName))
			{
				SkillDataByIdMap.Add(RowName, Row);
			}
		}
		if (Row && Row->AbilityClass && !SkillIdByAbilityClassMap.Contains(Row->AbilityClass))
		{
			SkillIdByAbilityClassMap.Add(Row->AbilityClass, RowName);
		}
	}
}
#endif
