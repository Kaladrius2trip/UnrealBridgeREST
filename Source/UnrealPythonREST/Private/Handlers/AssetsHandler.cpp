// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/AssetsHandler.h"
#include "Utils/JsonHelpers.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "UObject/UObjectIterator.h"
#include "Exporters/Exporter.h"

void FAssetsHandler::RegisterRoutes(FRESTRouter& Router)
{
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/assets/list"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleList));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/assets/search"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleSearch));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/assets/info"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleInfo));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/assets/refs"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleRefs));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/assets/export"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleExport));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/assets/validate"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleValidate));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/assets/mesh_details"),
		FRESTRouteHandler::CreateRaw(this, &FAssetsHandler::HandleMeshDetails));

	UE_LOG(LogTemp, Log, TEXT("AssetsHandler: Registered 7 routes at /assets"));
}

FRESTResponse FAssetsHandler::HandleList(const FRESTRequest& Request)
{
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Get optional filters from query params
	FString Path = TEXT("/Game");
	FString Type = TEXT("");
	int32 Limit = 1000;

	const FString* PathPtr = Request.QueryParams.Find(TEXT("path"));
	if (PathPtr)
	{
		Path = *PathPtr;
	}

	const FString* TypePtr = Request.QueryParams.Find(TEXT("type"));
	if (TypePtr)
	{
		Type = *TypePtr;
	}

	const FString* LimitPtr = Request.QueryParams.Find(TEXT("limit"));
	if (LimitPtr)
	{
		Limit = FCString::Atoi(**LimitPtr);
	}

	// Build filter
	FARFilter Filter;
	Filter.PackagePaths.Add(FName(*Path));
	Filter.bRecursivePaths = true;

	if (!Type.IsEmpty())
	{
		// Type can be simple name ("Material") or full path ("/Script/Engine.Material")
		FTopLevelAssetPath ClassPath;
		if (Type.StartsWith(TEXT("/")))
		{
			// Full path provided
			ClassPath = FTopLevelAssetPath(Type);
		}
		else
		{
			// Simple name - assume Engine module
			FString FullPath = FString::Printf(TEXT("/Script/Engine.%s"), *Type);
			ClassPath = FTopLevelAssetPath(FullPath);
		}

		if (ClassPath.IsValid())
		{
			Filter.ClassPaths.Add(ClassPath);
		}
		else
		{
			return FRESTResponse::BadRequest(
				FString::Printf(TEXT("Invalid asset type: %s. Use full path like /Script/Engine.Material or simple name like Material"), *Type));
		}
	}

	TArray<FAssetData> Assets;
	AssetRegistry.GetAssets(Filter, Assets);

	// Log count before processing
	UE_LOG(LogTemp, Log, TEXT("AssetsHandler: Found %d assets matching filter (path=%s, type=%s)"),
		Assets.Num(), *Path, Type.IsEmpty() ? TEXT("any") : *Type);

	// Hard safety limit to prevent server hangs
	const int32 HardLimit = FMath::Min(Limit, 10000);
	if (Limit > 10000)
	{
		UE_LOG(LogTemp, Warning, TEXT("AssetsHandler: Requested limit %d exceeds hard limit 10000, capping"), Limit);
	}

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetNumberField(TEXT("total"), Assets.Num());

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	int32 Count = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (Count >= HardLimit)
		{
			UE_LOG(LogTemp, Warning, TEXT("AssetsHandler: Hit limit %d, stopping enumeration (total available: %d)"),
				HardLimit, Assets.Num());
			break;
		}

		// Progress logging every 100 assets
		if (Count > 0 && Count % 100 == 0)
		{
			UE_LOG(LogTemp, Verbose, TEXT("AssetsHandler: Processing asset %d/%d"), Count, FMath::Min(Assets.Num(), HardLimit));
		}

		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(Asset)));
		Count++;
	}
	Response->SetArrayField(TEXT("assets"), AssetsArray);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleSearch(const FRESTRequest& Request)
{
	FString Query;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("query"), Query, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FString Type = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("type"), TEXT(""));
	int32 Limit = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("limit"), 100);

	// Search by name pattern
	TArray<FAssetData> AllAssets;
	AssetRegistry.GetAllAssets(AllAssets);

	TArray<FAssetData> Matches;
	for (const FAssetData& Asset : AllAssets)
	{
		if (Asset.AssetName.ToString().Contains(Query))
		{
			if (Type.IsEmpty() || Asset.AssetClassPath.GetAssetName().ToString() == Type)
			{
				Matches.Add(Asset);
				if (Matches.Num() >= Limit)
				{
					break;
				}
			}
		}
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetNumberField(TEXT("count"), Matches.Num());

	TArray<TSharedPtr<FJsonValue>> AssetsArray;
	for (const FAssetData& Asset : Matches)
	{
		AssetsArray.Add(MakeShared<FJsonValueObject>(AssetDataToJson(Asset)));
	}
	Response->SetArrayField(TEXT("assets"), AssetsArray);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleInfo(const FRESTRequest& Request)
{
	const FString* PathPtr = Request.QueryParams.Find(TEXT("path"));
	if (!PathPtr || PathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: path"));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(*PathPtr));

	if (!AssetData.IsValid())
	{
		TSharedPtr<FJsonObject> Details = JsonHelpers::CreateErrorDetails(
			*PathPtr,
			TEXT("Use GET /assets/list or POST /assets/search to find valid asset paths")
		);
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: %s"), **PathPtr));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("asset"), AssetDataToJson(AssetData));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleRefs(const FRESTRequest& Request)
{
	const FString* PathPtr = Request.QueryParams.Find(TEXT("path"));
	if (!PathPtr || PathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: path"));
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	// Use FName-based API which is more stable across UE versions
	FName PackageName(**PathPtr);
	TArray<FName> ReferencerNames;
	TArray<FName> DependencyNames;

	AssetRegistry.GetReferencers(PackageName, ReferencerNames);
	AssetRegistry.GetDependencies(PackageName, DependencyNames);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("asset"), *PathPtr);

	TArray<TSharedPtr<FJsonValue>> RefsArray;
	for (const FName& Ref : ReferencerNames)
	{
		RefsArray.Add(MakeShared<FJsonValueString>(Ref.ToString()));
	}
	Response->SetArrayField(TEXT("referencers"), RefsArray);

	TArray<TSharedPtr<FJsonValue>> DepsArray;
	for (const FName& Dep : DependencyNames)
	{
		DepsArray.Add(MakeShared<FJsonValueString>(Dep.ToString()));
	}
	Response->SetArrayField(TEXT("dependencies"), DepsArray);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleExport(const FRESTRequest& Request)
{
	FString Path;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("path"), Path, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load the asset
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
	if (!Asset)
	{
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: %s"), *Path));
	}

	// Export to text using ExportToOutputDevice
	FStringOutputDevice Output;
	UExporter::ExportToOutputDevice(nullptr, Asset, nullptr, Output, TEXT("copy"), 0, PPF_ExportsNotFullyQualified);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("path"), Path);
	Response->SetStringField(TEXT("exported_text"), *Output);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleValidate(const FRESTRequest& Request)
{
	FString Path;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("path"), Path, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load and validate
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *Path);
	if (!Asset)
	{
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: %s"), *Path));
	}

	TArray<FString> Errors;
	bool bValid = Asset->IsAsset(); // Basic validation

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("valid"), bValid && Errors.Num() == 0);
	Response->SetStringField(TEXT("path"), Path);

	TArray<TSharedPtr<FJsonValue>> ErrorsArray;
	for (const FString& Err : Errors)
	{
		ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
	}
	Response->SetArrayField(TEXT("errors"), ErrorsArray);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FAssetsHandler::HandleMeshDetails(const FRESTRequest& Request)
{
	const FString* PathPtr = Request.QueryParams.Find(TEXT("path"));
	if (!PathPtr || PathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: path"));
	}

	UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, **PathPtr));
	if (!Mesh)
	{
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Static mesh not found: %s"), **PathPtr));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("path"), *PathPtr);

	// Mesh info
	TSharedPtr<FJsonObject> MeshInfo = MakeShared<FJsonObject>();
	MeshInfo->SetNumberField(TEXT("lod_count"), Mesh->GetNumLODs());

	// Bounds (in centimeters)
	FBoxSphereBounds Bounds = Mesh->GetBounds();
	MeshInfo->SetObjectField(TEXT("bounds_origin"), JsonHelpers::VectorToJson(Bounds.Origin));
	MeshInfo->SetObjectField(TEXT("bounds_extent"), JsonHelpers::VectorToJson(Bounds.BoxExtent));
	MeshInfo->SetNumberField(TEXT("bounds_radius"), Bounds.SphereRadius);

	// LOD details
	TArray<TSharedPtr<FJsonValue>> LODArray;
	for (int32 LODIndex = 0; LODIndex < Mesh->GetNumLODs(); ++LODIndex)
	{
		TSharedPtr<FJsonObject> LODInfo = MakeShared<FJsonObject>();
		LODInfo->SetNumberField(TEXT("index"), LODIndex);

		if (Mesh->GetRenderData() && Mesh->GetRenderData()->LODResources.IsValidIndex(LODIndex))
		{
			const FStaticMeshLODResources& LODResources = Mesh->GetRenderData()->LODResources[LODIndex];
			LODInfo->SetNumberField(TEXT("vertices"), LODResources.GetNumVertices());
			LODInfo->SetNumberField(TEXT("triangles"), LODResources.GetNumTriangles());
			LODInfo->SetNumberField(TEXT("sections"), LODResources.Sections.Num());
		}

		LODArray.Add(MakeShared<FJsonValueObject>(LODInfo));
	}
	MeshInfo->SetArrayField(TEXT("lods"), LODArray);

	Response->SetObjectField(TEXT("mesh"), MeshInfo);

	return FRESTResponse::Ok(Response);
}

TSharedPtr<FJsonObject> FAssetsHandler::AssetDataToJson(const FAssetData& AssetData)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("name"), AssetData.AssetName.ToString());
	Json->SetStringField(TEXT("path"), AssetData.GetObjectPathString());
	Json->SetStringField(TEXT("class"), AssetData.AssetClassPath.GetAssetName().ToString());
	Json->SetStringField(TEXT("package"), AssetData.PackageName.ToString());
	return Json;
}

TArray<TSharedPtr<FJsonObject>> FAssetsHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	// GET /assets/list
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/list"));
		Endpoint->SetStringField(TEXT("description"), TEXT("List assets in a directory with optional filters"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), false);
		PathParam->SetStringField(TEXT("default"), TEXT("/Game"));
		PathParam->SetStringField(TEXT("description"), TEXT("Content path to search (e.g., /Game/MyFolder)"));
		Params->SetObjectField(TEXT("path"), PathParam);

		TSharedPtr<FJsonObject> TypeParam = MakeShared<FJsonObject>();
		TypeParam->SetStringField(TEXT("type"), TEXT("string"));
		TypeParam->SetBoolField(TEXT("required"), false);
		TypeParam->SetStringField(TEXT("default"), TEXT(""));
		TypeParam->SetStringField(TEXT("description"), TEXT("Asset type filter (e.g., Material, StaticMesh, or full path /Script/Engine.Material)"));
		Params->SetObjectField(TEXT("type"), TypeParam);

		TSharedPtr<FJsonObject> LimitParam = MakeShared<FJsonObject>();
		LimitParam->SetStringField(TEXT("type"), TEXT("integer"));
		LimitParam->SetBoolField(TEXT("required"), false);
		LimitParam->SetStringField(TEXT("default"), TEXT("1000"));
		LimitParam->SetStringField(TEXT("description"), TEXT("Maximum number of assets to return (hard limit: 10000)"));
		Params->SetObjectField(TEXT("limit"), LimitParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_TYPE")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// POST /assets/search
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/search"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Search assets by name pattern"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> QueryParam = MakeShared<FJsonObject>();
		QueryParam->SetStringField(TEXT("type"), TEXT("string"));
		QueryParam->SetBoolField(TEXT("required"), true);
		QueryParam->SetStringField(TEXT("description"), TEXT("Search string to match against asset names (case-sensitive contains)"));
		Params->SetObjectField(TEXT("query"), QueryParam);

		TSharedPtr<FJsonObject> TypeParam = MakeShared<FJsonObject>();
		TypeParam->SetStringField(TEXT("type"), TEXT("string"));
		TypeParam->SetBoolField(TEXT("required"), false);
		TypeParam->SetStringField(TEXT("default"), TEXT(""));
		TypeParam->SetStringField(TEXT("description"), TEXT("Filter by asset class name"));
		Params->SetObjectField(TEXT("type"), TypeParam);

		TSharedPtr<FJsonObject> LimitParam = MakeShared<FJsonObject>();
		LimitParam->SetStringField(TEXT("type"), TEXT("integer"));
		LimitParam->SetBoolField(TEXT("required"), false);
		LimitParam->SetStringField(TEXT("default"), TEXT("100"));
		LimitParam->SetStringField(TEXT("description"), TEXT("Maximum number of results to return"));
		Params->SetObjectField(TEXT("limit"), LimitParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// GET /assets/info
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/info"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Get detailed information about a specific asset"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), true);
		PathParam->SetStringField(TEXT("description"), TEXT("Full asset path (e.g., /Game/Materials/M_Basic.M_Basic)"));
		Params->SetObjectField(TEXT("path"), PathParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("ASSET_NOT_FOUND")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// GET /assets/refs
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/refs"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Get asset references (what this asset uses) and referencers (what uses this asset)"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), true);
		PathParam->SetStringField(TEXT("description"), TEXT("Package path of the asset (e.g., /Game/Materials/M_Basic)"));
		Params->SetObjectField(TEXT("path"), PathParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// POST /assets/export
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/export"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Export an asset to text format (T3D-like output)"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), true);
		PathParam->SetStringField(TEXT("description"), TEXT("Full asset path to export"));
		Params->SetObjectField(TEXT("path"), PathParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("ASSET_NOT_FOUND")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// POST /assets/validate
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/validate"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Validate asset integrity"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), true);
		PathParam->SetStringField(TEXT("description"), TEXT("Full asset path to validate"));
		Params->SetObjectField(TEXT("path"), PathParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("ASSET_NOT_FOUND")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	// GET /assets/mesh_details
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/assets/mesh_details"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Get detailed geometry information for a static mesh (LODs, vertices, triangles, bounds)"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();

		TSharedPtr<FJsonObject> PathParam = MakeShared<FJsonObject>();
		PathParam->SetStringField(TEXT("type"), TEXT("string"));
		PathParam->SetBoolField(TEXT("required"), true);
		PathParam->SetStringField(TEXT("description"), TEXT("Full path to the static mesh asset"));
		Params->SetObjectField(TEXT("path"), PathParam);

		Endpoint->SetObjectField(TEXT("parameters"), Params);

		TArray<TSharedPtr<FJsonValue>> Errors;
		Errors.Add(MakeShared<FJsonValueString>(TEXT("INVALID_PARAMS")));
		Errors.Add(MakeShared<FJsonValueString>(TEXT("ASSET_NOT_FOUND")));
		Endpoint->SetArrayField(TEXT("errors"), Errors);

		Schemas.Add(Endpoint);
	}

	return Schemas;
}
