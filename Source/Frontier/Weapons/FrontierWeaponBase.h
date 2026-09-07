#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Weapons/FrontierWeaponAudioSettings.h"
#include "FrontierWeaponBase.generated.h"

class USkeletalMeshComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UFrontierWeaponDataAsset;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UAudioComponent;

UCLASS()
class FRONTIER_API AFrontierWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AFrontierWeaponBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category="Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	UFrontierWeaponDataAsset* GetWeaponData() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	FName GetCharacterAttachSocketName() const;

	UFUNCTION(BlueprintPure, Category="Weapon")
	const FTransform& GetEquipOffset() const;

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void SetWeaponData(UFrontierWeaponDataAsset* InWeaponData);

	UFUNCTION(BlueprintCallable, Category="Weapon")
	void SetWeaponActive(bool bNewActive);

	/** Plays immediately for the owning player and multicasts to other relevant clients. */
	UFUNCTION(BlueprintCallable, Category="Weapon|Audio")
	void PlayAttackSound(EFrontierWeaponAttackSound SoundType);

	/** Stops the currently playing bow draw sound, if any. */
	UFUNCTION(BlueprintCallable, Category="Weapon|Audio")
	void StopBowDrawSound();

	UFUNCTION(BlueprintPure, Category="Weapon|Enhancement")
	int32 GetEnhancementLevel() const { return EnhancementLevel; }

	UFUNCTION(BlueprintPure, Category="Weapon|Enhancement")
	UMaterialInterface* GetEnhancementOverlayMaterial() const;

	UFUNCTION(BlueprintCallable, Category="Weapon|Enhancement")
	void SetEnhancementLevel(int32 InEnhancementLevel);

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	void ApplyWeaponData();
	void ApplyEnhancementAura();

	UFUNCTION()
	void OnRep_WeaponData();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayAttackSound(EFrontierWeaponAttackSound SoundType, int32 VoiceSoundIndex);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastStopBowDrawSound();

	void PlayAttackSoundLocal(EFrontierWeaponAttackSound SoundType, int32 VoiceSoundIndex = INDEX_NONE);
	void StopBowDrawSoundLocal();
	USoundBase* ResolveAttackSound(EFrontierWeaponAttackSound SoundType, int32 VoiceSoundIndex) const;
	int32 ChooseRandomAttackVoiceIndex();

	UFUNCTION()
	void OnRep_EnhancementLevel();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, ReplicatedUsing=OnRep_WeaponData, BlueprintReadOnly, Category="Weapon")
	TObjectPtr<UFrontierWeaponDataAsset> WeaponData;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Weapon|Enhancement")
	TObjectPtr<UNiagaraComponent> EnhancementAuraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Enhancement")
	TSoftObjectPtr<UNiagaraSystem> EnhancementAuraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Weapon|Enhancement")
	TSoftObjectPtr<UMaterialInterface> EnhancementOverlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> EnhancementOverlayMaterialInstance;

	UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_EnhancementLevel, BlueprintReadOnly, Category="Weapon|Enhancement|Preview",
		meta=(ClampMin="0", ClampMax="9", UIMin="0", UIMax="9"))
	int32 EnhancementLevel = 0;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedMeleeAttackSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedBowDrawSound;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> LoadedBowReleaseSound;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundBase>> LoadedAttackVoiceSounds;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveBowDrawAudioComponent;

	FName AttackSoundAttachSocketName = NAME_None;
	float AttackSoundVolumeMultiplier = 1.0f;
	float AttackSoundPitchMultiplier = 1.0f;
	int32 LastAttackVoiceSoundIndex = INDEX_NONE;
};
