// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Main log category used across the project */
DECLARE_LOG_CATEGORY_EXTERN(LogFrontier, Log, All);

/** Keep only low-frequency startup and raid-authorization diagnostics enabled. */
#define FRONTIER_LOG(Verbosity, Format, ...) \
	do \
	{ \
		const bool bFrontierDiagnosticLog = \
			FCString::Strstr((Format), TEXT("[RaidStartup]")) != nullptr \
			|| FCString::Strstr((Format), TEXT("[RaidJoinAuthorization]")) != nullptr \
			|| FCString::Strstr((Format), TEXT("[RaidServerFlow]")) != nullptr \
			|| FCString::Strstr((Format), TEXT("[NumberPop]")) != nullptr; \
		if (bFrontierDiagnosticLog) \
		{ \
			UE_LOG(LogFrontier, Verbosity, Format, ##__VA_ARGS__); \
		} \
	} while (false)

/** Function-entry logging is disabled for the shipping gameplay code path. */
#define FRONTIER_LOG_FUNC() do { } while (false)

