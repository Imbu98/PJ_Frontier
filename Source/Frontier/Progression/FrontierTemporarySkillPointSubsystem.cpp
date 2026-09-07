#include "Progression/FrontierTemporarySkillPointSubsystem.h"

#include "Engine/GameInstance.h"
#include "Frontier.h"
#include "Save/FrontierLocalProfileSubsystem.h"
#include "Save/FrontierProfileSaveGame.h"

namespace
{
constexpr TCHAR FrontierProfileSaveSlotName[] = TEXT("FrontierLocalProfile");
constexpr int32 FrontierProfileSaveUserIndex = 0;

bool IsOlderLevelSnapshot(const FString& IncomingUpdatedAt, const FString& AcceptedUpdatedAt)
{
	if (IncomingUpdatedAt.IsEmpty() || AcceptedUpdatedAt.IsEmpty())
	{
		return false;
	}

	FDateTime IncomingTime;
	FDateTime AcceptedTime;
	return FDateTime::ParseIso8601(*IncomingUpdatedAt, IncomingTime)
		&& FDateTime::ParseIso8601(*AcceptedUpdatedAt, AcceptedTime)
		&& IncomingTime < AcceptedTime;
}
}

FString UFrontierTemporarySkillPointSubsystem::BuildProfileSlotName(const UWorld* World)
{
	FString SlotName = FrontierProfileSaveSlotName;
	if (!World)
	{
		return SlotName;
	}

	const FString WorldPackageName = World->GetOutermost()->GetName();
	const int32 PieIndex = WorldPackageName.Find(TEXT("UEDPIE_"), ESearchCase::IgnoreCase);
	if (PieIndex != INDEX_NONE)
	{
		TArray<FString> Parts;
		WorldPackageName.Mid(PieIndex).ParseIntoArray(Parts, TEXT("_"), true);
		if (Parts.Num() >= 2)
		{
			SlotName += FString::Printf(TEXT("_PIE_%s"), *Parts[1]);
		}
	}
	return SlotName;
}

FFrontierTemporarySkillPointResult UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(
	FFrontierLocalAccountProgression& InOutAccount,
	const FFrontierPlayerLevelSnapshot& Level)
{
	FFrontierTemporarySkillPointResult Result;
	Result.CurrentLevel = Level.Level;
	Result.TemporarySkillPoints = InOutAccount.TemporarySkillPoints;
	Result.TotalSkillPoints = FMath::Max(0, InOutAccount.SkillTreeProgression.TotalSkillPointsEarned);
	Result.SpentSkillPoints = FMath::Max(0, InOutAccount.CachedSpentSkillPoints);
	if (InOutAccount.AccountId.IsEmpty() || Level.Level <= 0)
	{
		Result.ErrorMessage = TEXT("Invalid account identity or level snapshot.");
		return Result;
	}

	const int32 PreviousLevel = InOutAccount.LastVerifiedBackendLevel > 0
		? InOutAccount.LastVerifiedBackendLevel
		: (InOutAccount.bHasInitializedLevel ? FMath::Max(0, InOutAccount.LastProcessedLevel) : 0);
	Result.PreviousLevel = PreviousLevel;
	Result.bWasFirstLevelQuery = PreviousLevel <= 0;
	if (IsOlderLevelSnapshot(Level.UpdatedAt, InOutAccount.LastVerifiedLevelUpdatedAt))
	{
		Result.bSucceeded = true;
		Result.bWasStaleLevelSnapshot = true;
		return Result;
	}

	const int32 PreviousAvailablePoints = FMath::Max(0, InOutAccount.TemporarySkillPoints);
	const int32 PreviousTotalPoints = FMath::Max(0, InOutAccount.SkillTreeProgression.TotalSkillPointsEarned);
	const int32 PreviousCachedSpentPoints = FMath::Max(0, InOutAccount.CachedSpentSkillPoints);
	const FString PreviousUpdatedAt = InOutAccount.LastVerifiedLevelUpdatedAt;
	const int32 SnapshotDerivedSpent = FMath::Max(
		0,
		InOutAccount.SkillTreeProgression.TotalSkillPointsEarned
			- InOutAccount.SkillTreeProgression.AvailableSkillPoints);
	const int32 CachedSpentPoints = FMath::Max(InOutAccount.CachedSpentSkillPoints, SnapshotDerivedSpent);
	const int32 TotalPointBudget = CalculateTotalSkillPointsForLevel(Level.Level);
	const int32 ReconciledAvailablePoints = FMath::Max(0, TotalPointBudget - CachedSpentPoints);
	const FString AcceptedUpdatedAt = Level.UpdatedAt.IsEmpty() ? PreviousUpdatedAt : Level.UpdatedAt;

	Result.LevelDelta = Level.Level - PreviousLevel;
	Result.GrantedTemporarySkillPoints = FMath::Max(0, ReconciledAvailablePoints - PreviousAvailablePoints);
	Result.TemporarySkillPoints = ReconciledAvailablePoints;
	Result.TotalSkillPoints = TotalPointBudget;
	Result.SpentSkillPoints = CachedSpentPoints;

	InOutAccount.bHasInitializedLevel = true;
	InOutAccount.LastProcessedLevel = Level.Level;
	InOutAccount.LastVerifiedBackendLevel = Level.Level;
	InOutAccount.LastVerifiedLevelUpdatedAt = AcceptedUpdatedAt;
	InOutAccount.TemporarySkillPoints = ReconciledAvailablePoints;
	InOutAccount.CachedSpentSkillPoints = CachedSpentPoints;
	InOutAccount.SkillTreeProgression.AvailableSkillPoints = ReconciledAvailablePoints;
	InOutAccount.SkillTreeProgression.TotalSkillPointsEarned = TotalPointBudget;

	Result.bProgressionChanged = PreviousLevel != Level.Level
		|| PreviousAvailablePoints != ReconciledAvailablePoints
		|| PreviousTotalPoints != TotalPointBudget
		|| PreviousCachedSpentPoints != CachedSpentPoints
		|| PreviousUpdatedAt != AcceptedUpdatedAt;

	Result.bSucceeded = true;
	return Result;
}

