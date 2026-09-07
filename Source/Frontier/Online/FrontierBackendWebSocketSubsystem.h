#pragma once

#include "CoreMinimal.h"
#include "Matchmaking/FrontierOnlineMatchmakingTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierBackendWebSocketSubsystem.generated.h"

class IWebSocket;

DECLARE_MULTICAST_DELEGATE(FFrontierBackendWebSocketConnectedNative);
DECLARE_MULTICAST_DELEGATE(FFrontierBackendWebSocketDisconnectedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FFrontierPartyMessageNative,
	const FString& /* JsonBody */);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FFrontierMatchmakingStatusNative,
	const FFrontierOnlineMatchmakingStatusEvent& /* Event */);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FFrontierRaidServerReadyNative,
	const FFrontierOnlineRaidServerReadyEvent& /* Event */);
DECLARE_MULTICAST_DELEGATE_OneParam(
	FFrontierMatchmakingFailedNative,
	const FFrontierOnlineMatchmakingFailedEvent& /* Event */);

/** Process-local authenticated realtime channel for party and matchmaking events. */
UCLASS()
class FRONTIER_API UFrontierBackendWebSocketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	void ConnectAuthenticated();
	void Disconnect();
	bool SubscribeMatchmaking(const FString& TicketId);
	void UnsubscribeMatchmaking(const FString& TicketId);
	bool IsConnected() const;

	FFrontierBackendWebSocketConnectedNative OnConnected;
	FFrontierBackendWebSocketDisconnectedNative OnDisconnected;
	FFrontierPartyMessageNative OnPartyMessage;
	FFrontierMatchmakingStatusNative OnMatchmakingStatus;
	FFrontierRaidServerReadyNative OnRaidServerReady;
	FFrontierMatchmakingFailedNative OnMatchmakingFailed;

private:
	void ConnectWithToken(const FString& AccessToken);
	void HandleWebSocketConnected();
	void HandleConnectionError(const FString& Error);
	void HandleClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void HandleRawMessage(const void* Data, SIZE_T Size, SIZE_T BytesRemaining);
	void ProcessIncomingPacket(const TArray<uint8>& Packet);
	void HandleStompFrame(
		const FString& Command,
		const TMap<FString, FString>& Headers,
		const FString& Body);
	void HandleJsonEvent(const FString& JsonBody, const FString& Destination);
	void ScheduleReconnect();
	void SendConnectFrame();
	void SendPersonalSubscriptions();
	void SendStompFrame(
		const FString& Command,
		const TMap<FString, FString>& Headers,
		const FString& Body = FString());
	void TickStompHeartbeat();
	void ResetConnectionState();
	static FString UnescapeStompHeader(const FString& Value);

	TSharedPtr<IWebSocket> Socket;
	TSet<FString> MatchmakingSubscriptions;
	TArray<uint8> IncomingPacketBuffer;
	FString PendingAccessToken;
	FTimerHandle ReconnectTimerHandle;
	FTimerHandle HeartbeatTimerHandle;
	int32 ReconnectAttempt = 0;
	double LastSentSeconds = 0.0;
	double LastReceivedSeconds = 0.0;
	double StompConnectStartedSeconds = 0.0;
	int32 OutgoingHeartbeatMilliseconds = 10000;
	int32 IncomingHeartbeatMilliseconds = 10000;
	bool bWebSocketsModuleAvailable = false;
	bool bConnectInProgress = false;
	bool bDisconnectRequested = false;
	bool bStompConnected = false;
};
