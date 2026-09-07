#include "Combat/FrontierDamageStatics.h"

#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "AbilitySystem/Effects/FrontierDamageGameplayEffect.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/FrontierSkillTreeComponent.h"
#include "Frontier.h"
#include "FrontierPlayerController.h"
#include "Game/FrontierGameMode.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Perception/AISense_Damage.h"
#include "Progression/FrontierRaidExperienceSubsystem.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponDataAsset.h"

namespace
{
constexpr float DefenseRatingConstant = 100.0f;
constexpr float ResistanceRatingConstant = 100.0f;
constexpr float MinimumCombinedMitigationMultiplier = 0.20f;
constexpr float MaximumCombinedMitigationMultiplier = 2.00f;

const TCHAR* ElementalTypeToString(const EFrontierElementalType ElementalType)
{
	switch (ElementalType)
	{
	case EFrontierElementalType::None: return TEXT("None");
	case EFrontierElementalType::Normal: return TEXT("Normal");
	case EFrontierElementalType::Fire: return TEXT("Fire");
	case EFrontierElementalType::Ice: return TEXT("Ice");
	case EFrontierElementalType::Lightning: return TEXT("Lightning");
	case EFrontierElementalType::Poison: return TEXT("Poison");
	default: return TEXT("Unknown");
	}
}
	
bool IsLobbyWorld(const UWorld* World)
{
	if (!World)
	{
		return false;
	}

	const FString CurrentWorldPackageName = World->GetOutermost()->GetName();
	if (CurrentWorldPackageName.Contains(TEXT("Lobby"), ESearchCase::IgnoreCase))
	{
		return true;
	}

	const AFrontierGameMode* FrontierGameMode = World->GetAuthGameMode<AFrontierGameMode>();
	if (!FrontierGameMode)
	{
		return false;
	}

	const FString LobbyLevelPackageName = FrontierGameMode->GetLobbyLevelPackageName();
	if (LobbyLevelPackageName.IsEmpty())
	{
		return false;
	}

	return CurrentWorldPackageName.Equals(LobbyLevelPackageName, ESearchCase::CaseSensitive)
		|| CurrentWorldPackageName.EndsWith(LobbyLevelPackageName, ESearchCase::CaseSensitive);
}

// Resolves player ownership for characters, controllers, weapons, and projectiles.
AFrontierPlayerController* ResolveNumberPopPlayerController(AActor* Actor)
{
	AActor* CurrentActor = Actor;
	for (int32 OwnerDepth = 0; IsValid(CurrentActor) && OwnerDepth < 8; ++OwnerDepth)
	{
		if (AFrontierPlayerController* PlayerController =
			Cast<AFrontierPlayerController>(CurrentActor))
		{
			return PlayerController;
		}

		if (const APawn* Pawn = Cast<APawn>(CurrentActor))
		{
			if (AFrontierPlayerController* PlayerController =
				Cast<AFrontierPlayerController>(Pawn->GetController()))
			{
				return PlayerController;
			}
		}

		if (AFrontierPlayerController* PlayerController =
			Cast<AFrontierPlayerController>(CurrentActor->GetInstigatorController()))
		{
			return PlayerController;
		}

		AActor* Owner = CurrentActor->GetOwner();
		if (!IsValid(Owner) || Owner == CurrentActor)
		{
			break;
		}

		CurrentActor = Owner;
	}

	return nullptr;
}

int32 DispatchNumberPopToRelevantPlayers(
	AActor* SourceActor,
	AActor* TargetActor,
	const FFrontierNumberPopRequest& NumberPopRequest)
{
	AFrontierPlayerController* SourceController = ResolveNumberPopPlayerController(SourceActor);
	AFrontierPlayerController* TargetController = ResolveNumberPopPlayerController(TargetActor);
	int32 RecipientCount = 0;

	if (SourceController && SourceController != TargetController)
	{
		FFrontierNumberPopRequest DealtDamageRequest = NumberPopRequest;
		DealtDamageRequest.bIsReceivedDamage = false;
		SourceController->ClientAddNumberPop(DealtDamageRequest);
		++RecipientCount;
	}

	if (TargetController)
	{
		FFrontierNumberPopRequest ReceivedDamageRequest = NumberPopRequest;
		ReceivedDamageRequest.bIsReceivedDamage = true;
		TargetController->ClientAddNumberPop(ReceivedDamageRequest);
		++RecipientCount;
	}

	return RecipientCount;
}
}

