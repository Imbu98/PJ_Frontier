#include "UI/FrontierInteractionProgressWidget.h"

void UFrontierInteractionProgressWidget::SetInteractionProgress(const float InProgress)
{
	Progress = FMath::Clamp(InProgress, 0.0f, 1.0f);
	BP_OnInteractionProgressChanged(Progress);
}
