#include "AbilitySystem/Abilities/FrontierGameplayAbility_PlayerAttack.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/FrontierBaseCharacter.h"
#include "Character/FrontierPlayerCharacter.h"
#include "AbilitySystem/FrontierAbilitySystemComponent.h"
#include "AbilitySystem/FrontierAttributeSet.h"
#include "Components/FrontierCombatComponent.h"
#include "Components/FrontierEquipmentComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Frontier.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/EngineTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Tags/FrontierGameplayTags.h"
#include "Weapons/FrontierWeaponBase.h"
#include "Weapons/FrontierWeaponDataAsset.h"
#include "TimerManager.h"

UFrontierGameplayAbility_PlayerAttack::UFrontierGameplayAbility_PlayerAttack()
{
}

void UFrontierGameplayAbility_PlayerAttack::HandleAttackInputPressed()
{
	if (CanAcceptComboInput())
	{
		RequestComboAttackInput();
	}
}

bool UFrontierGameplayAbility_PlayerAttack::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const AFrontierBaseCharacter* SourceCharacter = ActorInfo ? Cast<AFrontierBaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceCharacter);
	const UFrontierEquipmentComponent* EquipmentComponent = SourceCharacter ? SourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>() : nullptr;
	const UFrontierWeaponDataAsset* WeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	const UCharacterMovementComponent* MovementComponent = SourceCharacter ? SourceCharacter->GetCharacterMovement() : nullptr;

	return SourceCharacter
		&& !SourceCharacter->IsDead()
		&& (!PlayerCharacter || !PlayerCharacter->IsAttackDelayActive())
		&& MovementComponent
		&& !MovementComponent->IsFalling()
		&& EquipmentComponent
		&& EquipmentComponent->AreCurrentWeaponAttackAssetsReady()
		&& WeaponData
		&& !WeaponData->BasicComboAttacks.IsEmpty();
}

void UFrontierGameplayAbility_PlayerAttack::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	AFrontierBaseCharacter* SourceCharacter = ActorInfo ? Cast<AFrontierBaseCharacter>(ActorInfo->AvatarActor.Get()) : nullptr;
	if (!SourceCharacter || !ResolveAttackData(SourceCharacter) || !LoadedAttackMontage)
	{
		FRONTIER_LOG(Warning, TEXT("Player attack ability activation failed because weapon attack data could not be resolved."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	const UFrontierAttributeSet* FrontierAttributeSet = SourceCharacter->GetFrontierAttributeSet();
	if (!FrontierAttributeSet || FrontierAttributeSet->GetStamina() < CachedAttackActionData.StaminaCost)
	{
		FRONTIER_LOG(Warning, TEXT("Player attack ability activation failed because stamina is insufficient. Required=%.2f Current=%.2f"),
			CachedAttackActionData.StaminaCost,
			FrontierAttributeSet ? FrontierAttributeSet->GetStamina() : -1.0f);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		FRONTIER_LOG(Warning, TEXT("Player attack ability activation failed because CommitAbility was rejected."));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ConsumeCachedAttackStamina();

	if (UFrontierCombatComponent* CombatComponent = SourceCharacter->GetCombatComponent())
	{
		CombatComponent->ResetHitActorsThisAttack();
	}

	CachedAttackMovementSpeedMultiplier = CachedAttackActionData.AttackMovementSpeedMultiplier;
	CachedAttackAnimationMode = CachedAttackActionData.AttackAnimationMode;
	bComboInputBuffered = false;
	bComboWindowOpen = false;
	ReportedHitActorsThisAttack.Reset();
	AttackActivationTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(SourceCharacter))
	{
		PlayerCharacter->SetAttackUseControllerRotationInternal(true);
		PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(CachedAttackMovementSpeedMultiplier);
		PlayerCharacter->SetCurrentAttackAnimationModeInternal(CachedAttackAnimationMode);
	}

	LockCharacterMovementIfNeeded();

	if (!PlayCachedAttackMontage())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
	}
}

bool UFrontierGameplayAbility_PlayerAttack::PlayCachedAttackMontage()
{
	CachedAttackPlayRate = ResolveAttackPlayRate();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		LoadedAttackMontage,
		CachedAttackPlayRate);

	if (!MontageTask)
	{
		FRONTIER_LOG(Warning, TEXT("Player attack ability activation failed because montage task creation failed."));
		return false;
	}

	MontageTask->OnCompleted.AddDynamic(this, &UFrontierGameplayAbility_PlayerAttack::HandleMontageCompleted);
	MontageTask->OnBlendOut.AddDynamic(this, &UFrontierGameplayAbility_PlayerAttack::HandleMontageBlendOut);
	MontageTask->OnInterrupted.AddDynamic(this, &UFrontierGameplayAbility_PlayerAttack::HandleMontageCancelled);
	MontageTask->OnCancelled.AddDynamic(this, &UFrontierGameplayAbility_PlayerAttack::HandleMontageCancelled);
	MontageTask->ReadyForActivation();
	if (CachedWeapon)
	{
		CachedWeapon->PlayAttackSound(EFrontierWeaponAttackSound::MeleeAttack);
		CachedWeapon->PlayAttackSound(EFrontierWeaponAttackSound::AttackVoice);
	}

	if (UWorld* World = GetWorld())
	{
		const float TimeoutSeconds = FMath::Max(
			LoadedAttackMontage->GetPlayLength() / FMath::Max(CachedAttackPlayRate, 0.1f),
			0.1f) + MontageTimeoutBuffer;
		World->GetTimerManager().ClearTimer(AttackAbilityTimeoutTimerHandle);
		World->GetTimerManager().SetTimer(
			AttackAbilityTimeoutTimerHandle,
			this,
			&UFrontierGameplayAbility_PlayerAttack::HandleMontageCompleted,
			TimeoutSeconds,
			false);
	}

	return true;
}

