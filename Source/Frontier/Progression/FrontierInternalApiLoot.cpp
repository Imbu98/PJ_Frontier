#include "Progression/FrontierInternalApiSubsystem.h"

#include "Frontier.h"

namespace
{
void CopyTransportFields(
	const FFrontierInternalApiResponse& Source,
	FFrontierRaidLootBatchResponse& Target)
{
	Target.bTransportSucceeded = Source.bTransportSucceeded;
	Target.HttpStatus = Source.HttpStatus;
	Target.bRetryable = Source.bRetryable;
	Target.ErrorCode = Source.ErrorCode;
	Target.Message = Source.Message;
}

void CopyTransportFields(
	const FFrontierInternalApiResponse& Source,
	FFrontierRaidServerReadyResponse& Target)
{
	Target.bTransportSucceeded = Source.bTransportSucceeded;
	Target.HttpStatus = Source.HttpStatus;
	Target.bRetryable = Source.bRetryable;
	Target.ErrorCode = Source.ErrorCode;
	Target.Message = Source.Message;
}
}

bool UFrontierInternalApiSubsystem::RequestRaidLootBatch(
	const FFrontierRaidLootBatchRequest& Request,
	const FString& IdempotencyKey,
	FRaidLootBatchCompletion Completion,
	FString& OutError)
{
	FString Body;
	if (Request.RaidServerId.IsEmpty() || Config.MatchId.IsEmpty()
		|| !FFrontierRaidLootJson::SerializeBatchRequest(Request, true, Body, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Initial raid loot request requires configured matchId and raidServerId.");
		}
		FRONTIER_LOG(
			Error,
			TEXT("[RaidStartup] Initial loot request serialization failed. RaidServerId=%s MatchId=%s Error=%s"),
			Request.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Request.RaidServerId,
			Config.MatchId.IsEmpty() ? TEXT("<empty>") : *Config.MatchId,
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError);
		return false;
	}

	const int32 BodyLength = Body.Len();
	FFrontierInternalApiRequest InternalRequest;
	InternalRequest.Url = Config.BuildRaidLootBatchUrl(Config.MatchId);
	InternalRequest.Body = MoveTemp(Body);
	InternalRequest.RaidServerId = Request.RaidServerId;
	InternalRequest.IdempotencyKey = IdempotencyKey;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Queueing initial loot HTTP request. Url=%s BodyLength=%d TableCount=%d"),
		*InternalRequest.Url,
		BodyLength,
		Request.LootRequests.Num());
	const bool bQueued = QueueAuthorizedRequest(
		MoveTemp(InternalRequest),
		[Completion = MoveTemp(Completion)](const FFrontierInternalApiResponse& Transport)
		{
			FRONTIER_LOG(
				Log,
				TEXT("[RaidStartup] Initial loot HTTP response received. Transport=%d HttpStatus=%d BodyLength=%d Success=%d ErrorCode=%s Message=%s"),
				Transport.bTransportSucceeded ? 1 : 0,
				Transport.HttpStatus,
				Transport.ResponseBody.Len(),
				Transport.bSucceeded ? 1 : 0,
				Transport.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Transport.ErrorCode,
				Transport.Message.IsEmpty() ? TEXT("<empty>") : *Transport.Message);
			FFrontierRaidLootBatchResponse Response;
			CopyTransportFields(Transport, Response);
			if (Transport.bSucceeded)
			{
				FString ParseError;
				if (!FFrontierRaidLootJson::ParseBatchResponse(Transport.ResponseBody, Response, ParseError))
				{
					FRONTIER_LOG(Error, TEXT("[RaidStartup] Initial loot response parsing failed. Error=%s"), *ParseError);
					Response.bSuccess = false;
					Response.ErrorCode = TEXT("INVALID_BACKEND_RESPONSE");
					Response.Message = MoveTemp(ParseError);
				}
			}
			if (Completion)
			{
				Completion(Response);
			}
		},
		OutError);
	if (!bQueued)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] Initial loot HTTP request was rejected before sending. Error=%s"), *OutError);
	}
	return bQueued;
}

bool UFrontierInternalApiSubsystem::RequestRaidLootRefill(
	const FFrontierRaidLootBatchRequest& Request,
	const FString& IdempotencyKey,
	FRaidLootBatchCompletion Completion,
	FString& OutError)
{
	FString Body;
	if (Request.RaidServerId.IsEmpty() || Config.MatchId.IsEmpty()
		|| !FFrontierRaidLootJson::SerializeBatchRequest(Request, false, Body, OutError))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("Raid loot refill requires configured matchId and raidServerId.");
		}
		return false;
	}

	FFrontierInternalApiRequest InternalRequest;
	InternalRequest.Url = Config.BuildRaidLootRefillUrl(Config.MatchId);
	InternalRequest.Body = MoveTemp(Body);
	InternalRequest.RaidServerId = Request.RaidServerId;
	InternalRequest.IdempotencyKey = IdempotencyKey;
	return QueueAuthorizedRequest(
		MoveTemp(InternalRequest),
		[Completion = MoveTemp(Completion)](const FFrontierInternalApiResponse& Transport)
		{
			FFrontierRaidLootBatchResponse Response;
			CopyTransportFields(Transport, Response);
			if (Transport.bSucceeded)
			{
				FString ParseError;
				if (!FFrontierRaidLootJson::ParseBatchResponse(Transport.ResponseBody, Response, ParseError))
				{
					Response.bSuccess = false;
					Response.ErrorCode = TEXT("INVALID_BACKEND_RESPONSE");
					Response.Message = MoveTemp(ParseError);
				}
			}
			if (Completion)
			{
				Completion(Response);
			}
		},
		OutError);
}

