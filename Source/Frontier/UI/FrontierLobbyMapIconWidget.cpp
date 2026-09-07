#include "UI/FrontierLobbyMapIconWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/Widget.h"

void UFrontierLobbyMapIconWidget::NativePreConstruct()
{
	Super::NativePreConstruct();

	if (MapIconImage && MapIcon)
	{
		MapIconImage->SetBrushFromTexture(MapIcon, true);
	}
}

void UFrontierLobbyMapIconWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (MapButton)
	{
		MapButton->OnClicked.RemoveDynamic(
			this,
			&UFrontierLobbyMapIconWidget::HandleMapButtonClicked);
		MapButton->OnClicked.AddUniqueDynamic(
			this,
			&UFrontierLobbyMapIconWidget::HandleMapButtonClicked);
		MapButton->SetIsEnabled(true);
	}

	// Older map icon blueprints may still contain the former level-lock visuals.
	// They are no longer authoritative because entry is validated by equipment score.
	if (UWidget* LegacyLockedOverlay = GetWidgetFromName(TEXT("LockedOverlay")))
	{
		LegacyLockedOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (UWidget* LegacyRequiredLevelText = GetWidgetFromName(TEXT("RequiredLevelText")))
	{
		LegacyRequiredLevelText->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshSelectionVisuals();
}

void UFrontierLobbyMapIconWidget::NativeDestruct()
{
	if (MapButton)
	{
		MapButton->OnClicked.RemoveDynamic(
			this,
			&UFrontierLobbyMapIconWidget::HandleMapButtonClicked);
	}

	Super::NativeDestruct();
}

void UFrontierLobbyMapIconWidget::SetMapId(const FName NewMapId)
{
	MapId = NewMapId;
}

void UFrontierLobbyMapIconWidget::SetSelected(const bool bNewSelected)
{
	bSelected = bNewSelected;
	RefreshSelectionVisuals();
}

void UFrontierLobbyMapIconWidget::HandleMapButtonClicked()
{
	if (MapId.IsNone())
	{
		return;
	}

	SetSelected(true);
	OnMapIconClicked.Broadcast();
}

void UFrontierLobbyMapIconWidget::RefreshSelectionVisuals()
{
	if (SelectedOutline)
	{
		SelectedOutline->SetVisibility(
			bSelected
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	BP_OnMapSelectionChanged(bSelected);
}
