#include "UI/FrontierCharacterPreviewActor.h"

#include "Animation/AnimInstance.h"
#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Character/FrontierCharacterSelectionSubsystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Character/FrontierPlayerCharacter.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/FrontierLoadoutComponent.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/Character.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Frontier.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AFrontierCharacterPreviewActor::AFrontierCharacterPreviewActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetReceivesDecals(false);
	PreviewMesh->SetRelativeRotation(FRotator::ZeroRotator);
	PreviewMesh->SetVisibility(true, false);
	PreviewMesh->SetHiddenInGame(false, false);
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	ArmorMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("ArmorMesh"));
	ArmorMeshComponent->SetupAttachment(PreviewMesh);

	GloveMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GloveMesh"));
	GloveMeshComponent->SetupAttachment(PreviewMesh);

	GreavesMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("GreavesMesh"));
	GreavesMeshComponent->SetupAttachment(PreviewMesh);

	HeadMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("HeadMesh"));
	HeadMeshComponent->SetupAttachment(PreviewMesh);

#if 0 // TEMP: Preserved modular preview leader-pose setup. Do not delete.
	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		ModularMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ModularMesh->SetGenerateOverlapEvents(false);
		ModularMesh->SetReceivesDecals(false);
		ModularMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		ModularMesh->SetLeaderPoseComponent(PreviewMesh, false, true);
	}
#endif

	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		ModularMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ModularMesh->SetGenerateOverlapEvents(false);
		ModularMesh->SetReceivesDecals(false);
		ModularMesh->SetVisibility(false, true);
		ModularMesh->SetHiddenInGame(true, true);
	}

	SceneCaptureComponent = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));
	SceneCaptureComponent->SetupAttachment(SceneRoot);
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
}

void AFrontierCharacterPreviewActor::BeginPlay()
{
	Super::BeginPlay();
	// RefreshModularMeshLeaderPose(); // TEMP: Modular preview mode is preserved but disabled.
	ApplyTemporarySingleMeshMode();
}

void AFrontierCharacterPreviewActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	
	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->CaptureScene();
	}
}

void AFrontierCharacterPreviewActor::InitializeFromCharacter(ACharacter* SourceCharacter)
{
	if (!SourceCharacter)
	{
		return;
	}

	USkeletalMeshComponent* SourceMesh = SourceCharacter->GetMesh();
	if (!SourceMesh)
	{
		return;
	}

	ClearAttachedPreviewMeshes();

	PreviewMesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->bPauseAnims = false;
	PreviewMesh->bNoSkeletonUpdate = false;
	PreviewMesh->GlobalAnimRateScale = 1.0f;
	PreviewMesh->SetComponentTickEnabled(true);
	ApplyPreviewAnimInstanceClass(SourceMesh->GetAnimInstance()
		? SourceMesh->GetAnimInstance()->GetClass()
		: nullptr);
	PreviewMesh->SetRelativeLocation(SourceMesh->GetRelativeLocation());
	PreviewMesh->SetRelativeScale3D(SourceMesh->GetRelativeScale3D());

	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		PreviewMesh->SetMaterial(MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}

	const AFrontierPlayerCharacter* FrontierPlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceCharacter);
#if 0 // TEMP: Preserved modular preview mesh-copy path. Do not delete.
	if (FrontierPlayerCharacter)
	{
		CopyModularMeshFromSource(ArmorMeshComponent, FrontierPlayerCharacter->GetArmorMesh());
		CopyModularMeshFromSource(GloveMeshComponent, FrontierPlayerCharacter->GetGloveMesh());
		CopyModularMeshFromSource(GreavesMeshComponent, FrontierPlayerCharacter->GetGreavesMesh());
		CopyModularMeshFromSource(HeadMeshComponent, FrontierPlayerCharacter->GetHeadMesh());
	}
