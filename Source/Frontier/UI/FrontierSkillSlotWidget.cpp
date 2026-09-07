#include "UI/FrontierSkillSlotWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/FrontierEquipmentSkillComponent.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Frontier.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Skill/FrontierSkillDataAsset.h"
#include "UI/FrontierSkillTooltipWidget.h"

void UFrontierSkillSlotWidget::SetSkillSlot(UFrontierEquipmentSkillComponent* InSkillComponent, const int32 InSlotIndex, const FText InKeyText)
{
	

	SkillComponent = InSkillComponent;
	SlotIndex = InSlotIndex;
	KeyText = InKeyText;
	RefreshSlot();
}

void UFrontierSkillSlotWidget::RefreshSlot()
{
	if (!SkillComponent)
	{
		ClearSkillVisuals();
		return;
	}

	FFrontierGeneratedWeaponSkill GeneratedSkill;
	const bool bHasGeneratedSkill = SkillComponent->GetGeneratedSkillAtSlot(SlotIndex, GeneratedSkill);
	if (!bHasGeneratedSkill || !GeneratedSkill.SkillTag.IsValid())
	{
		ClearSkillVisuals();
		return;
	}

	if (bSlotWasEmpty || CachedSkillTag != GeneratedSkill.SkillTag)
	{
		RefreshStaticSkillVisuals(GeneratedSkill);
	}

	const FGameplayTag SkillTag = GeneratedSkill.SkillTag;
	const float CooldownDuration = SkillComponent->GetCooldownDuration(SkillTag);
	const float CooldownRemaining = SkillComponent->GetCooldownRemaining(SkillTag);
	const float CooldownPercent = CooldownDuration > 0.0f ? FMath::Clamp(CooldownRemaining / CooldownDuration, 0.0f, 1.0f) : 0.0f;

	if (CooldownText)
	{
		CooldownText->SetText(CooldownRemaining > 0.0f ? FText::FromString(FString::Printf(TEXT("%.1f"), CooldownRemaining)) : FText::GetEmpty());
	}

	UpdateCooldownMaterial(CooldownPercent);

	if (SkillIconImage)
	{
		SkillIconImage->SetBrushFromTexture(bHasCachedSkillInfo ? CachedSkillInfo.Icon : nullptr);
	}
}

void UFrontierSkillSlotWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_SkillHorver)
	{
		Button_SkillHorver->OnHovered.AddDynamic(this, &UFrontierSkillSlotWidget::HandleSkillHover);
		Button_SkillHorver->OnUnhovered.AddDynamic(this, &UFrontierSkillSlotWidget::HandleSkillUnhover);
	}

	HideSkillTooltip();
	RefreshSlot();
}

void UFrontierSkillSlotWidget::NativeDestruct()
{
	if (Button_SkillHorver)
	{
		Button_SkillHorver->OnHovered.RemoveDynamic(this, &UFrontierSkillSlotWidget::HandleSkillHover);
		Button_SkillHorver->OnUnhovered.RemoveDynamic(this, &UFrontierSkillSlotWidget::HandleSkillUnhover);
	}

	HideSkillTooltip();
	Super::NativeDestruct();
}

void UFrontierSkillSlotWidget::HandleSkillHover()
{
	ShowSkillTooltip();
}

void UFrontierSkillSlotWidget::HandleSkillUnhover()
{
	HideSkillTooltip();
}

