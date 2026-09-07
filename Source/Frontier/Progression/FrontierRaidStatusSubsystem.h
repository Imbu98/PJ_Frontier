#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "HttpServerRequest.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "FrontierRaidStatusSubsystem.generated.h"

class IHttpRouter;

/**
 * Dedicated-server HTTP status listener polled by the backend.
 * This is intentionally inbound-only; it does not use the internal API client.
 */
UCLASS()
class FRONTIER_API UFrontierRaidStatusSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	bool HandleStatus(
		const FHttpServerRequest& Request,
		const FHttpResultCallback& OnComplete);

	bool IsConfigured() const;
	void StopListener();

	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle RouteHandle;
	FString RaidId;
	int32 BackendPort = 0;
	bool bListenersStarted = false;
};
