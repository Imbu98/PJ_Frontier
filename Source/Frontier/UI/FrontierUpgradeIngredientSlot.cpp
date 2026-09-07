#include "FrontierUpgradeIngredientSlot.h"

#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Inventory/Items/FrontierItemCatalogSubsystem.h"

void UFrontierUpgradeIngredientSlot::SetIngredient(
	const FName InItemTemplateId,
	UTexture2D* InIcon,
	const int32 InOwnedAmount,
	const int32 InRequiredAmount)
{
	ItemTemplateId = InItemTemplateId;
	if (Image_IngredientImage)
	{
		Image_IngredientImage->SetBrushFromTexture(InIcon, true);
	}
	if (Text_IngrediantName)
	{
		FText IngredientName = FText::FromName(InItemTemplateId);
		if (const UGameInstance* GameInstance = GetGameInstance())
		{
			if (const UFrontierItemCatalogSubsystem* Catalog =
				GameInstance->GetSubsystem<UFrontierItemCatalogSubsystem>())
			{
				if (const FFrontierResolvedItemTemplateData* TemplateData =
					Catalog->ResolveItemTemplateData(InItemTemplateId))
				{
					if (!TemplateData->Common.DisplayName.IsEmpty())
					{
						IngredientName = TemplateData->Common.DisplayName;
					}
				}
			}
		}
		Text_IngrediantName->SetText(IngredientName);
	}

	const bool bHasEnoughIngredients = InOwnedAmount >= InRequiredAmount;
	const FSlateColor AmountColor = bHasEnoughIngredients
		? FSlateColor(FLinearColor::Green)
		: FSlateColor(FLinearColor::Red);
	if (Text_OwnedIngredientAmount)
	{
		Text_OwnedIngredientAmount->SetText(FText::AsNumber(FMath::Max(0, InOwnedAmount)));
		Text_OwnedIngredientAmount->SetColorAndOpacity(AmountColor);
	}
	if (Text_RequiredIngredientAmount)
	{
		Text_RequiredIngredientAmount->SetText(FText::AsNumber(FMath::Max(0, InRequiredAmount)));
		Text_RequiredIngredientAmount->SetColorAndOpacity(AmountColor);
	}
}
