#include "FrontierUserCurrencySlot.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"

void UFrontierUserCurrencySlot::SetCurrency(const ECurrencyType CurrencyType, const int64 Balance)
{
	if (currencyImage)
	{
		const TObjectPtr<UTexture2D>* CurrencyTexture = CurrencyImageMap.Find(CurrencyType);
		currencyImage->SetBrushFromTexture(CurrencyTexture ? CurrencyTexture->Get() : nullptr, true);
	}

	if (currencyTextBlock)
	{
		currencyTextBlock->SetText(FText::AsNumber(Balance));
	}
}
