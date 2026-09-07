#include "Online/FrontierBackendWebSocketSubsystem.h"

#include "Dom/JsonObject.h"
#include "Frontier.h"
#include "FrontierOnlineConfig.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "IWebSocket.h"
#include "Modules/ModuleManager.h"
#include "Online/FrontierPlayerSessionSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"
#include "WebSocketsModule.h"

namespace
{
constexpr int32 RequestedHeartbeatMilliseconds = 10000;
constexpr double StompConnectTimeoutSeconds = 15.0;
constexpr double HeartbeatGraceMultiplier = 2.5;
const FString PartyDestination(TEXT("/user/queue/party"));
const FString MatchmakingDestination(TEXT("/user/queue/matchmaking"));

bool ParseHeartbeatHeader(const FString& Value, int32& OutServerOutgoing, int32& OutServerIncoming)
{
	FString Left;
	FString Right;
	if (!Value.Split(TEXT(","), &Left, &Right)
		|| !Left.IsNumeric() || !Right.IsNumeric())
	{
		return false;
	}
	OutServerOutgoing = FMath::Max(0, FCString::Atoi(*Left));
	OutServerIncoming = FMath::Max(0, FCString::Atoi(*Right));
	return true;
}
}

void UFrontierBackendWebSocketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bWebSocketsModuleAvailable = FModuleManager::Get().LoadModulePtr<FWebSocketsModule>(TEXT("WebSockets")) != nullptr;
	if (!bWebSocketsModuleAvailable)
	{
		FRONTIER_LOG(
			Error,
			TEXT("[BackendWebSocket] WebSockets module is unavailable. Realtime party and matchmaking events are disabled."));
	}
}

void UFrontierBackendWebSocketSubsystem::Deinitialize()
{
	bDisconnectRequested = true;
	Disconnect();
	MatchmakingSubscriptions.Reset();
	OnConnected.Clear();
	OnPartyMessage.Clear();
	OnMatchmakingStatus.Clear();
	OnRaidServerReady.Clear();
	Super::Deinitialize();
}

void UFrontierBackendWebSocketSubsystem::ConnectAuthenticated()
{
	if (!bWebSocketsModuleAvailable || IsConnected() || bConnectInProgress)
	{
		return;
	}
	UFrontierPlayerSessionSubsystem* Session = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UFrontierPlayerSessionSubsystem>()
		: nullptr;
	if (!Session || !Session->HasAuthenticatedSession())
	{
		return;
	}
	bDisconnectRequested = false;
	bConnectInProgress = true;
	const TWeakObjectPtr<UFrontierBackendWebSocketSubsystem> WeakThis(this);
	Session->AcquireAccessToken(
		[WeakThis](const bool bSucceeded, const FString& AccessToken, const FString& Error)
		{
			UFrontierBackendWebSocketSubsystem* This = WeakThis.Get();
			if (!This)
			{
				return;
			}
			This->bConnectInProgress = false;
			if (!bSucceeded || AccessToken.IsEmpty())
			{
				FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Access token acquisition failed. Error=%s"), *Error);
				This->ScheduleReconnect();
				return;
			}
			This->ConnectWithToken(AccessToken);
		});
}

