#include "Progression/FrontierInternalApiSubsystem.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "Frontier.h"
#include "HAL/PlatformMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"
#include "TimerManager.h"

#if FRONTIER_WITH_MTLS_CURL
THIRD_PARTY_INCLUDES_START
#include "curl/curl.h"
THIRD_PARTY_INCLUDES_END
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#ifdef GetEnvironmentVariable
#undef GetEnvironmentVariable
#endif

namespace
{
constexpr int32 ServiceTokenRefreshWindowSeconds = 60;

#if FRONTIER_WITH_MTLS_CURL
size_t WriteInternalCurlResponse(char* Data, const size_t Size, const size_t Count, void* UserData)
{
	const size_t ByteCount = Size * Count;
	if (TArray<uint8>* ResponseBytes = static_cast<TArray<uint8>*>(UserData))
	{
		ResponseBytes->Append(
			reinterpret_cast<const uint8*>(Data),
			static_cast<int32>(ByteCount));
	}
	return ByteCount;
}
#endif
}

void UFrontierInternalApiSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = FFrontierInternalApiConfig::Load();
	const bool bDedicatedServerBoundary = IsDedicatedServerBoundary();
	const TCHAR* ConfigSource = bDedicatedServerBoundary ? TEXT("GameIni") : TEXT("NonDedicatedServer");
	TransportExecutor = [this](
		const FFrontierInternalTransportRequest& Request,
		FTransportCompletion Completion)
	{
		const FFrontierInternalApiConfig FrozenConfig = Config;
		Async(EAsyncExecution::ThreadPool, [FrozenConfig, Request, Completion = MoveTemp(Completion)]() mutable
		{
			FFrontierInternalApiResponse Response = ExecuteMtlsTransport(FrozenConfig, Request);
			AsyncTask(ENamedThreads::GameThread, [Completion = MoveTemp(Completion), Response = MoveTemp(Response)]() mutable
			{
				if (Completion)
				{
					Completion(MoveTemp(Response));
				}
			});
		});
	};

	if (bDedicatedServerBoundary)
	{
		FString CommandLineBaseUrl;
		const bool bHasCommandLineOverride = FParse::Value(
			FCommandLine::Get(),
			TEXT("FrontierInternalApiBaseUrl="),
			CommandLineBaseUrl)
			&& !CommandLineBaseUrl.IsEmpty();
		const bool bHasEnvironmentOverride = !FPlatformMisc::GetEnvironmentVariable(
			TEXT("FRONTIER_INTERNAL_API_BASE_URL")).IsEmpty();
		ConfigSource = bHasCommandLineOverride
			? TEXT("CommandLine")
			: bHasEnvironmentOverride
				? TEXT("Environment")
				: TEXT("GameIni");
		PrewarmServiceToken();
	}

	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Internal API subsystem initialized. DedicatedBoundary=%d BaseUrl=%s ConfigSource=%s RaidServerId=%s MatchId=%s MapId=%s PublicAddress=%s PublicPort=%d"),
		bDedicatedServerBoundary ? 1 : 0,
		Config.InternalApiBaseUrl.IsEmpty() ? TEXT("<empty>") : *Config.InternalApiBaseUrl,
		ConfigSource,
		Config.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Config.RaidServerId,
		Config.MatchId.IsEmpty() ? TEXT("<empty>") : *Config.MatchId,
		Config.MapId.IsEmpty() ? TEXT("<empty>") : *Config.MapId,
		Config.PublicServerAddress.IsEmpty() ? TEXT("<empty>") : *Config.PublicServerAddress,
		Config.PublicServerPort);
}

void UFrontierInternalApiSubsystem::Deinitialize()
{
	FFrontierInternalApiResponse ShutdownResponse;
	ShutdownResponse.Message = TEXT("Internal API subsystem is shutting down.");
	TArray<FString> Keys;
	Operations.GetKeys(Keys);
	for (const FString& Key : Keys)
	{
		CompleteOperation(Key, ShutdownResponse);
	}
	CompleteTokenWaiters(false, ShutdownResponse.Message);
	TransportExecutor = nullptr;
	InvalidateServiceToken();
	Super::Deinitialize();
}

