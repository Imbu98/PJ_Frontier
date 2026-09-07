#pragma once

#include "CoreMinimal.h"
#include "Interaction/FrontierInteractableActor.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierDroppedItemActor.generated.h"

class AFrontierPlayerController;
class UPrimitiveComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USphereComponent;
struct FTimerHandle;

UCLASS()
class FRONTIER_API AFrontierDroppedItemActor : public AFrontierInteractableActor
{
	GENERATED_BODY()

public:
	AFrontierDroppedItemActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanInteract(const AFrontierPlayerController* InteractingController) const override;
	virtual bool IsInstantInteraction(const AFrontierPlayerController* InteractingController) const override;
	virtual void Interacted(AFrontierPlayerController* InteractingController) override;
	bool TryPickupToRaidInventorySlot(AFrontierPlayerController* InteractingController, int32 TargetSlotIndex);
	virtual FText GetInteractionDisplayName(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionActionText(const AFrontierPlayerController* InteractingController) const override;
	virtual FText GetInteractionPromptText(const AFrontierPlayerController* InteractingController) const override;
	virtual FVector GetInteractionWorldLocation() const override;
	virtual void GetInteractionHighlightComponents(TArray<UPrimitiveComponent*>& OutComponents) const override;

	UFUNCTION(BlueprintCallable, Category="Dropped Item")
	void InitializeDroppedItem(const FFrontierItemInstance& InItemInstance);

	UFUNCTION(BlueprintPure, Category="Dropped Item")
	const FFrontierItemInstance& GetItemInstance() const;

protected:
	UFUNCTION()
	void OnRep_ItemInstance();

	void RefreshDroppedItemVisual();
	void ApplyDropPhysics();
	void FinalizeDropPhysics();
	FVector ResolveDropWorldScale() const;
	float ResolveDropImpulseForward() const;
	float ResolveDropImpulseUpward() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dropped Item")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dropped Item")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dropped Item")
	TObjectPtr<USphereComponent> InteractionSphere;

	UPROPERTY(ReplicatedUsing=OnRep_ItemInstance, BlueprintReadOnly, Category="Dropped Item")
	FFrontierItemInstance ItemInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Dropped Item", meta=(ClampMin="0.0"))
	float PhysicsSettleDelay = 0.35f;

	FTimerHandle PhysicsSettleTimerHandle;
};
