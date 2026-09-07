#include "Combat/FrontierNumberPopComponent_NiagaraText.h"

#include "Frontier.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFloat.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/ConstructorHelpers.h"

UFrontierNumberPopComponent_NiagaraText::UFrontierNumberPopComponent_NiagaraText(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UNiagaraSystem> DefaultDamageNumberSystem(
		TEXT("/Game/Effects/Particles/Impacts/NS_DamageNumbers.NS_DamageNumbers"));
	if (DefaultDamageNumberSystem.Succeeded())
	{
		TextNiagara = DefaultDamageNumberSystem.Object;
	}

	ElementalDamageColors.Add(EFrontierElementalType::None, FLinearColor::White);
	ElementalDamageColors.Add(EFrontierElementalType::Normal, FLinearColor::White);
	ElementalDamageColors.Add(EFrontierElementalType::Fire, FLinearColor(1.0f, 0.12f, 0.02f, 1.0f));
	ElementalDamageColors.Add(EFrontierElementalType::Ice, FLinearColor(0.15f, 0.75f, 1.0f, 1.0f));
	ElementalDamageColors.Add(EFrontierElementalType::Lightning, FLinearColor(1.0f, 0.85f, 0.05f, 1.0f));
	ElementalDamageColors.Add(EFrontierElementalType::Poison, FLinearColor(0.2f, 1.0f, 0.15f, 1.0f));
}

FLinearColor UFrontierNumberPopComponent_NiagaraText::ResolveDamageColor(
	const FFrontierNumberPopRequest& Request) const
{
	if (Request.bIsReceivedDamage)
	{
		return ReceivedDamageColor;
	}

	if (Request.bIsCriticalDamage)
	{
		return CriticalDamageColor;
	}

	if (const FLinearColor* ElementalColor = ElementalDamageColors.Find(Request.ElementalType))
	{
		return *ElementalColor;
	}

	return FLinearColor::White;
}

void UFrontierNumberPopComponent_NiagaraText::AddNumberPop(
	const FFrontierNumberPopRequest& NewRequest)
{
	if (NewRequest.NumberToDisplay <= 0
		|| !TextNiagara
		|| NiagaraArrayName.IsNone()
		|| NiagaraColorParameterName.IsNone())
	{
		FRONTIER_LOG(Warning, TEXT("[NumberPop] Request rejected. Damage=%d Niagara=%s ArrayName=%s ColorParameter=%s"),
			NewRequest.NumberToDisplay,
			*GetNameSafe(TextNiagara),
			*NiagaraArrayName.ToString(),
			*NiagaraColorParameterName.ToString());
		return;
	}

	FVector DisplayLocation = NewRequest.WorldLocation;

	UNiagaraComponent* SpawnedNiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetOwner(),
		TextNiagara,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		FVector::OneVector,
		true,
		false,
		ENCPoolMethod::AutoRelease,
		false);

	if (!SpawnedNiagaraComponent)
	{
		FRONTIER_LOG(Warning, TEXT("[NumberPop] Niagara spawn failed. Niagara=%s Target=%s"),
			*GetNameSafe(TextNiagara),
			*GetNameSafe(NewRequest.TargetActor));
		return;
	}

	SpawnedNiagaraComponent->SetForceSolo(true);
	SpawnedNiagaraComponent->SetTickBehavior(ENiagaraTickBehavior::ForceTickFirst);
	SpawnedNiagaraComponent->SetBoundsScale(1.0f);
	SpawnedNiagaraComponent->SetVisibility(true, true);
	SpawnedNiagaraComponent->SetHiddenInGame(false, true);

	if (!UNiagaraFunctionLibrary::GetDataInterface<UNiagaraDataInterfaceArrayFloat4>(
		SpawnedNiagaraComponent,
		NiagaraArrayName))
	{
		FRONTIER_LOG(Warning, TEXT("[NumberPop] Niagara system does not expose a Float4 array named %s. Niagara=%s"),
			*NiagaraArrayName.ToString(),
			*GetNameSafe(TextNiagara));
		SpawnedNiagaraComponent->DestroyComponent();
		return;
	}

	const int32 DisplayNumber = NewRequest.bIsCriticalDamage
		? -NewRequest.NumberToDisplay
		: NewRequest.NumberToDisplay;
	const FLinearColor DamageColor = ResolveDamageColor(NewRequest);
	SpawnedNiagaraComponent->SetVariableLinearColor(
		NiagaraColorParameterName,
		DamageColor);

	// Store the hit point in world space so the Niagara payload and system
	// origin use the same impact location.
	const FVector NiagaraPayloadLocation = DisplayLocation;

	TArray<FVector4> DamageList;
	DamageList.Emplace(
		NiagaraPayloadLocation.X,
		NiagaraPayloadLocation.Y,
		NiagaraPayloadLocation.Z,
		static_cast<double>(DisplayNumber));
	UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(
		SpawnedNiagaraComponent,
		NiagaraArrayName,
		DamageList);

	// The first system tick must see the request. An empty first activation can
	// complete before the append-driven emitter observes DamageInfo.
	SpawnedNiagaraComponent->Activate(true);

	const TArray<FVector4> AppliedDamageList =
		UNiagaraDataInterfaceArrayFunctionLibrary::GetNiagaraArrayVector4(
			SpawnedNiagaraComponent,
			NiagaraArrayName);
	FRONTIER_LOG(Log, TEXT("[NumberPop] Submitted to Niagara. Damage=%d Received=%d Element=%d Color=%s ArrayCount=%d Active=%d WorldLocation=%s PayloadLocation=%s Target=%s"),
		DisplayNumber,
		NewRequest.bIsReceivedDamage,
		static_cast<int32>(NewRequest.ElementalType),
		*DamageColor.ToString(),
		AppliedDamageList.Num(),
		SpawnedNiagaraComponent->IsActive(),
		*DisplayLocation.ToString(),
		*NiagaraPayloadLocation.ToString(),
		*GetNameSafe(NewRequest.TargetActor));
}