bool UFrontierInternalApiSubsystem::IsDedicatedServerBoundary() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (bAllowTestTransport)
	{
		return true;
	}
#endif
	return IsRunningDedicatedServer()
		|| (GetWorld() && GetWorld()->GetNetMode() == NM_DedicatedServer);
}

bool UFrontierInternalApiSubsystem::QueueAuthorizedRequest(
	FFrontierInternalApiRequest Request,
	FCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (!IsDedicatedServerBoundary())
	{
		OutError = TEXT("Internal API calls are restricted to a dedicated-server authority boundary.");
		return false;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (!bAllowTestTransport)
#endif
	{
		if (Config.InternalApiBaseUrl.IsEmpty())
		{
			// Re-read runtime overrides before rejecting a request with an empty startup value.
			Config = FFrontierInternalApiConfig::Load();
		}
		if (!Config.Validate(OutError))
		{
			return false;
		}
	}
	if (!Completion || Request.Url.IsEmpty() || Request.Body.IsEmpty())
	{
		OutError = TEXT("Internal API request requires a URL, frozen body, and completion.");
		return false;
	}
	if (Request.IdempotencyKey.IsEmpty())
	{
		Request.IdempotencyKey = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}
	if (Request.bIncludeRaidServerHeader && Request.RaidServerId.IsEmpty())
	{
		OutError = TEXT("A session-scoped raid serverId is required for Internal Raid API calls.");
		return false;
	}
	const FString OperationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	FOperation& Operation = Operations.Add(OperationId);
	Operation.Request = MoveTemp(Request);
	Operation.Completion = MoveTemp(Completion);
	SendOperation(OperationId);
	return true;
}

void UFrontierInternalApiSubsystem::PrewarmServiceToken()
{
	AcquireServiceToken([](const bool, const FString&)
	{
		// Lazy acquisition on the first real operation remains available after a prewarm failure.
	});
}

void UFrontierInternalApiSubsystem::InvalidateServiceToken()
{
	CachedServiceJwt.Reset();
	CachedServiceJwtExpiresAt = FDateTime();
	CachedScopes.Reset();
}

bool UFrontierInternalApiSubsystem::IsRetryableFailure(
	const int32 HttpStatus,
	const bool bTransportSucceeded)
{
	return !bTransportSucceeded || HttpStatus == 429 || HttpStatus == 503;
}

bool UFrontierInternalApiSubsystem::IsServiceTokenInvalid(
	const int32 HttpStatus,
	const FString& ErrorCode)
{
	return HttpStatus == 401 && ErrorCode.Equals(TEXT("SERVICE_TOKEN_INVALID"), ESearchCase::CaseSensitive);
}

bool UFrontierInternalApiSubsystem::ShouldRefreshAt(
	const FDateTime& ExpiresAt,
	const FDateTime& NowUtc)
{
	return ExpiresAt <= NowUtc + FTimespan::FromSeconds(ServiceTokenRefreshWindowSeconds);
}

bool UFrontierInternalApiSubsystem::HasUsableServiceToken() const
{
	return !CachedServiceJwt.IsEmpty()
		&& !ShouldRefreshAt(CachedServiceJwtExpiresAt, FDateTime::UtcNow());
}

void UFrontierInternalApiSubsystem::AcquireServiceToken(
	FTokenCompletion Completion,
	const bool bForceRefresh)
{
	if (!Completion)
	{
		return;
	}
	if (!bForceRefresh && HasUsableServiceToken())
	{
		Completion(true, FString());
		return;
	}

	TokenWaiters.Add(MoveTemp(Completion));
	if (bTokenIssueInFlight)
	{
		return;
	}

	if (!bForceRefresh)
	{
		FString OverrideJwt;
		FString OverrideError;
		if (Config.ResolveServiceJwt(OverrideJwt, OverrideError))
		{
			CachedServiceJwt = MoveTemp(OverrideJwt);
			CachedServiceJwtExpiresAt = FDateTime::UtcNow() + FTimespan::FromSeconds(Config.ServiceTokenLifetimeSeconds);
			CompleteTokenWaiters(true, FString());
			return;
		}
	}
	BeginServiceTokenIssue();
}

void UFrontierInternalApiSubsystem::BeginServiceTokenIssue()
{
	if (bTokenIssueInFlight)
	{
		return;
	}
	if (!TransportExecutor)
	{
		CompleteTokenWaiters(false, TEXT("Internal mTLS transport is unavailable."));
		return;
	}
#if WITH_DEV_AUTOMATION_TESTS
	if (!bAllowTestTransport)
#endif
	{
		FString ConfigError;
		if (!Config.Validate(ConfigError))
		{
			CompleteTokenWaiters(false, ConfigError);
			return;
		}
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("serviceName"), Config.ServiceName);
	Body->SetNumberField(TEXT("expiresInSeconds"), static_cast<double>(Config.ServiceTokenLifetimeSeconds));
	FString BodyString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	if (!FJsonSerializer::Serialize(Body, Writer))
	{
		CompleteTokenWaiters(false, TEXT("Service token request body could not be serialized."));
		return;
	}

	FFrontierInternalTransportRequest Request;
	Request.Url = Config.BuildServiceTokenUrl();
	Request.Body = MoveTemp(BodyString);
	Request.bTokenIssuance = true;
	bTokenIssueInFlight = true;
	++TokenIssueRequestCount;
	const TWeakObjectPtr<UFrontierInternalApiSubsystem> WeakThis(this);
	ExecuteTransport(Request, [WeakThis](FFrontierInternalApiResponse Response)
	{
		if (UFrontierInternalApiSubsystem* This = WeakThis.Get())
		{
			This->HandleServiceTokenIssued(MoveTemp(Response));
		}
	});
}

void UFrontierInternalApiSubsystem::HandleServiceTokenIssued(
	FFrontierInternalApiResponse Response)
{
	bTokenIssueInFlight = false;
	if (!Response.bTransportSucceeded || Response.HttpStatus < 200 || Response.HttpStatus >= 300)
	{
		ParseInternalError(Response.ResponseBody, Response.ErrorCode, Response.Message, Response.bRetryable);
		CompleteTokenWaiters(false, Response.Message.IsEmpty()
			? TEXT("Service token issuance failed.")
			: Response.Message);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response.ResponseBody);
	const TSharedPtr<FJsonObject>* Data = nullptr;
	FString AccessToken;
	FString ExpiresAt;
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()
		|| !Root->TryGetObjectField(TEXT("data"), Data) || !Data || !Data->IsValid()
		|| !(*Data)->TryGetStringField(TEXT("accessToken"), AccessToken)
		|| !(*Data)->TryGetStringField(TEXT("expiresAt"), ExpiresAt)
		|| AccessToken.IsEmpty()
		|| !FDateTime::ParseIso8601(*ExpiresAt, CachedServiceJwtExpiresAt))
	{
		CompleteTokenWaiters(false, TEXT("Service token response is missing accessToken or expiresAt."));
		return;
	}

	CachedScopes.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Scopes = nullptr;
	if ((*Data)->TryGetArrayField(TEXT("scopes"), Scopes) && Scopes)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Scopes)
		{
			if (Value.IsValid() && Value->Type == EJson::String)
			{
				CachedScopes.Add(Value->AsString());
			}
		}
	}
	CachedServiceJwt = MoveTemp(AccessToken);
	CompleteTokenWaiters(true, FString());
}

