#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/FrontierLobbyMapIconWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FFrontierLobbyMapIconMapIdTest,
	"Frontier.UI.Lobby.MapIconMapId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFrontierLobbyMapIconMapIdTest::RunTest(
	const FString& Parameters)
{
	UFrontierLobbyMapIconWidget* MapIcon = NewObject<UFrontierLobbyMapIconWidget>();
	if (!MapIcon)
	{
		AddError(TEXT("Could not create a map icon widget for the MapId test."));
		return false;
	}

	MapIcon->SetMapId(TEXT("Map_DarkForest"));
	TestEqual(
		TEXT("Map icon returns the configured DataTable row ID"),
		MapIcon->GetMapId(),
		FName(TEXT("Map_DarkForest")));
	return true;
}

#endif
