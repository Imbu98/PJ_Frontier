#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "FrontierOnlineHttpClient.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Engine/GameInstance.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Progression/FrontierInternalApiConfig.h"
#include "Progression/FrontierInternalExperienceGrantService.h"
#include "Progression/FrontierRaidExperienceSettings.h"
#include "Progression/FrontierRaidExperienceSubsystem.h"
#include "Progression/FrontierRaidSessionSubsystem.h"
#include "Progression/FrontierTemporarySkillPointSubsystem.h"
#include "Save/FrontierProfileSaveGame.h"
#include "SkillTree/FrontierSkillTreeDataAsset.h"
#include "Tags/FrontierGameplayTags.h"

#ifdef GetEnvironmentVariable
#undef GetEnvironmentVariable
#endif

namespace
{
FFrontierPlayerLevelSnapshot MakeLevel(const int32 Level, const TCHAR* UpdatedAt = TEXT(""))
{
	FFrontierPlayerLevelSnapshot Snapshot;
	Snapshot.Level = Level;
	Snapshot.MaxLevel = 50;
	Snapshot.UpdatedAt = UpdatedAt;
	return Snapshot;
}

FFrontierAssistCandidate MakeCandidate(
	const TCHAR* PlayerId,
	const int32 TeamId,
	const double Damage,
	const double LastDamageTime)
{
	FFrontierAssistCandidate Candidate;
	Candidate.PlayerId = PlayerId;
	Candidate.TeamId = TeamId;
	Candidate.AccumulatedValidDamage = Damage;
	Candidate.LastDamageWorldSeconds = LastDamageTime;
	return Candidate;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierTemporaryLevelProgressionTest,
	"Frontier.Progression.Level.AccountScopedReconciliation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierTemporaryLevelProgressionTest::RunTest(const FString& Parameters)
{
	FFrontierLocalAccountProgression FirstAtOne;
	FirstAtOne.AccountId = TEXT("player-one");
	FFrontierTemporarySkillPointResult Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(
		FirstAtOne,
		MakeLevel(1, TEXT("2026-07-22T00:01:00Z")));
	TestTrue(TEXT("First Level 1 query succeeds"), Result.bSucceeded);
	TestTrue(TEXT("First Level 1 query is initialization"), Result.bWasFirstLevelQuery);
	TestEqual(TEXT("Level 1 owns one total point"), Result.TotalSkillPoints, 1);
	TestEqual(TEXT("Level 1 has one available point"), FirstAtOne.TemporarySkillPoints, 1);

	FFrontierLocalAccountProgression FirstAtFive;
	FirstAtFive.AccountId = TEXT("player-five");
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(FirstAtFive, MakeLevel(5));
	TestEqual(TEXT("A fresh Level 5 profile immediately owns five points"), FirstAtFive.TemporarySkillPoints, 5);

	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(FirstAtOne, MakeLevel(2));
	TestEqual(TEXT("Level 1 to 2 reconciles one additional point"), Result.GrantedTemporarySkillPoints, 1);
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(FirstAtOne, MakeLevel(5));
	TestEqual(TEXT("Level 2 to 5 reconciles three additional points"), Result.GrantedTemporarySkillPoints, 3);
	TestEqual(TEXT("Level 5 has five available points when nothing is spent"), FirstAtOne.TemporarySkillPoints, 5);

	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(FirstAtOne, MakeLevel(5));
	TestEqual(TEXT("Same level never duplicates points"), Result.GrantedTemporarySkillPoints, 0);
	TestFalse(TEXT("Identical level data does not require another local save"), Result.bProgressionChanged);
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(
		FirstAtOne,
		MakeLevel(3, TEXT("2026-07-22T00:06:00Z")));
	TestEqual(TEXT("A latest authoritative lower level is reconciled"), FirstAtOne.TemporarySkillPoints, 3);
	TestEqual(TEXT("Verified level cache follows the latest response"), FirstAtOne.LastVerifiedBackendLevel, 3);
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(
		FirstAtOne,
		MakeLevel(9, TEXT("2026-07-22T00:05:00Z")));
	TestTrue(TEXT("An older out-of-order response is identified"), Result.bWasStaleLevelSnapshot);
	TestEqual(TEXT("An older response cannot replace the accepted level"), FirstAtOne.LastVerifiedBackendLevel, 3);

	FFrontierLocalAccountProgression OtherAccount;
	OtherAccount.AccountId = TEXT("other-player");
	UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(OtherAccount, MakeLevel(9));
	TestEqual(TEXT("Other account is reconciled independently"), OtherAccount.TemporarySkillPoints, 9);
	TestEqual(TEXT("First account remains isolated"), FirstAtOne.TemporarySkillPoints, 3);

	FFrontierLocalAccountProgression InvestedAccount;
	InvestedAccount.AccountId = TEXT("invested-player");
	InvestedAccount.SkillTreeProgression.TotalSkillPointsEarned = 10;
	InvestedAccount.SkillTreeProgression.AvailableSkillPoints = 4;
	InvestedAccount.TemporarySkillPoints = 4;
	InvestedAccount.CachedSpentSkillPoints = 6;
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(InvestedAccount, MakeLevel(12));
	TestEqual(TEXT("Cached spent points are retained for the local preview"), Result.SpentSkillPoints, 6);
	TestEqual(TEXT("Available plus spent equals Backend level"), InvestedAccount.TemporarySkillPoints + Result.SpentSkillPoints, 12);

	const FFrontierLocalAccountProgression BeforeFailure = FirstAtOne;
	Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(FirstAtOne, MakeLevel(0));
	TestFalse(TEXT("Invalid/failed level is rejected"), Result.bSucceeded);
	TestEqual(TEXT("Failed level leaves watermark unchanged"), FirstAtOne.LastProcessedLevel, BeforeFailure.LastProcessedLevel);
	TestEqual(TEXT("Failed level leaves points unchanged"), FirstAtOne.TemporarySkillPoints, BeforeFailure.TemporarySkillPoints);

	UFrontierProfileSaveGame* ProfileToPersist = NewObject<UFrontierProfileSaveGame>();
	ProfileToPersist->InitializeNewProfile();
	ProfileToPersist->AccountProgression.Add(FirstAtOne);
	TArray<uint8> SerializedProfile;
	TestTrue(TEXT("Account progression serializes to SaveGame data"), UGameplayStatics::SaveGameToMemory(ProfileToPersist, SerializedProfile));
	UFrontierProfileSaveGame* ReloadedProfile = Cast<UFrontierProfileSaveGame>(UGameplayStatics::LoadGameFromMemory(SerializedProfile));
	TestNotNull(TEXT("Account progression reloads after a simulated restart"), ReloadedProfile);
	if (ReloadedProfile && ReloadedProfile->AccountProgression.Num() == 1)
	{
		Result = UFrontierTemporarySkillPointSubsystem::EvaluateLevelSnapshot(
			ReloadedProfile->AccountProgression[0],
			MakeLevel(3, TEXT("2026-07-22T00:06:00Z")));
		TestEqual(TEXT("Reloaded profile reconciles the same level idempotently"), Result.GrantedTemporarySkillPoints, 0);
		TestFalse(TEXT("Reloaded identical snapshot does not require a save"), Result.bProgressionChanged);
	}

	FFrontierOnlinePlayerLevelResponse ParsedLevel;
	FString ParseError;
	const FString LevelJson = TEXT("{\"success\":true,\"data\":{\"level\":12,\"totalExperience\":9000,\"currentLevelExperience\":250,\"nextLevelRequiredExperience\":null,\"maxLevel\":12,\"updatedAt\":\"2026-07-22T00:00:00Z\"},\"meta\":{\"requestId\":\"level-request\",\"serverTime\":\"2026-07-22T00:00:00Z\"}}");
	TestTrue(TEXT("Documented PlayerLevelDTO parses"), FFrontierOnlineHttpClient::ParsePlayerLevelResponse(200, LevelJson, ParsedLevel, ParseError));
	TestFalse(TEXT("Nullable next-level XP is preserved"), ParsedLevel.Data.bHasNextLevelRequiredExperience);
	const int32 WatermarkBeforeApiFailure = FirstAtOne.LastVerifiedBackendLevel;
	TestFalse(TEXT("Failed Level API response does not parse as progression"), FFrontierOnlineHttpClient::ParsePlayerLevelResponse(500, TEXT("{\"error\":{\"message\":\"failed\"}}"), ParsedLevel, ParseError));
	TestEqual(TEXT("Failed Level API response never changes verified level"), FirstAtOne.LastVerifiedBackendLevel, WatermarkBeforeApiFailure);

	const UFrontierSkillTreeDataAsset* SkillTreeData = GetDefault<UFrontierSkillTreeDataAsset>();
	FFrontierSkillTreeProgressionSnapshot UntrustedSnapshot;
	UntrustedSnapshot.AvailableSkillPoints = 999;
	UntrustedSnapshot.TotalSkillPointsEarned = 999;
	UntrustedSnapshot.Nodes.Add({FFrontierGameplayTags::Get().SkillTreeNodeCommonAttack, 3});
	FFrontierSkillTreeProgressionSnapshot ReconciledSnapshot;
	bool bAllocationReset = false;
	TestTrue(
		TEXT("Server reconciliation accepts a valid allocation"),
		UFrontierSkillTreeComponent::ReconcilePersistentSnapshotToPointBudget(
			SkillTreeData,
			UntrustedSnapshot,
			10,
			ReconciledSnapshot,
			bAllocationReset));
	TestFalse(TEXT("Valid allocation is not reset"), bAllocationReset);
	TestEqual(TEXT("Rank costs are derived from DA instead of client totals"), ReconciledSnapshot.AvailableSkillPoints, 6);
	TestEqual(TEXT("Verified Backend budget replaces the untrusted total"), ReconciledSnapshot.TotalSkillPointsEarned, 10);
	TestEqual(TEXT("Valid allocation remains"), ReconciledSnapshot.Nodes.Num(), 1);

	TestTrue(
		TEXT("Overspent allocation is still reconciled safely"),
		UFrontierSkillTreeComponent::ReconcilePersistentSnapshotToPointBudget(
			SkillTreeData,
			UntrustedSnapshot,
			3,
			ReconciledSnapshot,
			bAllocationReset));
	TestTrue(TEXT("Overspent allocation is reset because no purchase order exists"), bAllocationReset);
	TestTrue(TEXT("Reset removes invested nodes"), ReconciledSnapshot.Nodes.IsEmpty());
	TestEqual(TEXT("Reset preserves the entire verified budget"), ReconciledSnapshot.AvailableSkillPoints, 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidExperiencePolicyTest,
	"Frontier.Progression.RaidExperience.PolicyAndDeduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierRaidExperiencePolicyTest::RunTest(const FString& Parameters)
{
	const UFrontierRaidExperienceSettings* Settings = GetDefault<UFrontierRaidExperienceSettings>();
	FString ValidationError;
	TestTrue(TEXT("Default raid XP settings validate"), Settings->Validate(ValidationError));
	TestEqual(TEXT("Normal monster default"), Settings->NormalMonsterKillExperience, 40);
	TestEqual(TEXT("Boss default"), Settings->BossMonsterKillExperience, 600);
	TestEqual(TEXT("Player kill default"), Settings->PlayerKillExperience, 300);
	TestEqual(TEXT("Team wipe default"), Settings->TeamWipeExperience, 250);
	TestEqual(TEXT("Chest default"), Settings->ChestSearchExperience, 15);

	FFrontierRaidPlayerExperienceState State;
	State.Initialize(TEXT("player"), TEXT("raid"), 0.0);
	TestTrue(TEXT("Normal kill is awarded"), State.TryAward(TEXT("normal"), EFrontierRaidExperienceCategory::NormalMonsterKill, 40));
	TestTrue(TEXT("Boss kill is awarded"), State.TryAward(TEXT("boss"), EFrontierRaidExperienceCategory::BossMonsterKill, 600));
	TestFalse(TEXT("Boss event cannot also award normal kill"), State.TryAward(TEXT("boss"), EFrontierRaidExperienceCategory::NormalMonsterKill, 40));
	TestTrue(TEXT("Player kill is awarded"), State.TryAward(TEXT("player-kill"), EFrontierRaidExperienceCategory::PlayerKill, 300));
	TestTrue(TEXT("Team wipe is awarded"), State.TryAward(TEXT("wipe"), EFrontierRaidExperienceCategory::TeamWipe, 250));
	TestTrue(TEXT("Chest is awarded"), State.TryAward(TEXT("chest"), EFrontierRaidExperienceCategory::ChestSearch, 15));
	TestFalse(TEXT("Duplicate event is rejected"), State.TryAward(TEXT("normal"), EFrontierRaidExperienceCategory::NormalMonsterKill, 40));
	TestEqual(TEXT("Category total is exact"), State.GetTotalTemporaryExperience(), static_cast<int64>(1205));
	State.bRaidResultFinalized = true;
	TestFalse(TEXT("Awards after raid finalization are rejected"), State.TryAward(TEXT("late"), EFrontierRaidExperienceCategory::ChestSearch, 15));

	TSet<FString> DamageEvents;
	TestTrue(TEXT("First damage event is accepted"), UFrontierRaidExperienceSubsystem::TryAcceptEventId(DamageEvents, TEXT("damage-1")));
	TestFalse(TEXT("Duplicate damage event is rejected"), UFrontierRaidExperienceSubsystem::TryAcceptEventId(DamageEvents, TEXT("damage-1")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidAssistPolicyTest,
	"Frontier.Progression.RaidExperience.Assists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierRaidAssistPolicyTest::RunTest(const FString& Parameters)
{
	TArray<FFrontierAssistAward> Awards = UFrontierRaidExperienceSubsystem::DistributeAssistExperience(
		{ MakeCandidate(TEXT("one"), 1, 20.0, 100.0) }, 120);
	TestEqual(TEXT("One assistant receives full pool"), Awards[0].Experience, 120);

	Awards = UFrontierRaidExperienceSubsystem::DistributeAssistExperience(
		{ MakeCandidate(TEXT("one"), 1, 20.0, 100.0), MakeCandidate(TEXT("two"), 1, 20.0, 99.0) }, 120);
	TestEqual(TEXT("First of two receives 60"), Awards[0].Experience, 60);
	TestEqual(TEXT("Second of two receives 60"), Awards[1].Experience, 60);

	TArray<FFrontierAssistCandidate> Many;
	for (int32 Index = 0; Index < 7; ++Index)
	{
		Many.Add(MakeCandidate(*FString::Printf(TEXT("p%02d"), Index), 1, 20.0 + Index, 90.0 + Index));
	}
	Awards = UFrontierRaidExperienceSubsystem::DistributeAssistExperience(Many, 120);
	int32 AwardSum = 0;
	for (const FFrontierAssistAward& Award : Awards)
	{
		AwardSum += Award.Experience;
	}
	TestEqual(TEXT("Many assistants split exactly the full pool"), AwardSum, 120);

	const TArray<FFrontierAssistCandidate> Recorded = {
		MakeCandidate(TEXT("eligible"), 1, 15.0, 100.0),
		MakeCandidate(TEXT("too-old"), 1, 50.0, 84.9),
		MakeCandidate(TEXT("too-small"), 1, 14.9, 100.0),
		MakeCandidate(TEXT("killer"), 1, 50.0, 100.0),
		MakeCandidate(TEXT("victim-team"), 2, 50.0, 100.0)
	};
	const TArray<FFrontierAssistCandidate> Eligible = UFrontierRaidExperienceSubsystem::SelectEligibleAssistCandidates(
		Recorded, TEXT("killer"), 2, 100.0, 100.0, 15.0, 0.15);
	TestEqual(TEXT("Window, threshold, killer, and same-team filters leave one assistant"), Eligible.Num(), 1);
	if (Eligible.Num() == 1)
	{
		TestEqual(TEXT("Correct assistant remains"), Eligible[0].PlayerId, FString(TEXT("eligible")));
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierRaidSettlementAndApiTest,
	"Frontier.Progression.RaidExperience.SettlementAndApiSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierRaidSettlementAndApiTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("4:59 survival is zero"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(299.0, false, 300.0, 25, 4), static_cast<int64>(0));
	TestEqual(TEXT("5:00 survival is 25"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(300.0, false, 300.0, 25, 4), static_cast<int64>(25));
	TestEqual(TEXT("10:00 survival is 50"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(600.0, false, 300.0, 25, 4), static_cast<int64>(50));
	TestEqual(TEXT("13:40 survival is 50"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(820.0, false, 300.0, 25, 4), static_cast<int64>(50));
	TestEqual(TEXT("20:00 survival is 100"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(1200.0, false, 300.0, 25, 4), static_cast<int64>(100));
	TestEqual(TEXT("Survival remains capped after 20 minutes"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(9999.0, false, 300.0, 25, 4), static_cast<int64>(100));
	TestEqual(TEXT("AFK survival is zero"), UFrontierRaidExperienceSubsystem::CalculateSurvivalExperience(1200.0, true, 300.0, 25, 4), static_cast<int64>(0));

	int64 FinalExperience = -1;
	TestTrue(TEXT("Extraction settlement is defined"), UFrontierRaidExperienceSubsystem::CalculateFinalExperience(EFrontierRaidOutcome::Extracted, 1000, 3, FinalExperience));
	TestEqual(TEXT("Extraction keeps all temporary XP"), FinalExperience, static_cast<int64>(1000));
	TestTrue(TEXT("Death settlement is defined"), UFrontierRaidExperienceSubsystem::CalculateFinalExperience(EFrontierRaidOutcome::Dead, 1000, 3, FinalExperience));
	TestEqual(TEXT("Death floors one third"), FinalExperience, static_cast<int64>(333));
	UFrontierRaidExperienceSubsystem::CalculateFinalExperience(EFrontierRaidOutcome::Dead, 2, 3, FinalExperience);
	TestEqual(TEXT("Two XP on death grants zero, never a negative request"), FinalExperience, static_cast<int64>(0));
	TestFalse(TEXT("Timed-out policy is intentionally undefined"), UFrontierRaidExperienceSubsystem::CalculateFinalExperience(EFrontierRaidOutcome::TimedOut, 1000, 3, FinalExperience));

	FFrontierExperienceGrantRequest Request;
	Request.PlayerId = TEXT("0190f7e4-b088-7608-a30d-0fcb9ceff8fd");
	Request.SourceId = TEXT("stable-source-id");
	Request.Amount = 333;
	Request.OccurredAt = TEXT("2026-07-22T00:00:00Z");
	FString FreezeError;
	TestTrue(TEXT("Positive immutable grant body is created"), UFrontierInternalExperienceGrantService::FreezeRequestBody(Request, FreezeError));
	const FString FrozenBody = Request.Body;
	const FString FrozenSource = Request.SourceId;
	Request.Amount = 9999;
	TestTrue(TEXT("Already frozen request remains frozen"), UFrontierInternalExperienceGrantService::FreezeRequestBody(Request, FreezeError));
	TestEqual(TEXT("Retry body is unchanged"), Request.Body, FrozenBody);
	TestEqual(TEXT("Retry source ID is unchanged"), Request.SourceId, FrozenSource);
	TestTrue(TEXT("Transport failure is retryable"), UFrontierInternalExperienceGrantService::IsRetryableFailure(0, false));
	TestTrue(TEXT("429 is retryable"), UFrontierInternalExperienceGrantService::IsRetryableFailure(429, true));
	TestTrue(TEXT("503 is retryable"), UFrontierInternalExperienceGrantService::IsRetryableFailure(503, true));
	TestFalse(TEXT("400 is permanent"), UFrontierInternalExperienceGrantService::IsRetryableFailure(400, true));

	const FString PreviousBaseUrl = FPlatformMisc::GetEnvironmentVariable(TEXT("FRONTIER_INTERNAL_API_BASE_URL"));
	const FString PreviousCertificatePath = FPlatformMisc::GetEnvironmentVariable(TEXT("FRONTIER_MTLS_CERT_PATH"));
	const FString PreviousPrivateKeyPath = FPlatformMisc::GetEnvironmentVariable(TEXT("FRONTIER_MTLS_KEY_PATH"));
	const FString PreviousCaPath = FPlatformMisc::GetEnvironmentVariable(TEXT("FRONTIER_MTLS_CA_PATH"));
	const FFrontierInternalApiConfig Config = FFrontierInternalApiConfig::Load();
	TestEqual(
		TEXT("Configured certificate path loads"),
		Config.MtlsCertificatePath,
		PreviousCertificatePath.IsEmpty() ? FString(TEXT("C:/mtls/client.crt")) : PreviousCertificatePath);
	TestEqual(
		TEXT("Configured private-key path loads without key contents"),
		Config.MtlsPrivateKeyPath,
		PreviousPrivateKeyPath.IsEmpty() ? FString(TEXT("C:/mtls/client.key")) : PreviousPrivateKeyPath);
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_INTERNAL_API_BASE_URL"), TEXT("https://env.internal.frontier.test"));
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_CERT_PATH"), TEXT("/run/frontier/client.crt"));
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_KEY_PATH"), TEXT("/run/frontier/client.key"));
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_CA_PATH"), TEXT("/run/frontier/ca.crt"));
	const FFrontierInternalApiConfig EnvironmentConfig = FFrontierInternalApiConfig::Load();
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_INTERNAL_API_BASE_URL"), *PreviousBaseUrl);
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_CERT_PATH"), *PreviousCertificatePath);
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_KEY_PATH"), *PreviousPrivateKeyPath);
	FPlatformMisc::SetEnvironmentVar(TEXT("FRONTIER_MTLS_CA_PATH"), *PreviousCaPath);
	TestEqual(TEXT("Internal API base URL can be injected by server environment"), EnvironmentConfig.InternalApiBaseUrl, FString(TEXT("https://env.internal.frontier.test")));
	TestEqual(TEXT("Certificate default can be overridden"), EnvironmentConfig.MtlsCertificatePath, FString(TEXT("/run/frontier/client.crt")));
	TestEqual(TEXT("Private-key path can be overridden"), EnvironmentConfig.MtlsPrivateKeyPath, FString(TEXT("/run/frontier/client.key")));
	TestEqual(TEXT("CA path can be overridden"), EnvironmentConfig.MtlsCaCertificatePath, FString(TEXT("/run/frontier/ca.crt")));
	FFrontierInternalApiConfig ExplicitInternalConfig;
	ExplicitInternalConfig.InternalApiBaseUrl = TEXT("https://internal.frontier.test");
	const FString InternalUrl = ExplicitInternalConfig.BuildExperienceGrantUrl(TEXT("player/id"));
	TestTrue(TEXT("Internal URL uses configured HTTPS base"), InternalUrl.StartsWith(TEXT("https://internal.frontier.test/")));
	TestFalse(TEXT("Internal URL never falls back to public test IP"), InternalUrl.Contains(TEXT("15.135.135.207")));
	TestTrue(TEXT("Player ID is URL encoded"), InternalUrl.Contains(TEXT("player%2Fid")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierBackendRaidFlowPolicyTest,
	"Frontier.Progression.RaidBackend.EntryFlowGuard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierBackendRaidFlowPolicyTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UFrontierRaidSessionSubsystem* RaidFlow = NewObject<UFrontierRaidSessionSubsystem>(GameInstance);
	FString Error;
	TestTrue(TEXT("First entry click starts one logical request"), RaidFlow->BeginClientRaidEntry(Error));
	TestFalse(TEXT("Duplicate entry click is rejected"), RaidFlow->BeginClientRaidEntry(Error));
	TestEqual(TEXT("Entry remains in CreatingEntry before the response"), RaidFlow->GetClientState(), EFrontierRaidFlowState::CreatingEntry);
	RaidFlow->AcceptClientRaidEntryContext(TEXT("backend-raid"));
	TestEqual(TEXT("Entry success proceeds to join authorization"), RaidFlow->GetClientState(), EFrontierRaidFlowState::AuthorizingJoin);
	TestEqual(TEXT("Backend raidSessionId is retained"), RaidFlow->GetLastRaidSessionId(), FString(TEXT("backend-raid")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierPlayerSessionSingleFlightTest,
	"Frontier.Progression.RaidBackend.PlayerSessionSingleFlight",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierPlayerSessionSingleFlightTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UFrontierPlayerSessionSubsystem* SessionStore = NewObject<UFrontierPlayerSessionSubsystem>(GameInstance);
	FFrontierOnlineSessionInfo Session;
	Session.SessionId = TEXT("session-1");
	Session.PlayerId = 9000000000LL;
	FFrontierOnlineTokenInfo Tokens;
	Tokens.AccessToken = TEXT("old-access");
	Tokens.RefreshToken = TEXT("old-refresh");
	Tokens.AccessTokenExpiresAt = (FDateTime::UtcNow() + FTimespan::FromSeconds(30)).ToIso8601();
	SessionStore->SetAuthenticatedSession(Session, Tokens, Session.PlayerId, TEXT("steam"));

	int32 RefreshCalls = 0;
	FFrontierOnlineRefreshCompletion DeferredRefresh;
	SessionStore->SetRefreshExecutorForTests(
		[&RefreshCalls, &DeferredRefresh](
			const FString& RefreshToken,
			const FString& SessionId,
			FFrontierOnlineRefreshCompletion Completion,
			FString& OutError)
		{
			++RefreshCalls;
			DeferredRefresh = MoveTemp(Completion);
			return true;
		});

	int32 CompletedWaiters = 0;
	SessionStore->AcquireAccessToken([&CompletedWaiters](const bool bSucceeded, const FString&, const FString&)
	{
		if (bSucceeded)
		{
			++CompletedWaiters;
		}
	});
	SessionStore->AcquireAccessToken([&CompletedWaiters](const bool bSucceeded, const FString&, const FString&)
	{
		if (bSucceeded)
		{
			++CompletedWaiters;
		}
	});
	TestEqual(TEXT("Near-expiry concurrent access starts one refresh"), RefreshCalls, 1);
	TestTrue(TEXT("Refresh remains single-flight while response is pending"), SessionStore->IsRefreshInFlight());
	TestEqual(TEXT("Both requests wait behind the same refresh"), SessionStore->GetPendingRequestCount(), 2);

	FFrontierOnlineRefreshResponse RefreshResponse;
	RefreshResponse.bTransportSucceeded = true;
	RefreshResponse.bSuccess = true;
	RefreshResponse.Session = Session;
	RefreshResponse.Tokens = Tokens;
	RefreshResponse.Tokens.AccessToken = TEXT("rotated-access");
	RefreshResponse.Tokens.RefreshToken = TEXT("rotated-refresh");
	RefreshResponse.Tokens.AccessTokenExpiresAt = (FDateTime::UtcNow() + FTimespan::FromHours(1)).ToIso8601();
	DeferredRefresh(RefreshResponse);
	TestEqual(TEXT("All waiters resume once after atomic token rotation"), CompletedWaiters, 2);

	int32 ImmediateCallbacks = 0;
	SessionStore->AcquireAccessToken([&ImmediateCallbacks](const bool bSucceeded, const FString& AccessToken, const FString&)
	{
		if (bSucceeded && AccessToken == TEXT("rotated-access"))
		{
			++ImmediateCallbacks;
		}
	});
	TestEqual(TEXT("Valid rotated token is reused without another refresh"), RefreshCalls, 1);
	TestEqual(TEXT("Valid-token request completes immediately"), ImmediateCallbacks, 1);
	return true;
}

#endif
