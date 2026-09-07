#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FrontierPlayerStatusWidget.generated.h"

class UOverlay;
class UImage;
class UHorizontalBox;
class UProgressBar;
class UButton;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UWidget;
class AFrontierGameState;
class AFrontierBaseCharacter;
class AFrontierPlayerState;
class UFrontierAbilitySystemComponent;
class UFrontierAttributeSet;
class UFrontierEquipmentSkillComponent;
class UFrontierEquipmentComponent;
class UFrontierSkillBarWidget;
class UFrontierQuickSlotBarWidget;
class UFrontierCrossbowWidget;
struct FOnAttributeChangeData;

UCLASS()
class FRONTIER_API UFrontierPlayerStatusWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	void RebindToOwningPawnAttributes();
	void RefreshFromCachedAttributes();
	void RefreshSkillBarWidget();
	void AttachLobbyStatusWidget(UWidget* InWidget);
	void ShowTransientWarning(const FText& WarningText);
	void SetExtractionProgress(float InProgress);
	void ShowInteractionProgress();
	void SetInteractionProgress(float InProgress);
	void HideInteractionProgress();
	void ShowInteractionPrompt(const FText& TargetDisplayName, const FText& ActionText);
	void HideInteractionPrompt();
	void SetWeaponAttackCharge(float ChargeProgress);

private:
	void BindToOwningPawnAttributes();
	void UnbindFromOwningPawnAttributes();
	void BindSkillBarWidget();
	void BindQuickSlotWidget();
	void BindWeaponAttackWidget();
	void RefreshWeaponAttackWidgetVisibility();
	void RefreshObservedPlayerState();
	void RefreshTeamAndReadyTexts();
	void RefreshReadyCountText();
	void RefreshRaidTimerDisplay();
	void BuildRaidTimerImageSlots();
	void HideRaidTimerImageSlots();
	bool SetRaidTimerAtlasImage(UImage* Image, int32 AtlasIndex, const FLinearColor& Color) const;
	bool SetRaidTimerSeparatorImage(UImage* Image, const FLinearColor& Color) const;
	void RefreshSpectatorControls();
	bool ApplyInteractionProgressPercent(float InProgress);
	bool IsLobbyWorld() const;
	void ClearTransientWarning();
	void HandleReadyCountRefreshTick();
	void HandlePrevSpectatorClicked();
	void HandleNextSpectatorClicked();

	void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxHealthChanged(const FOnAttributeChangeData& ChangeData);
	void HandleStaminaChanged(const FOnAttributeChangeData& ChangeData);
	void HandleMaxStaminaChanged(const FOnAttributeChangeData& ChangeData);
	void HandleObservedTeamChanged(AFrontierPlayerState* PlayerState, int32 TeamId);
	void HandleObservedReadyChanged(AFrontierPlayerState* PlayerState);
	void ShowDamageIndicator();
	void HideDamageIndicator();

	UFUNCTION()
	void HandleCurrentWeaponChanged(AActor* NewWeapon, int32 NewWeaponIndex);

	UPROPERTY(Transient)
	TObjectPtr<AFrontierBaseCharacter> CachedCharacter;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAbilitySystemComponent> CachedAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAttributeSet> CachedAttributeSet;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> CachedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentSkillComponent> CachedSkillComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentComponent> CachedEquipmentComponent;

	FDelegateHandle HealthChangedHandle;
	FDelegateHandle MaxHealthChangedHandle;
	FDelegateHandle StaminaChangedHandle;
	FDelegateHandle MaxStaminaChangedHandle;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> HPBar;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> StaminaBar;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> StaminaText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> TeamText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ReadyCountText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> CenterWarningText;

	/** Replace the old RaidTimerText with a HorizontalBox using this exact name. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UHorizontalBox> RaidTimerImageContainer;

	/** Atlas cells are ordered left-to-right, with digits 0-9 by default. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer")
	TObjectPtr<UTexture2D> RaidTimerDigitAtlas;

	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer", meta=(ClampMin="1"))
	int32 RaidTimerAtlasColumns = 6;

	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer", meta=(ClampMin="1"))
	int32 RaidTimerAtlasRows = 2;

	/** Use index 10 when the atlas contains 0-9 followed by a colon. Set to -1 to use RaidTimerSeparatorTexture. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer")
	int32 RaidTimerSeparatorAtlasIndex = 10;

	/** Optional full-texture separator for a digits-only atlas. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer")
	TObjectPtr<UTexture2D> RaidTimerSeparatorTexture;

	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer")
	FVector2D RaidTimerDigitSize = FVector2D(32.0f, 48.0f);

	UPROPERTY(EditDefaultsOnly, Category="UI|Raid Timer")
	FMargin RaidTimerDigitPadding;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RaidTimerImageSlots;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UProgressBar> ExtractionProgressBar;
	
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UOverlay> ExtractionOverlay;
	
	

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> ExtractionProgressText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> PrevSpectatorButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UButton> NextSpectatorButton;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> SpectatorTargetText;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UVerticalBox> LobbyStatusHost;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillBarWidget> SkillBarWidget;

	/** Put the three action cells under this widget in the player status WBP. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierQuickSlotBarWidget> QuickSlotBarWidget;

	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UFrontierCrossbowWidget> CrossbowWidget;

	/** Optional full-screen image in WBP_PlayerStatus. Keep this exact instance name. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UImage> DamageIndicatorImage;

	/** Total time from full opacity to fully hidden. Editable in WBP_PlayerStatus Class Defaults. */
	UPROPERTY(EditDefaultsOnly, Category="UI|Damage Indicator", meta=(ClampMin="0.01"))
	float DamageIndicatorFadeDuration = 0.9f;

	UPROPERTY(EditDefaultsOnly, Category="UI|Damage Indicator", meta=(ClampMin="0.0", ClampMax="1.0"))
	float DamageIndicatorPeakOpacity = 1.0f;

	/** Add WBP_CircleProgressBar to WBP_PlayerStatus and keep this exact instance name. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> WBP_CircleProgressBar;

	/** Optional centered prompt root authored in WBP_PlayerStatus. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UWidget> InteractionPromptContainer;

	/** Optional key label authored in WBP_PlayerStatus; code supplies "[ F ] 키로". */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractionKeyText;

	/** Optional nearest interactable name authored in WBP_PlayerStatus. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractionTargetNameText;

	/** Optional numeric text authored in WBP_PlayerStatus; code supplies 0%..100%. */
	UPROPERTY(Transient, meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> InteractionProgressPercentText;

	
	
	
	

	FTimerHandle WarningClearTimerHandle;
	FTimerHandle ReadyCountRefreshTimerHandle;
	float CachedExtractionProgress = 0.0f;
	int32 CachedRaidTimerSeconds = INDEX_NONE;
	bool bLoggedAttributeBindPending = false;
	bool bLoggedInvalidInteractionProgressContract = false;
	float DamageIndicatorElapsedTime = 0.0f;
	bool bDamageIndicatorActive = false;
	
	
};
