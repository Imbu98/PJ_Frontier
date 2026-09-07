#include "UI/FrontierEquippedSkillPanelWidget.h"

#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/TextBlock.h"
#include "Game/FrontierPlayerState.h"
#include "Skill/FrontierSkillTypes.h"

void UFrontierEquippedSkillPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	
	BindSkillComponent();
	RefreshSkills();
}

void UFrontierEquippedSkillPanelWidget::NativeDestruct()
{
	UnbindSkillComponent();
	Super::NativeDestruct();
}

void UFrontierEquippedSkillPanelWidget::SetObservedPlayerState(AFrontierPlayerState* InPlayerState)
{
	ObservedPlayerState = InPlayerState;
	SetSkillComponent(ObservedPlayerState ? ObservedPlayerState->GetEquipmentSkillComponent() : nullptr);
}

void UFrontierEquippedSkillPanelWidget::SetSkillComponent(UFrontierEquipmentSkillComponent* InSkillComponent)
{
	if (SkillComponent == InSkillComponent)
	{
		RefreshSkills();
		return;
	}

	UnbindSkillComponent();
	SkillComponent = InSkillComponent;
	BindSkillComponent();
	RefreshSkills();
}

void UFrontierEquippedSkillPanelWidget::RefreshSkills()
{
	if (Text_SkillList1)
	{
		Text_SkillList1->SetText(FText::FromString(BuildSkillLine(0)));
	}
	if (Text_SkillList2)
	{
		Text_SkillList2->SetText(FText::FromString(BuildSkillLine(1)));
	}
	if (Text_SkillList3)
	{
		Text_SkillList3->SetText(FText::FromString(BuildSkillLine(2)));
	}
}

void UFrontierEquippedSkillPanelWidget::BindSkillComponent()
{
	if (!SkillComponent)
	{
		if (!ObservedPlayerState && GetOwningPlayer())
		{
			ObservedPlayerState = GetOwningPlayer()->GetPlayerState<AFrontierPlayerState>();
		}
		SkillComponent = ObservedPlayerState ? ObservedPlayerState->GetEquipmentSkillComponent() : nullptr;
	}

	if (SkillComponent)
	{
		SkillComponent->OnEquipmentSkillsChanged.AddUObject(this, &UFrontierEquippedSkillPanelWidget::HandleEquipmentSkillsChanged);
	}
}

void UFrontierEquippedSkillPanelWidget::UnbindSkillComponent()
{
	if (SkillComponent)
	{
		SkillComponent->OnEquipmentSkillsChanged.RemoveAll(this);
	}
}

void UFrontierEquippedSkillPanelWidget::HandleEquipmentSkillsChanged()
{
	RefreshSkills();
}

FString UFrontierEquippedSkillPanelWidget::BuildSkillLine(const int32 SlotIndex) const
{
	if (!SkillComponent)
	{
		return FString::Printf(TEXT("%d. Empty"), SlotIndex + 1);
	}

	FFrontierGeneratedWeaponSkill GeneratedSkill;
	if (!SkillComponent->GetGeneratedSkillAtSlot(SlotIndex, GeneratedSkill) || !GeneratedSkill.SkillTag.IsValid())
	{
		return FString::Printf(TEXT("%d. Empty"), SlotIndex + 1);
	}

	const FFrontierSkillInfo* SkillInfo = SkillComponent->FindSkillInfo(GeneratedSkill.SkillTag);
	const FString SkillName = SkillInfo && !SkillInfo->DisplayName.IsEmpty()
		? SkillInfo->DisplayName.ToString()
		: GeneratedSkill.SkillTag.GetTagName().ToString();
	const int32 SkillLevel = SkillComponent->GetSkillLevel(GeneratedSkill.SkillTag);

	return FString::Printf(TEXT("%d. %s Lv.%d"), SlotIndex + 1, *SkillName, SkillLevel);
}
