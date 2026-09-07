#include "Save/FrontierProfileSaveGame.h"

void UFrontierProfileSaveGame::InitializeNewProfile()
{
	SaveVersion = CurrentSaveVersion;
}

FFrontierLocalAccountProgression* UFrontierProfileSaveGame::FindAccountProgression(const FString& AccountId)
{
	return AccountProgression.FindByPredicate(
		[&AccountId](const FFrontierLocalAccountProgression& Account)
		{
			return Account.AccountId.Equals(AccountId, ESearchCase::CaseSensitive);
		});
}

const FFrontierLocalAccountProgression* UFrontierProfileSaveGame::FindAccountProgression(const FString& AccountId) const
{
	return AccountProgression.FindByPredicate(
		[&AccountId](const FFrontierLocalAccountProgression& Account)
		{
			return Account.AccountId.Equals(AccountId, ESearchCase::CaseSensitive);
		});
}

FFrontierLocalAccountProgression& UFrontierProfileSaveGame::FindOrAddAccountProgression(const FString& AccountId)
{
	if (FFrontierLocalAccountProgression* Existing = FindAccountProgression(AccountId))
	{
		return *Existing;
	}

	FFrontierLocalAccountProgression& NewAccount = AccountProgression.AddDefaulted_GetRef();
	NewAccount.AccountId = AccountId;
	if (!bLegacySkillTreeProgressionClaimed)
	{
		NewAccount.SkillTreeProgression = SkillTreeProgression;
		NewAccount.TemporarySkillPoints = SkillTreeProgression.AvailableSkillPoints;
		bLegacySkillTreeProgressionClaimed = true;
	}
	return NewAccount;
}

bool UFrontierProfileSaveGame::MigrateToLatest(bool& bOutWasMigrated, FString& OutError)
{
	bOutWasMigrated = false;
	OutError.Reset();

	if (SaveVersion < 0)
	{
		OutError = FString::Printf(TEXT("Invalid negative profile save version: %d"), SaveVersion);
		return false;
	}
	if (SaveVersion > CurrentSaveVersion)
	{
		OutError = FString::Printf(
			TEXT("Profile save version %d is newer than the supported version %d."),
			SaveVersion,
			CurrentSaveVersion);
		return false;
	}

	if (SaveVersion == 0)
	{
		// Version 0 is the original unversioned layout. Its serialized fields are already compatible.
		SaveVersion = 1;
		bOutWasMigrated = true;
	}

	if (SaveVersion == 1)
	{
		// Version 2 adds permanent passive skill-tree progression. Legacy profiles start with an empty tree.
		SkillTreeProgression = FFrontierSkillTreeProgressionSnapshot();
		SaveVersion = 2;
		bOutWasMigrated = true;
	}

	if (SaveVersion == 2)
	{
		// Version 3 isolates temporary level and skill-tree state by verified account identity.
		// The legacy snapshot is claimed by the first account only, preventing cross-account reuse.
		AccountProgression.Reset();
		bLegacySkillTreeProgressionClaimed = false;
		SaveVersion = 3;
		bOutWasMigrated = true;
	}

	if (SaveVersion == 3)
	{
		// Version 4 replaces incremental local payouts with idempotent Backend-level reconciliation.
		// Preserve node allocation, but treat all numeric balances as caches until the next level GET.
		for (FFrontierLocalAccountProgression& Account : AccountProgression)
		{
			Account.LastVerifiedBackendLevel = Account.bHasInitializedLevel
				? FMath::Max(0, Account.LastProcessedLevel)
				: 0;
			Account.LastVerifiedLevelUpdatedAt.Reset();
			Account.CachedSpentSkillPoints = FMath::Max(
				0,
				Account.SkillTreeProgression.TotalSkillPointsEarned
					- Account.SkillTreeProgression.AvailableSkillPoints);
		}
		SaveVersion = 4;
		bOutWasMigrated = true;
	}

	return SaveVersion == CurrentSaveVersion;
}
