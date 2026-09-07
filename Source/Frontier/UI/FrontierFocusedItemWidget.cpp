#include "UI/FrontierFocusedItemWidget.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Inventory/Items/FrontierArmorItemDataAsset.h"
#include "Inventory/Items/FrontierConsumableItemDataAsset.h"
#include "Inventory/Items/FrontierItemDataAsset.h"
#include "Inventory/Items/FrontierWeaponItemDataAsset.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "UI/FrontierItemRarityBorderWidget.h"

void UFrontierFocusedItemWidget::SetFocusedItemActor(AFrontierDroppedItemActor* InFocusedItemActor)
{
	FocusedItemActor = InFocusedItemActor;
	RefreshItemInfo();
}

void UFrontierFocusedItemWidget::RefreshItemInfo()
{
	const FFrontierItemInstance* ItemInstance = FocusedItemActor ? &FocusedItemActor->GetItemInstance() : nullptr;
	const TSoftObjectPtr<UTexture2D> Icon = ItemInstance && ItemInstance->IsValid() ? ItemInstance->GetIcon() : TSoftObjectPtr<UTexture2D>();
	UTexture2D* IconTexture = !Icon.IsNull() ? Icon.LoadSynchronous() : nullptr;

	SetVisibility(ItemInstance && ItemInstance->IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

	if (FocusedItemRoot)
	{
		FocusedItemRoot->SetVisibility(ItemInstance && ItemInstance->IsValid() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	if (FocusedItemIconImage)
	{
		FocusedItemIconImage->SetBrushFromTexture(IconTexture, true);
		FocusedItemIconImage->SetVisibility(IconTexture ? ESlateVisibility::Visible : ESlateVisibility::Hidden);
	}

	if (FocusedItemRarityBorderWidget)
	{
		if (ItemInstance && ItemInstance->IsValid())
		{
			FocusedItemRarityBorderWidget->SetItemRarity(ItemInstance->GetDisplayRarity());
		}
		else
		{
			FocusedItemRarityBorderWidget->ClearRarity();
		}
	}

	if (FocusedItemNameText)
	{
		FocusedItemNameText->SetText(ItemInstance && ItemInstance->IsValid() ? ItemInstance->GetDisplayNameText() : FText::GetEmpty());
	}

	if (FocusedItemDescriptionText)
	{
		FocusedItemDescriptionText->SetText(ItemInstance && ItemInstance->IsValid() ? ItemInstance->GetDescriptionText() : FText::GetEmpty());
	}

	if (FocusedItemStatsText)
	{
		if (!ItemInstance || !ItemInstance->IsValid())
		{
			FocusedItemStatsText->SetText(FText::GetEmpty());
			return;
		}

		FString StatsText = FString::Printf(TEXT("Qty: %d"), ItemInstance->Quantity);
		if (ItemInstance->GetCategory() == EFrontierItemCategory::Weapon)
		{
			for (const FFrontierRuntimeStatData& InstanceStat : ItemInstance->RuntimeGeneratedStats)
			{
				const FString OptionName = InstanceStat.StatTag.IsValid() ? InstanceStat.StatTag.GetTagName().ToString() : InstanceStat.OptionId;
				if (!OptionName.IsEmpty())
				{
					StatsText += FString::Printf(TEXT("\n%s: %.0f"), *OptionName, InstanceStat.FinalValue);
				}
			}
		}
		else if (ItemInstance->GetCategory() == EFrontierItemCategory::Armor
			|| ItemInstance->GetCategory() == EFrontierItemCategory::Accessory)
		{
			for (const FFrontierRuntimeStatData& InstanceStat : ItemInstance->RuntimeGeneratedStats)
			{
				const FString OptionName = InstanceStat.StatTag.IsValid() ? InstanceStat.StatTag.GetTagName().ToString() : InstanceStat.OptionId;
				if (!OptionName.IsEmpty())
				{
					StatsText += FString::Printf(TEXT("\n%s: %.0f"), *OptionName, InstanceStat.FinalValue);
				}
			}
		}
		else if (ItemInstance->GetCategory() == EFrontierItemCategory::Consumable)
		{
			StatsText += FString::Printf(TEXT("\nHeal Amount: %.0f"), ItemInstance->GetHealthRestoreAmount());
		}

		FocusedItemStatsText->SetText(FText::FromString(StatsText));
	}
}