#endif
	ApplyTemporarySingleMeshMode();
	const UFrontierEquipmentComponent* EquipmentComponent = FrontierPlayerCharacter ? FrontierPlayerCharacter->GetEquipmentComponent() : nullptr;
	const AFrontierWeaponBase* CurrentWeapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
	PreviewWeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	const USkeletalMeshComponent* SourceWeaponMesh = CurrentWeapon ? CurrentWeapon->GetWeaponMesh() : nullptr;
	if (CurrentWeapon && SourceWeaponMesh && SourceWeaponMesh->GetSkeletalMeshAsset())
	{
		USkeletalMeshComponent* PreviewWeaponMesh = NewObject<USkeletalMeshComponent>(this);
		PreviewWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PreviewWeaponMesh->SetGenerateOverlapEvents(false);
		PreviewWeaponMesh->SetSkeletalMesh(SourceWeaponMesh->GetSkeletalMeshAsset());
		PreviewWeaponMesh->RegisterComponent();
		PreviewWeaponMesh->AttachToComponent(
			PreviewMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			CurrentWeapon->GetCharacterAttachSocketName());
		PreviewWeaponMesh->SetRelativeTransform(CurrentWeapon->GetEquipOffset());

		for (int32 MaterialIndex = 0; MaterialIndex < SourceWeaponMesh->GetNumMaterials(); ++MaterialIndex)
		{
			PreviewWeaponMesh->SetMaterial(MaterialIndex, SourceWeaponMesh->GetMaterial(MaterialIndex));
		}
		ApplyPreviewWeaponOverlay(
			PreviewWeaponMesh,
			CurrentWeapon->GetEnhancementLevel(),
			CurrentWeapon->GetEnhancementOverlayMaterial());

		AttachedPreviewComponents.Add(PreviewWeaponMesh);
	}

	AttachLoadoutPreviewMeshes(
		SourceCharacter->GetPlayerState<AFrontierPlayerState>(),
		false);
	ApplyPreviewWeaponAnimationLayer();
	StartPreviewCapture();
}

void AFrontierCharacterPreviewActor::InitializeFromPlayerState(const AFrontierPlayerState* PlayerState)
{
	if (!PlayerState || !PreviewMesh)
	{
		return;
	}

	if (!PreviewMesh->GetSkeletalMeshAsset() && !DefaultPreviewMesh.IsNull())
	{
		PreviewMesh->SetSkeletalMesh(DefaultPreviewMesh.LoadSynchronous());
	}
	if (!PreviewMesh->GetSkeletalMeshAsset())
	{
		return;
	}

	ClearAttachedPreviewMeshes();
	PreviewWeaponData = DefaultUnarmedWeaponData.LoadSynchronous();
	FVector PreviewMeshLocation = PreviewMesh->GetRelativeLocation();
	PreviewMeshLocation.Z = -90.0f;
	PreviewMesh->SetRelativeLocation(PreviewMeshLocation);
	PreviewMesh->SetVisibility(true, false);
	PreviewMesh->SetHiddenInGame(false);
	PreviewMesh->SetComponentTickEnabled(true);
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->bPauseAnims = false;
	PreviewMesh->bNoSkeletonUpdate = false;
	PreviewMesh->GlobalAnimRateScale = 1.0f;
	ApplyTemporarySingleMeshMode();
	ApplyPreviewAnimInstanceClass(PreviewMesh->GetAnimInstance()
		? PreviewMesh->GetAnimInstance()->GetClass()
		: nullptr);
	AttachLoadoutPreviewMeshes(PlayerState, true);
	ApplyPreviewWeaponAnimationLayer();
	StartPreviewCapture();
}

bool AFrontierCharacterPreviewActor::ApplyCharacterAppearance(
	const FFrontierCharacterAppearanceData& AppearanceData)
{
	if (!PreviewMesh)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot apply preview appearance because PreviewMesh is null."));
		return false;
	}

	USkeletalMesh* LoadedMesh = AppearanceData.SkeletalMesh.LoadSynchronous();
	if (!LoadedMesh)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Failed to load preview character mesh. CharacterType=%s Asset=%s"),
			*UEnum::GetValueAsString(AppearanceData.CharacterType),
			*AppearanceData.SkeletalMesh.ToSoftObjectPath().ToString());
		return false;
	}

	UClass* LoadedAnimationClass = AppearanceData.AnimationClass.LoadSynchronous();
	if (!AppearanceData.AnimationClass.IsNull() && !LoadedAnimationClass)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Failed to load preview AnimInstance class. CharacterType=%s Asset=%s"),
			*UEnum::GetValueAsString(AppearanceData.CharacterType),
			*AppearanceData.AnimationClass.ToSoftObjectPath().ToString());
	}

	PreviewMesh->EmptyOverrideMaterials();
	PreviewMesh->SetSkeletalMesh(LoadedMesh);
	// RefreshModularMeshLeaderPose(); // TEMP: Modular preview mode is preserved but disabled.
	ApplyTemporarySingleMeshMode();
	ApplyPreviewAnimInstanceClass(LoadedAnimationClass);
	ApplyPreviewWeaponAnimationLayer();
	StartPreviewCapture();

	FRONTIER_LOG(
		Log,
		TEXT("Preview character appearance applied. CharacterType=%s Mesh=%s"),
		*UEnum::GetValueAsString(AppearanceData.CharacterType),
		*LoadedMesh->GetPathName());
	return true;
}

