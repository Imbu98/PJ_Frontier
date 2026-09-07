#include "Progression/FrontierRaidStatusSubsystem.h"

#include "Game/FrontierGameMode.h"
#include "Game/FrontierPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Engine/World.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void UFrontierRaidStatusSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (!IsRunningDedicatedServer())
	{
		return;
	}

	FParse::Value(FCommandLine::Get(), TEXT("RaidId="), RaidId);
	FParse::Value(FCommandLine::Get(), TEXT("BackendPort="), BackendPort);

	// Keep compatibility with the existing DS launch arguments, while documenting
	// RaidId as the canonical status identity.
	if (RaidId.IsEmpty())
	{
		FParse::Value(FCommandLine::Get(), TEXT("RaidServerId="), RaidId);
	}

	if (!IsConfigured())
	{
		return;
	}

	FHttpServerModule& HttpServer = FHttpServerModule::Get();
	Router = HttpServer.GetHttpRouter(static_cast<uint32>(BackendPort), true);
	if (!Router.IsValid())
	{
		return;
	}

	RouteHandle = Router->BindRoute(
		FHttpPath(TEXT("/status")),
		EHttpServerRequestVerbs::VERB_GET,
		FHttpRequestHandler::CreateUObject(this, &UFrontierRaidStatusSubsystem::HandleStatus));

	if (!RouteHandle.IsValid())
	{
		Router.Reset();
		return;
	}

	HttpServer.StartAllListeners();
	bListenersStarted = true;
}

void UFrontierRaidStatusSubsystem::Deinitialize()
{
	StopListener();
	Super::Deinitialize();
}

bool UFrontierRaidStatusSubsystem::IsConfigured() const
{
	return !RaidId.IsEmpty() && BackendPort > 0 && BackendPort <= 65535;
}

void UFrontierRaidStatusSubsystem::StopListener()
{
	if (Router.IsValid() && RouteHandle.IsValid())
	{
		Router->UnbindRoute(RouteHandle);
		RouteHandle = FHttpRouteHandle();
	}

	Router.Reset();
	if (bListenersStarted && FHttpServerModule::IsAvailable())
	{
		FHttpServerModule::Get().StopAllListeners();
	}
	bListenersStarted = false;
}

bool UFrontierRaidStatusSubsystem::HandleStatus(
	const FHttpServerRequest& Request,
	const FHttpResultCallback& OnComplete)
{
	UWorld* World = GetWorld();
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("raidId"), RaidId);

	FString MapId;
	if (World)
	{
		MapId = World->GetMapName();
		MapId.RemoveFromStart(World->StreamingLevelsPrefix);
	}
	Root->SetStringField(TEXT("mapId"), MapId);

	FString State = TEXT("WAITING");
	if (const AFrontierGameMode* GameMode = World ? World->GetAuthGameMode<AFrontierGameMode>() : nullptr)
	{
		State = GameMode->GetRaidStatusState();
	}
	Root->SetStringField(TEXT("state"), State);
	Root->SetNumberField(TEXT("uptimeSec"), World ? World->GetTimeSeconds() : 0.0);

	TArray<TSharedPtr<FJsonValue>> Players;
	if (World && World->GetGameState())
	{
		for (APlayerState* PlayerState : World->GetGameState()->PlayerArray)
		{
			const AFrontierPlayerState* FrontierPlayerState = Cast<AFrontierPlayerState>(PlayerState);
			if (!FrontierPlayerState || FrontierPlayerState->IsOnlyASpectator())
			{
				continue;
			}

			TSharedRef<FJsonObject> PlayerObject = MakeShared<FJsonObject>();
			PlayerObject->SetNumberField(TEXT("userId"), static_cast<double>(FrontierPlayerState->GetBackendUserId()));
			PlayerObject->SetStringField(TEXT("nickname"), FrontierPlayerState->GetPlayerName());
			PlayerObject->SetNumberField(TEXT("score"), FrontierPlayerState->GetScore());
			Players.Add(MakeShared<FJsonValueObject>(PlayerObject));
		}
	}
	Root->SetArrayField(TEXT("players"), Players);

	FString Body;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Body);
	FJsonSerializer::Serialize(Root, Writer);
	OnComplete(FHttpServerResponse::Create(Body, TEXT("application/json")));
	return true;
}
