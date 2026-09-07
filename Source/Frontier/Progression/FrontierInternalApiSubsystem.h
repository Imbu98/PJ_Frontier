#pragma once

#include "CoreMinimal.h"
#include "Progression/FrontierInternalApiConfig.h"
#include "Progression/FrontierRaidLootTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FrontierInternalApiSubsystem.generated.h"

struct FRONTIER_API FFrontierInternalApiRequest
{
	FString Url;
	FString Body;
	FString RaidServerId;
	FString IdempotencyKey;
	bool bIncludeRaidServerHeader = true;
};

struct FRONTIER_API FFrontierInternalApiResponse
{
	bool bTransportSucceeded = false;
	bool bSucceeded = false;
	bool bRetryable = false;
	int32 HttpStatus = 0;
	FString ResponseBody;
	FString ErrorCode;
	FString Message;
};

struct FRONTIER_API FFrontierInternalTransportRequest
{
	FString Url;
	FString Body;
	FString ServiceJwt;
	FString RaidServerId;
	FString IdempotencyKey;
	bool bTokenIssuance = false;
	bool bIncludeRaidServerHeader = false;
};

/**
 * Dedicated-server-only Internal API boundary. It owns mTLS transport and the
 * process-scoped Service JWT. No token or private-key material is reflected.
 */
UCLASS()
class FRONTIER_API UFrontierInternalApiSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	using FCompletion = TFunction<void(const FFrontierInternalApiResponse&)>;
	using FTransportCompletion = TFunction<void(FFrontierInternalApiResponse)>;
	using FTransportExecutor = TFunction<void(
		const FFrontierInternalTransportRequest& Request,
		FTransportCompletion Completion)>;
	using FRaidLootBatchCompletion = TFunction<void(const FFrontierRaidLootBatchResponse&)>;
	using FRaidServerReadyCompletion = TFunction<void(const FFrontierRaidServerReadyResponse&)>;
	using FRaidServerFailureCompletion = TFunction<void(const FFrontierInternalApiResponse&)>;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	bool QueueAuthorizedRequest(
		FFrontierInternalApiRequest Request,
		FCompletion Completion,
		FString& OutError);
	bool RequestRaidLootBatch(
		const FFrontierRaidLootBatchRequest& Request,
		const FString& IdempotencyKey,
		FRaidLootBatchCompletion Completion,
		FString& OutError);
	bool RequestRaidLootRefill(
		const FFrontierRaidLootBatchRequest& Request,
		const FString& IdempotencyKey,
		FRaidLootBatchCompletion Completion,
		FString& OutError);
	bool NotifyRaidServerReady(
		const FFrontierRaidServerReadyRequest& Request,
		const FString& IdempotencyKey,
		FRaidServerReadyCompletion Completion,
		FString& OutError);
	bool NotifyRaidServerFailure(
		const FFrontierRaidServerFailureRequest& Request,
		const FString& IdempotencyKey,
		FRaidServerFailureCompletion Completion,
		FString& OutError);
	void PrewarmServiceToken();
	void InvalidateServiceToken();

	static bool IsRetryableFailure(int32 HttpStatus, bool bTransportSucceeded);
	static bool IsServiceTokenInvalid(int32 HttpStatus, const FString& ErrorCode);
	static bool ShouldRefreshAt(const FDateTime& ExpiresAt, const FDateTime& NowUtc);

	const FFrontierInternalApiConfig& GetConfig() const { return Config; }

#if WITH_DEV_AUTOMATION_TESTS
	void ConfigureForTests(const FFrontierInternalApiConfig& InConfig) { Config = InConfig; }
	void SetTransportExecutorForTests(FTransportExecutor InExecutor)
	{
		TransportExecutor = MoveTemp(InExecutor);
		bAllowTestTransport = true;
	}
	int32 GetTokenIssueRequestCountForTests() const { return TokenIssueRequestCount; }
#endif

private:
	struct FOperation
	{
		FFrontierInternalApiRequest Request;
		FCompletion Completion;
		bool bRetriedAfterInvalidServiceToken = false;
		int32 RetryAttempt = 0;
	};

	using FTokenCompletion = TFunction<void(bool bSucceeded, const FString& Error)>;

	bool IsDedicatedServerBoundary() const;
	bool HasUsableServiceToken() const;
	void AcquireServiceToken(FTokenCompletion Completion, bool bForceRefresh = false);
	void BeginServiceTokenIssue();
	void HandleServiceTokenIssued(FFrontierInternalApiResponse Response);
	void CompleteTokenWaiters(bool bSucceeded, const FString& Error);
	void SendOperation(FString OperationId);
	void HandleOperationCompleted(FString OperationId, FFrontierInternalApiResponse Response);
	void CompleteOperation(const FString& OperationId, const FFrontierInternalApiResponse& Response);
	void ExecuteTransport(
		const FFrontierInternalTransportRequest& Request,
		FTransportCompletion Completion) const;
	static FFrontierInternalApiResponse ExecuteMtlsTransport(
		const FFrontierInternalApiConfig& Config,
		const FFrontierInternalTransportRequest& Request);
	static void ParseInternalError(
		const FString& Body,
		FString& OutCode,
		FString& OutMessage,
		bool& bOutRetryable);

	FFrontierInternalApiConfig Config;
	FString CachedServiceJwt;
	FDateTime CachedServiceJwtExpiresAt;
	TArray<FString> CachedScopes;
	bool bTokenIssueInFlight = false;
	TArray<FTokenCompletion> TokenWaiters;
	TMap<FString, FOperation> Operations;
	FTransportExecutor TransportExecutor;
	int32 TokenIssueRequestCount = 0;
#if WITH_DEV_AUTOMATION_TESTS
	bool bAllowTestTransport = false;
#endif
};