bool AFrontierCharacterPreviewActor::SetCharacterType(
	const EFrontierCharacterType CharacterType)
{
	UGameInstance* GameInstance = GetGameInstance();
	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GameInstance
		? GameInstance->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	const FFrontierCharacterAppearanceData* Appearance = SelectionSubsystem
		? SelectionSubsystem->FindAppearance(CharacterType)
		: nullptr;
	if (!Appearance)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("No preview appearance mapping found. CharacterType=%s"),
			*UEnum::GetValueAsString(CharacterType));
		return false;
	}

	return ApplyCharacterAppearance(*Appearance);
}

void AFrontierCharacterPreviewActor::AttachLoadoutPreviewMeshes(
	const AFrontierPlayerState* PlayerState,
	const bool bIncludeWeapons)
{
	const UFrontierLoadoutComponent* Loadout = PlayerState ? PlayerState->GetLoadoutComponent() : nullptr;
	if (!Loadout || !PreviewMesh)
	{
		return;
	}

	const UFrontierCharacterSelectionSubsystem* SelectionSubsystem = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierCharacterSelectionSubsystem>()
		: nullptr;
	const EFrontierCharacterType CharacterType = SelectionSubsystem
		? SelectionSubsystem->GetSelectedCharacterType()
		: EFrontierCharacterType::DarkKnight;

	for (const FFrontierLoadoutSlot& Slot : Loadout->GetLoadoutSlots())
	{
		if (!Slot.bOccupied || !Slot.ItemInstance.IsValid())
		{
			continue;
		}

		if (bIncludeWeapons
			&& Slot.ItemInstance.GetCategory() == EFrontierItemCategory::Weapon)
		{
			UFrontierWeaponDataAsset* WeaponData =
				Slot.ItemInstance.GetWeaponData().LoadSynchronous();
			USkeletalMesh* EquipmentMesh = WeaponData
				? WeaponData->WeaponMesh.LoadSynchronous()
				: nullptr;
			if (!WeaponData || !EquipmentMesh)
			{
				continue;
			}

			if (Slot.ItemInstance.GetEquipSlot() == EFrontierEquipmentSlot::MainWeapon)
			{
				PreviewWeaponData = WeaponData;
			}

			USkeletalMeshComponent* PreviewEquipmentMesh =
				NewObject<USkeletalMeshComponent>(this);
			PreviewEquipmentMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PreviewEquipmentMesh->SetGenerateOverlapEvents(false);
			PreviewEquipmentMesh->SetSkeletalMesh(EquipmentMesh);
			PreviewEquipmentMesh->RegisterComponent();
			PreviewEquipmentMesh->AttachToComponent(
				PreviewMesh,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				WeaponData->CharacterAttachSocketName);
			PreviewEquipmentMesh->SetRelativeTransform(WeaponData->EquipOffset);
			const TSubclassOf<AFrontierWeaponBase> WeaponClass =
				Slot.ItemInstance.GetWeaponActorClass().LoadSynchronous();
			ApplyPreviewWeaponOverlay(
				PreviewEquipmentMesh,
				Slot.ItemInstance.EnhancementLevel,
				ResolvePreviewWeaponOverlayMaterial(WeaponClass));
			AttachedPreviewComponents.Add(PreviewEquipmentMesh);
			continue;
		}

		if (Slot.ItemInstance.GetCategory() != EFrontierItemCategory::Armor)
		{
			continue;
		}

		FFrontierArmorAppearanceData Appearance;
		if (!Slot.ItemInstance.ResolveArmorAppearance(CharacterType, Appearance))
		{
			continue;
		}

		if (!Appearance.EquipmentStaticMesh.IsNull()
			&& !Appearance.EquipmentSkeletalMesh.IsNull())
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Preview armor has both mesh types; StaticMesh takes priority. ItemTemplateId=%s CharacterType=%s"),
				*Slot.ItemInstance.GetTemplateId().ToString(),
				*UEnum::GetValueAsString(CharacterType));
		}

		USceneComponent* PreviewArmorComponent = nullptr;
		if (!Appearance.EquipmentStaticMesh.IsNull())
		{
			if (UStaticMesh* StaticMesh = Appearance.EquipmentStaticMesh.LoadSynchronous())
			{
				UStaticMeshComponent* StaticMeshComponent =
					NewObject<UStaticMeshComponent>(this);
				StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StaticMeshComponent->SetGenerateOverlapEvents(false);
				StaticMeshComponent->SetStaticMesh(StaticMesh);
				PreviewArmorComponent = StaticMeshComponent;
			}
		}
		else if (!Appearance.EquipmentSkeletalMesh.IsNull())
		{
			if (USkeletalMesh* SkeletalMesh =
				Appearance.EquipmentSkeletalMesh.LoadSynchronous())
			{
				USkeletalMeshComponent* SkeletalMeshComponent =
					NewObject<USkeletalMeshComponent>(this);
				SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SkeletalMeshComponent->SetGenerateOverlapEvents(false);
				SkeletalMeshComponent->SetSkeletalMesh(SkeletalMesh);
				PreviewArmorComponent = SkeletalMeshComponent;
			}
		}

		if (!PreviewArmorComponent)
		{
			continue;
		}