void UFrontierGameplayAbility_PlayerAttack::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AttackAbilityTimeoutTimerHandle);
		World->GetTimerManager().ClearTimer(AutoComboHoldTimerHandle);
	}

	UnlockCharacterMovementIfNeeded();

	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->SetAttackUseControllerRotationInternal(false);
		PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(1.0f);
		PlayerCharacter->SetCurrentAttackAnimationModeInternal(EFrontierAttackAnimationMode::UpperBodyOnly);
	}

	CachedSourceCharacter = nullptr;
	CachedWeapon = nullptr;
	CachedWeaponData = nullptr;
	CachedWeaponElementalType = EFrontierElementalType::Normal;
	LoadedAttackMontage = nullptr;
	LoadedDamageEffectClass = nullptr;
	LoadedAdditionalHitEffectClasses.Reset();
	bHasCachedAttackActionData = false;
	CachedAttackMovementSpeedMultiplier = 1.0f;
	CachedAttackAnimationMode = EFrontierAttackAnimationMode::UpperBodyOnly;
	bComboInputBuffered = false;
	bComboWindowOpen = false;
	AttackActivationTimeSeconds = 0.0f;
	CachedAttackPlayRate = 1.0f;
	bSwitchingComboMontage = false;
	ReportedHitActorsThisAttack.Reset();
	NextComboIndex = 0;
	CurrentComboAttackIndex = INDEX_NONE;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UFrontierGameplayAbility_PlayerAttack::BeginAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (TraceOverrides.bResetHitActorsOnBegin && CachedSourceCharacter && CachedSourceCharacter->HasAuthority())
	{
		if (UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent())
		{
			CombatComponent->ResetHitActorsThisAttack();
		}
	}
}

void UFrontierGameplayAbility_PlayerAttack::TickAttackTraceWindow(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	ExecuteAttackTrace(TraceOverrides);
}

void UFrontierGameplayAbility_PlayerAttack::EndAttackTraceWindow()
{
}

void UFrontierGameplayAbility_PlayerAttack::RequestComboAttackInput()
{
	if (!CanAcceptComboInput())
	{
		return;
	}

	if (bComboInputBuffered && !bComboWindowOpen)
	{
		return;
	}

	bComboInputBuffered = true;
	if (bComboWindowOpen)
	{
		TryStartBufferedComboAttack();
	}
}

