#include "AbilitySystem/FrontierAbilitySystemComponent.h"

#include "AbilitySystem/Effects/FrontierStaminaGameplayEffect.h"
#include "Frontier.h"
#include "Tags/FrontierGameplayTags.h"

UFrontierAbilitySystemComponent::UFrontierAbilitySystemComponent()
{
	

	SetIsReplicatedByDefault(true);
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;
}

void UFrontierAbilitySystemComponent::InitializeAbilityActorInfo(AActor* InOwnerActor, AActor* InAvatarActor)
{
	FRONTIER_LOG(Log, TEXT("Initializing ASC actor info. Owner=%s Avatar=%s"), *GetNameSafe(InOwnerActor), *GetNameSafe(InAvatarActor));
	InitAbilityActorInfo(InOwnerActor, InAvatarActor);
}

bool UFrontierAbilitySystemComponent::ApplyStaminaDelta(const float StaminaDelta)
{
	if (FMath::IsNearlyZero(StaminaDelta))
	{
		return true;
	}

	const AActor* ComponentOwner = GetOwner();
	if (!ComponentOwner || !ComponentOwner->HasAuthority())
	{
		return false;
	}

	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingSpec(
		UFrontierStaminaGameplayEffect::StaticClass(),
		1.0f,
		MakeEffectContext());
	if (!SpecHandle.IsValid() || !SpecHandle.Data.IsValid())
	{
		return false;
	}

	SpecHandle.Data->SetSetByCallerMagnitude(FFrontierGameplayTags::Get().DataStaminaDelta, StaminaDelta);
	ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	return true;
}