void UFrontierInternalApiSubsystem::CompleteTokenWaiters(
	const bool bSucceeded,
	const FString& Error)
{
	TArray<FTokenCompletion> Waiters = MoveTemp(TokenWaiters);
	TokenWaiters.Reset();
	for (FTokenCompletion& Waiter : Waiters)
	{
		if (Waiter)
		{
			Waiter(bSucceeded, Error);
		}
	}
}

void UFrontierInternalApiSubsystem::SendOperation(FString OperationId)
{
	FOperation* Operation = Operations.Find(OperationId);
	if (!Operation)
	{
		return;
	}

	const TWeakObjectPtr<UFrontierInternalApiSubsystem> WeakThis(this);
	AcquireServiceToken(
		[WeakThis, OperationId](const bool bSucceeded, const FString& Error)
		{
			UFrontierInternalApiSubsystem* This = WeakThis.Get();
			FOperation* Pending = This ? This->Operations.Find(OperationId) : nullptr;
			if (!This || !Pending)
			{
				return;
			}
			if (!bSucceeded)
			{
				FFrontierInternalApiResponse Failure;
				Failure.Message = Error;
				This->CompleteOperation(OperationId, Failure);
				return;
			}

			FFrontierInternalTransportRequest Request;
			Request.Url = Pending->Request.Url;
			Request.Body = Pending->Request.Body;
			Request.ServiceJwt = This->CachedServiceJwt;
			Request.RaidServerId = Pending->Request.RaidServerId;
			Request.IdempotencyKey = Pending->Request.IdempotencyKey;
			Request.bIncludeRaidServerHeader = Pending->Request.bIncludeRaidServerHeader;
			FRONTIER_LOG(
				Log,
				TEXT("[InternalApi] Sending authorized request. OperationId=%s Url=%s RaidServerId=%s IncludeRaidServerHeader=%d BodyLength=%d"),
				*OperationId,
				*Request.Url,
				Request.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Request.RaidServerId,
				Request.bIncludeRaidServerHeader ? 1 : 0,
				Request.Body.Len());
			This->ExecuteTransport(Request, [WeakThis, OperationId](FFrontierInternalApiResponse Response)
			{
				if (UFrontierInternalApiSubsystem* InnerThis = WeakThis.Get())
				{
					InnerThis->HandleOperationCompleted(OperationId, MoveTemp(Response));
				}
			});
		});
}

