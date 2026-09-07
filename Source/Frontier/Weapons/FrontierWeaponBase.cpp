#include "Weapons/FrontierWeaponBase.h"

#include "Character/FrontierPlayerCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "Weapons/FrontierWeaponAudioSettings.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/UnrealType.h"
#include "Weapons/FrontierWeaponDataAsset.h"

AFrontierWeaponBase::AFrontierWeaponBase()
{
	bReplicates = true;
	SetReplicateMovement(false);

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetGenerateOverlapEvents(false);

	EnhancementAuraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EnhancementAura"));
	EnhancementAuraComponent->SetupAttachment(WeaponMesh);
	EnhancementAuraComponent->SetAutoActivate(false);
	EnhancementAuraComponent->SetVisibility(false, true);
}

void AFrontierWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFrontierWeaponBase, WeaponData);
	DOREPLIFETIME(AFrontierWeaponBase, EnhancementLevel);
}

void AFrontierWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	ApplyWeaponData();
	ApplyEnhancementAura();
}

void AFrontierWeaponBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyWeaponData();
	ApplyEnhancementAura();
}

#if WITH_EDITOR
void AFrontierWeaponBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property
		&& PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AFrontierWeaponBase, EnhancementOverlayMaterial))
	{
		EnhancementOverlayMaterialInstance = nullptr;
	}

	if (!PropertyChangedEvent.Property
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AFrontierWeaponBase, EnhancementLevel)
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AFrontierWeaponBase, EnhancementAuraSystem)
		|| PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AFrontierWeaponBase, EnhancementOverlayMaterial))
	{
		ApplyEnhancementAura();
	}
}
#endif

USkeletalMeshComponent* AFrontierWeaponBase::GetWeaponMesh() const
{
	return WeaponMesh;
}

UFrontierWeaponDataAsset* AFrontierWeaponBase::GetWeaponData() const
{
	return WeaponData;
}

FName AFrontierWeaponBase::GetCharacterAttachSocketName() const
{
	return WeaponData ? WeaponData->CharacterAttachSocketName : TEXT("WeaponSocket");
}

const FTransform& AFrontierWeaponBase::GetEquipOffset() const
{
	static const FTransform IdentityTransform = FTransform::Identity;
	return WeaponData ? WeaponData->EquipOffset : IdentityTransform;
}

void AFrontierWeaponBase::SetWeaponData(UFrontierWeaponDataAsset* InWeaponData)
{
	WeaponData = InWeaponData;
	ApplyWeaponData();
}

void AFrontierWeaponBase::SetEnhancementLevel(const int32 InEnhancementLevel)
{
	EnhancementLevel = FMath::Clamp(InEnhancementLevel, 0, 9);
	ApplyEnhancementAura();
}

void AFrontierWeaponBase::SetWeaponActive(const bool bNewActive)
{
	SetActorHiddenInGame(!bNewActive);
	SetActorEnableCollision(false);

	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(bNewActive, true);
	}

	ApplyEnhancementAura();
}

void AFrontierWeaponBase::PlayAttackSound(const EFrontierWeaponAttackSound SoundType)
{
	if (SoundType == EFrontierWeaponAttackSound::AttackVoice)
	{
		// The server selects once so every client hears the same random voice.
		if (HasAuthority())
		{
			MulticastPlayAttackSound(SoundType, ChooseRandomAttackVoiceIndex());
		}
		return;
	}

	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		PlayAttackSoundLocal(SoundType);
	}

	if (HasAuthority())
	{
		MulticastPlayAttackSound(SoundType, INDEX_NONE);
	}
}

void AFrontierWeaponBase::MulticastPlayAttackSound_Implementation(
	const EFrontierWeaponAttackSound SoundType,
	const int32 VoiceSoundIndex)
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (SoundType != EFrontierWeaponAttackSound::AttackVoice
		&& PawnOwner
		&& PawnOwner->IsLocallyControlled())
	{
		// The autonomous player already played this sound as part of local prediction.
		return;
	}

	PlayAttackSoundLocal(SoundType, VoiceSoundIndex);
}

void AFrontierWeaponBase::StopBowDrawSound()
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		StopBowDrawSoundLocal();
	}

	if (HasAuthority())
	{
		MulticastStopBowDrawSound();
	}
}

void AFrontierWeaponBase::MulticastStopBowDrawSound_Implementation()
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (PawnOwner && PawnOwner->IsLocallyControlled())
	{
		return;
	}

	StopBowDrawSoundLocal();
}

