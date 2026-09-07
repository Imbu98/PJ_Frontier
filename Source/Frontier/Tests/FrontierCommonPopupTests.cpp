#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "EnhancedActionKeyMapping.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "UI/FrontierCommonPopupTypes.h"
#include "UI/FrontierCommonPopupWidget.h"
#include "UI/FrontierPopupSubsystem.h"
#include "UI/FrontierSkillTreeNodeDetailPopupWidget.h"
#include "UI/FrontierSkillTreeWidget.h"
#include "UI/FrontierUISettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierCommonPopupStructureTest,
	"Frontier.UI.CommonPopup.Structure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierCommonPopupStructureTest::RunTest(const FString& Parameters)
{
	const FFrontierPopupRequest DefaultRequest;
	TestEqual(TEXT("Messages enqueue by default"), DefaultRequest.QueuePolicy, EFrontierPopupQueuePolicy::Enqueue);
	TestEqual(TEXT("Default popup type is information"), DefaultRequest.Type, EFrontierPopupType::Information);
	TestTrue(TEXT("Default popup shows its confirm button"), DefaultRequest.bShowConfirmButton);
	TestFalse(TEXT("Default popup hides its cancel button"), DefaultRequest.bShowCancelButton);
	TestEqual(TEXT("Default popup waits for input"), DefaultRequest.AutoCloseSeconds, 0.0f);
	const FFrontierPopupVisualStyle DefaultVisualStyle;
	TestFalse(TEXT("Popup styles preserve the WBP icon until explicitly overridden"),
		DefaultVisualStyle.bOverrideIconBrush);
	TestFalse(TEXT("Popup styles preserve the WBP background until explicitly overridden"),
		DefaultVisualStyle.bOverrideBackgroundBrush);
	TestFalse(TEXT("Popup styles preserve the WBP text colors until explicitly overridden"),
		DefaultVisualStyle.bOverrideTextColors);
	TestFalse(TEXT("Popup styles preserve the WBP button colors until explicitly overridden"),
		DefaultVisualStyle.bOverrideButtonColors);

	TestNotNull(TEXT("Common popup exposes title binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupTitleText")));
	TestNotNull(TEXT("Common popup exposes message binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupMessageText")));
	TestNotNull(TEXT("Common popup exposes icon binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupIconImage")));
	TestNotNull(TEXT("Common popup exposes background binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupBackgroundImage")));
	TestNotNull(TEXT("Common popup exposes confirm button binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupConfirmButton")));
	TestNotNull(TEXT("Common popup supports an optional fixed-width confirm container"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupConfirmButtonContainer")));
	TestNotNull(TEXT("Common popup exposes optional cancel button binding"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupCancelButton")));
	TestNotNull(TEXT("Common popup supports an optional fixed-width cancel container"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupCancelButtonContainer")));
	TestNotNull(TEXT("Common popup exposes a collapsible middle button gap"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupButtonGap")));
	TestNotNull(TEXT("Common popup exposes per-type visual styles"),
		FindFProperty<FProperty>(UFrontierCommonPopupWidget::StaticClass(), TEXT("PopupStyles")));

	TestNull(TEXT("Skill tree no longer owns unlock popup visuals"),
		FindFProperty<FProperty>(UFrontierSkillTreeWidget::StaticClass(), TEXT("UnlockConfirmPopupWidget")));
	TestNull(TEXT("Skill tree no longer owns reset popup visuals"),
		FindFProperty<FProperty>(UFrontierSkillTreeWidget::StaticClass(), TEXT("ResetConfirmPopupWidget")));
	TestNull(TEXT("Skill tree no longer owns warning popup visuals"),
		FindFProperty<FProperty>(UFrontierSkillTreeWidget::StaticClass(), TEXT("WarningPopupWidget")));
	TestNotNull(TEXT("Skill-specific hover detail popup remains on the skill tree"),
		FindFProperty<FProperty>(UFrontierSkillTreeWidget::StaticClass(), TEXT("NodeDetailPopupWidget")));
	const UFrontierSkillTreeWidget* SkillTreeDefaults = GetDefault<UFrontierSkillTreeWidget>();
	TestNotNull(TEXT("Skill-tree widget defaults exist"), SkillTreeDefaults);
	TestFalse(
		TEXT("Node double-click unlocks directly unless the designer explicitly enables confirmation"),
		SkillTreeDefaults && SkillTreeDefaults->bConfirmNodeUnlock);

	const UFrontierUISettings* Settings = GetDefault<UFrontierUISettings>();
	TestNotNull(TEXT("Frontier UI project settings exist"), Settings);
	TestTrue(TEXT("Common popup renders above normal game UI"), Settings && Settings->CommonPopupZOrder >= 100);
	TestTrue(TEXT("Popup manager is a local-player subsystem"),
		UFrontierPopupSubsystem::StaticClass()->IsChildOf(ULocalPlayerSubsystem::StaticClass()));

	for (const TCHAR* MappingPath : {
		TEXT("/Game/Input/IMC_Default.IMC_Default"),
		TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"),
		TEXT("/Game/SY/Input/IMC_Frontier.IMC_Frontier")})
	{
		const UInputMappingContext* MappingContext = LoadObject<UInputMappingContext>(nullptr, MappingPath);
		TestNotNull(FString::Printf(TEXT("Input mapping loads: %s"), MappingPath), MappingContext);
		if (!MappingContext)
		{
			continue;
		}
		const bool bUsesDevelopmentPointKey = MappingContext->GetMappings().ContainsByPredicate(
			[](const FEnhancedActionKeyMapping& Mapping)
			{
				return Mapping.Key == EKeys::P;
			});
		TestFalse(
			FString::Printf(TEXT("P remains free for the development skill-point shortcut: %s"), MappingPath),
			bUsesDevelopmentPointKey);
	}
	return true;
}

#endif