bool UFrontierDamageStatics::CanActorsDamageEachOther(const AActor* SourceActor, const AActor* TargetActor)
{
	if (!IsValid(SourceActor) || !IsValid(TargetActor) || SourceActor == TargetActor)
	{
		return false;
	}

	const AFrontierBaseCharacter* SourceCharacter = Cast<AFrontierBaseCharacter>(SourceActor);
	const AFrontierBaseCharacter* TargetCharacter = Cast<AFrontierBaseCharacter>(TargetActor);
	if (!SourceCharacter || !TargetCharacter)
	{
		return true;
	}

	const EFrontierTeam SourceTeam = SourceCharacter->GetTeam();
	const EFrontierTeam TargetTeam = TargetCharacter->GetTeam();

	if (SourceTeam == EFrontierTeam::Monster && TargetTeam == EFrontierTeam::Monster)
	{
		return false;
	}

	if (SourceTeam != EFrontierTeam::None && SourceTeam == TargetTeam)
	{
		return false;
	}

	return true;
}

float UFrontierDamageStatics::GetDamageTypeMultiplier(const AActor* TargetActor, const EFrontierElementalType ElementalType)
{
	const AFrontierBaseCharacter* TargetCharacter = Cast<AFrontierBaseCharacter>(TargetActor);
	if (!TargetCharacter)
	{
		return 1.0f;
	}

	const EFrontierElementalType TargetElementalType = TargetCharacter->GetElementalType();
	if (IsElementalAdvantage(ElementalType, TargetElementalType))
	{
		return 1.25f;
	}

	if (IsElementalDisadvantage(ElementalType, TargetElementalType))
	{
		return 0.75f;
	}

	return 1.0f;
}

bool UFrontierDamageStatics::IsDamageTypeWeakness(const AActor* TargetActor, const EFrontierElementalType ElementalType)
{
	return GetDamageTypeMultiplier(TargetActor, ElementalType) > 1.0f;
}

bool UFrontierDamageStatics::IsElementalAdvantage(
	const EFrontierElementalType AttackElementalType,
	const EFrontierElementalType TargetElementalType)
{
	if (AttackElementalType == EFrontierElementalType::None
		|| AttackElementalType == EFrontierElementalType::Normal
		|| TargetElementalType == EFrontierElementalType::None
		|| TargetElementalType == EFrontierElementalType::Normal)
	{
		return false;
	}

	return (AttackElementalType == EFrontierElementalType::Fire && TargetElementalType == EFrontierElementalType::Ice)
		|| (AttackElementalType == EFrontierElementalType::Ice && TargetElementalType == EFrontierElementalType::Poison)
		|| (AttackElementalType == EFrontierElementalType::Poison && TargetElementalType == EFrontierElementalType::Lightning)
		|| (AttackElementalType == EFrontierElementalType::Lightning && TargetElementalType == EFrontierElementalType::Fire);
}

bool UFrontierDamageStatics::IsElementalDisadvantage(
	const EFrontierElementalType AttackElementalType,
	const EFrontierElementalType TargetElementalType)
{
	return IsElementalAdvantage(TargetElementalType, AttackElementalType);
}

