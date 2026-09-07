#pragma once

#include "CoreMinimal.h"
#include "Components/FrontierInventoryComponent.h"
#include "FrontierLootInventoryComponent.generated.h"

UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierLootInventoryComponent : public UFrontierInventoryComponent
{
	GENERATED_BODY()

public:
	UFrontierLootInventoryComponent();

	UFUNCTION(BlueprintCallable, Category="Loot")
	void InitializeLootSlots(const TArray<FFrontierInventorySlot>& InitialLootSlots);
};