#if 0 // TEMP: Preserved modular armor-slot routing. Do not delete.
		USkeletalMeshComponent* AttachMesh = ResolveArmorAttachMesh(Slot.SlotType);
#else
		// TEMP: Preview armor uses the matching socket on the full preview mesh.
		USkeletalMeshComponent* AttachMesh = PreviewMesh;
#endif
		if (!AttachMesh)
		{
			PreviewArmorComponent->DestroyComponent();
			continue;
		}

		if (!Appearance.CharacterAttachSocketName.IsNone()
			&& !AttachMesh->DoesSocketExist(Appearance.CharacterAttachSocketName))
		{
			FRONTIER_LOG(
				Warning,
				TEXT("Preview armor attach socket does not exist. ItemTemplateId=%s CharacterType=%s Socket=%s"),
				*Slot.ItemInstance.GetTemplateId().ToString(),
				*UEnum::GetValueAsString(CharacterType),
				*Appearance.CharacterAttachSocketName.ToString());
			PreviewArmorComponent->DestroyComponent();
			continue;
		}

		PreviewArmorComponent->RegisterComponent();
		PreviewArmorComponent->AttachToComponent(
			AttachMesh,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale,
			Appearance.CharacterAttachSocketName);
		PreviewArmorComponent->SetRelativeTransform(Appearance.EquipOffset);
		AttachedPreviewComponents.Add(PreviewArmorComponent);
	}
}

void AFrontierCharacterPreviewActor::ApplyTemporarySingleMeshMode()
{
	if (!PreviewMesh)
	{
		return;
	}

	PreviewMesh->SetVisibility(true, false);
	PreviewMesh->SetHiddenInGame(false, false);
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;

	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		if (ModularMesh)
		{
			ModularMesh->SetVisibility(false, true);
			ModularMesh->SetHiddenInGame(true, true);
		}
	}
}

#if 0 // TEMP: Preserved modular preview implementation. Do not delete.
void AFrontierCharacterPreviewActor::RefreshModularMeshLeaderPose()
{
	if (!PreviewMesh)
	{
		return;
	}

	PreviewMesh->SetVisibility(false, false);
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	for (USkeletalMeshComponent* ModularMesh : { ArmorMeshComponent.Get(), GloveMeshComponent.Get(), GreavesMeshComponent.Get(), HeadMeshComponent.Get() })
	{
		if (ModularMesh)
		{
			ModularMesh->SetLeaderPoseComponent(PreviewMesh, false, true);
		}
	}
}

void AFrontierCharacterPreviewActor::CopyModularMeshFromSource(
	USkeletalMeshComponent* TargetMesh,
	const USkeletalMeshComponent* SourceMesh)
{
	if (!TargetMesh || !SourceMesh)
	{
		return;
	}

	TargetMesh->EmptyOverrideMaterials();
	TargetMesh->SetSkeletalMesh(SourceMesh->GetSkeletalMeshAsset());
	TargetMesh->SetRelativeTransform(SourceMesh->GetRelativeTransform());
	TargetMesh->SetVisibility(SourceMesh->IsVisible(), false);
	TargetMesh->SetHiddenInGame(SourceMesh->bHiddenInGame);
	for (int32 MaterialIndex = 0; MaterialIndex < SourceMesh->GetNumMaterials(); ++MaterialIndex)
	{
		TargetMesh->SetMaterial(MaterialIndex, SourceMesh->GetMaterial(MaterialIndex));
	}
	TargetMesh->SetLeaderPoseComponent(PreviewMesh, false, true);
}

