#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimEnums.h"
#include "AbilitySystem/Abilities/FrontierMeleeTraceAbilityInterface.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierCharacterAppearanceDataAsset.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "FrontierPlayerCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UAnimMontage;
class UGameplayEffect;
class USpringArmComponent;
class UWidgetComponent;
class UStaticMesh;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class AFrontierWeaponBase;
class AFrontierCharacterPreviewActor;
class AFrontierPlayerState;
class UFrontierEquipmentComponent;
class UFrontierOverheadPlayerNameWidget;
class UFrontierTeamVisualDataAsset;
struct FInputActionValue;
struct FOnAttributeChangeData;

UCLASS()
class FRONTIER_API AFrontierPlayerCharacter : public AFrontierBaseCharacter
{
	GENERATED_BODY()

public:
	AFrontierPlayerCharacter();
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual EFrontierHitReactionLevel GetCurrentHitReactionResistance() const override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void OnJumped_Implementation() override;
	virtual void Landed(const FHitResult& Hit) override;

	UFUNCTION(BlueprintPure, Category="Weapon")
	AFrontierWeaponBase* GetCurrentWeapon() const;

	UFUNCTION(BlueprintPure, Category="Equipment")
	UFrontierEquipmentComponent* GetEquipmentComponent() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Modular Mesh")
	USkeletalMeshComponent* GetArmorMesh() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Modular Mesh")
	USkeletalMeshComponent* GetGloveMesh() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Modular Mesh")
	USkeletalMeshComponent* GetGreavesMesh() const;

	UFUNCTION(BlueprintPure, Category="Equipment|Modular Mesh")
	USkeletalMeshComponent* GetHeadMesh() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Preview")
	AFrontierCharacterPreviewActor* GetOrCreateCharacterPreviewActor(TSubclassOf<AFrontierCharacterPreviewActor> OverridePreviewActorClass, const FVector& PreviewLocation);

	UFUNCTION(BlueprintPure, Category="Frontier|Preview")
	AFrontierCharacterPreviewActor* GetCharacterPreviewActor() const;

	UFUNCTION(BlueprintCallable, Category="Frontier|Appearance")
	bool ApplyCharacterAppearance(const FFrontierCharacterAppearanceData& AppearanceData);

	UFUNCTION(BlueprintCallable, Category="Frontier|Appearance")
	bool ApplyCharacterType(EFrontierCharacterType CharacterType);

	UFUNCTION(BlueprintCallable, Category="Frontier|Appearance")
	void RequestCharacterType(EFrontierCharacterType CharacterType);

	UFUNCTION(BlueprintPure, Category="Frontier|Appearance")
	EFrontierCharacterType GetSelectedCharacterType() const { return SelectedCharacterType; }

	UFUNCTION(BlueprintPure, Category="Frontier|Movement")
	bool IsRolling() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Combat")
	bool IsAttackAnimationActive() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Combat")
	float GetAttackMovementSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Combat")
	EFrontierAttackAnimationMode GetCurrentAttackAnimationMode() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Combat")
	bool IsAttackDelayActive() const;

	UFUNCTION(BlueprintPure, Category="Frontier|Movement")
	float GetMovementDirection() const;

	bool IsAttackInputHeld() const { return bAttackInputHeld; }
	void RequestComboAttackToServer(int32 RequestedComboIndex);
	/** Sends a locally detected melee hit to the server for authoritative damage application. */
	void ReportClientAttackHit(AActor* TargetActor, FVector_NetQuantize HitLocation, const FFrontierMeleeTraceOverrides& TraceOverrides);
	void SetNockedArrowVisual(bool bVisible, UStaticMesh* ArrowMesh, FName CharacterSocketName, const FTransform& RelativeTransform);