void UFrontierGameplayAbility_PlayerAttack::RequestComboAttackFromServer(const int32 RequestedComboIndex)
{
	if (!CachedSourceCharacter
		|| !CachedSourceCharacter->HasAuthority()
		|| !IsActive()
		|| !CachedWeaponData
		|| CachedWeaponData->BasicComboAttacks.Num() <= 1)
	{
		return;
	}

	const int32 ComboCount = CachedWeaponData->BasicComboAttacks.Num();
	const int32 NormalizedRequestedIndex =
		((RequestedComboIndex % ComboCount) + ComboCount) % ComboCount;

	// The server may already have processed the same transition from its own
	// animation notify. Treat the client request as idempotent in that case.
	if (CurrentComboAttackIndex == NormalizedRequestedIndex)
	{
		return;
	}

	if (NextComboIndex != NormalizedRequestedIndex)
	{
		FRONTIER_LOG(
			Warning,
			TEXT("Rejected combo transition because the requested combo index was out of sequence. Owner=%s Requested=%d Expected=%d Current=%d"),
			*GetNameSafe(CachedSourceCharacter),
			NormalizedRequestedIndex,
			NextComboIndex,
			CurrentComboAttackIndex);
		return;
	}

	bComboInputBuffered = true;
	TryStartBufferedComboAttack(false);
}

bool UFrontierGameplayAbility_PlayerAttack::CanAcceptComboInput() const
{
	if (!IsActive()
		|| !CachedSourceCharacter
		|| !CachedWeaponData
		|| CachedWeaponData->BasicComboAttacks.Num() <= 1
		|| !LoadedAttackMontage)
	{
		return false;
	}

	// The animation state is presentation data and is not reliable on a
	// dedicated server. Buffer input while the ability is active and let the
	// combo window or the client transition request decide when to advance.
	return true;
}

void UFrontierGameplayAbility_PlayerAttack::OpenComboAttackWindowFromNotify()
{
	if (!IsActive())
	{
		return;
	}

	bComboWindowOpen = true;
	const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter);
	if (bComboInputBuffered)
	{
		TryStartBufferedComboAttack();
		return;
	}

	if (!PlayerCharacter || !PlayerCharacter->IsAttackInputHeld())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || MinimumAutoComboHoldSeconds <= 0.0f)
	{
		TryStartBufferedComboAttack();
		return;
	}

	const float HeldSeconds = World->GetTimeSeconds() - AttackActivationTimeSeconds;
	const float ScaledMinimumHoldSeconds = MinimumAutoComboHoldSeconds / FMath::Max(CachedAttackPlayRate, 0.1f);
	const float RemainingHoldSeconds = ScaledMinimumHoldSeconds - HeldSeconds;
	if (RemainingHoldSeconds <= 0.0f)
	{
		TryStartBufferedComboAttack();
		return;
	}

	World->GetTimerManager().SetTimer(
		AutoComboHoldTimerHandle,
		this,
		&UFrontierGameplayAbility_PlayerAttack::TryStartHeldComboAttack,
		RemainingHoldSeconds,
		false);
}

void UFrontierGameplayAbility_PlayerAttack::ResetAttackStateFromNotify()
{
	NextComboIndex = 0;
	bComboInputBuffered = false;
	bComboWindowOpen = false;

	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->StartAttackDelay(CachedAttackPlayRate);
	}

	if (IsActive())
	{
		K2_EndAbility();
	}
}

void UFrontierGameplayAbility_PlayerAttack::HandleMontageCompleted()
{
	if (bSwitchingComboMontage)
	{
		return;
	}

	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->NotifyStaminaConsumptionFinished();
	}

	K2_EndAbility();
}

void UFrontierGameplayAbility_PlayerAttack::HandleMontageBlendOut()
{
	if (bSwitchingComboMontage)
	{
		return;
	}

	HandleMontageCompleted();
}

void UFrontierGameplayAbility_PlayerAttack::HandleMontageCancelled()
{
	if (bSwitchingComboMontage)
	{
		return;
	}

	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->NotifyStaminaConsumptionFinished();
	}

	K2_CancelAbility();
}

