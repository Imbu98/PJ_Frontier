#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierInGameInventoryWrapperWidget.generated.h"

class UCanvasPanel;
class AFrontierPlayerState;
class UFrontierCharacterPreviewWidget;
class UFrontierCharacterStatPanelWidget;
class UFrontierEquippedSkillPanelWidget;
class UFrontierInventoryWidget;
class UHorizontalBox;
class UVerticalBox;
class UWidget;

UCLASS()
class FRONTIER_API UFrontierInGameInventoryWrapperWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual bool NativeOnDrop(const FGeometry& InGeometry, const FDragDropEvent& InDragDropEvent, UDragDropOperation* InOperation) override;

	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void SetObservedPlayerState(AFrontierPlayerState* InObservedPlayerState);

	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	bool AddPanelWidget(UWidget* PanelWidget);

	UFUNCTION(BlueprintCallable, Category="Inventory UI")
	void ClearPanelWidgets();

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	bool HasPanelHost() const;

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	bool HasPanelWidget(const UWidget* PanelWidget) const;

	UFUNCTION(BlueprintPure, Category="Inventory UI")
	UFrontierInventoryWidget* GetFixedInventoryWidget() const;

	bool TryHandlePanelDrop(const FVector2D& ScreenSpacePosition, class UFrontierInventoryDragDropOperation* DragOperation);

	void RefreshCharacterDetailsPanel();
	void ClearCharacterDetailsPanel();

private:
	UFrontierInventoryWidget* ResolveDropTargetInventoryWidget(const FVector2D& ScreenSpacePosition) const;
	
	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UHorizontalBox> HorizontalBox_Wrapper;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI|Character", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierCharacterPreviewWidget> CharacterPreviewWidget;

	UPROPERTY(BlueprintReadOnly, Category="Inventory UI", meta=(BindWidget, AllowPrivateAccess="true"))
	TObjectPtr<UFrontierInventoryWidget> FixedInventoryWidget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Character", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierCharacterPreviewWidget> CharacterPreviewWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Character", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierCharacterStatPanelWidget> CharacterStatPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory UI|Character", meta=(AllowPrivateAccess="true"))
	TSubclassOf<UFrontierEquippedSkillPanelWidget> EquippedSkillPanelWidgetClass;
	
	UPROPERTY(BlueprintReadOnly, Category="Inventory UI|Character", meta=(BindWidgetOptional, AllowPrivateAccess="true"))
	TObjectPtr<UCanvasPanel> AddPanel;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWidget>> AttachedPanelWidgets;
	
};
