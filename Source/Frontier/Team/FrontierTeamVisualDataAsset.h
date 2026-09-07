#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FrontierTeamVisualDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FFrontierTeamVisualEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	int32 TeamId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	TObjectPtr<UMaterialInterface> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Team")
	FLinearColor TeamColor = FLinearColor::White;
};

UCLASS(BlueprintType)
class FRONTIER_API UFrontierTeamVisualDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	UMaterialInterface* GetMaterialForTeam(const int32 TeamId) const;

	UFUNCTION(BlueprintPure, Category="Frontier|Team")
	FLinearColor GetColorForTeam(const int32 TeamId) const;

private:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Team", meta=(AllowPrivateAccess="true"))
	TArray<FFrontierTeamVisualEntry> TeamVisuals;
};