bool UFrontierGameplayAbility_PlayerAttack::ResolveAttackData(AFrontierBaseCharacter* SourceCharacter)
{
	CachedSourceCharacter = SourceCharacter;

	UFrontierEquipmentComponent* EquipmentComponent = SourceCharacter ? SourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>() : nullptr;
	CachedWeapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
	CachedWeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	CachedWeaponElementalType = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponElementalType()
		: EFrontierElementalType::Normal;

	if (!CachedWeaponData)
	{
		return false;
	}

	return ResolveAttackActionData();
}

bool UFrontierGameplayAbility_PlayerAttack::ResolveAttackDataForNextCombo()
{
	if (!CachedSourceCharacter)
	{
		return false;
	}

	UFrontierEquipmentComponent* EquipmentComponent = CachedSourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>();
	CachedWeapon = EquipmentComponent ? EquipmentComponent->GetCurrentWeapon() : nullptr;
	CachedWeaponData = EquipmentComponent ? EquipmentComponent->GetCurrentWeaponData() : nullptr;
	CachedWeaponElementalType = EquipmentComponent
		? EquipmentComponent->GetCurrentWeaponElementalType()
		: EFrontierElementalType::Normal;
	return CachedWeaponData && ResolveAttackActionData();
}

bool UFrontierGameplayAbility_PlayerAttack::HasEnoughStaminaForCachedAttack() const
{
	const UFrontierAttributeSet* FrontierAttributeSet = CachedSourceCharacter ? CachedSourceCharacter->GetFrontierAttributeSet() : nullptr;
	return FrontierAttributeSet && FrontierAttributeSet->GetStamina() >= CachedAttackActionData.StaminaCost;
}

void UFrontierGameplayAbility_PlayerAttack::ConsumeCachedAttackStamina()
{
	if (CachedSourceCharacter && CachedSourceCharacter->HasAuthority())
	{
		const bool bAppliedThroughGas = CachedSourceCharacter->GetFrontierAbilitySystemComponent()
			&& CachedSourceCharacter->GetFrontierAbilitySystemComponent()->ApplyStaminaDelta(-CachedAttackActionData.StaminaCost);
		if (!bAppliedThroughGas)
		{
			if (UFrontierAttributeSet* MutableAttributeSet = CachedSourceCharacter->GetMutableFrontierAttributeSet())
			{
				MutableAttributeSet->SetStamina(FMath::Max(0.0f, MutableAttributeSet->GetStamina() - CachedAttackActionData.StaminaCost));
			}
		}

		if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
		{
			PlayerCharacter->NotifyAttackStaminaConsumed();
		}
	}
}

float UFrontierGameplayAbility_PlayerAttack::ResolveAttackPlayRate() const
{
	if (!CachedSourceCharacter || !CachedWeaponData)
	{
		return 1.0f;
	}

	const UFrontierAttributeSet* Attributes = CachedSourceCharacter->GetFrontierAttributeSet();
	if (!Attributes)
	{
		return 1.0f;
	}

	const FFrontierGameplayTags& Tags = FFrontierGameplayTags::Get();
	float AttackSpeedBonus = 0.0f;
	if (CachedWeaponData->WeaponTypeTag.MatchesTag(Tags.WeaponTypeSword))
	{
		AttackSpeedBonus = Attributes->GetSwordAttackSpeedBonus();
	}
	else if (CachedWeaponData->WeaponTypeTag.MatchesTag(Tags.WeaponTypeAxe))
	{
		AttackSpeedBonus = Attributes->GetAxeAttackSpeedBonus();
	}

	return FMath::Clamp(1.0f + AttackSpeedBonus, 1.0f, FMath::Max(1.0f, MaxAttackPlayRate));
}

