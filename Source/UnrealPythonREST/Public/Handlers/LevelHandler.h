// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Level/world management endpoints.
 *
 * Uses GEditor and FEditorFileUtils for level operations.
 * Provides level info, world outliner data, and level loading.
 *
 * Endpoints:
 *   GET  /level/info     - Get current level information
 *   GET  /level/outliner - Get world outliner actor hierarchy
 *   POST /level/load     - Load a level by path
 */
class FLevelHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/level"); }
	virtual FString GetHandlerName() const override { return TEXT("Level"); }
	virtual FString GetDescription() const override { return TEXT("Level info and management"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;

private:
	/** GET /level/info - Get current level name, path, actor count, bounds */
	FRESTResponse HandleInfo(const FRESTRequest& Request);

	/** GET /level/outliner - Get world outliner tree or flat list */
	FRESTResponse HandleOutliner(const FRESTRequest& Request);

	/** POST /level/load - Load a level by path */
	FRESTResponse HandleLoad(const FRESTRequest& Request);

	/** Recursively build outliner tree from actor hierarchy */
	TSharedPtr<FJsonObject> ActorToOutlinerJson(AActor* Actor, bool bIncludeChildren);
};