bool UFrontierDamageStatics::AreElementsOpposed(
	const EFrontierElementalType FirstElementalType,
	const EFrontierElementalType SecondElementalType)
{
	if (FirstElementalType == EFrontierElementalType::None
		|| FirstElementalType == EFrontierElementalType::Normal
		|| SecondElementalType == EFrontierElementalType::None
		|| SecondElementalType == EFrontierElementalType::Normal)
	{
		return false;
	}

	return (FirstElementalType == EFrontierElementalType::Fire && SecondElementalType == EFrontierElementalType::Ice)
		|| (FirstElementalType == EFrontierElementalType::Ice && SecondElementalType == EFrontierElementalType::Fire)
		|| (FirstElementalType == EFrontierElementalType::Poison && SecondElementalType == EFrontierElementalType::Lightning)
		|| (FirstElementalType == EFrontierElementalType::Lightning && SecondElementalType == EFrontierElementalType::Poison);
}

float UFrontierDamageStatics::CalculateRatingDamageMultiplier(const float Rating, const float RatingConstant)
{
	const float SafeConstant = FMath::Max(RatingConstant, KINDA_SMALL_NUMBER);
	if (Rating >= 0.0f)
	{
		return SafeConstant / (SafeConstant + Rating);
	}

	// Negative ratings represent armor/resistance shred and asymptotically approach 2x damage.
	return 2.0f - (SafeConstant / (SafeConstant - Rating));
}

float UFrontierDamageStatics::GetElementAttackPower(const UFrontierAttributeSet* Attributes, const EFrontierElementalType ElementalType)
{
	if (!Attributes)
	{
		return 0.0f;
	}

	if (ElementalType == EFrontierElementalType::Fire)
	{
		return Attributes->GetFireAttackPower();
	}
	if (ElementalType == EFrontierElementalType::Ice)
	{
		return Attributes->GetIceAttackPower();
	}
	if (ElementalType == EFrontierElementalType::Lightning)
	{
		return Attributes->GetLightningAttackPower();
	}
	if (ElementalType == EFrontierElementalType::Poison)
	{
		return Attributes->GetPoisonAttackPower();
	}

	return 0.0f;
}

float UFrontierDamageStatics::GetElementResistance(const UFrontierAttributeSet* Attributes, const EFrontierElementalType ElementalType)
{
	if (!Attributes)
	{
		return 0.0f;
	}

	if (ElementalType == EFrontierElementalType::Fire)
	{
		return Attributes->GetFireResistance();
	}
	if (ElementalType == EFrontierElementalType::Ice)
	{
		return Attributes->GetIceResistance();
	}
	if (ElementalType == EFrontierElementalType::Lightning)
	{
		return Attributes->GetLightningResistance();
	}
	if (ElementalType == EFrontierElementalType::Poison)
	{
		return Attributes->GetPoisonResistance();
	}

	return 0.0f;
}

FGameplayTag UFrontierDamageStatics::GetHitGameplayCueTagForElementalType(const EFrontierElementalType ElementalType)
{
	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	switch (ElementalType)
	{
	case EFrontierElementalType::Fire: return Tags.GameplayCueHitFire;
	case EFrontierElementalType::Ice: return Tags.GameplayCueHitIce;
	case EFrontierElementalType::Lightning: return Tags.GameplayCueHitLightning;
	case EFrontierElementalType::Poison: return Tags.GameplayCueHitPoison;
	case EFrontierElementalType::None:
	case EFrontierElementalType::Normal:
	default:
		return Tags.GameplayCueHitNormal;
	}
}