	void SetAttackMovementSpeedMultiplierInternal(float NewMultiplier);
	void SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode NewMode);
	void SetAttackUseControllerRotationInternal(bool bNewUseControllerRotation);

	void NotifyStaminaConsumptionFinished();
	void NotifyAttackStaminaConsumed();
	/** Starts an animated consumable use. Recovery is applied by the montage notify. */
	bool BeginConsumableUse(const FFrontierItemInstance& ItemInstance);
	/** Called by FrontierAnimNotify_ConsumePotion at the drink/effect frame. */
	void HandleConsumableUseNotify();
	void StartAttackDelay(float AttackPlayRate = 1.0f);
	void BeginRollMovementWindow();
	void EndRollMovementWindow();
	void HandleRollMoveStartNotify();
	void HandleRollMoveEndNotify();
	void CancelRollImmediately();
	void ApplyTemporaryDamageMoveSlow();
	void ApplyTeamMaterialFromPlayerState();
	void RefreshOverheadNameWidget();
	virtual void RevealCombatOverheadLocally(float HealthPercent) override;
	/** Smoothly faces an interaction target while keeping camera look input independent. */
	void BeginInteractionFacing(const FVector& TargetWorldLocation);
	void EndInteractionFacing();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlaySkillMontage(UAnimMontage* Montage, EFrontierAttackAnimationMode AnimationMode);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual AFrontierLootContainerActor* SpawnDeathLootContainer() override;
	virtual void CreateDeathLootSlots(TArray<FFrontierInventorySlot>& OutLootSlots) const override;
	virtual void CreateDeathLoadoutSlots(TArray<FFrontierLoadoutSlot>& OutLoadoutSlots) const override;
	virtual EFrontierLootContainerSourceType GetDeathLootContainerSourceType() const override;
	virtual void HandleItemsDroppedOnDeath(const TArray<FFrontierInventorySlot>& DroppedLootSlots);
	virtual void HandleAimingStateChanged() override;
	virtual void HandleMovementSpeedModifiersChanged() override;

	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Input_AttackStarted();
	void Input_AttackReleased();
	void Input_JumpStarted();
	void Input_JumpEnded();
	void Input_Inventory();
	void Input_Interact();
	void Input_SprintStarted();
	void Input_SprintEnded();
	void Input_Roll();
	void Input_UseSkillSlot1();
	void Input_UseSkillSlot2();
	void Input_UseSkillSlot3();
	void OnRightClick();
	void Input_FreeLookStarted();
	void Input_FreeLookEnded();

	bool CanStartSprinting() const;
	bool CanRoll() const;
	void CancelActiveAttackForRoll();
	bool TryConfirmAreaSkillInput();
	bool TryCancelAreaSkillInput();
	bool TryAdjustAreaSkillTargetDistance(float LookPitchAxis);
	void SetAttackInputState(bool bPressed);
	void TryActivateHeldAttack();
	void ScheduleHeldAttackRetry(float DelaySeconds);
	void RequestUseSkillSlot(int32 SlotIndex);
	void RefreshMovementSpeed();
	void BindPassiveMovementAttributes();
	void UnbindPassiveMovementAttributes();
	void HandlePassiveMovementAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void ApplyTemporarySingleMeshMode();
	// TEMP: Declaration retained with the disabled implementation for easy restoration.
	void RefreshModularMeshLeaderPose();
	void SetSprintingInternal(bool bNewSprinting);
	void UpdateStamina(float DeltaSeconds);
	void UpdateOverheadNameVisibility();
	void BeginRoll(const FVector& RollDirection);
	void FinishRoll();
	FVector GetDesiredRollDirection() const;
	void ApplyRollDirectionRotation(const FVector& RollDirection);
	void SetRollingInternal(bool bNewRolling);
	void UpdateRollMovement();
	void UpdateRollCamera(float DeltaSeconds);
	void UpdateFreeLookCameraReturn(float DeltaSeconds);
	void UpdateAimingCamera(float DeltaSeconds);
	void UpdateInteractionFacing(float DeltaSeconds);
	void SetFreeLooking(bool bNewFreeLooking);
	void BindAttackStateTagDelegate();
	void ClearDamageMoveSlow();

	UFUNCTION()
	void HandleAttackStateTagChanged(FGameplayTag CallbackTag, int32 NewCount);
	void BindToFrontierPlayerState();
	void UnbindFromFrontierPlayerState();
	void HandleTeamIdChanged(AFrontierPlayerState* ChangedPlayerState, int32 NewTeamId);
	void ResetSkillMontageAnimationMode();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> RightClickAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SwapWeaponAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> RollAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> UseSkillSlot1Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> UseSkillSlot2Action;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> UseSkillSlot3Action;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UFrontierEquipmentComponent> EquipmentComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Modular Mesh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> ArmorMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Modular Mesh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> GloveMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Modular Mesh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> GreavesMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components|Modular Mesh", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USkeletalMeshComponent> HeadMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UWidgetComponent> OverheadNameWidgetComponent;

	/** How long an enemy player's name and HP remain visible after this client damages them. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|UI|Overhead", meta=(ClampMin="0.1", AllowPrivateAccess="true"))
	float EnemyOverheadRevealDuration = 3.0f;

	float EnemyOverheadVisibleUntilSeconds = -1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> NockedArrowMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PotionMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input")
	TObjectPtr<UInputAction> InventoryAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Team")
	TObjectPtr<UFrontierTeamVisualDataAsset> TeamVisualDataAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Preview")
	TSubclassOf<AFrontierCharacterPreviewActor> CharacterPreviewActorClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Preview")
	FVector CharacterPreviewWorldOffset = FVector(0.0f, 0.0f, -10000.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float WalkSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float SprintSpeed = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float BaseJumpZVelocity = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float SprintStaminaDrainPerSecond = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float StaminaRecoveryPerSecond = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float StaminaRecoveryDelay = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Combat", meta=(ClampMin="0.0"))
	float AttackDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float MinStaminaToStartSprint = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float JumpStaminaCost = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Stamina", meta=(ClampMin="0.0"))
	float RollStaminaCost = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float RollLaunchStrength = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float RollMoveSpeed = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement", meta=(ClampMin="0.0"))
	float RollDuration = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement|HitReact", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DamageMoveSlowMultiplier = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement|HitReact", meta=(ClampMin="0.0"))
	float DamageMoveSlowDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Movement")
	TObjectPtr<UAnimMontage> RollMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera")
	bool bRotateCameraBehindDuringRoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera", meta=(ClampMin="0.0"))
	float RollCameraFollowInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera")
	float RollCameraPitchOffset = -12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera")
	bool bEnableAltFreeLook = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera", meta=(ClampMin="0.0"))
	float FreeLookCameraReturnInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera")
	float FreeLookMinPitch = -70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Frontier|Camera")
	float FreeLookMaxPitch = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Camera|Aiming")
	FVector AimingCameraSocketOffset = FVector(239.544019f, 55.739818f, 68.525823f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Camera|Aiming", meta=(ClampMin="0.0"))
	float AimingCameraActivationDelay = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Camera|Aiming", meta=(ClampMin="0.0"))
	float AimingCameraInterpSpeed = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Frontier|Interaction", meta=(ClampMin="0.0"))
	float InteractionFacingInterpSpeed = 10.0f;

	UPROPERTY(Transient)
	FVector DefaultCameraSocketOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	float AimingStartedAtSeconds = -1.0f;

	UFUNCTION()
	void OnRep_IsSprinting();

	UFUNCTION()
	void OnRep_IsRolling();

	UFUNCTION()
	void OnRep_SelectedCharacterType();

	UFUNCTION(Server, Reliable)
	void ServerSetSprinting(bool bNewSprinting);

	UFUNCTION(Server, Reliable)
	void ServerSetCharacterType(EFrontierCharacterType NewCharacterType);

	UFUNCTION(Server, Reliable)
	void ServerRequestRoll(FVector_NetQuantizeNormal RollDirection);

	UFUNCTION(Server, Reliable)
	void ServerSetAttackInputState(bool bPressed);

	UFUNCTION(Server, Reliable)
	void ServerReportAttackHit(AActor* TargetActor, FVector_NetQuantize HitLocation, FFrontierMeleeTraceOverrides TraceOverrides);

	UFUNCTION(Server, Reliable)
	void ServerRequestComboAttack(int32 RequestedComboIndex);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastSetNockedArrowVisual(bool bVisible, UStaticMesh* ArrowMesh, FName CharacterSocketName, FTransform RelativeTransform);

	void ApplyNockedArrowVisual(bool bVisible, UStaticMesh* ArrowMesh, FName CharacterSocketName, const FTransform& RelativeTransform);

	UFUNCTION(Server, Reliable)
	void ServerConfirmAreaSkillInput();

	UFUNCTION(Server, Reliable)
	void ServerCancelAreaSkillInput();

	UFUNCTION(Server, Unreliable)
	void ServerAdjustAreaSkillTargetDistance(float LookPitchAxis);

	UFUNCTION(Server, Reliable)
	void ServerBeginRollMovementWindow();

	UFUNCTION(Server, Reliable)
	void ServerEndRollMovementWindow();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayRollMontage(FVector_NetQuantizeNormal RollDirection);

	UFUNCTION(Server, Reliable)
	void ServerConsumeConsumableFromNotify();

	UFUNCTION(Server, Reliable)
	void ServerCancelConsumableUse();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayConsumableUse(
		UAnimMontage* Montage,
		UStaticMesh* PotionMesh,
		FName PotionSocketName,
		FTransform PotionMeshRelativeTransform,
		float HealthRestoreAmount,
		float StaminaRestoreAmount);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastFinishConsumableUse();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastCancelConsumableUse();

	void ApplyConsumablePotionVisual(UStaticMesh* PotionMesh, FName PotionSocketName, const FTransform& RelativeTransform);
	void ClearConsumablePotionVisual();
	void HandleConsumableMontageEnded(UAnimMontage* Montage, bool bInterrupted);

	UPROPERTY(ReplicatedUsing=OnRep_IsSprinting)
	bool bIsSprinting = false;

	UPROPERTY(ReplicatedUsing=OnRep_IsRolling)
	bool bIsRolling = false;

	UPROPERTY(ReplicatedUsing=OnRep_SelectedCharacterType, VisibleAnywhere, BlueprintReadOnly, Category="Frontier|Appearance")
	EFrontierCharacterType SelectedCharacterType = EFrontierCharacterType::DarkKnight;

	UPROPERTY(Transient)
	float AttackMovementSpeedMultiplier = 1.0f;

	UPROPERTY(Transient)
	bool bDamageMoveSlowActive = false;

	UPROPERTY(Transient)
	EFrontierAttackAnimationMode CurrentAttackAnimationMode;

	TEnumAsByte<ERootMotionMode::Type> CachedRootMotionMode = ERootMotionMode::RootMotionFromMontagesOnly;
	bool bCachedAttackRootMotionMode = false;
	bool bAttackUseControllerRotation = false;
	bool bInteractionRotationLocked = false;
	bool bAttackInputHeld = false;
	float InteractionTargetYaw = 0.0f;
	bool bIsFreeLooking = false;
	bool bReturningFreeLookCamera = false;

	bool bRollRequestPending = false;
	bool bRollMovementActive = false;

	bool bConsumableUsePending = false;
	float PendingConsumableHealthRestore = 0.0f;
	float PendingConsumableStaminaRestore = 0.0f;
	UPROPERTY(Transient)
	TSubclassOf<UGameplayEffect> PendingConsumableEffectClass;
	TObjectPtr<UAnimMontage> ActiveConsumableMontage = nullptr;

	float LastStaminaConsumeTime = -1000.0f;
	float LastAttackResetTime = -1000.0f;
	float ResolvedAttackDelay = 0.25f;

	FTimerHandle DamageMoveSlowTimerHandle;
	FTimerHandle SkillMontageAnimationModeResetTimerHandle;
	FTimerHandle HeldAttackRetryTimerHandle;
	TWeakObjectPtr<AFrontierPlayerState> BoundFrontierPlayerState;

	FVector CachedRollDirection = FVector::ForwardVector;
	FVector CachedDesiredMovementDirection = FVector::ZeroVector;
	float CachedGroundFriction = 8.0f;
	float CachedBrakingDecelerationWalking = 2048.0f;
	float CachedBrakingFrictionFactor = 2.0f;
	bool bCachedRollMovementSettings = false;

	TWeakObjectPtr<UFrontierAbilitySystemComponent> BoundAttackStateASC;
	FDelegateHandle AttackStateTagChangedHandle;
	TWeakObjectPtr<UFrontierAbilitySystemComponent> BoundPassiveMovementASC;
	FDelegateHandle MoveSpeedBonusChangedHandle;
	FDelegateHandle JumpPowerBonusChangedHandle;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierCharacterPreviewActor> CharacterPreviewActor;
};