void AFrontierWeaponBase::PlayAttackSoundLocal(
	const EFrontierWeaponAttackSound SoundType,
	const int32 VoiceSoundIndex)
{
	if (SoundType == EFrontierWeaponAttackSound::BowDraw)
	{
		StopBowDrawSoundLocal();
	}
	else if (SoundType == EFrontierWeaponAttackSound::BowRelease)
	{
		// A quick release must cut off even a long or looping draw sound before firing audio starts.
		StopBowDrawSoundLocal();
	}

	UWorld* World = GetWorld();
	USoundBase* Sound = ResolveAttackSound(SoundType, VoiceSoundIndex);
	USceneComponent* AttachComponent = SoundType == EFrontierWeaponAttackSound::AttackVoice && GetOwner()
		? GetOwner()->GetRootComponent()
		: WeaponMesh.Get();
	if (!World || World->GetNetMode() == NM_DedicatedServer || !AttachComponent || !Sound)
	{
		return;
	}

	UAudioComponent* SpawnedAudioComponent = UGameplayStatics::SpawnSoundAttached(
		Sound,
		AttachComponent,
		SoundType == EFrontierWeaponAttackSound::AttackVoice ? NAME_None : AttackSoundAttachSocketName,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		EAttachLocation::KeepRelativeOffset,
		false,
		AttackSoundVolumeMultiplier,
		AttackSoundPitchMultiplier);

	if (SoundType == EFrontierWeaponAttackSound::BowDraw)
	{
		ActiveBowDrawAudioComponent = SpawnedAudioComponent;
	}
}

void AFrontierWeaponBase::StopBowDrawSoundLocal()
{
	if (IsValid(ActiveBowDrawAudioComponent))
	{
		ActiveBowDrawAudioComponent->Stop();
	}
	ActiveBowDrawAudioComponent = nullptr;
}

USoundBase* AFrontierWeaponBase::ResolveAttackSound(
	const EFrontierWeaponAttackSound SoundType,
	const int32 VoiceSoundIndex) const
{
	switch (SoundType)
	{
	case EFrontierWeaponAttackSound::MeleeAttack:
		return LoadedMeleeAttackSound;
	case EFrontierWeaponAttackSound::BowDraw:
		return LoadedBowDrawSound;
	case EFrontierWeaponAttackSound::BowRelease:
		return LoadedBowReleaseSound;
	case EFrontierWeaponAttackSound::AttackVoice:
		return LoadedAttackVoiceSounds.IsValidIndex(VoiceSoundIndex)
			? LoadedAttackVoiceSounds[VoiceSoundIndex]
			: nullptr;
	default:
		return nullptr;
	}
}

int32 AFrontierWeaponBase::ChooseRandomAttackVoiceIndex()
{
	const int32 VoiceCount = LoadedAttackVoiceSounds.Num();
	if (VoiceCount <= 0)
	{
		LastAttackVoiceSoundIndex = INDEX_NONE;
		return INDEX_NONE;
	}

	if (VoiceCount == 1)
	{
		LastAttackVoiceSoundIndex = 0;
		return 0;
	}

	int32 SelectedIndex = INDEX_NONE;
	if (!LoadedAttackVoiceSounds.IsValidIndex(LastAttackVoiceSoundIndex))
	{
		SelectedIndex = FMath::RandHelper(VoiceCount);
	}
	else
	{
		SelectedIndex = FMath::RandHelper(VoiceCount - 1);
		if (SelectedIndex >= LastAttackVoiceSoundIndex)
		{
			++SelectedIndex;
		}
	}
	LastAttackVoiceSoundIndex = SelectedIndex;
	return SelectedIndex;
}

UMaterialInterface* AFrontierWeaponBase::GetEnhancementOverlayMaterial() const
{
	return EnhancementOverlayMaterial.LoadSynchronous();
}

void AFrontierWeaponBase::ApplyWeaponData()
{
	StopBowDrawSoundLocal();
	LoadedMeleeAttackSound = nullptr;
	LoadedBowDrawSound = nullptr;
	LoadedBowReleaseSound = nullptr;
	LoadedAttackVoiceSounds.Reset();
	LastAttackVoiceSoundIndex = INDEX_NONE;
	AttackSoundAttachSocketName = NAME_None;
	AttackSoundVolumeMultiplier = 1.0f;
	AttackSoundPitchMultiplier = 1.0f;

	if (!WeaponData)
	{
		return;
	}

	const UFrontierWeaponAudioSettings* AudioSettings = GetDefault<UFrontierWeaponAudioSettings>();
	const FFrontierMeleeWeaponAudioData* MeleeAudio = AudioSettings
		? AudioSettings->FindMeleeAudioForWeaponType(WeaponData->WeaponTypeTag)
		: nullptr;
	if (MeleeAudio)
	{
		LoadedMeleeAttackSound = MeleeAudio->AttackSound.LoadSynchronous();
		for (const TSoftObjectPtr<USoundBase>& VoiceSound : MeleeAudio->AttackVoiceSounds)
		{
			if (USoundBase* LoadedVoiceSound = VoiceSound.LoadSynchronous())
			{
				LoadedAttackVoiceSounds.Add(LoadedVoiceSound);
			}
		}
		AttackSoundAttachSocketName = MeleeAudio->AttachSocketName;
		AttackSoundVolumeMultiplier = FMath::Max(0.0f, MeleeAudio->VolumeMultiplier);
		AttackSoundPitchMultiplier = FMath::Max(0.01f, MeleeAudio->PitchMultiplier);
	}
	else
	{
		const FFrontierBowWeaponAudioData* BowAudio = AudioSettings
			? AudioSettings->FindBowAudioForWeaponType(WeaponData->WeaponTypeTag)
			: nullptr;
		if (BowAudio)
		{
			LoadedBowDrawSound = BowAudio->DrawSound.LoadSynchronous();
			LoadedBowReleaseSound = BowAudio->ReleaseSound.LoadSynchronous();
			for (const TSoftObjectPtr<USoundBase>& VoiceSound : BowAudio->AttackVoiceSounds)
			{
				if (USoundBase* LoadedVoiceSound = VoiceSound.LoadSynchronous())
				{
					LoadedAttackVoiceSounds.Add(LoadedVoiceSound);
				}
			}
			AttackSoundAttachSocketName = BowAudio->AttachSocketName;
			AttackSoundVolumeMultiplier = FMath::Max(0.0f, BowAudio->VolumeMultiplier);
			AttackSoundPitchMultiplier = FMath::Max(0.01f, BowAudio->PitchMultiplier);
		}
	}

	if (!WeaponMesh || WeaponData->WeaponMesh.IsNull())
	{
		return;
	}

	if (USkeletalMesh* ResolvedMesh = WeaponData->WeaponMesh.LoadSynchronous())
	{
		WeaponMesh->SetSkeletalMesh(ResolvedMesh);
	}
}