void UFrontierBackendWebSocketSubsystem::ConnectWithToken(const FString& AccessToken)
{
	if (!bWebSocketsModuleAvailable)
	{
		return;
	}

	const FFrontierOnlineConfig Config = FFrontierOnlineConfig::Load();
	if ((!Config.BackendWebSocketUrl.StartsWith(TEXT("ws://"), ESearchCase::IgnoreCase)
		&& !Config.BackendWebSocketUrl.StartsWith(TEXT("wss://"), ESearchCase::IgnoreCase))
		|| AccessToken.IsEmpty())
	{
		FRONTIER_LOG(Error, TEXT("[BackendWebSocket] BackendWebSocketUrl must use ws:// or wss://."));
		return;
	}

	PendingAccessToken = AccessToken;
	IncomingPacketBuffer.Reset();
	bStompConnected = false;
	Socket = FWebSocketsModule::Get().CreateWebSocket(
		Config.BackendWebSocketUrl,
		TEXT("v12.stomp"));
	Socket->OnConnected().AddUObject(this, &UFrontierBackendWebSocketSubsystem::HandleWebSocketConnected);
	Socket->OnConnectionError().AddUObject(this, &UFrontierBackendWebSocketSubsystem::HandleConnectionError);
	Socket->OnClosed().AddUObject(this, &UFrontierBackendWebSocketSubsystem::HandleClosed);
	Socket->OnRawMessage().AddUObject(this, &UFrontierBackendWebSocketSubsystem::HandleRawMessage);
	bConnectInProgress = true;
	Socket->Connect();
	FRONTIER_LOG(
		Log,
		TEXT("[BackendWebSocket] STOMP WebSocket connection started. Url=%s TokenLength=%d"),
		*Config.BackendWebSocketUrl,
		AccessToken.Len());
}

void UFrontierBackendWebSocketSubsystem::Disconnect()
{
	bDisconnectRequested = true;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ReconnectTimerHandle);
		GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}
	if (Socket.IsValid())
	{
		if (bStompConnected && Socket->IsConnected())
		{
			SendStompFrame(TEXT("DISCONNECT"), TMap<FString, FString>());
		}
		Socket->OnConnected().Clear();
		Socket->OnConnectionError().Clear();
		Socket->OnClosed().Clear();
		Socket->OnRawMessage().Clear();
		if (Socket->IsConnected())
		{
			Socket->Close(1000, TEXT("Session closed"));
		}
		Socket.Reset();
	}
	ResetConnectionState();
}

bool UFrontierBackendWebSocketSubsystem::SubscribeMatchmaking(const FString& TicketId)
{
	if (!bWebSocketsModuleAvailable)
	{
		return false;
	}

	FString NormalizedTicketId = TicketId;
	NormalizedTicketId.TrimStartAndEndInline();
	if (NormalizedTicketId.IsEmpty())
	{
		return false;
	}
	MatchmakingSubscriptions.Add(NormalizedTicketId);
	if (!IsConnected())
	{
		ConnectAuthenticated();
	}
	return true;
}

void UFrontierBackendWebSocketSubsystem::UnsubscribeMatchmaking(const FString& TicketId)
{
	FString NormalizedTicketId = TicketId;
	NormalizedTicketId.TrimStartAndEndInline();
	if (!NormalizedTicketId.IsEmpty())
	{
		MatchmakingSubscriptions.Remove(NormalizedTicketId);
	}
}

bool UFrontierBackendWebSocketSubsystem::IsConnected() const
{
	return bStompConnected && Socket.IsValid() && Socket->IsConnected();
}

void UFrontierBackendWebSocketSubsystem::HandleWebSocketConnected()
{
	const double Now = FPlatformTime::Seconds();
	StompConnectStartedSeconds = Now;
	LastSentSeconds = Now;
	LastReceivedSeconds = Now;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(
			HeartbeatTimerHandle,
			this,
			&UFrontierBackendWebSocketSubsystem::TickStompHeartbeat,
			1.0f,
			true);
	}
	SendConnectFrame();
}

void UFrontierBackendWebSocketSubsystem::HandleConnectionError(const FString& Error)
{
	FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Connection error. Error=%s"), *Error);
	ResetConnectionState();
	if (!bDisconnectRequested)
	{
		OnDisconnected.Broadcast();
	}
	ScheduleReconnect();
}

