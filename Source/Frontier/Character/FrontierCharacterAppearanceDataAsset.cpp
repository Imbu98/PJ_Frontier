#include "Character/FrontierCharacterAppearanceDataAsset.h"

const FFrontierCharacterAppearanceData* UFrontierCharacterAppearanceDataAsset::FindAppearance(
	const EFrontierCharacterType CharacterType) const
{
	for (const FFrontierCharacterAppearanceData& Appearance : CharacterAppearances)
	{
		if (Appearance.CharacterType == CharacterType)
		{
			return &Appearance;
		}
	}

	if (CharacterType != EFrontierCharacterType::DarkKnight)
	{
		for (const FFrontierCharacterAppearanceData& Appearance : CharacterAppearances)
		{
			if (Appearance.CharacterType == EFrontierCharacterType::DarkKnight)
			{
				return &Appearance;
			}
		}
	}

	return nullptr;
}
