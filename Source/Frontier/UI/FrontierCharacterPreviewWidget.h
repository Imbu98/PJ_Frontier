#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayEffectTypes.h"
#include "Character/FrontierCharacterTypes.h"
#include "Inventory/FrontierInventoryTypes.h"
#include "FrontierCharacterPreviewWidget.generated.h"

class ACharacter;
class AFrontierCharacterPreviewActor;
class AFrontierPlayerState;
class UButton;
class UFrontierAbilitySystemComponent;
class UFrontierEquipmentComponent;
class UFrontierEquipmentSkillComponent;
class UFrontierLoadoutComponent;
class UFrontierQuickSlotBarWidget;
class UFrontierSkillBarWidget;
class UFrontierAttributeSet;
class UFrontierCharacterAppearanceDataAsset;
class UImage;
class UTextBlock;
class UWidget;

UCLASS()
class FRONTIER_API UFrontierCharacterPreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetPreviewCharacter(ACharacter* InCharacter);
	void SetPreviewPlayerState(AFrontierPlayerState* InPlayerState);
	void ClearPreview();
	void RefreshPreview();

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	void EnsurePreviewActor();
	FVector ResolvePreviewActorLocation() const;
	void BindEquipmentComponent();
	void UnbindEquipmentComponent();
	void BindLoadoutComponent();
	void UnbindLoadoutComponent();
	void BindQuickSlotWidget();
	void BindSkillBarWidget();
	void BindCharacterInfoSources();
	void UnbindCharacterInfoSources();
	void RefreshCharacterInfoPanel();
	float ResolvePreviewAttackPower() const;
	float ResolvePreviewDefense() const;
	void RefreshCharacterSelectionUI();
	void RefreshPreviewCharacter();
	bool ApplyPreviewAppearance(EFrontierCharacterType CharacterType);
	void SelectCharacter(EFrontierCharacterType CharacterType);
	FString BuildSkillLine(int32 SlotIndex) const;

	UFUNCTION()
	void HandleDarkKnightClicked();

	UFUNCTION()
	void HandleDarkLadyClicked();

	UFUNCTION()
	void HandleCurrentWeaponChanged(AActor* NewWeapon, int32 NewWeaponIndex);

	UFUNCTION()
	void HandleLoadoutChanged(const TArray<FFrontierLoadoutSlot>& Slots);

	UFUNCTION()
	void ShowCharacterInfoPanel();

	UFUNCTION()
	void HideCharacterInfoPanel();

	void HandleCharacterAttributeChanged(const FOnAttributeChangeData& ChangeData);
	void HandleEquipmentSkillsChanged();

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UImage> CharacterPreviewImage;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> CharacterInfoButton;
	
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UWidget> CharacterInfoPanel;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CharacterAttackPower;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CharacterDefense;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_EquipmentScore;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CharacterSkill1;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CharacterSkill2;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_CharacterSkill3;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_DarkKnight;

	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UButton> Button_DarkLady;

	/** Put the three quick-slot cells under this widget in the inventory character preview WBP. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UFrontierQuickSlotBarWidget> QuickSlotBarWidget;

	/** Displays the equipped weapon skills in the preview widget. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UFrontierSkillBarWidget> WBP_Skillbarwidget;

	UPROPERTY(EditDefaultsOnly, Category="Preview")
	TSubclassOf<AFrontierCharacterPreviewActor> PreviewActorClass;

	/** Optional per-widget override. Falls back to the project Character Appearance setting when empty. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preview|Appearance")
	TSoftObjectPtr<UFrontierCharacterAppearanceDataAsset> CharacterAppearanceData;

	UPROPERTY(EditDefaultsOnly, Category="Preview")
	FVector PreviewWorldOffset = FVector(0.0f, 0.0f, -10000.0f);

	UPROPERTY(Transient)
	TObjectPtr<ACharacter> PreviewSourceCharacter;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> PreviewPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierCharacterPreviewActor> PreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentComponent> BoundEquipmentComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierLoadoutComponent> BoundLoadoutComponent;

	UPROPERTY(Transient)
	TObjectPtr<AFrontierPlayerState> ObservedPlayerState;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAbilitySystemComponent> CachedAbilitySystemComponent;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierAttributeSet> CachedAttributeSet;

	UPROPERTY(Transient)
	TObjectPtr<UFrontierEquipmentSkillComponent> CachedSkillComponent;

	FDelegateHandle AttackPowerChangedHandle;
	FDelegateHandle DefenseChangedHandle;
	bool bOwnsPreviewActor = false;
};