void UFrontierInternalApiSubsystem::HandleOperationCompleted(
	FString OperationId,
	FFrontierInternalApiResponse Response)
{
	FOperation* Operation = Operations.Find(OperationId);
	if (!Operation)
	{
		return;
	}

	if (Response.bTransportSucceeded && Response.HttpStatus >= 200 && Response.HttpStatus < 300)
	{
		Response.bSucceeded = true;
		CompleteOperation(OperationId, Response);
		return;
	}

	ParseInternalError(Response.ResponseBody, Response.ErrorCode, Response.Message, Response.bRetryable);
	if (IsServiceTokenInvalid(Response.HttpStatus, Response.ErrorCode)
		&& !Operation->bRetriedAfterInvalidServiceToken)
	{
		Operation->bRetriedAfterInvalidServiceToken = true;
		InvalidateServiceToken();
		const TWeakObjectPtr<UFrontierInternalApiSubsystem> WeakThis(this);
		AcquireServiceToken(
			[WeakThis, OperationId](const bool bSucceeded, const FString& Error)
			{
				if (UFrontierInternalApiSubsystem* This = WeakThis.Get())
				{
					if (bSucceeded)
					{
						This->SendOperation(OperationId);
					}
					else
					{
						FFrontierInternalApiResponse Failure;
						Failure.Message = Error;
						This->CompleteOperation(OperationId, Failure);
					}
				}
			},
			true);
		return;
	}

	Response.bRetryable = Response.HttpStatus != 403
		&& (Response.bRetryable || IsRetryableFailure(Response.HttpStatus, Response.bTransportSucceeded));
	if (Response.bRetryable && Operation->RetryAttempt + 1 < Config.MaximumRetryAttempts)
	{
		++Operation->RetryAttempt;
		const float ExponentialDelay = Config.RetryBaseDelaySeconds
			* FMath::Pow(2.0f, static_cast<float>(Operation->RetryAttempt - 1));
		const float RetryDelay = ExponentialDelay + FMath::FRandRange(0.0f, ExponentialDelay * 0.25f);
		FRONTIER_LOG(
			Warning,
			TEXT("[InternalApi] Retrying idempotent operation. OperationId=%s Attempt=%d/%d Delay=%.2f HttpStatus=%d ErrorCode=%s"),
			*OperationId,
			Operation->RetryAttempt + 1,
			Config.MaximumRetryAttempts,
			RetryDelay,
			Response.HttpStatus,
			*Response.ErrorCode);
		if (UWorld* World = GetWorld())
		{
			const TWeakObjectPtr<UFrontierInternalApiSubsystem> WeakThis(this);
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				FTimerDelegate::CreateLambda([WeakThis, OperationId]()
				{
					if (UFrontierInternalApiSubsystem* This = WeakThis.Get())
					{
						This->SendOperation(OperationId);
					}
				}),
				RetryDelay,
				false);
			return;
		}
	}
	if (Response.Message.IsEmpty())
	{
		Response.Message = FString::Printf(TEXT("Internal API request failed with HTTP %d."), Response.HttpStatus);
	}
	FRONTIER_LOG(
		Error,
		TEXT("[InternalApi] Authorized request failed. OperationId=%s Url=%s HttpStatus=%d TransportSucceeded=%d ErrorCode=%s Retryable=%d Message=%s ResponseBodyLength=%d"),
		*OperationId,
		*Operation->Request.Url,
		Response.HttpStatus,
		Response.bTransportSucceeded ? 1 : 0,
		Response.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Response.ErrorCode,
		Response.bRetryable ? 1 : 0,
		*Response.Message,
		Response.ResponseBody.Len());
	CompleteOperation(OperationId, Response);
}