void UFrontierSkillSlotWidget::ShowSkillTooltip()
{
	if (bSlotWasEmpty || !bHasCachedSkillInfo || !SkillComponent || !WBP_SkillTooltip)
	{
		return;
	}

	FFrontierGeneratedWeaponSkill GeneratedSkill;
	if (!SkillComponent->GetGeneratedSkillAtSlot(SlotIndex, GeneratedSkill))
	{
		return;
	}

	WBP_SkillTooltip->SetSkillInfo(
		CachedSkillInfo,
		SkillComponent->GetSkillLevel(GeneratedSkill.SkillTag),
		CachedSkillInfo.Cooldown);
	WBP_SkillTooltip->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UFrontierSkillSlotWidget::HideSkillTooltip()
{
	if (WBP_SkillTooltip)
	{
		WBP_SkillTooltip->ClearSkillInfo();
		WBP_SkillTooltip->SetVisibility(ESlateVisibility::Collapsed);
	}
}


FLinearColor UFrontierSkillSlotWidget::GetRarityColor() const
{
	if (!SkillComponent)
	{
		return FLinearColor(0.02f, 0.025f, 0.03f, 0.82f);
	}

	FFrontierGeneratedWeaponSkill GeneratedSkill;
	if (!SkillComponent->GetGeneratedSkillAtSlot(SlotIndex, GeneratedSkill))
	{
		return FLinearColor(0.02f, 0.025f, 0.03f, 0.82f);
	}

	switch (GeneratedSkill.SkillRarity)
	{
	case EFrontierItemRarity::Rare:
		return FLinearColor(0.08f, 0.36f, 1.0f, 0.9f);
	case EFrontierItemRarity::Epic:
		return FLinearColor(0.45f, 0.12f, 0.95f, 0.9f);
	case EFrontierItemRarity::Legendary:
		return FLinearColor(1.0f, 0.58f, 0.08f, 0.9f);
	case EFrontierItemRarity::Common:
	default:
		return FLinearColor(0.08f, 0.12f, 0.1f, 0.9f);
	}
}

void UFrontierSkillSlotWidget::UpdateCooldownMaterial(const float CooldownPercent)
{
	if (!CoolDownProgressImage)
	{
		return;
	}

	if (!CooldownMID)
	{
		CooldownMID = CoolDownProgressImage->GetDynamicMaterial();
	}

	if (!CooldownMID)
	{
		return;
	}

	CooldownMID->SetScalarParameterValue(TEXT("Percent"), CooldownPercent);
	
	const float OverlayAlpha = CooldownPercent > 0.0f ? 0.5f : 0.0f;
	CoolDownProgressImage->SetRenderOpacity(OverlayAlpha);
}

void UFrontierSkillSlotWidget::ClearSkillVisuals()
{
	HideSkillTooltip();
	CachedSkillTag = FGameplayTag();
	CachedSkillInfo = FFrontierSkillInfo();
	bHasCachedSkillInfo = false;
	bSlotWasEmpty = true;

	if (RootBorder)
	{
		RootBorder->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.03f, 0.82f));
	}

	if (SkillKeyText)
	{
		SkillKeyText->SetText(KeyText);
	}

	if (CooldownText)
	{
		CooldownText->SetText(FText::GetEmpty());
	}

	if (SkillIconImage)
	{
		SkillIconImage->SetBrushFromTexture(nullptr);
		SkillIconImage->SetOpacity(0.35f);
	}

	UpdateCooldownMaterial(0.0f);
}

void UFrontierSkillSlotWidget::RefreshStaticSkillVisuals(const FFrontierGeneratedWeaponSkill& GeneratedSkill)
{
	CachedSkillTag = GeneratedSkill.SkillTag;
	CachedSkillInfo = FFrontierSkillInfo();
	bHasCachedSkillInfo = false;
	bSlotWasEmpty = false;

	if (const FFrontierSkillInfo* SkillInfo = SkillComponent ? SkillComponent->FindSkillInfo(GeneratedSkill.SkillTag) : nullptr)
	{
		CachedSkillInfo = *SkillInfo;
		bHasCachedSkillInfo = true;
	}

	if (RootBorder)
	{
		RootBorder->SetBrushColor(GetRarityColor());
	}

	if (SkillKeyText)
	{
		SkillKeyText->SetText(KeyText);
	}

	if (SkillIconImage)
	{
		SkillIconImage->SetBrushFromTexture(bHasCachedSkillInfo ? CachedSkillInfo.Icon : nullptr);
		SkillIconImage->SetOpacity(1.0f);
	}
}
