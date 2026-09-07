#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Engine/DataAsset.h"
#include "Character/FrontierCharacterTypes.h"
#include "FrontierCharacterAppearanceDataAsset.generated.h"

class UAnimationAsset;
class USkeletalMesh;

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierCharacterAppearanceData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	EFrontierCharacterType CharacterType = EFrontierCharacterType::DarkKnight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TSoftClassPtr<UAnimInstance> AnimationClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TSoftObjectPtr<UAnimationAsset> PreviewAnimation;
};

UCLASS(BlueprintType)
class FRONTIER_API UFrontierCharacterAppearanceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance")
	TArray<FFrontierCharacterAppearanceData> CharacterAppearances;

	const FFrontierCharacterAppearanceData* FindAppearance(EFrontierCharacterType CharacterType) const;
};
