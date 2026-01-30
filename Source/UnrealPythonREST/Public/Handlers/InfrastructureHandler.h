// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Infrastructure endpoints for server health and API discovery.
 *
 * Endpoints:
 *   GET  /health  - Server health check
 *   GET  /schema  - Self-documenting API specification
 *   POST /batch   - Execute multiple requests in a single call
 */
class FInfrastructureHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT(""); }
	virtual FString GetHandlerName() const override { return TEXT("Infrastructure"); }
	virtual FString GetDescription() const override { return TEXT("Server health and API discovery"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;

private:
	/** GET /health - Health check */
	FRESTResponse HandleHealth(const FRESTRequest& Request);

	/** GET /schema - API schema */
	FRESTResponse HandleSchema(const FRESTRequest& Request);

	/** POST /batch - Execute multiple requests */
	FRESTResponse HandleBatch(const FRESTRequest& Request);

	/** Build schema from registered handlers */
	TSharedPtr<FJsonObject> BuildSchema(const TArray<TSharedPtr<IRESTHandler>>& Handlers);

	/** Build full schema with all handlers and endpoint details */
	TSharedPtr<FJsonObject> BuildFullSchema();

	/** Build schema for a specific handler by name */
	TSharedPtr<FJsonObject> BuildHandlerSchema(const FString& HandlerName);

	/** Build schema for a specific endpoint path */
	TSharedPtr<FJsonObject> BuildEndpointSchema(const FString& EndpointPath);

	/** Find handler by name (case-insensitive) */
	TSharedPtr<IRESTHandler> FindHandlerByName(const FString& Name);

	/** Find handler whose base_path matches endpoint prefix */
	TSharedPtr<IRESTHandler> FindHandlerByEndpoint(const FString& EndpointPath);

	/** Reference to router for schema generation */
	FRESTRouter* RouterRef = nullptr;

	/** Resolve variable references like $0.node.id in request body */
	TSharedPtr<FJsonObject> ResolveVariableReferences(
		TSharedPtr<FJsonObject> Body,
		const TArray<TSharedPtr<FJsonObject>>& PreviousResults);

	/** Resolve variable patterns in a string value */
	FString ResolveStringVariables(
		const FString& Value,
		const TArray<TSharedPtr<FJsonObject>>& PreviousResults);

	/** Extract value from JSON object using dot-separated path */
	TSharedPtr<FJsonValue> ExtractJsonPath(
		TSharedPtr<FJsonObject> Root,
		const FString& Path);
};
