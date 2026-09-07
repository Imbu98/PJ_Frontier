#pragma once

#include "CoreMinimal.h"
#include "Loot/FrontierLootContainerActor.h"
#include "FrontierDeathLootContainerActor.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneComponent;

/** Character and monster death loot with local floating and rarity presentation. */
UCLASS()
class FRONTIER_API AFrontierDeathLootContainerActor : public AFrontierLootContainerActor
{
	GENERATED_BODY()

public:
	AFrontierDeathLootContainerActor();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleDeathLootInventoryChanged(const TArray<FFrontierInventorySlot>& UpdatedSlots);

	UFUNCTION()
	void HandleDeathLootLoadoutChanged(const TArray<FFrontierLoadoutSlot>& UpdatedSlots);

	void RefreshRarityEffect();
	EFrontierItemRarity FindHighestLootRarity() const;
	void SetRarityEffect(UNiagaraSystem* DesiredEffect);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot|Presentation")
	TObjectPtr<USceneComponent> FloatingVisualRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot|Presentation")
	TObjectPtr<UNiagaraComponent> RarityEffectComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot|Presentation")
	TObjectPtr<UNiagaraSystem> EpicLootEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot|Presentation")
	TObjectPtr<UNiagaraSystem> LegendaryLootEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot|Presentation", meta=(ClampMin="0.0"))
	float FloatAmplitude = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot|Presentation", meta=(ClampMin="0.0"))
	float FloatAngularSpeed = 2.0f;

private:
	FVector InitialFloatingRootLocation = FVector::ZeroVector;
	float FloatingElapsedSeconds = 0.0f;
};
