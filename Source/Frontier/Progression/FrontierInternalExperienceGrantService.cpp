#include "Progression/FrontierInternalExperienceGrantService.h"

#include "Dom/JsonObject.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Progression/FrontierInternalApiSubsystem.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

void UFrontierInternalExperienceGrantService::Initialize(UWorld* InWorld)
{
	ServiceWorld = InWorld;
	InternalApi = InWorld && InWorld->GetGameInstance()
		? InWorld->GetGameInstance()->GetSubsystem<UFrontierInternalApiSubsystem>()
		: nullptr;
}

UWorld* UFrontierInternalExperienceGrantService::GetWorld() const
{
	return ServiceWorld.Get();
}

bool UFrontierInternalExperienceGrantService::FreezeRequestBody(
	FFrontierExperienceGrantRequest& InOutRequest,
	FString& OutError)
{
	OutError.Reset();
	if (InOutRequest.PlayerId.IsEmpty() || InOutRequest.SourceId.IsEmpty()
		|| InOutRequest.ReasonCode.IsEmpty()
		|| InOutRequest.Amount <= 0 || InOutRequest.OccurredAt.IsEmpty())
	{
		OutError = TEXT("Experience grant request contains an invalid identity, amount, reason, or timestamp.");
		return false;
	}
	if (!InOutRequest.Body.IsEmpty())
	{
		return true;
	}

	TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("sourceType"), InOutRequest.SourceType);
	Body->SetStringField(TEXT("sourceId"), InOutRequest.SourceId);
	Body->SetNumberField(TEXT("amount"), static_cast<double>(InOutRequest.Amount));
	Body->SetStringField(TEXT("reasonCode"), InOutRequest.ReasonCode);
	Body->SetStringField(TEXT("occurredAt"), InOutRequest.OccurredAt);
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&InOutRequest.Body);
	if (!FJsonSerializer::Serialize(Body, Writer))
	{
		OutError = TEXT("Experience grant JSON body could not be serialized.");
		return false;
	}
	return true;
}

bool UFrontierInternalExperienceGrantService::IsRetryableFailure(
	const int32 HttpStatus,
	const bool bTransportSucceeded)
{
	return UFrontierInternalApiSubsystem::IsRetryableFailure(HttpStatus, bTransportSucceeded);
}

bool UFrontierInternalExperienceGrantService::QueueGrant(
	FFrontierExperienceGrantRequest Request,
	FCompletion Completion,
	FString& OutError)
{
	OutError.Reset();
	if (!GetWorld() || GetWorld()->GetNetMode() != NM_DedicatedServer)
	{
		OutError = TEXT("Internal experience grants are restricted to a dedicated server process.");
		return false;
	}
	if (!Completion)
	{
		OutError = TEXT("Experience grant completion callback is not bound.");
		return false;
	}
	if (!FreezeRequestBody(Request, OutError))
	{
		return false;
	}
	UFrontierInternalApiSubsystem* Api = InternalApi.Get();
	if (!Api)
	{
		OutError = TEXT("Internal API subsystem is unavailable.");
		return false;
	}

	FFrontierInternalApiRequest ApiRequest;
	ApiRequest.Url = Api->GetConfig().BuildExperienceGrantUrl(Request.PlayerId);
	ApiRequest.Body = MoveTemp(Request.Body);
	ApiRequest.bIncludeRaidServerHeader = false;
	return Api->QueueAuthorizedRequest(
		MoveTemp(ApiRequest),
		[Completion = MoveTemp(Completion)](const FFrontierInternalApiResponse& ApiResponse)
		{
			FFrontierExperienceGrantResponse Response;
			Response.bSucceeded = ApiResponse.bSucceeded;
			Response.bRetryable = ApiResponse.bRetryable;
			Response.HttpStatus = ApiResponse.HttpStatus;
			Response.ErrorCode = ApiResponse.ErrorCode;
			Response.Message = ApiResponse.Message;
			Completion(Response);
		},
		OutError);
}