bool UFrontierGameplayAbility_PlayerAttack::TryStartBufferedComboAttack(const bool bNotifyServer)
{
	FRONTIER_LOG_FUNC();
	if (!CachedSourceCharacter || !CachedWeaponData || CachedWeaponData->BasicComboAttacks.Num() <= 1)
	{
		return false;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AutoComboHoldTimerHandle);
	}

	bComboInputBuffered = false;
	bComboWindowOpen = false;
	ReportedHitActorsThisAttack.Reset();

	UnlockCharacterMovementIfNeeded();

	const int32 RequestedComboIndex = NextComboIndex;
	if (!ResolveAttackDataForNextCombo() || !LoadedAttackMontage)
	{
		ResetAttackStateFromNotify();
		return false;
	}

	if (!HasEnoughStaminaForCachedAttack())
	{
		ResetAttackStateFromNotify();
		return false;
	}

	ConsumeCachedAttackStamina();

	if (UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent())
	{
		CombatComponent->ResetHitActorsThisAttack();
	}

	CachedAttackMovementSpeedMultiplier = CachedAttackActionData.AttackMovementSpeedMultiplier;
	CachedAttackAnimationMode = CachedAttackActionData.AttackAnimationMode;
	if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
	{
		PlayerCharacter->SetAttackUseControllerRotationInternal(true);
		PlayerCharacter->SetAttackMovementSpeedMultiplierInternal(CachedAttackMovementSpeedMultiplier);
		PlayerCharacter->SetCurrentAttackAnimationModeInternal(CachedAttackAnimationMode);
	}

	LockCharacterMovementIfNeeded();

	bSwitchingComboMontage = true;
	const bool bStartedComboMontage = PlayCachedAttackMontage();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UFrontierGameplayAbility_PlayerAttack::FinishComboMontageSwitch);
	}
	else
	{
		FinishComboMontageSwitch();
	}

	if (bStartedComboMontage
		&& bNotifyServer
		&& CachedSourceCharacter
		&& !CachedSourceCharacter->HasAuthority())
	{
		if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
		{
			PlayerCharacter->RequestComboAttackToServer(RequestedComboIndex);
		}
	}

	return bStartedComboMontage;
}

void UFrontierGameplayAbility_PlayerAttack::TryStartHeldComboAttack()
{
	const AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter);
	if (!IsActive()
		|| !bComboWindowOpen
		|| bComboInputBuffered
		|| !PlayerCharacter
		|| !PlayerCharacter->IsAttackInputHeld())
	{
		return;
	}

	TryStartBufferedComboAttack();
}

void UFrontierGameplayAbility_PlayerAttack::FinishComboMontageSwitch()
{
	bSwitchingComboMontage = false;
}

void UFrontierGameplayAbility_PlayerAttack::ExecuteAttackTrace(const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (!CachedSourceCharacter
		|| (CachedSourceCharacter->HasAuthority() && !CachedSourceCharacter->IsLocallyControlled())
		|| !CachedWeaponData
		|| !bHasCachedAttackActionData)
	{
		return;
	}

	UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const FVector FallbackTraceStart = CachedSourceCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 50.0f);
	FVector TraceStart = FallbackTraceStart;
	FVector TraceEnd = TraceStart + CachedSourceCharacter->GetActorForwardVector().GetSafeNormal() * CachedAttackActionData.AttackRange;
	bool bResolvedSocketTrace = false;

	if (CachedWeapon && CachedWeapon->GetWeaponMesh())
	{
		USkeletalMeshComponent* WeaponMesh = CachedWeapon->GetWeaponMesh();
		const bool bHasStartSocket = WeaponMesh->DoesSocketExist(CachedAttackActionData.TraceStartSocketName);
		const bool bHasEndSocket = WeaponMesh->DoesSocketExist(CachedAttackActionData.TraceEndSocketName);
		if (bHasStartSocket && bHasEndSocket)
		{
			TraceStart = WeaponMesh->GetSocketLocation(CachedAttackActionData.TraceStartSocketName);
			TraceEnd = WeaponMesh->GetSocketLocation(CachedAttackActionData.TraceEndSocketName);
			bResolvedSocketTrace = true;
		}
	}

	if (!bResolvedSocketTrace)
	{
		if (USkeletalMeshComponent* CharacterMesh = CachedSourceCharacter->GetMesh())
		{
			const bool bHasStartSocket = CharacterMesh->DoesSocketExist(CachedAttackActionData.TraceStartSocketName);
			const bool bHasEndSocket = CharacterMesh->DoesSocketExist(CachedAttackActionData.TraceEndSocketName);
			if (bHasStartSocket && bHasEndSocket)
			{
				TraceStart = CharacterMesh->GetSocketLocation(CachedAttackActionData.TraceStartSocketName);
				TraceEnd = CharacterMesh->GetSocketLocation(CachedAttackActionData.TraceEndSocketName);
			}
		}
	}

	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(CachedSourceCharacter);
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes;
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel2));
	ObjectTypes.Add(UEngineTypes::ConvertToObjectType(ECC_GameTraceChannel3));

	TArray<FHitResult> HitResults;
	if (!UKismetSystemLibrary::SphereTraceMultiForObjects(
		CombatComponent,
		TraceStart,
		TraceEnd,
		CachedAttackActionData.HitRadius,
		ObjectTypes,
		false,
		ActorsToIgnore,
		CachedAttackActionData.bDrawDebugTrace ? EDrawDebugTrace::ForDuration : EDrawDebugTrace::None,
		HitResults,
		true))
	{
		return;
	}

	for (const FHitResult& HitResult : HitResults)
	{
		AActor* TargetActor = HitResult.GetActor();
		if (!IsValid(TargetActor) || ReportedHitActorsThisAttack.Contains(TargetActor))
		{
			continue;
		}

		ReportedHitActorsThisAttack.Add(TargetActor);
		if (AFrontierPlayerCharacter* PlayerCharacter = Cast<AFrontierPlayerCharacter>(CachedSourceCharacter))
		{
			PlayerCharacter->ReportClientAttackHit(TargetActor, HitResult.ImpactPoint, TraceOverrides);
		}
	}
}

