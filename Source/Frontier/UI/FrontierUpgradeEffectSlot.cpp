#include "FrontierUpgradeEffectSlot.h"

#include "Components/TextBlock.h"

void UFrontierUpgradeEffectSlot::SetEffect(const FText& InEffectName, const FText& InEffectAmount)
{
	if (Text_EffectName)
	{
		Text_EffectName->SetText(InEffectName);
	}
	if (Text_EffectAmountText)
	{
		Text_EffectAmountText->SetText(InEffectAmount);
	}
}
