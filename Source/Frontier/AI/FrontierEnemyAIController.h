#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "FrontierEnemyAIController.generated.h"

class UStateTreeAIComponent;
class UAIPerceptionComponent;
class UAISenseConfig_Damage;
class UAISenseConfig_Sight;

UCLASS()
class FRONTIER_API AFrontierEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	AFrontierEnemyAIController();

	UFUNCTION(BlueprintPure, Category="Frontier|AI")
	UStateTreeAIComponent* GetStateTreeAIComponent() const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandlePerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI")
	TObjectPtr<UStateTreeAIComponent> StateTreeAIComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI|Perception")
	TObjectPtr<UAIPerceptionComponent> FrontierPerceptionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI|Perception")
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Frontier|AI|Perception")
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;
};
