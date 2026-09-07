#include "Game/FrontierLobbyGameMode.h"

#include "Game/FrontierLobbyPlayerController.h"
#include "Game/FrontierPlayerState.h"

AFrontierLobbyGameMode::AFrontierLobbyGameMode()
{
	PlayerControllerClass = AFrontierLobbyPlayerController::StaticClass();
	PlayerStateClass = AFrontierPlayerState::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}