void UFrontierBackendWebSocketSubsystem::HandleClosed(
	const int32 StatusCode,
	const FString& Reason,
	const bool bWasClean)
{
	FRONTIER_LOG(
		Warning,
		TEXT("[BackendWebSocket] Closed. StatusCode=%d Clean=%d Reason=%s"),
		StatusCode,
		bWasClean ? 1 : 0,
		*Reason);
	ResetConnectionState();
	if (!bDisconnectRequested)
	{
		OnDisconnected.Broadcast();
		ScheduleReconnect();
	}
}

void UFrontierBackendWebSocketSubsystem::HandleRawMessage(
	const void* Data,
	const SIZE_T Size,
	const SIZE_T BytesRemaining)
{
	if (!Data || Size == 0)
	{
		return;
	}
	LastReceivedSeconds = FPlatformTime::Seconds();
	IncomingPacketBuffer.Append(static_cast<const uint8*>(Data), Size);
	if (BytesRemaining == 0)
	{
		ProcessIncomingPacket(IncomingPacketBuffer);
		IncomingPacketBuffer.Reset();
	}
}

void UFrontierBackendWebSocketSubsystem::ProcessIncomingPacket(const TArray<uint8>& Packet)
{
	int32 Offset = 0;
	while (Offset < Packet.Num())
	{
		while (Offset < Packet.Num() && (Packet[Offset] == '\n' || Packet[Offset] == '\r'))
		{
			++Offset;
		}
		if (Offset >= Packet.Num())
		{
			return;
		}

		int32 TerminatorIndex = INDEX_NONE;
		for (int32 Index = Offset; Index < Packet.Num(); ++Index)
		{
			if (Packet[Index] == 0)
			{
				TerminatorIndex = Index;
				break;
			}
		}
		if (TerminatorIndex == INDEX_NONE)
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Ignored STOMP packet without a NUL terminator."));
			return;
		}

		const FUTF8ToTCHAR Converted(
			reinterpret_cast<const ANSICHAR*>(Packet.GetData() + Offset),
			TerminatorIndex - Offset);
		FString Frame(Converted.Length(), Converted.Get());
		Frame.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		const int32 HeaderEnd = Frame.Find(TEXT("\n\n"));
		if (HeaderEnd == INDEX_NONE)
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Ignored malformed STOMP frame."));
			Offset = TerminatorIndex + 1;
			continue;
		}

		TArray<FString> HeaderLines;
		Frame.Left(HeaderEnd).ParseIntoArrayLines(HeaderLines, false);
		if (HeaderLines.IsEmpty())
		{
			Offset = TerminatorIndex + 1;
			continue;
		}
		const FString Command = HeaderLines[0].TrimStartAndEnd();
		TMap<FString, FString> Headers;
		for (int32 LineIndex = 1; LineIndex < HeaderLines.Num(); ++LineIndex)
		{
			const int32 SeparatorIndex = HeaderLines[LineIndex].Find(TEXT(":"));
			if (SeparatorIndex > 0)
			{
				Headers.Add(
					UnescapeStompHeader(HeaderLines[LineIndex].Left(SeparatorIndex)),
					UnescapeStompHeader(HeaderLines[LineIndex].Mid(SeparatorIndex + 1)));
			}
		}
		HandleStompFrame(Command, Headers, Frame.Mid(HeaderEnd + 2));
		Offset = TerminatorIndex + 1;
	}
}

