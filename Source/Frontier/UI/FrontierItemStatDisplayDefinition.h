#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "FrontierItemStatDisplayDefinition.generated.h"

USTRUCT(BlueprintType)
struct FFrontierItemStatDisplayNameRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Stat Display")
	FGameplayTag StatTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Item Stat Display")
	FText DisplayText;
};
