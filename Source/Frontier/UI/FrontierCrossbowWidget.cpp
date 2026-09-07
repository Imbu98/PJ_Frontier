#include "UI/FrontierCrossbowWidget.h"

#include "Components/Widget.h"

void UFrontierCrossbowWidget::SetChargeProgress(const float InChargeProgress)
{
	ChargeProgress = FMath::Clamp(InChargeProgress, 0.0f, 1.0f);
	if (ChargeReticle)
	{
		const float ReticleScale = FMath::Lerp(UnchargedReticleScale, FullyChargedReticleScale, ChargeProgress);
		ChargeReticle->SetRenderTransformPivot(FVector2D(0.5f));
		ChargeReticle->SetRenderScale(FVector2D(ReticleScale));
	}

	BP_OnChargeProgressChanged(ChargeProgress);
}
