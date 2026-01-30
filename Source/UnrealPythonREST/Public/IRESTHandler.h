// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FRESTRouter;

/**
 * Interface for REST API handlers.
 * Implement this to add new endpoint groups (e.g., /python, /editor, /asset).
 */
class IRESTHandler
{
public:
	virtual ~IRESTHandler() = default;

	/**
	 * Get the base path for this handler's routes.
	 * Example: "/python" for Python execution endpoints.
	 */
	virtual FString GetBasePath() const = 0;

	/**
	 * Get a human-readable name for this handler.
	 * Used in /handlers endpoint and logging.
	 */
	virtual FString GetHandlerName() const = 0;

	/**
	 * Get description of this handler's purpose.
	 */
	virtual FString GetDescription() const = 0;

	/**
	 * Get schema information for all endpoints in this handler.
	 * Returns array of endpoint definitions with parameters, responses, errors.
	 */
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const { return TArray<TSharedPtr<FJsonObject>>(); }

	/**
	 * Register this handler's routes with the router.
	 * Called during module startup.
	 */
	virtual void RegisterRoutes(FRESTRouter& Router) = 0;

	/**
	 * Called when the handler should clean up resources.
	 */
	virtual void Shutdown() {}
};
