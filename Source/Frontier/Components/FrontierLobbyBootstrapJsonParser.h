#pragma once

#include "CoreMinimal.h"
#include "Player/FrontierOnlineBootstrapTypes.h"

namespace FrontierLobbyBootstrapJson
{
/**
 * Parses the lobby bootstrap payload in the Frontier module.
 *
 * The HTTP transport lives in FrontierOnline, but the composite DTO is created
 * here so no nested inventory/storage/equipment object crosses a DLL boundary.
 */
bool Parse(
	int32 HttpStatus,
	const FString& ResponseBody,
	FFrontierOnlineLobbyBootstrapResponse& OutResponse,
	FString& OutError);
}
