#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OutGameInventoryWrapperWidget.generated.h"

class AFrontierPlayerState;
class UFrontierCharacterStatPanelWidget;
class UFrontierEquippedSkillPanelWidget;

UCLASS()
class FRONTIER_API UOutGameInventoryWrapperWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION(BlueprintCallable, Category="Lobby Inventory")
	void RefreshFromPlayerState(AFrontierPlayerState* PlayerState);

	UFUNCTION(BlueprintPure, Category="Lobby Inventory")
	class UFrontierInventoryWidget* GetPlayerInventoryWidget() const;

protected:
	bool TryHandlePanelDrop(const FVector2D& ScreenSpacePosition, class UFrontierInventoryDragDropOperation* DragOperation);
	class UFrontierInventoryWidget* ResolveDropTargetInventoryWidget(const FVector2D& ScreenSpacePosition) const;
	
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierCharacterPreviewWidget> WBP_CharacterPreviewWidget;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierInventoryWidget> 	WBP_PlayerInventory;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierInventoryWidget> WBP_PlayerStorage;
};
