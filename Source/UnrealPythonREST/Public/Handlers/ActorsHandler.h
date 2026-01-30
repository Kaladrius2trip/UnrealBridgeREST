// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Actor management endpoints.
 * Uses ActorUtils for spawn, find, raycast operations.
 *
 * Endpoints:
 *   GET  /actors/list          - List all actors with locations
 *   GET  /actors/details       - Full actor metadata (requires label query param)
 *   POST /actors/spawn         - Create from class_path with location/rotation/scale
 *   POST /actors/spawn_raycast - Place on surface (raycast down from location)
 *   POST /actors/duplicate     - Clone actor with offset
 *   POST /actors/transform     - Set location/rotation/scale by label
 *   POST /actors/delete        - Remove actor by label
 *   GET  /actors/in_view       - Actors in viewport frustum
 */
class FActorsHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/actors"); }
	virtual FString GetHandlerName() const override { return TEXT("Actors"); }
	virtual FString GetDescription() const override { return TEXT("Actor spawn, transform, and management"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;

private:
	/** GET /actors/list - List all actors with labels, classes, and locations */
	FRESTResponse HandleList(const FRESTRequest& Request);

	/** GET /actors/details - Get full metadata for a specific actor */
	FRESTResponse HandleDetails(const FRESTRequest& Request);

	/** POST /actors/spawn - Spawn actor from class path with transform */
	FRESTResponse HandleSpawn(const FRESTRequest& Request);

	/** POST /actors/spawn_raycast - Spawn actor on surface via raycast */
	FRESTResponse HandleSpawnRaycast(const FRESTRequest& Request);

	/** POST /actors/duplicate - Duplicate an existing actor with offset */
	FRESTResponse HandleDuplicate(const FRESTRequest& Request);

	/** POST /actors/transform - Update actor location/rotation/scale */
	FRESTResponse HandleTransform(const FRESTRequest& Request);

	/** POST /actors/delete - Remove actor from level */
	FRESTResponse HandleDelete(const FRESTRequest& Request);

	/** GET /actors/in_view - List actors visible in editor viewport frustum */
	FRESTResponse HandleInView(const FRESTRequest& Request);
};
