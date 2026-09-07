#include "FrontierUpgradeCurrencySlot.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UFrontierUpgradeCurrencySlot::SetCurrency(
	const FString& InCurrencyCode,
	UTexture2D* InIcon,
	const int64 InRequiredAmount)
{
	CurrencyCode = InCurrencyCode;
	if (Image_RequiredCurrencyImage)
	{
		Image_RequiredCurrencyImage->SetBrushFromTexture(InIcon, true);
	}
	if (Text_RequiredCurrency)
	{
		Text_RequiredCurrency->SetText(FText::AsNumber(FMath::Max<int64>(0, InRequiredAmount)));
	}
}