void UFrontierBackendWebSocketSubsystem::HandleStompFrame(
	const FString& Command,
	const TMap<FString, FString>& Headers,
	const FString& Body)
{
	if (Command == TEXT("CONNECTED"))
	{
		const FString Version = Headers.FindRef(TEXT("version"));
		if (Version != TEXT("1.2"))
		{
			FRONTIER_LOG(Error, TEXT("[BackendWebSocket] Backend did not negotiate STOMP 1.2. Version=%s"), *Version);
			if (Socket.IsValid())
			{
				Socket->Close(1002, TEXT("STOMP 1.2 required"));
			}
			return;
		}

		int32 ServerOutgoing = 0;
		int32 ServerIncoming = 0;
		if (ParseHeartbeatHeader(Headers.FindRef(TEXT("heart-beat")), ServerOutgoing, ServerIncoming))
		{
			OutgoingHeartbeatMilliseconds = ServerIncoming > 0
				? FMath::Max(RequestedHeartbeatMilliseconds, ServerIncoming)
				: 0;
			IncomingHeartbeatMilliseconds = ServerOutgoing > 0
				? FMath::Max(RequestedHeartbeatMilliseconds, ServerOutgoing)
				: 0;
		}
		else
		{
			OutgoingHeartbeatMilliseconds = 0;
			IncomingHeartbeatMilliseconds = 0;
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] CONNECTED frame omitted a valid heart-beat header."));
		}

		bConnectInProgress = false;
		bStompConnected = true;
		ReconnectAttempt = 0;
		SendPersonalSubscriptions();
		FRONTIER_LOG(
			Log,
			TEXT("[BackendWebSocket] STOMP 1.2 connected. HeartbeatOut=%d HeartbeatIn=%d"),
			OutgoingHeartbeatMilliseconds,
			IncomingHeartbeatMilliseconds);
		OnConnected.Broadcast();
		return;
	}

	if (Command == TEXT("MESSAGE"))
	{
		HandleJsonEvent(Body, Headers.FindRef(TEXT("destination")));
		return;
	}
	if (Command == TEXT("ERROR"))
	{
		const FString Error = !Headers.FindRef(TEXT("message")).IsEmpty()
			? Headers.FindRef(TEXT("message"))
			: Body;
		FRONTIER_LOG(Error, TEXT("[BackendWebSocket] STOMP ERROR received. Error=%s"), *Error);
		if (Socket.IsValid())
		{
			Socket->Close(1002, TEXT("STOMP error"));
		}
		return;
	}
	if (Command != TEXT("RECEIPT"))
	{
		FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Ignored unsupported STOMP command. Command=%s"), *Command);
	}
}

void UFrontierBackendWebSocketSubsystem::HandleJsonEvent(
	const FString& JsonBody,
	const FString& Destination)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonBody);
	FString Type;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetStringField(TEXT("type"), Type))
	{
		FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Ignored MESSAGE with invalid JSON body. Length=%d"), JsonBody.Len());
		return;
	}

	if (Destination == PartyDestination)
	{
		OnPartyMessage.Broadcast(JsonBody);
	}
	if (Type == TEXT("MATCHMAKING_STATUS"))
	{
		FFrontierOnlineMatchmakingStatusEvent Event;
		Event.Type = Type;
		Root->TryGetStringField(TEXT("ticketId"), Event.TicketId);
		Root->TryGetStringField(TEXT("ticketStatus"), Event.TicketStatus);
		Root->TryGetStringField(TEXT("matchId"), Event.MatchId);
		Root->TryGetStringField(TEXT("matchStatus"), Event.MatchStatus);
		Root->TryGetStringField(TEXT("teamSide"), Event.TeamSide);
		double Number = 0.0;
		if (Root->TryGetNumberField(TEXT("currentPlayerCount"), Number)
			&& Number >= 0.0 && Number <= static_cast<double>(MAX_int32))
		{
			Event.CurrentPlayerCount = FMath::Max(0, FMath::RoundToInt(Number));
		}
		if (Root->TryGetNumberField(TEXT("maximumPlayerCount"), Number)
			&& Number >= 0.0 && Number <= static_cast<double>(MAX_int32))
		{
			Event.MaximumPlayerCount = FMath::Max(0, FMath::RoundToInt(Number));
		}
		if (!Event.TicketId.IsEmpty() && !Event.TicketStatus.IsEmpty())
		{
			OnMatchmakingStatus.Broadcast(Event);
		}
		else
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] MATCHMAKING_STATUS omitted ticketId or ticketStatus."));
		}
		return;
	}
	if (Type == TEXT("RAID_SERVER_READY"))
	{
		FFrontierOnlineRaidServerReadyEvent Event;
		Event.Type = Type;
		if (Root->TryGetStringField(TEXT("ticketId"), Event.TicketId)
			&& Root->TryGetStringField(TEXT("matchId"), Event.MatchId)
			&& Root->TryGetStringField(TEXT("raidServerId"), Event.RaidServerId)
			&& Root->TryGetStringField(TEXT("matchStatus"), Event.MatchStatus)
			&& !Event.TicketId.IsEmpty() && !Event.MatchId.IsEmpty() && !Event.RaidServerId.IsEmpty()
			&& Event.MatchStatus == TEXT("SERVER_READY"))
		{
			OnRaidServerReady.Broadcast(Event);
		}
		else
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Ignored incomplete RAID_SERVER_READY event."));
		}
		return;
	}
	if (Type == TEXT("MATCHMAKING_FAILED"))
	{
		FFrontierOnlineMatchmakingFailedEvent Event;
		Event.Type = Type;
		Root->TryGetStringField(TEXT("ticketId"), Event.TicketId);
		Root->TryGetStringField(TEXT("status"), Event.Status);
		Root->TryGetStringField(TEXT("errorCode"), Event.ErrorCode);
		Root->TryGetStringField(TEXT("message"), Event.Message);
		Root->TryGetBoolField(TEXT("retryable"), Event.bRetryable);
		if (!Event.TicketId.IsEmpty())
		{
			OnMatchmakingFailed.Broadcast(Event);
		}
		else
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] MATCHMAKING_FAILED omitted ticketId."));
		}
	}
}

