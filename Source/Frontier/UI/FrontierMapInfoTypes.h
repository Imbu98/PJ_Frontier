#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FrontierMapInfoTypes.generated.h"

class UTexture2D;

USTRUCT(BlueprintType)
struct FRONTIER_API FFrontierMapInfoTableRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	FText MapName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map")
	TSoftObjectPtr<UTexture2D> MapImage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Equipment Score", meta=(ClampMin="0.0"))
	float MinTotalEquipmentScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Equipment Score", meta=(ClampMin="0.0"))
	float MaxTotalEquipmentScore = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Map|Equipment Score", meta=(ClampMin="0.0"))
	float LimitEquipmentScore = 0.0f;
};
