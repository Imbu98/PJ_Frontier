#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SkillTree/FrontierSkillTreeTypes.h"
#include "FrontierProfileSaveGame.generated.h"

/**
 * Account-scoped temporary progression. This is a local bridge only and must be replaced when
 * Backend skill-point persistence is available.
 */
USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierLocalAccountProgression
{
	GENERATED_BODY()

	/** Verified Backend playerId or SteamID used to prevent different local accounts sharing state. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	FString AccountId;

	/** Legacy v3 watermark retained only so old saves can migrate to LastVerifiedBackendLevel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	bool bHasInitializedLevel = false;

	/** Legacy v3 watermark retained for save compatibility. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	int32 LastProcessedLevel = 0;

	/** Last accepted Backend level. It is a cache/audit field, never the point-balance authority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	int32 LastVerifiedBackendLevel = 0;

	/** Backend PlayerLevelDTO.updatedAt used to reject an out-of-order older response. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	FString LastVerifiedLevelUpdatedAt;

	/** Unspent local points; mirrored from SkillTreeProgression.AvailableSkillPoints. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	int32 TemporarySkillPoints = 0;

	/** Derived cache only. Runtime validation recalculates this from Nodes and current DA RankCosts. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	int32 CachedSpentSkillPoints = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	FFrontierSkillTreeProgressionSnapshot SkillTreeProgression;
};

UCLASS()
class FRONTIER_API UFrontierProfileSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 CurrentSaveVersion = 4;

	/** New objects must call InitializeNewProfile. A missing field in legacy saves remains version 0. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Version")
	int32 SaveVersion = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Profile|Progression")
	FFrontierSkillTreeProgressionSnapshot SkillTreeProgression;

	/** Account-isolated progression introduced in save version 3. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	TArray<FFrontierLocalAccountProgression> AccountProgression;

	/** Ensures the pre-v3 single-account skill tree is moved to at most one verified account. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Profile|Progression")
	bool bLegacySkillTreeProgressionClaimed = false;

	FFrontierLocalAccountProgression* FindAccountProgression(const FString& AccountId);
	const FFrontierLocalAccountProgression* FindAccountProgression(const FString& AccountId) const;
	FFrontierLocalAccountProgression& FindOrAddAccountProgression(const FString& AccountId);

	void InitializeNewProfile();
	bool MigrateToLatest(bool& bOutWasMigrated, FString& OutError);
};