void UFrontierBackendWebSocketSubsystem::ScheduleReconnect()
{
	if (bDisconnectRequested || !GetWorld()
		|| GetWorld()->GetTimerManager().IsTimerActive(ReconnectTimerHandle))
	{
		return;
	}
	const float Delay = FMath::Min(30.0f, FMath::Pow(2.0f, static_cast<float>(ReconnectAttempt++)));
	GetWorld()->GetTimerManager().SetTimer(
		ReconnectTimerHandle,
		this,
		&UFrontierBackendWebSocketSubsystem::ConnectAuthenticated,
		Delay,
		false);
}

void UFrontierBackendWebSocketSubsystem::SendConnectFrame()
{
	if (!Socket.IsValid() || !Socket->IsConnected() || PendingAccessToken.IsEmpty())
	{
		return;
	}
	const FFrontierOnlineConfig Config = FFrontierOnlineConfig::Load();
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("accept-version"), TEXT("1.2"));
	Headers.Add(TEXT("host"), FGenericPlatformHttp::GetUrlDomain(Config.BackendWebSocketUrl));
	Headers.Add(TEXT("heart-beat"), TEXT("10000,10000"));
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *PendingAccessToken));
	SendStompFrame(TEXT("CONNECT"), Headers);
	PendingAccessToken.Reset();
}

void UFrontierBackendWebSocketSubsystem::SendPersonalSubscriptions()
{
	if (!IsConnected())
	{
		return;
	}
	TMap<FString, FString> PartyHeaders;
	PartyHeaders.Add(TEXT("id"), TEXT("sub-party"));
	PartyHeaders.Add(TEXT("destination"), PartyDestination);
	PartyHeaders.Add(TEXT("ack"), TEXT("auto"));
	SendStompFrame(TEXT("SUBSCRIBE"), PartyHeaders);

	TMap<FString, FString> MatchmakingHeaders;
	MatchmakingHeaders.Add(TEXT("id"), TEXT("sub-matchmaking"));
	MatchmakingHeaders.Add(TEXT("destination"), MatchmakingDestination);
	MatchmakingHeaders.Add(TEXT("ack"), TEXT("auto"));
	SendStompFrame(TEXT("SUBSCRIBE"), MatchmakingHeaders);
}

