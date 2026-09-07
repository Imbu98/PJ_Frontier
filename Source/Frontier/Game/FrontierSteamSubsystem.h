#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "FrontierSteamSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSteamAuthTicketReceived,
	const FString&,
	AuthTicket
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSteamAuthTicketFailed,
	const FString&,
	ErrorMessage
);

UCLASS()
class FRONTIER_API UFrontierSteamSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	bool CheckSteamLogin();

	UFUNCTION(BlueprintCallable)
	void RequestSteamWebApiTicket();

	UFUNCTION(BlueprintPure)
	const FString& GetCachedSteamId() const
	{
		return CachedSteamId;
	}

	UFUNCTION(BlueprintPure)
	const FString& GetCachedNickname() const
	{
		return CachedNickname;
	}

	UFUNCTION(BlueprintPure)
	const FString& GetCachedAuthTicket() const
	{
		return CachedAuthTicket;
	}

	UPROPERTY(BlueprintAssignable)
	FOnSteamAuthTicketReceived OnSteamAuthTicketReceived;

	UPROPERTY(BlueprintAssignable)
	FOnSteamAuthTicketFailed OnSteamAuthTicketFailed;

private:
	void HandleLinkedAccountAuthTokenComplete(
		int32 LocalUserNum,
		bool bWasSuccessful,
		const FExternalAuthToken& AuthToken);

	static FString BytesToHexString(
		const TArray<uint8>& Bytes);

private:
	FString CachedSteamId;
	FString CachedNickname;

	// 백엔드 전송 전까지만 메모리에 보관
	FString CachedAuthTicket;
	
};