void UFrontierDamageStatics::ExecuteDamageHitGameplayCue(
	AActor* SourceActor,
	AActor* TargetActor,
	UFrontierAbilitySystemComponent* TargetASC,
	const EFrontierElementalType ElementalType,
	const FGameplayTag DamageTypeTag,
	const float FinalDamage,
	const FVector& HitLocation)
{
	if (!IsValid(TargetActor) || !TargetASC)
	{
		return;
	}

	FGameplayTag GameplayCueTag = GetHitGameplayCueTagForElementalType(ElementalType);
	if (ElementalType == EFrontierElementalType::None || ElementalType == EFrontierElementalType::Normal)
	{
		const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
		if (DamageTypeTag.MatchesTagExact(Tags.DamageTypeBlunt))
		{
			GameplayCueTag = Tags.GameplayCueHitBlunt;
		}
		else if (DamageTypeTag.MatchesTagExact(Tags.DamageTypeSlash))
		{
			GameplayCueTag = Tags.GameplayCueHitSlash;
		}
		else if (DamageTypeTag.MatchesTagExact(Tags.DamageTypePierce))
		{
			GameplayCueTag = Tags.GameplayCueHitPierce;
		}
		else if (DamageTypeTag.MatchesTagExact(Tags.DamageTypePhysical))
		{
			GameplayCueTag = Tags.GameplayCueHitPhysical;
		}
	}
	if (!GameplayCueTag.IsValid())
	{
		return;
	}

	FGameplayCueParameters CueParameters;
	CueParameters.RawMagnitude = FinalDamage;
	CueParameters.NormalizedMagnitude = FinalDamage;
	CueParameters.Instigator = SourceActor;
	CueParameters.EffectCauser = SourceActor;
	CueParameters.SourceObject = SourceActor;
	CueParameters.Location = HitLocation.IsNearlyZero()
		? TargetActor->GetActorLocation()
		: HitLocation;
	CueParameters.MatchedTagName = DamageTypeTag;
	CueParameters.OriginalTag = GameplayCueTag;

	FRONTIER_LOG(VeryVerbose, TEXT("Executing damage hit gameplay cue. Target=%s DamageType=%s Element=%s Cue=%s FinalDamage=%.2f"),
		*GetNameSafe(TargetActor),
		*DamageTypeTag.ToString(),
		ElementalTypeToString(ElementalType),
		*GameplayCueTag.ToString(),
		FinalDamage);

	TargetASC->ExecuteGameplayCue(GameplayCueTag, CueParameters);
}

