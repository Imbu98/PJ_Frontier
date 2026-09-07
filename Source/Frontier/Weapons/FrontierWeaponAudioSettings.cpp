#include "Weapons/FrontierWeaponAudioSettings.h"

#include "Tags/FrontierGameplayTags.h"

const FFrontierMeleeWeaponAudioData* UFrontierWeaponAudioSettings::FindMeleeAudioForWeaponType(
	const FGameplayTag WeaponTypeTag) const
{
	if (!WeaponTypeTag.IsValid())
	{
		return nullptr;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	if (WeaponTypeTag.MatchesTag(Tags.WeaponTypeSword))
	{
		return &SwordAudio;
	}
	if (WeaponTypeTag.MatchesTag(Tags.WeaponTypeAxe))
	{
		return &AxeAudio;
	}
	if (WeaponTypeTag.MatchesTag(Tags.WeaponTypeSpear))
	{
		return &SpearAudio;
	}

	return nullptr;
}

const FFrontierBowWeaponAudioData* UFrontierWeaponAudioSettings::FindBowAudioForWeaponType(
	const FGameplayTag WeaponTypeTag) const
{
	return WeaponTypeTag.IsValid()
		&& WeaponTypeTag.MatchesTag(FFrontierGameplayTags::Get().WeaponTypeBow)
		? &BowAudio
		: nullptr;
}
