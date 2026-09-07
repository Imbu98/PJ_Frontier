#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FrontierCameraFeedbackComponent.generated.h"

class UCameraComponent;
class UCurveFloat;
class UMaterialInstanceDynamic;
class UMaterialInterface;

/** Local-only camera presentation for gameplay feedback received through the owning PlayerController. */
UCLASS(ClassGroup=(Frontier), meta=(BlueprintSpawnableComponent))
class FRONTIER_API UFrontierCameraFeedbackComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFrontierCameraFeedbackComponent();

	void PlayHeavyHitFeedback(const FVector& HitLocation);
	void StopHeavyHitFeedback();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	bool EnsureHitFocusSetup();
	UCameraComponent* ResolveLocalCamera() const;
	FVector2D ResolveFocusCenter(const FVector& HitLocation) const;
	float EvaluateBlurCurve(float NormalizedTime) const;
	void ApplyHitFocusParameters(float BlurStrength, const FVector2D& FocusCenter) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus")
	TSoftObjectPtr<UMaterialInterface> HitFocusMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus")
	TObjectPtr<UCurveFloat> HeavyHitBlurCurve;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus", meta=(ClampMin="0.01"))
	float HeavyHitEffectDuration = 0.12f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus", meta=(ClampMin="0.0"))
	float MaximumBlurStrength = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FocusRadius = 0.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus")
	FName BlurStrengthParameterName = TEXT("BlurStrength");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus")
	FName FocusRadiusParameterName = TEXT("FocusRadius");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Camera Feedback|Hit Focus")
	FName FocusCenterParameterName = TEXT("FocusCenter");

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HitFocusMID;

	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> RegisteredCamera;

	FVector2D ActiveFocusCenter = FVector2D(0.5f, 0.5f);
	float HeavyHitEffectElapsedTime = 0.0f;
	bool bPlayingHeavyHitEffect = false;
};
