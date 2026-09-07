#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "FrontierWeaponAudioSettings.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EFrontierWeaponAttackSound : uint8
{
	MeleeAttack UMETA(DisplayName="Melee Attack"),
	BowDraw UMETA(DisplayName="Bow Draw"),
	BowRelease UMETA(DisplayName="Bow Release"),
	AttackVoice UMETA(DisplayName="Attack Voice")
};

/** Attack sound shared by every concrete weapon in a melee weapon family. */
USTRUCT(BlueprintType)
struct FFrontierMeleeWeaponAudioData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TSoftObjectPtr<USoundBase> AttackSound;

	/** One entry is selected randomly for every melee combo attack. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TArray<TSoftObjectPtr<USoundBase>> AttackVoiceSounds;

	/** Optional weapon-mesh socket used as the sound origin. None uses the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ClampMin="0.01"))
	float PitchMultiplier = 1.0f;
};

/** Bow-only sounds for its distinct draw and release phases. */
USTRUCT(BlueprintType)
struct FFrontierBowWeaponAudioData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TSoftObjectPtr<USoundBase> DrawSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TSoftObjectPtr<USoundBase> ReleaseSound;

	/** One entry is selected randomly when the arrow is released. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Attack")
	TArray<TSoftObjectPtr<USoundBase>> AttackVoiceSounds;

	/** Optional weapon-mesh socket used as the sound origin. None uses the mesh origin. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback")
	FName AttachSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ClampMin="0.0"))
	float VolumeMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ClampMin="0.01"))
	float PitchMultiplier = 1.0f;
};

/** Global attack audio configured once per weapon family rather than per weapon item. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Frontier Weapon Audio"))
class FRONTIER_API UFrontierWeaponAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	virtual FName GetSectionName() const override { return TEXT("Frontier Weapon Audio"); }

	const FFrontierMeleeWeaponAudioData* FindMeleeAudioForWeaponType(FGameplayTag WeaponTypeTag) const;
	const FFrontierBowWeaponAudioData* FindBowAudioForWeaponType(FGameplayTag WeaponTypeTag) const;

	UPROPERTY(Config, EditAnywhere, Category="Weapon Family", meta=(DisplayName="Sword"))
	FFrontierMeleeWeaponAudioData SwordAudio;

	UPROPERTY(Config, EditAnywhere, Category="Weapon Family", meta=(DisplayName="Axe"))
	FFrontierMeleeWeaponAudioData AxeAudio;

	UPROPERTY(Config, EditAnywhere, Category="Weapon Family", meta=(DisplayName="Spear"))
	FFrontierMeleeWeaponAudioData SpearAudio;

	UPROPERTY(Config, EditAnywhere, Category="Weapon Family", meta=(DisplayName="Bow"))
	FFrontierBowWeaponAudioData BowAudio;
};
