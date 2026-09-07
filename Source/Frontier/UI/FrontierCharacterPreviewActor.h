#pragma once

#include "CoreMinimal.h"
#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "GameFramework/Actor.h"
#include "FrontierCharacterPreviewActor.generated.h"

class ACharacter;
class AFrontierPlayerState;
class AFrontierWeaponBase;
class USceneCaptureComponent2D;
class USkeletalMesh;
class USkeletalMeshComponent;
class UAnimInstance;
class UFrontierWeaponDataAsset;
class UMaterialInterface;
class UMaterialInstanceDynamic;
enum class EFrontierEquipmentSlot : uint8;

UCLASS()
class FRONTIER_API AFrontierCharacterPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AFrontierCharacterPreviewActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void InitializeFromCharacter(ACharacter* SourceCharacter);
	void InitializeFromPlayerState(const AFrontierPlayerState* PlayerState);

	UFUNCTION(BlueprintCallable, Category="Frontier|Preview")
	bool ApplyCharacterAppearance(const FFrontierCharacterAppearanceData& AppearanceData);

	UFUNCTION(BlueprintCallable, Category="Frontier|Preview")
	bool SetCharacterType(EFrontierCharacterType CharacterType);

	void SetPreviewYaw(float NewYaw);
	void StopPreviewCapture();
	void ShutdownPreview();

protected:
	void ApplyPreviewAnimInstanceClass(UClass* AnimInstanceClass);
	void ApplyPreviewWeaponAnimationLayer();
	void ApplyPreviewWeaponOverlay(
		USkeletalMeshComponent* WeaponComponent,
		int32 EnhancementLevel,
		UMaterialInterface* OverlayMaterial);
	UMaterialInterface* ResolvePreviewWeaponOverlayMaterial(
		TSubclassOf<AFrontierWeaponBase> WeaponClass) const;
	void ClearPreviewWeaponAnimationLayer();
	void ClearAttachedPreviewMeshes();
	void ApplyTemporarySingleMeshMode();
	// TEMP: Modular preview declarations are retained with their disabled implementations.
	void RefreshModularMeshLeaderPose();
	void CopyModularMeshFromSource(USkeletalMeshComponent* TargetMesh, const USkeletalMeshComponent* SourceMesh);
	USkeletalMeshComponent* ResolveArmorAttachMesh(EFrontierEquipmentSlot SlotType) const;
	void AttachLoadoutPreviewMeshes(const AFrontierPlayerState* PlayerState, bool bIncludeWeapons = true);
	void StartPreviewCapture();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	TObjectPtr<USkeletalMeshComponent> PreviewMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview|Modular Mesh")
	TObjectPtr<USkeletalMeshComponent> ArmorMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview|Modular Mesh")
	TObjectPtr<USkeletalMeshComponent> GloveMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview|Modular Mesh")
	TObjectPtr<USkeletalMeshComponent> GreavesMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview|Modular Mesh")
	TObjectPtr<USkeletalMeshComponent> HeadMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Preview")
	TObjectPtr<USceneCaptureComponent2D> SceneCaptureComponent;

	/** Used by pawn-free lobby previews when no mesh is assigned directly on PreviewMesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview")
	TSoftObjectPtr<USkeletalMesh> DefaultPreviewMesh;

	/** Animation layer used when the preview has no equipped weapon. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview|Animation")
	TSoftObjectPtr<UFrontierWeaponDataAsset> DefaultUnarmedWeaponData;

	/** Assign the same overlay material used by weapon actors, for lobby preview meshes. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview|Enhancement")
	TSoftObjectPtr<UMaterialInterface> PreviewEnhancementOverlayMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierWeaponDataAsset> PreviewWeaponData;

	UPROPERTY(Transient)
	TSubclassOf<UAnimInstance> LinkedWeaponAnimLayerClass;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> AttachedPreviewComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> PreviewWeaponOverlayInstances;
};
