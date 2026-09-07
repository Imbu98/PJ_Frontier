#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "FrontierTeamPlayerStart.generated.h"

UCLASS()
class FRONTIER_API AFrontierTeamPlayerStart : public APlayerStart
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Frontier|Spawn")
	int32 GetTeamId() const;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Spawn")
	int32 TeamId = 0;
};