void UFrontierGameplayAbility_PlayerAttack::HandleClientReportedAttackHit(
	AActor* TargetActor,
	const FVector& HitLocation,
	const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (!CachedSourceCharacter || !CachedSourceCharacter->HasAuthority() || !IsActive() || !IsValid(TargetActor))
	{
		return;
	}

	if (ReportedHitActorsThisAttack.Contains(TargetActor))
	{
		FRONTIER_LOG(VeryVerbose, TEXT("Ignored duplicate client attack hit report. Source=%s Target=%s"), *GetNameSafe(CachedSourceCharacter), *GetNameSafe(TargetActor));
		return;
	}

	ReportedHitActorsThisAttack.Add(TargetActor);
	ApplyAttackDamageToTarget(TargetActor, HitLocation, TraceOverrides);
}

void UFrontierGameplayAbility_PlayerAttack::ApplyAttackDamageToTarget(
	AActor* TargetActor,
	const FVector& HitLocation,
	const FFrontierMeleeTraceOverrides& TraceOverrides)
{
	if (!CachedSourceCharacter || !CachedSourceCharacter->HasAuthority() || !TargetActor)
	{
		return;
	}

	UFrontierCombatComponent* CombatComponent = CachedSourceCharacter->GetCombatComponent();
	if (!CombatComponent)
	{
		return;
	}

	const FGameplayTag DefaultDamageTypeTag = CachedAttackActionData.DamageTypeTag.IsValid()
		? CachedAttackActionData.DamageTypeTag
		: FFrontierGameplayTags::Get().DamageTypeNormal;
	const FGameplayTag DamageTypeTag = TraceOverrides.bOverrideDamageType && TraceOverrides.DamageTypeTag.IsValid()
		? TraceOverrides.DamageTypeTag
		: DefaultDamageTypeTag;
	const float DamageMultiplier = TraceOverrides.bOverrideDamageMultiplier
		? FMath::Max(TraceOverrides.DamageMultiplier, 0.0f)
		: CachedAttackActionData.DamageMultiplier;
	const FGameplayTag HitReactionTag = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.HitReactionTag
		: CachedAttackActionData.HitReactionTag;
	const EFrontierHitReactionLevel HitReactionLevel = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.HitReactionLevel
		: CachedAttackActionData.HitReactionLevel;
	const float StaggerDuration = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.StaggerDuration
		: CachedAttackActionData.StaggerDuration;
	const float KnockbackHorizontalStrength = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.KnockbackHorizontalStrength
		: CachedAttackActionData.KnockbackHorizontalStrength;
	const float KnockbackVerticalStrength = TraceOverrides.bOverrideHitReaction
		? TraceOverrides.KnockbackVerticalStrength
		: CachedAttackActionData.KnockbackVerticalStrength;

	CombatComponent->ApplyDamageToTarget(
		TargetActor,
		0.0f,
		DamageTypeTag,
		LoadedDamageEffectClass,
		&LoadedAdditionalHitEffectClasses,
		DamageMultiplier,
		CachedWeaponElementalType,
		HitReactionTag,
		HitReactionLevel,
		StaggerDuration,
		KnockbackHorizontalStrength,
		KnockbackVerticalStrength,
		HitLocation);
}

