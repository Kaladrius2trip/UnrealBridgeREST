// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRESTRouter;

/**
 * Writes discovery config file for clients to find the server.
 *
 * Creates a JSON file in {ProjectDir}/Saved/UnrealPythonREST.json containing:
 * - Server port and PID
 * - Project name
 * - List of registered handlers
 * - Server start time
 */
class FConfigWriter
{
public:
	/** Write config to {ProjectDir}/Saved/UnrealPythonREST.json */
	static bool WriteConfig(const FRESTRouter& Router);

	/** Delete config file (called on shutdown) */
	static bool DeleteConfig();

	/** Get config file path */
	static FString GetConfigPath();
};
