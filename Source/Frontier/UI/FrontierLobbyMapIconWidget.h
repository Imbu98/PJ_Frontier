#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "FrontierLobbyMapIconWidget.generated.h"

class UButton;
class UImage;
class UTexture2D;
class UWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFrontierLobbyMapIconClickedSignature);

/**
 * Parent class for a map icon WBP placed directly in the lobby map-selection UI.
 *
 * Each placed instance carries a MapId. The lobby resolves that ID through the
 * map information DataTable when the icon is clicked.
 */
UCLASS(Abstract, Blueprintable)
class FRONTIER_API UFrontierLobbyMapIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** DataTable row name used to resolve this map's information. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Lobby|Map Icon",
		meta=(ExposeOnSpawn=true))
	FName MapId = NAME_None;

	/** Optional. When unset, the brush authored on MapIconImage is preserved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Frontier|Lobby|Map Icon",
		meta=(ExposeOnSpawn=true))
	TObjectPtr<UTexture2D> MapIcon;

	/** Broadcast when the map icon is clicked. */
	UPROPERTY(BlueprintAssignable, Category="Frontier|Lobby|Map Icon")
	FFrontierLobbyMapIconClickedSignature OnMapIconClicked;

	UFUNCTION(BlueprintCallable, Category="Frontier|Lobby|Map Icon")
	void SetMapId(FName NewMapId);

	UFUNCTION(BlueprintCallable, Category="Frontier|Lobby|Map Icon")
	FName GetMapId() const { return MapId; }

	UFUNCTION(BlueprintCallable, Category="Frontier|Lobby|Map Icon")
	void SetSelected(bool bNewSelected);

	UFUNCTION(BlueprintPure, Category="Frontier|Lobby|Map Icon")
	bool IsSelected() const { return bSelected; }

	UFUNCTION(BlueprintImplementableEvent, Category="Frontier|Lobby|Map Icon",
		meta=(DisplayName="On Map Selection Changed"))
	void BP_OnMapSelectionChanged(bool bIsSelected);

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Use this exact optional name in the child WBP to receive click handling. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> MapButton;

	/** Use this exact optional name to receive MapIcon. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> MapIconImage;

	/** Optional outline authored in the WBP. Only its visibility is changed. */
	UPROPERTY(BlueprintReadOnly, Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> SelectedOutline;

private:
	UFUNCTION()
	void HandleMapButtonClicked();
	void RefreshSelectionVisuals();

	UPROPERTY(VisibleInstanceOnly, Category="Frontier|Lobby|Map Icon")
	bool bSelected = false;
};