void AFrontierWeaponBase::ApplyEnhancementAura()
{
	if (!EnhancementAuraComponent || !WeaponMesh)
	{
		return;
	}

	UNiagaraSystem* AuraSystem = EnhancementAuraSystem.LoadSynchronous();
	EnhancementAuraComponent->SetAsset(AuraSystem);

	const int32 ClampedEnhancementLevel = FMath::Clamp(EnhancementLevel, 0, 9);
	const bool bEnhancementVisible = ClampedEnhancementLevel > 0 && !IsHidden();
	const float EnhancementAlpha = ClampedEnhancementLevel > 0
		? static_cast<float>(ClampedEnhancementLevel - 1) / 8.0f
		: 0.0f;
	const float AuraIntensity = ClampedEnhancementLevel > 0
		? FMath::Lerp(1.0f, 5.0f, EnhancementAlpha)
		: 0.0f;
	const float EmberSpawnRate = ClampedEnhancementLevel > 0
		? FMath::Lerp(5.0f, 20.0f, EnhancementAlpha)
		: 0.0f;
	static constexpr float OutlineWidthByLevel[] =
	{
		0.0f,
		1.0f,
		3.0f,
		5.0f,
		7.0f,
		10.0f,
		12.0f,
		12.5f,
		13.0f,
		20.0f
	};
	const float OutlineWidth = OutlineWidthByLevel[ClampedEnhancementLevel];

	EnhancementAuraComponent->SetRelativeTransform(FTransform::Identity);
	EnhancementAuraComponent->SetVariableFloat(TEXT("User.AuraIntensity"), AuraIntensity);
	EnhancementAuraComponent->SetVariableFloat(TEXT("User.EmberSpawnRate"), EmberSpawnRate);
	UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(
		EnhancementAuraComponent,
		TEXT("User.WeaponMesh"),
		WeaponMesh);
	EnhancementAuraComponent->SetVisibility(bEnhancementVisible && AuraSystem != nullptr, true);

	if (bEnhancementVisible && AuraSystem)
	{
		EnhancementAuraComponent->ReinitializeSystem();
	}
	else
	{
		EnhancementAuraComponent->DeactivateImmediate();
	}

	UMaterialInterface* OverlayMaterial = EnhancementOverlayMaterial.LoadSynchronous();
	if (OverlayMaterial && !EnhancementOverlayMaterialInstance)
	{
		EnhancementOverlayMaterialInstance = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
	}

	if (EnhancementOverlayMaterialInstance)
	{
		EnhancementOverlayMaterialInstance->SetScalarParameterValue(TEXT("OutlineWidth"), OutlineWidth);
	}
	WeaponMesh->SetOverlayMaterial(bEnhancementVisible ? EnhancementOverlayMaterialInstance : nullptr);
}

void AFrontierWeaponBase::OnRep_WeaponData()
{
	ApplyWeaponData();
	ApplyEnhancementAura();

	AFrontierPlayerCharacter* OwnerCharacter = Cast<AFrontierPlayerCharacter>(GetOwner());
	UFrontierEquipmentComponent* EquipmentComponent = OwnerCharacter ? OwnerCharacter->GetEquipmentComponent() : nullptr;
	if (EquipmentComponent && EquipmentComponent->GetCurrentWeapon() == this)
	{
		EquipmentComponent->RefreshCurrentWeaponPresentation();
	}
}

void AFrontierWeaponBase::OnRep_EnhancementLevel()
{
	ApplyEnhancementAura();
}
