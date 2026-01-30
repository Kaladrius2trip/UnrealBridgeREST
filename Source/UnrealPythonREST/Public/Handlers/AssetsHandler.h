// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Asset management endpoints.
 *
 * Uses UE Asset Registry for queries.
 * All paths use game content format: /Game/Path/To/Asset
 *
 * Endpoints:
 *   GET  /assets/list         - List assets with optional filters
 *   POST /assets/search       - Search assets by name pattern
 *   GET  /assets/info         - Get detailed asset information
 *   GET  /assets/refs         - Get asset references and dependencies
 *   POST /assets/export       - Export asset to text format
 *   POST /assets/validate     - Validate asset integrity
 *   GET  /assets/mesh_details - Get static mesh geometry details
 */
class FAssetsHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/assets"); }
	virtual FString GetHandlerName() const override { return TEXT("Assets"); }
	virtual FString GetDescription() const override { return TEXT("Asset registry queries and management"); }
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;
	virtual void RegisterRoutes(FRESTRouter& Router) override;

private:
	/** GET /assets/list - List assets with optional path/type/limit filters */
	FRESTResponse HandleList(const FRESTRequest& Request);

	/** POST /assets/search - Search assets by name pattern */
	FRESTResponse HandleSearch(const FRESTRequest& Request);

	/** GET /assets/info - Get detailed asset information */
	FRESTResponse HandleInfo(const FRESTRequest& Request);

	/** GET /assets/refs - Get asset references and dependencies */
	FRESTResponse HandleRefs(const FRESTRequest& Request);

	/** POST /assets/export - Export asset to text format */
	FRESTResponse HandleExport(const FRESTRequest& Request);

	/** POST /assets/validate - Validate asset integrity */
	FRESTResponse HandleValidate(const FRESTRequest& Request);

	/** GET /assets/mesh_details - Get static mesh geometry details */
	FRESTResponse HandleMeshDetails(const FRESTRequest& Request);

	/** Convert FAssetData to JSON representation */
	TSharedPtr<FJsonObject> AssetDataToJson(const FAssetData& AssetData);
};