void UFrontierInternalApiSubsystem::CompleteOperation(
	const FString& OperationId,
	const FFrontierInternalApiResponse& Response)
{
	FOperation Operation;
	if (Operations.RemoveAndCopyValue(OperationId, Operation) && Operation.Completion)
	{
		Operation.Completion(Response);
	}
}

void UFrontierInternalApiSubsystem::ExecuteTransport(
	const FFrontierInternalTransportRequest& Request,
	FTransportCompletion Completion) const
{
	if (TransportExecutor)
	{
		TransportExecutor(Request, MoveTemp(Completion));
	}
	else if (Completion)
	{
		FFrontierInternalApiResponse Failure;
		Failure.Message = TEXT("Internal mTLS transport is unavailable.");
		Completion(MoveTemp(Failure));
	}
}

void UFrontierInternalApiSubsystem::ParseInternalError(
	const FString& Body,
	FString& OutCode,
	FString& OutMessage,
	bool& bOutRetryable)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	const TSharedPtr<FJsonObject>* Error = nullptr;
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid()
		&& Root->TryGetObjectField(TEXT("error"), Error) && Error && Error->IsValid())
	{
		(*Error)->TryGetStringField(TEXT("code"), OutCode);
		(*Error)->TryGetStringField(TEXT("message"), OutMessage);
		(*Error)->TryGetBoolField(TEXT("retryable"), bOutRetryable);
	}
}

