#pragma once

#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierInventoryDragDropOperation.generated.h"

class APlayerController;
class AFrontierDroppedItemActor;

UENUM(BlueprintType)
enum class EFrontierDragSourceType : uint8
{
	None,
	InventorySlot,
	LoadoutSlot,
	QuickSlot,
	DroppedItem
};

UENUM(BlueprintType)
enum class EFrontierInventoryCollectionType : uint8
{
	None,
	Storage,
	RaidInventory,
	LootInventory
};

UCLASS()
class FRONTIER_API UFrontierInventoryDragDropOperation : public UDragDropOperation
{
	GENERATED_BODY()

public:
	virtual void DragCancelled_Implementation(const FPointerEvent& PointerEvent) override;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	EFrontierDragSourceType SourceType = EFrontierDragSourceType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	EFrontierInventoryCollectionType SourceInventoryCollection = EFrontierInventoryCollectionType::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	int32 SourceInventorySlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	int32 SourceQuickSlotIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	EFrontierEquipmentSlot SourceLoadoutSlotType = EFrontierEquipmentSlot::None;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	FFrontierItemInstance DraggedItemInstance;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	TObjectPtr<APlayerController> OwningPlayerController;

	UPROPERTY(BlueprintReadOnly, Category="Inventory Drag")
	TObjectPtr<AFrontierDroppedItemActor> SourceDroppedItem;
};
