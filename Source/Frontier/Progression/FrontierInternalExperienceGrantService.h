#pragma once

#include "CoreMinimal.h"
#include "FrontierInternalExperienceGrantService.generated.h"

class UFrontierInternalApiSubsystem;

USTRUCT()
struct FRONTIER_API FFrontierExperienceGrantRequest
{
	GENERATED_BODY()

	FString PlayerId;
	FString SourceType = TEXT("RAID_RESULT");
	FString SourceId;
	int64 Amount = 0;
	FString ReasonCode = TEXT("RAID_EXTRACT_REWARD");
	FString OccurredAt;
	FString Body;
};

struct FRONTIER_API FFrontierExperienceGrantResponse
{
	bool bSucceeded = false;
	bool bRetryable = false;
	int32 HttpStatus = 0;
	FString ErrorCode;
	FString Message;
};

UCLASS()
class FRONTIER_API UFrontierInternalExperienceGrantService : public UObject
{
	GENERATED_BODY()

public:
	using FCompletion = TFunction<void(const FFrontierExperienceGrantResponse&)>;

	void Initialize(UWorld* InWorld);
	virtual UWorld* GetWorld() const override;
	bool QueueGrant(FFrontierExperienceGrantRequest Request, FCompletion Completion, FString& OutError);

	static bool FreezeRequestBody(FFrontierExperienceGrantRequest& InOutRequest, FString& OutError);
	static bool IsRetryableFailure(int32 HttpStatus, bool bTransportSucceeded);

private:
	TWeakObjectPtr<UWorld> ServiceWorld;
	TWeakObjectPtr<UFrontierInternalApiSubsystem> InternalApi;
};