FFrontierInternalApiResponse UFrontierInternalApiSubsystem::ExecuteMtlsTransport(
	const FFrontierInternalApiConfig& RequestConfig,
	const FFrontierInternalTransportRequest& Request)
{
	FFrontierInternalApiResponse Result;
#if FRONTIER_WITH_MTLS_CURL
	CURL* Handle = curl_easy_init();
	if (!Handle)
	{
		Result.Message = TEXT("libcurl could not create an mTLS request handle.");
		return Result;
	}

	curl_slist* Headers = nullptr;
	TArray<uint8> ResponseBytes;
	Headers = curl_slist_append(Headers, "Content-Type: application/json");
	Headers = curl_slist_append(Headers, "Accept: application/json");
	if (!Request.bTokenIssuance)
	{
		const FString Authorization = FString::Printf(TEXT("Authorization: Bearer %s"), *Request.ServiceJwt);
		const FString ServiceHeader = FString::Printf(TEXT("X-Service-Name: %s"), *RequestConfig.ServiceName);
		const FString RequestIdHeader = FString::Printf(
			TEXT("X-Request-ID: %s"),
			*FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
		const FString IdempotencyHeader = FString::Printf(
			TEXT("Idempotency-Key: %s"),
			*Request.IdempotencyKey);
		Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*Authorization));
		Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*ServiceHeader));
		Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*RequestIdHeader));
		Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*IdempotencyHeader));
		if (Request.bIncludeRaidServerHeader)
		{
			const FString RaidServerHeader = FString::Printf(
				TEXT("X-Raid-Server-ID: %s"),
				*Request.RaidServerId);
			Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*RaidServerHeader));
		}
	}

	const FTCHARToUTF8 UrlUtf8(*Request.Url);
	const FTCHARToUTF8 BodyUtf8(*Request.Body);
	const FTCHARToUTF8 CertUtf8(*RequestConfig.MtlsCertificatePath);
	const FTCHARToUTF8 KeyUtf8(*RequestConfig.MtlsPrivateKeyPath);
	const FTCHARToUTF8 CaUtf8(*RequestConfig.MtlsCaCertificatePath);
	curl_easy_setopt(Handle, CURLOPT_URL, UrlUtf8.Get());
	curl_easy_setopt(Handle, CURLOPT_POST, 1L);
	curl_easy_setopt(Handle, CURLOPT_POSTFIELDS, BodyUtf8.Get());
	curl_easy_setopt(Handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(BodyUtf8.Length()));
	curl_easy_setopt(Handle, CURLOPT_HTTPHEADER, Headers);
	curl_easy_setopt(Handle, CURLOPT_SSLCERTTYPE, "PEM");
	curl_easy_setopt(Handle, CURLOPT_SSLCERT, CertUtf8.Get());
	curl_easy_setopt(Handle, CURLOPT_SSLKEYTYPE, "PEM");
	curl_easy_setopt(Handle, CURLOPT_SSLKEY, KeyUtf8.Get());
	curl_easy_setopt(Handle, CURLOPT_CAINFO, CaUtf8.Get());
	curl_easy_setopt(Handle, CURLOPT_SSL_VERIFYPEER, 1L);
	curl_easy_setopt(Handle, CURLOPT_SSL_VERIFYHOST, 2L);
	curl_easy_setopt(Handle, CURLOPT_TIMEOUT_MS, static_cast<long>(RequestConfig.RequestTimeoutSeconds * 1000.0f));
	curl_easy_setopt(Handle, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(Handle, CURLOPT_WRITEFUNCTION, WriteInternalCurlResponse);
	curl_easy_setopt(Handle, CURLOPT_WRITEDATA, &ResponseBytes);

	const CURLcode CurlResult = curl_easy_perform(Handle);
	if (!ResponseBytes.IsEmpty())
	{
		const FUTF8ToTCHAR Converted(
			reinterpret_cast<const ANSICHAR*>(ResponseBytes.GetData()),
			ResponseBytes.Num());
		Result.ResponseBody = FString(Converted.Length(), Converted.Get());
	}
	long HttpCode = 0;
	curl_easy_getinfo(Handle, CURLINFO_RESPONSE_CODE, &HttpCode);
	Result.HttpStatus = static_cast<int32>(HttpCode);
	Result.bTransportSucceeded = CurlResult == CURLE_OK;
	if (!Result.bTransportSucceeded)
	{
		Result.Message = FString::Printf(
			TEXT("mTLS transport failed with libcurl code %d."),
			static_cast<int32>(CurlResult));
	}
	curl_slist_free_all(Headers);
	curl_easy_cleanup(Handle);
#else
	Result.Message = TEXT("mTLS transport is compiled only into supported Dedicated Server targets.");
#endif
	return Result;
}