bool UFrontierGameplayAbility_PlayerAttack::ResolveAttackActionData()
{
	if (!CachedWeaponData)
	{
		return false;
	}

	LoadedAdditionalHitEffectClasses.Reset();
	bHasCachedAttackActionData = false;
	CachedAttackActionData = FFrontierAttackActionData();

	if (CachedWeaponData->BasicComboAttacks.IsEmpty())
	{
		FRONTIER_LOG(Warning, TEXT("Player attack data resolve failed because BasicComboAttacks is empty. WeaponData=%s"),
			*GetNameSafe(CachedWeaponData));
		return false;
	}

	const int32 ComboAttackIndex = FMath::Clamp(NextComboIndex, 0, CachedWeaponData->BasicComboAttacks.Num() - 1);
	CachedAttackActionData = CachedWeaponData->BasicComboAttacks[ComboAttackIndex];
	CurrentComboAttackIndex = ComboAttackIndex;
	NextComboIndex = (ComboAttackIndex + 1) % CachedWeaponData->BasicComboAttacks.Num();
	bHasCachedAttackActionData = true;

	UFrontierEquipmentComponent* EquipmentComponent = CachedSourceCharacter
		? CachedSourceCharacter->FindComponentByClass<UFrontierEquipmentComponent>()
		: nullptr;
	if (!EquipmentComponent
		|| !EquipmentComponent->GetPreloadedAttackActionAssets(
			ComboAttackIndex,
			LoadedAttackMontage,
			LoadedDamageEffectClass,
			LoadedAdditionalHitEffectClasses))
	{
		FRONTIER_LOG(Warning, TEXT("Player attack data resolve failed because preloaded attack assets are not ready. Character=%s ComboIndex=%d"),
			*GetNameSafe(CachedSourceCharacter),
			ComboAttackIndex);
		return false;
	}

	return bHasCachedAttackActionData
		&& LoadedAttackMontage
		&& CachedAttackActionData.DamageMultiplier > 0.0f
		&& CachedAttackActionData.HitRadius > 0.0f;
}

void UFrontierGameplayAbility_PlayerAttack::LockCharacterMovementIfNeeded()
{
	bMovementLocked = false;

	if (!CachedSourceCharacter || !bHasCachedAttackActionData || !CachedAttackActionData.bLockMovementDuringAttack)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = CachedSourceCharacter->GetCharacterMovement())
	{
		if (MovementComponent->IsFalling())
		{
			return;
		}

		CachedMovementMode = MovementComponent->MovementMode;
		CachedCustomMovementMode = MovementComponent->CustomMovementMode;
		MovementComponent->StopMovementImmediately();

		// Root motion is extracted by the movement component while the montage is
		// evaluated. MOVE_None prevents that extraction from moving the character.
		// Keep the current mode for root-motion attacks and only disable regular
		// movement for montages that do not contain root motion.
		if (!LoadedAttackMontage || !LoadedAttackMontage->HasRootMotion())
		{
			MovementComponent->DisableMovement();
		}

		bMovementLocked = true;
	}
}

void UFrontierGameplayAbility_PlayerAttack::UnlockCharacterMovementIfNeeded()
{
	if (!bMovementLocked || !CachedSourceCharacter)
	{
		return;
	}

	if (UCharacterMovementComponent* MovementComponent = CachedSourceCharacter->GetCharacterMovement())
	{
		MovementComponent->SetMovementMode(CachedMovementMode, CachedCustomMovementMode);
	}

	bMovementLocked = false;
}