bool UFrontierInternalApiSubsystem::NotifyRaidServerReady(
	const FFrontierRaidServerReadyRequest& Request,
	const FString& IdempotencyKey,
	FRaidServerReadyCompletion Completion,
	FString& OutError)
{
	FString Body;
	if (!FFrontierRaidLootJson::SerializeReadyRequest(Request, Body, OutError))
	{
		FRONTIER_LOG(
			Error,
			TEXT("[RaidStartup] SERVER_READY request serialization failed. RaidServerId=%s MatchId=%s MapId=%s Address=%s Port=%d Error=%s"),
			Request.RaidServerId.IsEmpty() ? TEXT("<empty>") : *Request.RaidServerId,
			Request.MatchId.IsEmpty() ? TEXT("<empty>") : *Request.MatchId,
			Request.MapId.IsEmpty() ? TEXT("<empty>") : *Request.MapId,
			Request.ServerAddress.IsEmpty() ? TEXT("<empty>") : *Request.ServerAddress,
			Request.Port,
			OutError.IsEmpty() ? TEXT("<empty>") : *OutError);
		return false;
	}

	const int32 BodyLength = Body.Len();
	FFrontierInternalApiRequest InternalRequest;
	InternalRequest.Url = Config.BuildRaidServerReadyUrl(Request.RaidServerId);
	InternalRequest.Body = MoveTemp(Body);
	InternalRequest.RaidServerId = Request.RaidServerId;
	InternalRequest.IdempotencyKey = IdempotencyKey;
	FRONTIER_LOG(
		Log,
		TEXT("[RaidStartup] Queueing SERVER_READY HTTP request. Url=%s BodyLength=%d"),
		*InternalRequest.Url,
		BodyLength);
	const bool bQueued = QueueAuthorizedRequest(
		MoveTemp(InternalRequest),
		[Completion = MoveTemp(Completion)](const FFrontierInternalApiResponse& Transport)
		{
			FRONTIER_LOG(
				Log,
				TEXT("[RaidStartup] SERVER_READY HTTP response received. Transport=%d HttpStatus=%d BodyLength=%d Success=%d ErrorCode=%s Message=%s"),
				Transport.bTransportSucceeded ? 1 : 0,
				Transport.HttpStatus,
				Transport.ResponseBody.Len(),
				Transport.bSucceeded ? 1 : 0,
				Transport.ErrorCode.IsEmpty() ? TEXT("<empty>") : *Transport.ErrorCode,
				Transport.Message.IsEmpty() ? TEXT("<empty>") : *Transport.Message);
			FFrontierRaidServerReadyResponse Response;
			CopyTransportFields(Transport, Response);
			if (Transport.bSucceeded)
			{
				FString ParseError;
				if (!FFrontierRaidLootJson::ParseReadyResponse(Transport.ResponseBody, Response, ParseError))
				{
					FRONTIER_LOG(Error, TEXT("[RaidStartup] SERVER_READY response parsing failed. Error=%s"), *ParseError);
					Response.bSuccess = false;
					Response.ErrorCode = TEXT("INVALID_BACKEND_RESPONSE");
					Response.Message = MoveTemp(ParseError);
				}
			}
			if (Completion)
			{
				Completion(Response);
			}
		},
		OutError);
	if (!bQueued)
	{
		FRONTIER_LOG(Error, TEXT("[RaidStartup] SERVER_READY HTTP request was rejected before sending. Error=%s"), *OutError);
	}
	return bQueued;
}

bool UFrontierInternalApiSubsystem::NotifyRaidServerFailure(
	const FFrontierRaidServerFailureRequest& Request,
	const FString& IdempotencyKey,
	FRaidServerFailureCompletion Completion,
	FString& OutError)
{
	FString Body;
	if (!FFrontierRaidLootJson::SerializeFailureRequest(Request, Body, OutError))
	{
		return false;
	}

	FFrontierInternalApiRequest InternalRequest;
	InternalRequest.Url = Config.BuildRaidServerFailureUrl(Request.RaidServerId);
	InternalRequest.Body = MoveTemp(Body);
	InternalRequest.RaidServerId = Request.RaidServerId;
	InternalRequest.IdempotencyKey = IdempotencyKey;
	return QueueAuthorizedRequest(
		MoveTemp(InternalRequest),
		[Completion = MoveTemp(Completion)](const FFrontierInternalApiResponse& Response)
		{
			if (Completion)
			{
				Completion(Response);
			}
		},
		OutError);
}
