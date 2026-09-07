#include "UI/FrontierItemRarityBorderWidget.h"

#include "Components/Border.h"

void UFrontierItemRarityBorderWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (bHasCurrentRarity)
	{
		SetItemRarity(CurrentRarity);
	}
	else
	{
		ClearRarity();
	}
}

void UFrontierItemRarityBorderWidget::SetItemRarity(const EFrontierItemRarity Rarity)
{
	CurrentRarity = Rarity;
	bHasCurrentRarity = true;
	if (RarityBorder)
	{
		RarityBorder->SetBrushColor(GetColorForRarity(Rarity));
		RarityBorder->SetVisibility(ESlateVisibility::Visible);
	}
}

void UFrontierItemRarityBorderWidget::SetItemRarityTag(const FGameplayTag RarityTag)
{
	EFrontierItemRarity ParsedRarity = EFrontierItemRarity::Common;
	if (RarityTag.IsValid() && TryParseItemRarity(RarityTag.ToString(), ParsedRarity))
	{
		SetItemRarity(ParsedRarity);
	}
	else
	{
		ClearRarity();
	}
}

void UFrontierItemRarityBorderWidget::ClearRarity()
{
	bHasCurrentRarity = false;
	if (RarityBorder)
	{
		RarityBorder->SetBrushColor(NoneColor);
		RarityBorder->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

FLinearColor UFrontierItemRarityBorderWidget::GetColorForRarity(const EFrontierItemRarity Rarity) const
{
	switch (Rarity)
	{
	case EFrontierItemRarity::Rare:
		return RareColor;
	case EFrontierItemRarity::Epic:
		return EpicColor;
	case EFrontierItemRarity::Legendary:
		return LegendaryColor;
	case EFrontierItemRarity::Common:
	default:
		return CommonColor;
	}
}
