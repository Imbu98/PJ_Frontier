#include "UpgradePanel.h"

#include "Game/FrontierPlayerState.h"
#include "UI/FrontierInventoryWidget.h"
#include "UI/FrontierUpgradeWidget.h"

void UUpgradePanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (WBP_PlayerInventoryWidget)
	{
		WBP_PlayerInventoryWidget->OnUpgradeItemSelected.RemoveDynamic(this, &UUpgradePanel::HandleUpgradeItemSelected);
		WBP_PlayerInventoryWidget->OnUpgradeItemSelected.AddDynamic(this, &UUpgradePanel::HandleUpgradeItemSelected);
	}
}

void UUpgradePanel::NativeDestruct()
{
	if (WBP_PlayerInventoryWidget)
	{
		WBP_PlayerInventoryWidget->OnUpgradeItemSelected.RemoveDynamic(this, &UUpgradePanel::HandleUpgradeItemSelected);
	}

	Super::NativeDestruct();
}

void UUpgradePanel::RefreshUpgradePanel()
{
	AFrontierPlayerState* PlayerState = GetOwningPlayer()
		? GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>()
		: nullptr;

	if (WBP_UpgradeWidget)
	{
		WBP_UpgradeWidget->ClearSelectedUpgradeItem();
	}

	if (WBP_PlayerInventoryWidget)
	{
		WBP_PlayerInventoryWidget->SetObservedPlayerState(PlayerState);
		WBP_PlayerInventoryWidget->SetInventoryWidgetMode(EInventoryWidgetMode::UpgradeSelect);
		WBP_PlayerInventoryWidget->RefreshFromPlayerState();
	}

	if (WBP_StorageWidget)
	{
		WBP_StorageWidget->SetObservedPlayerState(PlayerState);
		WBP_StorageWidget->SetInventoryWidgetMode(EInventoryWidgetMode::StorageView);
		WBP_StorageWidget->RefreshFromPlayerState();
	}
}

void UUpgradePanel::HandleUpgradeItemSelected(const FFrontierItemInstance& ItemInstance)
{
	if (WBP_UpgradeWidget)
	{
		WBP_UpgradeWidget->SetSelectedUpgradeItem(ItemInstance);
	}
}