USkeletalMeshComponent* AFrontierCharacterPreviewActor::ResolveArmorAttachMesh(
	const EFrontierEquipmentSlot SlotType) const
{
	switch (SlotType)
	{
	case EFrontierEquipmentSlot::Helmet:
		return HeadMeshComponent;
	case EFrontierEquipmentSlot::Chest:
		return ArmorMeshComponent;
	case EFrontierEquipmentSlot::Gloves:
		return GloveMeshComponent;
	case EFrontierEquipmentSlot::Boots:
		return GreavesMeshComponent;
	default:
		return PreviewMesh;
	}
}
#endif

void AFrontierCharacterPreviewActor::StartPreviewCapture()
{
	if (!SceneCaptureComponent)
	{
		return;
	}

	SceneCaptureComponent->ShowOnlyActors.Reset();
	SceneCaptureComponent->ShowOnlyActorComponents(this);
	SceneCaptureComponent->bCaptureEveryFrame = false;
	SceneCaptureComponent->bCaptureOnMovement = false;
	SceneCaptureComponent->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	if (SceneCaptureComponent->TextureTarget)
	{
		SceneCaptureComponent->TextureTarget->ClearColor = FLinearColor::Transparent;
		SceneCaptureComponent->TextureTarget->UpdateResourceImmediate(false);
		SceneCaptureComponent->CaptureScene();
		SetActorTickEnabled(true);
	}
	else
	{
	}
}

void AFrontierCharacterPreviewActor::SetPreviewYaw(const float NewYaw)
{
	PreviewMesh->SetRelativeRotation(FRotator(0.0f, NewYaw, 0.0f));
	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->CaptureScene();
	}
}

void AFrontierCharacterPreviewActor::StopPreviewCapture()
{
	SetActorTickEnabled(false);

	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->bCaptureEveryFrame = false;
		SceneCaptureComponent->bCaptureOnMovement = false;
		SceneCaptureComponent->ShowOnlyActors.Reset();
	}
}

void AFrontierCharacterPreviewActor::ShutdownPreview()
{
	StopPreviewCapture();
	ClearAttachedPreviewMeshes();

	Destroy();
}

void AFrontierCharacterPreviewActor::ApplyPreviewAnimInstanceClass(UClass* AnimInstanceClass)
{
	if (!PreviewMesh)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot apply preview AnimInstance because PreviewMesh is null."));
		return;
	}

	USkeletalMesh* MeshAsset = PreviewMesh->GetSkeletalMeshAsset();
	if (!MeshAsset)
	{
		FRONTIER_LOG(Warning, TEXT("Cannot apply preview AnimInstance because PreviewMesh has no SkeletalMesh."));
		return;
	}

	ClearPreviewWeaponAnimationLayer();
	PreviewMesh->SetAnimationMode(EAnimationMode::AnimationBlueprint);
	PreviewMesh->SetAnimInstanceClass(AnimInstanceClass);
	PreviewMesh->SetVisibility(true);
	PreviewMesh->SetHiddenInGame(false);
	PreviewMesh->SetComponentTickEnabled(true);
	PreviewMesh->bPauseAnims = false;
	PreviewMesh->bNoSkeletonUpdate = false;
	PreviewMesh->GlobalAnimRateScale = 1.0f;
	PreviewMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
	PreviewMesh->RefreshBoneTransforms();

	if (!AnimInstanceClass)
	{
		FRONTIER_LOG(Warning, TEXT("Preview AnimInstance class is null. PreviewMesh=%s"), *GetNameSafe(MeshAsset));	
		return;
	}

	FRONTIER_LOG(Log, TEXT("Preview AnimInstance applied. Mesh=%s AnimClass=%s"),
		*GetNameSafe(MeshAsset),
		*GetNameSafe(AnimInstanceClass));
}

void AFrontierCharacterPreviewActor::ApplyPreviewWeaponAnimationLayer()

