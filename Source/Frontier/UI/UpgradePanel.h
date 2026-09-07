#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "UpgradePanel.generated.h"

UCLASS()
class FRONTIER_API UUpgradePanel : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category="Upgrade")
	void RefreshUpgradePanel();

protected:
	UFUNCTION()
	void HandleUpgradeItemSelected(const FFrontierItemInstance& ItemInstance);

	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierUpgradeWidget> WBP_UpgradeWidget;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierInventoryWidget> WBP_PlayerInventoryWidget;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<class UFrontierInventoryWidget> WBP_StorageWidget;
};
