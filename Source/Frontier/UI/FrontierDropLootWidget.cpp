#include "UI/FrontierDropLootWidget.h"

#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Frontier.h"
#include "Loot/FrontierDroppedItemActor.h"
#include "UI/FrontierDropLootEntryWidget.h"

void UFrontierDropLootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (!EntryWidgetClass)
	{
		FRONTIER_LOG(Error, TEXT("Drop-loot widget has no EntryWidgetClass. Configure it on the Widget Blueprint. Widget=%s"), *GetNameSafe(this));
	}
	RefreshVisibility();
}

void UFrontierDropLootWidget::NativeDestruct()
{
	ClearObservedDroppedItems();
	Super::NativeDestruct();
}

void UFrontierDropLootWidget::SetObservedDroppedItems(const TArray<AFrontierDroppedItemActor*>& InDroppedItems)
{
	TSet<TWeakObjectPtr<AFrontierDroppedItemActor>> NewItems;
	for (AFrontierDroppedItemActor* DroppedItem : InDroppedItems)
	{
		if (IsValid(DroppedItem) && DroppedItem->GetItemInstance().IsValid())
		{
			NewItems.Add(DroppedItem);
		}
	}

	bool bMembershipChanged = false;
	for (int32 EntryIndex = CreatedEntryWidgets.Num() - 1; EntryIndex >= 0; --EntryIndex)
	{
		UFrontierDropLootEntryWidget* EntryWidget = CreatedEntryWidgets[EntryIndex];
		AFrontierDroppedItemActor* DroppedItem = EntryWidget ? EntryWidget->GetObservedDroppedItem() : nullptr;
		if (!DroppedItem || !NewItems.Contains(DroppedItem))
		{
			if (EntryWidget)
			{
				EntryWidget->RemoveFromParent();
			}
			CreatedEntryWidgets.RemoveAt(EntryIndex);
			bMembershipChanged = true;
		}
	}

	for (AFrontierDroppedItemActor* DroppedItem : InDroppedItems)
	{
		if (!NewItems.Contains(DroppedItem))
		{
			continue;
		}

		const bool bAlreadyObserved = CreatedEntryWidgets.ContainsByPredicate(
			[DroppedItem](const UFrontierDropLootEntryWidget* EntryWidget)
			{
				return EntryWidget && EntryWidget->GetObservedDroppedItem() == DroppedItem;
			});
		if (bAlreadyObserved)
		{
			continue;
		}

		if (!EntryWidgetClass)
		{
			continue;
		}

		UFrontierDropLootEntryWidget* EntryWidget = CreateWidget<UFrontierDropLootEntryWidget>(GetOwningPlayer(), EntryWidgetClass);
		if (!EntryWidget)
		{
			continue;
		}

		EntryWidget->SetObservedDroppedItem(DroppedItem);
		EntryWidget->SetOwningInventoryWidget(OwningInventoryWidget);
		CreatedEntryWidgets.Add(EntryWidget);
		bMembershipChanged = true;
	}

	if (bMembershipChanged)
	{
		RebuildEntryOrder();
	}
	RefreshVisibility();
}

void UFrontierDropLootWidget::ClearObservedDroppedItems()
{
	if (DroppedItemList)
	{
		DroppedItemList->ClearChildren();
	}
	CreatedEntryWidgets.Reset();
	RefreshVisibility();
}

int32 UFrontierDropLootWidget::GetObservedDroppedItemCount() const
{
	return CreatedEntryWidgets.Num();
}

void UFrontierDropLootWidget::SetOwningInventoryWidget(UFrontierInventoryWidget* InOwningInventoryWidget)
{
	OwningInventoryWidget = InOwningInventoryWidget;
	for (UFrontierDropLootEntryWidget* EntryWidget : CreatedEntryWidgets)
	{
		if (EntryWidget)
		{
			EntryWidget->SetOwningInventoryWidget(InOwningInventoryWidget);
		}
	}
}

void UFrontierDropLootWidget::RebuildEntryOrder()
{
	APawn* OwningPawn = GetOwningPlayerPawn();
	const FVector PawnLocation = OwningPawn ? OwningPawn->GetActorLocation() : FVector::ZeroVector;
	CreatedEntryWidgets.Sort([PawnLocation](const UFrontierDropLootEntryWidget& Left, const UFrontierDropLootEntryWidget& Right)
	{
		const AFrontierDroppedItemActor* LeftActor = Left.GetObservedDroppedItem();
		const AFrontierDroppedItemActor* RightActor = Right.GetObservedDroppedItem();
		const float LeftDistance = LeftActor ? FVector::DistSquared(PawnLocation, LeftActor->GetActorLocation()) : TNumericLimits<float>::Max();
		const float RightDistance = RightActor ? FVector::DistSquared(PawnLocation, RightActor->GetActorLocation()) : TNumericLimits<float>::Max();
		return LeftDistance < RightDistance;
	});

	if (!DroppedItemList)
	{
		return;
	}

	DroppedItemList->ClearChildren();
	for (UFrontierDropLootEntryWidget* EntryWidget : CreatedEntryWidgets)
	{
		if (EntryWidget)
		{
			if (UScrollBoxSlot* EntrySlot = Cast<UScrollBoxSlot>(DroppedItemList->AddChild(EntryWidget)))
			{
				EntrySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
			}
		}
	}
}

void UFrontierDropLootWidget::RefreshVisibility()
{
	SetVisibility(CreatedEntryWidgets.Num() > 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}