{
	if (!PreviewMesh)
	{
		return;
	}

	const UFrontierWeaponDataAsset* EffectiveWeaponData = PreviewWeaponData;
	if (!EffectiveWeaponData)
	{
		EffectiveWeaponData = DefaultUnarmedWeaponData.LoadSynchronous();
	}

	if (!EffectiveWeaponData)
	{
		FRONTIER_LOG(Warning, TEXT("Preview weapon animation layer skipped because neither equipped nor default Unarmed WeaponData is configured."));
		return;
	}

	UClass* AnimLayerClass = EffectiveWeaponData->AnimLayerClass.LoadSynchronous();
	if (!AnimLayerClass)
	{
		FRONTIER_LOG(Warning, TEXT("Preview weapon has no valid animation layer. WeaponData=%s"),
			*GetNameSafe(EffectiveWeaponData));
		return;
	}

	UAnimInstance* MainAnimInstance = PreviewMesh->GetAnimInstance();
	if (!MainAnimInstance)
	{
		FRONTIER_LOG(Warning, TEXT("Preview weapon animation layer deferred because AnimInstance is not ready. WeaponData=%s"),
			*GetNameSafe(EffectiveWeaponData));
		return;
	}

	if (LinkedWeaponAnimLayerClass == AnimLayerClass)
	{
		return;
	}

	ClearPreviewWeaponAnimationLayer();
	PreviewMesh->LinkAnimClassLayers(AnimLayerClass);
	if (MainAnimInstance->GetLinkedAnimLayerInstanceByClass(AnimLayerClass, true))
	{
		LinkedWeaponAnimLayerClass = AnimLayerClass;
		FRONTIER_LOG(Log, TEXT("Preview weapon animation layer applied. WeaponData=%s LayerClass=%s"),
			*GetNameSafe(EffectiveWeaponData),
			*GetNameSafe(AnimLayerClass));
	}
	else
	{
		FRONTIER_LOG(Warning, TEXT("Preview weapon animation layer was rejected. WeaponData=%s MainAnimClass=%s LayerClass=%s"),
			*GetNameSafe(EffectiveWeaponData),
			*GetNameSafe(MainAnimInstance->GetClass()),
			*GetNameSafe(AnimLayerClass));
	}
}

void AFrontierCharacterPreviewActor::ClearPreviewWeaponAnimationLayer()
{
	if (PreviewMesh && LinkedWeaponAnimLayerClass)
	{
		PreviewMesh->UnlinkAnimClassLayers(LinkedWeaponAnimLayerClass);
	}

	LinkedWeaponAnimLayerClass = nullptr;

	if (SceneCaptureComponent)
	{
		SceneCaptureComponent->CaptureScene();
	}
}

void AFrontierCharacterPreviewActor::ClearAttachedPreviewMeshes()
{
	for (USceneComponent* MeshComponent : AttachedPreviewComponents)
	{
		if (MeshComponent)
		{
			MeshComponent->DestroyComponent();
		}
	}

	AttachedPreviewComponents.Reset();
	PreviewWeaponOverlayInstances.Reset();
}

void AFrontierCharacterPreviewActor::ApplyPreviewWeaponOverlay(
	USkeletalMeshComponent* WeaponComponent,
	const int32 InEnhancementLevel,
	UMaterialInterface* InOverlayMaterial)
{
	if (!WeaponComponent)
	{
		return;
	}

	WeaponComponent->SetOverlayMaterial(nullptr);
	const int32 ClampedLevel = FMath::Clamp(InEnhancementLevel, 0, 9);
	if (ClampedLevel <= 0)
	{
		return;
	}

	UMaterialInterface* OverlayMaterial = InOverlayMaterial;
	if (!OverlayMaterial)
	{
		OverlayMaterial = PreviewEnhancementOverlayMaterial.LoadSynchronous();
	}
	if (!OverlayMaterial)
	{
		return;
	}

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

	UMaterialInstanceDynamic* OverlayMID = UMaterialInstanceDynamic::Create(OverlayMaterial, this);
	if (!OverlayMID)
	{
		return;
	}

	OverlayMID->SetScalarParameterValue(TEXT("OutlineWidth"), OutlineWidthByLevel[ClampedLevel]);
	WeaponComponent->SetOverlayMaterial(OverlayMID);
	PreviewWeaponOverlayInstances.Add(OverlayMID);
}

UMaterialInterface* AFrontierCharacterPreviewActor::ResolvePreviewWeaponOverlayMaterial(
	const TSubclassOf<AFrontierWeaponBase> WeaponClass) const
{
	if (WeaponClass)
	{
		const AFrontierWeaponBase* WeaponCDO = WeaponClass->GetDefaultObject<AFrontierWeaponBase>();
		if (WeaponCDO)
		{
			if (UMaterialInterface* WeaponOverlay = WeaponCDO->GetEnhancementOverlayMaterial())
			{
				return WeaponOverlay;
			}
		}
	}

	return PreviewEnhancementOverlayMaterial.LoadSynchronous();
}
