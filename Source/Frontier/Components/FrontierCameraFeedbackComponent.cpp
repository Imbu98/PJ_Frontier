#include "Components/FrontierCameraFeedbackComponent.h"

#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

UFrontierCameraFeedbackComponent::UFrontierCameraFeedbackComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	SetIsReplicatedByDefault(false);
}

void UFrontierCameraFeedbackComponent::BeginPlay()
{
	Super::BeginPlay();

	if (APlayerController* PlayerController = Cast<APlayerController>(GetOwner()))
	{
		if (!PlayerController->IsLocalController())
		{
			SetComponentTickEnabled(false);
		}
	}
}

void UFrontierCameraFeedbackComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopHeavyHitFeedback();
	if (RegisteredCamera && HitFocusMID)
	{
		RegisteredCamera->PostProcessSettings.RemoveBlendable(HitFocusMID);
	}
	RegisteredCamera = nullptr;
	HitFocusMID = nullptr;

	Super::EndPlay(EndPlayReason);
}

void UFrontierCameraFeedbackComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlayingHeavyHitEffect || !HitFocusMID)
	{
		SetComponentTickEnabled(false);
		return;
	}

	HeavyHitEffectElapsedTime += FMath::Max(0.0f, DeltaTime);
	const float NormalizedTime = FMath::Clamp(
		HeavyHitEffectElapsedTime / FMath::Max(HeavyHitEffectDuration, KINDA_SMALL_NUMBER),
		0.0f,
		1.0f);
	ApplyHitFocusParameters(EvaluateBlurCurve(NormalizedTime), ActiveFocusCenter);

	if (NormalizedTime >= 1.0f)
	{
		StopHeavyHitFeedback();
	}
}

void UFrontierCameraFeedbackComponent::PlayHeavyHitFeedback(const FVector& HitLocation)
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController() || !EnsureHitFocusSetup())
	{
		return;
	}

	ActiveFocusCenter = ResolveFocusCenter(HitLocation);
	HeavyHitEffectElapsedTime = 0.0f;
	bPlayingHeavyHitEffect = true;
	ApplyHitFocusParameters(0.0f, ActiveFocusCenter);
	SetComponentTickEnabled(true);
}

void UFrontierCameraFeedbackComponent::StopHeavyHitFeedback()
{
	bPlayingHeavyHitEffect = false;
	HeavyHitEffectElapsedTime = 0.0f;
	if (HitFocusMID)
	{
		ApplyHitFocusParameters(0.0f, ActiveFocusCenter);
	}
	SetComponentTickEnabled(false);
}

bool UFrontierCameraFeedbackComponent::EnsureHitFocusSetup()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	UCameraComponent* Camera = ResolveLocalCamera();
	if (!Camera)
	{
		return false;
	}

	if (!HitFocusMID)
	{
		UMaterialInterface* Material = HitFocusMaterial.LoadSynchronous();
		if (!Material)
		{
			return false;
		}
		HitFocusMID = UMaterialInstanceDynamic::Create(Material, this);
		if (!HitFocusMID)
		{
			return false;
		}
	}

	if (RegisteredCamera != Camera)
	{
		if (RegisteredCamera)
		{
			RegisteredCamera->PostProcessSettings.RemoveBlendable(HitFocusMID);
		}
		Camera->PostProcessSettings.AddBlendable(HitFocusMID, 1.0f);
		RegisteredCamera = Camera;
	}

	HitFocusMID->SetScalarParameterValue(FocusRadiusParameterName, FocusRadius);
	HitFocusMID->SetVectorParameterValue(
		FocusCenterParameterName,
		FLinearColor(ActiveFocusCenter.X, ActiveFocusCenter.Y, 0.0f, 0.0f));
	return true;
}

UCameraComponent* UFrontierCameraFeedbackComponent::ResolveLocalCamera() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const APawn* Pawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UCameraComponent>() : nullptr;
}

FVector2D UFrontierCameraFeedbackComponent::ResolveFocusCenter(const FVector& HitLocation) const
{
	const FVector2D DefaultFocusCenter(0.5f, 0.5f);
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || HitLocation.IsNearlyZero())
	{
		return DefaultFocusCenter;
	}

	FVector2D ScreenPosition;
	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
	if (ViewportWidth <= 0 || ViewportHeight <= 0
		|| !PlayerController->ProjectWorldLocationToScreen(HitLocation, ScreenPosition, true))
	{
		return DefaultFocusCenter;
	}

	return FVector2D(
		FMath::Clamp(ScreenPosition.X / static_cast<float>(ViewportWidth), 0.0f, 1.0f),
		FMath::Clamp(ScreenPosition.Y / static_cast<float>(ViewportHeight), 0.0f, 1.0f));
}

float UFrontierCameraFeedbackComponent::EvaluateBlurCurve(const float NormalizedTime) const
{
	float CurveValue = 0.0f;
	if (HeavyHitBlurCurve)
	{
		CurveValue = HeavyHitBlurCurve->GetFloatValue(NormalizedTime);
	}
	else if (NormalizedTime < 0.1667f)
	{
		CurveValue = FMath::Lerp(0.0f, 1.0f, NormalizedTime / 0.1667f);
	}
	else if (NormalizedTime < 0.6667f)
	{
		CurveValue = FMath::Lerp(1.0f, 0.375f, (NormalizedTime - 0.1667f) / 0.5f);
	}
	else
	{
		CurveValue = FMath::Lerp(0.375f, 0.0f, (NormalizedTime - 0.6667f) / 0.3333f);
	}

	return FMath::Max(0.0f, CurveValue) * MaximumBlurStrength;
}

void UFrontierCameraFeedbackComponent::ApplyHitFocusParameters(
	const float BlurStrength,
	const FVector2D& FocusCenter) const
{
	if (!HitFocusMID)
	{
		return;
	}

	HitFocusMID->SetScalarParameterValue(BlurStrengthParameterName, FMath::Max(0.0f, BlurStrength));
	HitFocusMID->SetScalarParameterValue(FocusRadiusParameterName, FocusRadius);
	HitFocusMID->SetVectorParameterValue(
		FocusCenterParameterName,
		FLinearColor(FocusCenter.X, FocusCenter.Y, 0.0f, 0.0f));
}
