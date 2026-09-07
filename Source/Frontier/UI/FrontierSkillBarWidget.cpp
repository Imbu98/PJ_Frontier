#include "UI/FrontierSkillBarWidget.h"

#include "Components/FrontierEquipmentSkillComponent.h"
#include "Frontier.h"
#include "UI/FrontierSkillSlotWidget.h"

UFrontierSkillBarWidget::UFrontierSkillBarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	KeyTexts = {
		FText::FromString(TEXT("Q")),
		FText::FromString(TEXT("E")),
		FText::FromString(TEXT("R"))
	};
}

void UFrontierSkillBarWidget::SetSkillComponent(UFrontierEquipmentSkillComponent* InSkillComponent)
{
	

	if (SkillComponent)
	{
		SkillComponent->OnEquipmentSkillsChanged.RemoveAll(this);
	}

	SkillComponent = InSkillComponent;
	if (SkillComponent)
	{
		SkillComponent->OnEquipmentSkillsChanged.AddUObject(this, &UFrontierSkillBarWidget::HandleEquipmentSkillsChanged);
	}

	RefreshSlots();
}

void UFrontierSkillBarWidget::RefreshSlots()
{
	
	CacheBoundSlotWidgets();

	for (int32 Index = 0; Index < SlotWidgets.Num(); ++Index)
	{
		if (UFrontierSkillSlotWidget* SlotWidget = SlotWidgets[Index])
		{
			SlotWidget->SetSkillSlot(SkillComponent, Index, GetKeyTextForSlot(Index));
		}
	}
}

void UFrontierSkillBarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	
	
	RefreshSlots();
}

void UFrontierSkillBarWidget::CacheBoundSlotWidgets()
{
	SlotWidgets.Reset();
	SlotWidgets.SetNum(3);
	SlotWidgets[0] = WBP_SkillSlotWidget;
	SlotWidgets[1] = WBP_SkillSlotWidget_1;
	SlotWidgets[2] = WBP_SkillSlotWidget_2;
}

FText UFrontierSkillBarWidget::GetKeyTextForSlot(const int32 SlotIndex) const
{
	return KeyTexts.IsValidIndex(SlotIndex)
		? KeyTexts[SlotIndex]
		: FText::FromString(FString::Printf(TEXT("%d"), SlotIndex + 1));
}

void UFrontierSkillBarWidget::NativeDestruct()
{
	if (SkillComponent)
	{
		SkillComponent->OnEquipmentSkillsChanged.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UFrontierSkillBarWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	RefreshAccumulator += InDeltaTime;
	if (RefreshAccumulator < 0.05f)
	{
		return;
	}

	RefreshAccumulator = 0.0f;
	for (UFrontierSkillSlotWidget* SlotWidget : SlotWidgets)
	{
		if (SlotWidget)
		{
			SlotWidget->RefreshSlot();
		}
	}
}


void UFrontierSkillBarWidget::HandleEquipmentSkillsChanged()
{
	
	RefreshSlots();
}
