#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Save/FrontierLocalProfileSubsystem.h"
#include "Save/FrontierProfileSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierProfileSaveMigrationTest,
	"Frontier.Save.Profile.VersionMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierProfileSaveMigrationTest::RunTest(const FString& Parameters)
{
	UFrontierProfileSaveGame* NewProfile = NewObject<UFrontierProfileSaveGame>();
	TestNotNull(TEXT("New profile object exists"), NewProfile);
	if (!NewProfile)
	{
		return false;
	}

	TestEqual(TEXT("Uninitialized objects retain the legacy sentinel"), NewProfile->SaveVersion, 0);
	NewProfile->InitializeNewProfile();
	TestEqual(
		TEXT("New profiles start at the current version"),
		NewProfile->SaveVersion,
		UFrontierProfileSaveGame::CurrentSaveVersion);

	UFrontierProfileSaveGame* LegacyProfile = NewObject<UFrontierProfileSaveGame>();
	LegacyProfile->SaveVersion = 0;

	bool bWasMigrated = false;
	FString MigrationError;
	TestTrue(
		TEXT("Original unversioned profile migrates"),
		LegacyProfile->MigrateToLatest(bWasMigrated, MigrationError));
	TestTrue(TEXT("Migration reports a version change"), bWasMigrated);
	TestTrue(TEXT("Migration has no error"), MigrationError.IsEmpty());
	TestEqual(TEXT("Legacy profile starts with no available skill points"), LegacyProfile->SkillTreeProgression.AvailableSkillPoints, 0);
	TestTrue(TEXT("Legacy profile starts with no invested skill nodes"), LegacyProfile->SkillTreeProgression.Nodes.IsEmpty());

	UFrontierProfileSaveGame* VersionThreeProfile = NewObject<UFrontierProfileSaveGame>();
	VersionThreeProfile->SaveVersion = 3;
	FFrontierLocalAccountProgression& VersionThreeAccount = VersionThreeProfile->AccountProgression.AddDefaulted_GetRef();
	VersionThreeAccount.AccountId = TEXT("migration-player");
	VersionThreeAccount.bHasInitializedLevel = true;
	VersionThreeAccount.LastProcessedLevel = 8;
	VersionThreeAccount.TemporarySkillPoints = 3;
	VersionThreeAccount.SkillTreeProgression.TotalSkillPointsEarned = 8;
	VersionThreeAccount.SkillTreeProgression.AvailableSkillPoints = 3;
	bWasMigrated = false;
	TestTrue(TEXT("Version 3 account progression migrates"), VersionThreeProfile->MigrateToLatest(bWasMigrated, MigrationError));
	TestTrue(TEXT("Version 3 migration is reported"), bWasMigrated);
	TestEqual(TEXT("Legacy watermark becomes the verified Backend level cache"), VersionThreeAccount.LastVerifiedBackendLevel, 8);
	TestEqual(TEXT("Spent cache is derived from the old snapshot"), VersionThreeAccount.CachedSpentSkillPoints, 5);
	TestTrue(TEXT("Version 3 node allocation remains available for the next reconciliation"), VersionThreeAccount.SkillTreeProgression.Nodes.IsEmpty());

	bWasMigrated = true;
	TestTrue(TEXT("Current migration is idempotent"), LegacyProfile->MigrateToLatest(bWasMigrated, MigrationError));
	TestFalse(TEXT("Current profile is not migrated twice"), bWasMigrated);

	UFrontierProfileSaveGame* FutureProfile = NewObject<UFrontierProfileSaveGame>();
	FutureProfile->SaveVersion = UFrontierProfileSaveGame::CurrentSaveVersion + 1;
	FutureProfile->SkillTreeProgression.AvailableSkillPoints = 13;
	const int32 FutureVersion = FutureProfile->SaveVersion;
	bWasMigrated = false;
	TestFalse(
		TEXT("A save from a newer build is rejected"),
		FutureProfile->MigrateToLatest(bWasMigrated, MigrationError));
	TestEqual(TEXT("Future version remains untouched"), FutureProfile->SaveVersion, FutureVersion);
	TestEqual(TEXT("Future data remains untouched"), FutureProfile->SkillTreeProgression.AvailableSkillPoints, 13);
	TestFalse(TEXT("Rejected future save is not reported as migrated"), bWasMigrated);
	TestFalse(TEXT("Rejected future save explains the failure"), MigrationError.IsEmpty());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierProfileSaveShutdownFlushTest,
	"Frontier.Save.Profile.ShutdownFlushesLatestSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierProfileSaveShutdownFlushTest::RunTest(const FString& Parameters)
{
	const FString SlotName = FString::Printf(TEXT("FrontierAutomation_%s"), *FGuid::NewGuid().ToString(EGuidFormats::Digits));
	constexpr int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

	UGameInstance* GameInstance = GEngine ? NewObject<UGameInstance>(GEngine) : nullptr;
	TestNotNull(TEXT("Standalone GameInstance exists"), GameInstance);
	if (!GameInstance)
	{
		return false;
	}

	GameInstance->InitializeStandalone(TEXT("FrontierSaveFlushTest"));
	UFrontierLocalProfileSubsystem* ProfileSubsystem = GameInstance->GetSubsystem<UFrontierLocalProfileSubsystem>();
	TestNotNull(TEXT("Local profile subsystem exists"), ProfileSubsystem);
	if (!ProfileSubsystem)
	{
		GameInstance->Shutdown();
		return false;
	}

	UFrontierProfileSaveGame* Profile = ProfileSubsystem->LoadOrCreateProfile(SlotName, UserIndex);
	TestNotNull(TEXT("Temporary profile exists"), Profile);
	if (!Profile)
	{
		GameInstance->Shutdown();
		return false;
	}

	Profile->SkillTreeProgression.AvailableSkillPoints = 3;
	Profile->SkillTreeProgression.TotalSkillPointsEarned = 5;
	TestTrue(TEXT("First snapshot is queued"), ProfileSubsystem->QueueProfileSave(Profile, SlotName, UserIndex));
	Profile->SkillTreeProgression.AvailableSkillPoints = 7;
	Profile->SkillTreeProgression.TotalSkillPointsEarned = 11;
	TestTrue(TEXT("Latest snapshot is coalesced"), ProfileSubsystem->QueueProfileSave(Profile, SlotName, UserIndex));

	GameInstance->Shutdown();

	UFrontierProfileSaveGame* LoadedProfile = Cast<UFrontierProfileSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex));
	TestNotNull(TEXT("Flushed profile can be loaded"), LoadedProfile);
	if (LoadedProfile)
	{
		TestEqual(TEXT("Shutdown preserves available skill points"), LoadedProfile->SkillTreeProgression.AvailableSkillPoints, 7);
		TestEqual(TEXT("Shutdown preserves total earned skill points"), LoadedProfile->SkillTreeProgression.TotalSkillPointsEarned, 11);
	}

	TestTrue(TEXT("Temporary test profile is deleted"), UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex));
	return true;
}

#endif