void UFrontierBackendWebSocketSubsystem::SendStompFrame(
	const FString& Command,
	const TMap<FString, FString>& Headers,
	const FString& Body)
{
	if (!Socket.IsValid() || !Socket->IsConnected())
	{
		return;
	}
	FString Frame = Command + TEXT("\n");
	for (const TPair<FString, FString>& Header : Headers)
	{
		Frame += Header.Key + TEXT(":") + Header.Value + TEXT("\n");
	}
	if (!Body.IsEmpty())
	{
		const FTCHARToUTF8 EncodedBody(*Body);
		Frame += FString::Printf(TEXT("content-length:%d\n"), EncodedBody.Length());
	}
	Frame += TEXT("\n");
	Frame += Body;

	const FTCHARToUTF8 EncodedFrame(*Frame);
	TArray<uint8> FrameBytes;
	FrameBytes.Append(reinterpret_cast<const uint8*>(EncodedFrame.Get()), EncodedFrame.Length());
	FrameBytes.Add(0);
	Socket->Send(FrameBytes.GetData(), FrameBytes.Num(), false);
	LastSentSeconds = FPlatformTime::Seconds();
}

void UFrontierBackendWebSocketSubsystem::TickStompHeartbeat()
{
	if (!Socket.IsValid() || !Socket->IsConnected())
	{
		return;
	}
	const double Now = FPlatformTime::Seconds();
	if (!bStompConnected)
	{
		if (StompConnectStartedSeconds > 0.0
			&& Now - StompConnectStartedSeconds > StompConnectTimeoutSeconds)
		{
			FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] STOMP CONNECTED timeout."));
			Socket->Close(1002, TEXT("STOMP CONNECTED timeout"));
		}
		return;
	}

	if (OutgoingHeartbeatMilliseconds > 0
		&& (Now - LastSentSeconds) * 1000.0 >= OutgoingHeartbeatMilliseconds)
	{
		const uint8 Heartbeat = '\n';
		Socket->Send(&Heartbeat, 1, false);
		LastSentSeconds = Now;
	}
	if (IncomingHeartbeatMilliseconds > 0
		&& (Now - LastReceivedSeconds) * 1000.0
			>= IncomingHeartbeatMilliseconds * HeartbeatGraceMultiplier)
	{
		FRONTIER_LOG(Warning, TEXT("[BackendWebSocket] Server heartbeat timeout; reconnecting."));
		bStompConnected = false;
		Socket->Close(4000, TEXT("STOMP heartbeat timeout"));
	}
}

void UFrontierBackendWebSocketSubsystem::ResetConnectionState()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(HeartbeatTimerHandle);
	}
	bConnectInProgress = false;
	bStompConnected = false;
	PendingAccessToken.Reset();
	IncomingPacketBuffer.Reset();
	OutgoingHeartbeatMilliseconds = RequestedHeartbeatMilliseconds;
	IncomingHeartbeatMilliseconds = RequestedHeartbeatMilliseconds;
	LastSentSeconds = 0.0;
	LastReceivedSeconds = 0.0;
	StompConnectStartedSeconds = 0.0;
}

FString UFrontierBackendWebSocketSubsystem::UnescapeStompHeader(const FString& Value)
{
	FString Result;
	Result.Reserve(Value.Len());
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		if (Value[Index] != '\\' || Index + 1 >= Value.Len())
		{
			Result.AppendChar(Value[Index]);
			continue;
		}
		const TCHAR Escaped = Value[++Index];
		switch (Escaped)
		{
		case 'n': Result.AppendChar('\n'); break;
		case 'r': Result.AppendChar('\r'); break;
		case 'c': Result.AppendChar(':'); break;
		case '\\': Result.AppendChar('\\'); break;
		default:
			Result.AppendChar('\\');
			Result.AppendChar(Escaped);
			break;
		}
	}
	return Result;
}