FFrontierDamageResult UFrontierDamageStatics::ApplyDamage(AActor* SourceActor, AActor* TargetActor, const FFrontierDamageRequest& DamageRequest)
{
	EFrontierElementalType ElementalType = DamageRequest.ElementalType;

	FRONTIER_LOG(VeryVerbose, TEXT("ApplyDamage called. Source=%s Target=%s BaseDamage=%.2f Multiplier=%.2f DamageType=%s HitReaction=%s Element=%s"),
		*GetNameSafe(SourceActor),
		*GetNameSafe(TargetActor),
		DamageRequest.BaseDamage,
		DamageRequest.DamageMultiplier,
		*DamageRequest.DamageTypeTag.ToString(),
		*DamageRequest.HitReactionTag.ToString(),
		ElementalTypeToString(ElementalType));

	FFrontierDamageResult DamageResult;

	if (!IsValid(TargetActor))
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage failed because target is invalid."));
		return DamageResult;
	}

	if (!CanActorsDamageEachOther(SourceActor, TargetActor))
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage blocked by team rules. Source=%s Target=%s"), *GetNameSafe(SourceActor), *GetNameSafe(TargetActor));
		return DamageResult;
	}

	if (!TargetActor->HasAuthority())
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage ignored on non-authority target %s."), *GetNameSafe(TargetActor));
		return DamageResult;
	}

	if (IsLobbyWorld(TargetActor->GetWorld()))
	{
		if (AFrontierBaseCharacter* LobbyTargetCharacter = Cast<AFrontierBaseCharacter>(TargetActor))
		{
			LobbyTargetCharacter->PlayCosmeticHitReaction(DamageRequest.DamageTypeTag);
		}

		FRONTIER_LOG(VeryVerbose, TEXT("ApplyDamage ignored because the target is in the lobby world. Source=%s Target=%s"), *GetNameSafe(SourceActor), *GetNameSafe(TargetActor));
		return DamageResult;
	}

	AFrontierBaseCharacter* SourceCharacter = Cast<AFrontierBaseCharacter>(SourceActor);
	AFrontierBaseCharacter* TargetCharacter = Cast<AFrontierBaseCharacter>(TargetActor);

	// Normal skills inherit the elemental type of the player's currently equipped weapon.
	// Explicit elemental skills keep their own type.
	if (ElementalType == EFrontierElementalType::Normal)
	{
		if (const AFrontierPlayerCharacter* SourcePlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceActor))
		{
			if (const UFrontierEquipmentComponent* Equipment = SourcePlayerCharacter->GetEquipmentComponent())
			{
				ElementalType = Equipment->GetCurrentWeaponElementalType();
			}
		}
	}

	if (const AFrontierPlayerCharacter* TargetPlayerCharacter = Cast<AFrontierPlayerCharacter>(TargetActor))
	{
		if (TargetPlayerCharacter->IsRolling())
		{
			FRONTIER_LOG(VeryVerbose, TEXT("ApplyDamage ignored because target player is currently rolling. Target=%s"), *GetNameSafe(TargetActor));
			return DamageResult;
		}
	}

	UFrontierAbilitySystemComponent* SourceASC = SourceCharacter ? SourceCharacter->GetFrontierAbilitySystemComponent() : SourceActor->FindComponentByClass<UFrontierAbilitySystemComponent>();

	UFrontierAbilitySystemComponent* TargetASC = TargetCharacter ? TargetCharacter->GetFrontierAbilitySystemComponent() : TargetActor->FindComponentByClass<UFrontierAbilitySystemComponent>();
	UFrontierAttributeSet* TargetAttributes = TargetCharacter
		? const_cast<UFrontierAttributeSet*>(TargetCharacter->GetFrontierAttributeSet())
		: (TargetASC ? const_cast<UFrontierAttributeSet*>(TargetASC->GetSet<UFrontierAttributeSet>()) : nullptr);

	if (!SourceASC || !TargetASC || !TargetAttributes)
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage failed because source/target ASC or target AttributeSet is missing. Source=%s Target=%s"),
			*GetNameSafe(SourceActor),
			*GetNameSafe(TargetActor));
		return DamageResult;
	}

	float SourceAttackPower = 0.0f;
	float SourceElementAttackPower = 0.0f;
	float SourceWeaponTypeAttackPower = 0.0f;
	float SourceWeaponElementAttackPower = 0.0f;
	float TargetDefense = 0.0f;
	float TargetElementResistance = 0.0f;
	if (SourceCharacter)
	{
		if (const UFrontierAttributeSet* SourceAttributes = SourceCharacter->GetFrontierAttributeSet())
		{
			SourceAttackPower = SourceAttributes->GetAttackPower();
			SourceElementAttackPower = GetElementAttackPower(SourceAttributes, ElementalType);
			if (const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceCharacter))
			{
				const UFrontierEquipmentComponent* Equipment = PlayerCharacter->GetEquipmentComponent();
				const UFrontierWeaponDataAsset* WeaponData = Equipment ? Equipment->GetCurrentWeaponData() : nullptr;
				if (WeaponData)
				{
					const AFrontierPlayerState* PlayerState = PlayerCharacter->GetPlayerState<AFrontierPlayerState>();
					const UFrontierSkillTreeComponent* SkillTree = PlayerState
						? PlayerState->GetSkillTreeComponent()
						: nullptr;
					SourceWeaponElementAttackPower = SkillTree
						? SkillTree->GetWeaponElementAttackPowerBonus(WeaponData->WeaponTypeTag, ElementalType)
						: 0.0f;
				}
				if (WeaponData && WeaponData->WeaponTypeTag.MatchesTag(FFrontierGameplayTags::Get().WeaponTypeSword))
				{
					SourceWeaponTypeAttackPower = SourceAttributes->GetSwordAttackPower();
				}
				else if (WeaponData && WeaponData->WeaponTypeTag.MatchesTag(FFrontierGameplayTags::Get().WeaponTypeAxe))
				{
					SourceWeaponTypeAttackPower = SourceAttributes->GetAxeAttackPower();
				}
			}
		}
	}

	if (TargetCharacter)
	{
		if (const UFrontierAttributeSet* TargetAttributeSet = TargetCharacter->GetFrontierAttributeSet())
		{
			TargetDefense = TargetAttributeSet->GetDefense();
			TargetElementResistance = GetElementResistance(TargetAttributeSet, ElementalType);
		}
	}

	const float TypeMultiplier = GetDamageTypeMultiplier(TargetActor, ElementalType);
	const float ResolvedElementDamageMultiplier = DamageRequest.ElementDamageMultiplier >= 0.0f
		? DamageRequest.ElementDamageMultiplier
		: DamageRequest.DamageMultiplier;
	const float RawDamage = DamageRequest.BaseDamage
		+ (SourceAttackPower * DamageRequest.DamageMultiplier)
		+ (SourceWeaponTypeAttackPower * DamageRequest.DamageMultiplier)
		+ (SourceElementAttackPower * ResolvedElementDamageMultiplier)
		+ (SourceWeaponElementAttackPower * ResolvedElementDamageMultiplier);
	const float DefenseMultiplier = CalculateRatingDamageMultiplier(TargetDefense, DefenseRatingConstant);
	const float ResistanceMultiplier = CalculateRatingDamageMultiplier(TargetElementResistance, ResistanceRatingConstant);
	const float CombinedMitigationMultiplier = FMath::Clamp(
		DefenseMultiplier * ResistanceMultiplier,
		MinimumCombinedMitigationMultiplier,
		MaximumCombinedMitigationMultiplier);
	const float MitigatedDamage = RawDamage * TypeMultiplier * CombinedMitigationMultiplier;
	const float FinalDamage = RawDamage > KINDA_SMALL_NUMBER ? FMath::Max(1.0f, MitigatedDamage) : 0.0f;
	if (FinalDamage <= KINDA_SMALL_NUMBER)
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage skipped because final damage is zero. Target=%s"), *GetNameSafe(TargetActor));
		return DamageResult;
	}

	const float OldHealth = TargetAttributes->GetHealth();

	FGameplayEffectContextHandle EffectContext = SourceASC->MakeEffectContext();
	EffectContext.AddSourceObject(SourceActor);
	if (!DamageRequest.HitLocation.IsNearlyZero())
	{
		FHitResult DamageHitResult;
		DamageHitResult.Location = DamageRequest.HitLocation;
		DamageHitResult.ImpactPoint = DamageRequest.HitLocation;
		EffectContext.AddHitResult(DamageHitResult, true);
	}

	const TSubclassOf<UGameplayEffect> DamageEffectClass = DamageRequest.DamageEffectClass
		? DamageRequest.DamageEffectClass
		: TSubclassOf<UGameplayEffect>(UFrontierDamageGameplayEffect::StaticClass());

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		1.0f,
		EffectContext);

	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		FRONTIER_LOG(Warning, TEXT("ApplyDamage failed because damage effect spec creation failed. Source=%s Target=%s"),
			*GetNameSafe(SourceActor),
			*GetNameSafe(TargetActor));
		return DamageResult;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataDamage, FinalDamage);
	SpecHandle.Data->SetSetByCallerMagnitude(
		FFrontierGameplayTags::Get().DataHitReactionLevel,
		static_cast<float>(DamageRequest.HitReactionLevel));
	SpecHandle.Data->SetSetByCallerMagnitude(
		FFrontierGameplayTags::Get().DataHitReactionStaggerDuration,
		FMath::Max(0.0f, DamageRequest.StaggerDuration));
	SpecHandle.Data->SetSetByCallerMagnitude(
		FFrontierGameplayTags::Get().DataHitReactionKnockbackHorizontalStrength,
		FMath::Max(0.0f, DamageRequest.KnockbackHorizontalStrength));
	SpecHandle.Data->SetSetByCallerMagnitude(
		FFrontierGameplayTags::Get().DataHitReactionKnockbackVerticalStrength,
		FMath::Max(0.0f, DamageRequest.KnockbackVerticalStrength));
	if (DamageRequest.DamageTypeTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(DamageRequest.DamageTypeTag);
	}
	if (DamageRequest.HitReactionTag.IsValid())
	{
		SpecHandle.Data->AddDynamicAssetTag(DamageRequest.HitReactionTag);
	}
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	for (TSubclassOf<UGameplayEffect> AdditionalEffectClass : DamageRequest.AdditionalEffectClasses)
	{
		if (!AdditionalEffectClass)
		{
			continue;
		}

		FGameplayEffectSpecHandle AdditionalSpecHandle = SourceASC->MakeOutgoingSpec(
			AdditionalEffectClass,
			1.0f,
			EffectContext);

		if (AdditionalSpecHandle.IsValid() && AdditionalSpecHandle.Data.IsValid())
		{
			TargetASC->ApplyGameplayEffectSpecToSelf(*AdditionalSpecHandle.Data.Get());
		}
	}

	const float NewHealth = TargetAttributes->GetHealth();

	DamageResult.bApplied = true;
	DamageResult.bHit = true;
	const float EffectiveDamage = FMath::Max(0.0f, OldHealth - NewHealth);
	DamageResult.AppliedDamage = EffectiveDamage;
	DamageResult.RawDamage = RawDamage;
	DamageResult.SourceAttackPower = SourceAttackPower;
	DamageResult.SourceElementAttackPower = SourceElementAttackPower;
	DamageResult.SourceWeaponTypeAttackPower = SourceWeaponTypeAttackPower;
	DamageResult.SourceWeaponElementAttackPower = SourceWeaponElementAttackPower;
	DamageResult.TargetDefense = TargetDefense;
	DamageResult.TargetElementResistance = TargetElementResistance;
	DamageResult.DefenseMultiplier = DefenseMultiplier;
	DamageResult.ResistanceMultiplier = ResistanceMultiplier;
	DamageResult.AffinityMultiplier = TypeMultiplier;
	DamageResult.bTargetDied = NewHealth <= 0.0f;

	if (EffectiveDamage > KINDA_SMALL_NUMBER
		&& IsHeavyHitReaction(DamageRequest.HitReactionLevel))
	{
		if (AFrontierPlayerController* SourceController = ResolveNumberPopPlayerController(SourceActor))
		{
			const FVector FeedbackHitLocation = DamageRequest.HitLocation.IsNearlyZero()
				? TargetActor->GetActorLocation()
				: DamageRequest.HitLocation;
			SourceController->ClientPlayHeavyHitFeedback(FeedbackHitLocation);
		}
	}

	if (EffectiveDamage > KINDA_SMALL_NUMBER)
	{
		FVector DisplayLocation = DamageRequest.HitLocation;
		if (DisplayLocation.IsNearlyZero())
		{
			FVector BoundsExtent;
			TargetActor->GetActorBounds(true, DisplayLocation, BoundsExtent);
			DisplayLocation.Z += FMath::Max(BoundsExtent.Z * 0.5f, 50.0f);
		}

		FFrontierNumberPopRequest NumberPopRequest;
		NumberPopRequest.WorldLocation = DisplayLocation;
		NumberPopRequest.TargetActor = TargetActor;
		NumberPopRequest.NumberToDisplay = FMath::Max(0, FMath::RoundToInt(EffectiveDamage));
		NumberPopRequest.ElementalType = ElementalType;

		const int32 RecipientCount = DispatchNumberPopToRelevantPlayers(
			SourceActor,
			TargetActor,
			NumberPopRequest);
		if (RecipientCount > 0)
		{
			FRONTIER_LOG(Log, TEXT("[NumberPop] Dispatched number pop. Source=%s Target=%s Damage=%d Recipients=%d Location=%s"),
				*GetNameSafe(SourceActor),
				*GetNameSafe(TargetActor),
				NumberPopRequest.NumberToDisplay,
				RecipientCount,
				*NumberPopRequest.WorldLocation.ToString());
		}
		else
		{
			FRONTIER_LOG(Warning, TEXT("[NumberPop] Number pop had no player recipients. Source=%s Target=%s Damage=%d"),
				*GetNameSafe(SourceActor),
				*GetNameSafe(TargetActor),
				NumberPopRequest.NumberToDisplay);
		}
	}

	if (EffectiveDamage > KINDA_SMALL_NUMBER && TargetCharacter)
	{
		AFrontierPlayerCharacter* SourcePlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceActor);
		if (!SourcePlayerCharacter && SourceActor)
		{
			SourcePlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceActor->GetInstigator());
		}
		if (!SourcePlayerCharacter && SourceActor)
		{
			SourcePlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceActor->GetOwner());
		}

		if (AFrontierPlayerController* SourcePlayerController = SourcePlayerCharacter
			? Cast<AFrontierPlayerController>(SourcePlayerCharacter->GetController())
			: nullptr)
		{
			const float MaxHealth = FMath::Max(TargetAttributes->GetMaxHealth(), 1.0f);
			SourcePlayerController->ClientRevealDamagedTarget(TargetCharacter, NewHealth / MaxHealth);
		}
	}

	if (UWorld* World = TargetActor->GetWorld())
	{
		if (UFrontierRaidExperienceSubsystem* RaidExperience = World->GetSubsystem<UFrontierRaidExperienceSubsystem>())
		{
			RaidExperience->RecordAppliedDamage(
				SourceActor,
				TargetActor,
				EffectiveDamage,
				TargetAttributes->GetMaxHealth(),
				DamageResult.bTargetDied,
				DamageRequest.DamageEventId);
		}
	}

	if (IsValid(SourceActor))
	{
		UAISense_Damage::ReportDamageEvent(
			TargetActor,
			TargetActor,
			SourceActor,
			FinalDamage,
			SourceActor->GetActorLocation(),
			TargetActor->GetActorLocation());
	}

	ExecuteDamageHitGameplayCue(
		SourceActor,
		TargetActor,
		TargetASC,
		ElementalType,
		DamageRequest.DamageTypeTag,
		FinalDamage,
		DamageRequest.HitLocation);

	FRONTIER_LOG(VeryVerbose, TEXT("ApplyDamage succeeded. Target=%s FinalDamage=%.2f RawDamage=%.2f BaseDamage=%.2f AttackPower=%.2f ElementAttackPower=%.2f WeaponElementAttackPower=%.2f AttackMultiplier=%.2f ElementMultiplier=%.2f AffinityMultiplier=%.2f TargetDefense=%.2f DefenseMultiplier=%.3f TargetResistance=%.2f ResistanceMultiplier=%.3f OldHealth=%.2f NewHealth=%.2f bTargetDied=%d Element=%s"),
		*GetNameSafe(TargetActor),
		FinalDamage,
		RawDamage,
		DamageRequest.BaseDamage,
		SourceAttackPower,
		SourceElementAttackPower,
		SourceWeaponElementAttackPower,
		DamageRequest.DamageMultiplier,
		ResolvedElementDamageMultiplier,
		TypeMultiplier,
		TargetDefense,
		DefenseMultiplier,
		TargetElementResistance,
		ResistanceMultiplier,
		OldHealth,
		NewHealth,
		DamageResult.bTargetDied,
		ElementalTypeToString(ElementalType));

	return DamageResult;
}

bool UFrontierDamageStatics::IsHeavyHitReaction(const EFrontierHitReactionLevel HitReactionLevel)
{
	switch (HitReactionLevel)
	{
	case EFrontierHitReactionLevel::KnockBack:
		return true;
	case EFrontierHitReactionLevel::None:
	case EFrontierHitReactionLevel::Light:
	case EFrontierHitReactionLevel::Stagger:
	default:
		return false;
	}
}