int32 UFrontierTemporarySkillPointSubsystem::CalculateTotalSkillPointsForLevel(const int32 BackendLevel)
{
	return FMath::Max(0, BackendLevel);
}

void UFrontierTemporarySkillPointSubsystem::ProcessLevelSnapshot(
	const FString& AccountId,
	const FFrontierPlayerLevelSnapshot& Level,
	FProcessCompletion Completion)
{
	FFrontierTemporarySkillPointResult Failure;
	Failure.CurrentLevel = Level.Level;
	if (AccountId.IsEmpty())
	{
		Failure.ErrorMessage = TEXT("A verified account identity is required for local progression.");
		if (Completion)
		{
			Completion(Failure);
		}
		return;
	}
	if (AccountsWithPendingSave.Contains(AccountId))
	{
		Failure.ErrorMessage = TEXT("A local progression save is already pending for this account.");
		if (Completion)
		{
			Completion(Failure);
		}
		return;
	}

	UFrontierLocalProfileSubsystem* ProfileSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierLocalProfileSubsystem>()
		: nullptr;
	const FString SlotName = BuildProfileSlotName(GetWorld());
	UFrontierProfileSaveGame* Profile = ProfileSubsystem
		? ProfileSubsystem->LoadOrCreateProfile(SlotName, FrontierProfileSaveUserIndex)
		: nullptr;
	if (!ProfileSubsystem || !Profile)
	{
		Failure.ErrorMessage = TEXT("The local profile could not be loaded.");
		if (Completion)
		{
			Completion(Failure);
		}
		return;
	}

	FFrontierLocalAccountProgression& Account = Profile->FindOrAddAccountProgression(AccountId);
	const FFrontierLocalAccountProgression PreviousAccount = Account;
	FFrontierTemporarySkillPointResult Result = EvaluateLevelSnapshot(Account, Level);
	if (!Result.bSucceeded)
	{
		Account = PreviousAccount;
		if (Completion)
		{
			Completion(Result);
		}
		return;
	}
	if (!Result.bProgressionChanged)
	{
		ActiveAccountId = AccountId;
		OnTemporarySkillPointsUpdated.Broadcast(Level, Result);
		if (Completion)
		{
			Completion(Result);
		}
		return;
	}

	AccountsWithPendingSave.Add(AccountId);
	const TWeakObjectPtr<UFrontierTemporarySkillPointSubsystem> WeakThis(this);
	const TWeakObjectPtr<UFrontierProfileSaveGame> WeakProfile(Profile);
	const bool bQueued = ProfileSubsystem->QueueProfileSave(
		Profile,
		SlotName,
		FrontierProfileSaveUserIndex,
		[WeakThis, WeakProfile, AccountId, PreviousAccount, Level, Result, Completion](const bool bSaved) mutable
		{
			UFrontierTemporarySkillPointSubsystem* This = WeakThis.Get();
			UFrontierProfileSaveGame* SavedProfile = WeakProfile.Get();
			if (This)
			{
				This->AccountsWithPendingSave.Remove(AccountId);
			}

			FFrontierTemporarySkillPointResult CompletedResult = Result;
			if (!bSaved)
			{
				CompletedResult.bSucceeded = false;
				CompletedResult.ErrorMessage = TEXT("Local progression could not be committed to disk.");
				if (SavedProfile)
				{
					if (FFrontierLocalAccountProgression* Current = SavedProfile->FindAccountProgression(AccountId))
					{
						// No other mutation is allowed while this account save is pending, so rollback is safe.
						*Current = PreviousAccount;
					}
				}
			}
			else if (This)
			{
				This->ActiveAccountId = AccountId;
				This->OnTemporarySkillPointsUpdated.Broadcast(Level, CompletedResult);
			}

			if (Completion)
			{
				Completion(CompletedResult);
			}
		});

	if (!bQueued)
	{
		AccountsWithPendingSave.Remove(AccountId);
		Account = PreviousAccount;
		Result.bSucceeded = false;
		Result.ErrorMessage = TEXT("Local progression save was rejected.");
		if (Completion)
		{
			Completion(Result);
		}
	}
}

int32 UFrontierTemporarySkillPointSubsystem::GetActiveTemporarySkillPoints() const
{
	if (ActiveAccountId.IsEmpty())
	{
		return 0;
	}

	UFrontierLocalProfileSubsystem* ProfileSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierLocalProfileSubsystem>()
		: nullptr;
	UFrontierProfileSaveGame* Profile = ProfileSubsystem
		? ProfileSubsystem->LoadOrCreateProfile(BuildProfileSlotName(GetWorld()), FrontierProfileSaveUserIndex)
		: nullptr;
	const FFrontierLocalAccountProgression* Account = Profile
		? Profile->FindAccountProgression(ActiveAccountId)
		: nullptr;
	return Account ? Account->TemporarySkillPoints : 0;
}
