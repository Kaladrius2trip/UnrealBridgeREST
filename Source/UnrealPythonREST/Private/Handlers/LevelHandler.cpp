// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/LevelHandler.h"
#include "Utils/JsonHelpers.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "Editor.h"
#include "FileHelpers.h"

void FLevelHandler::RegisterRoutes(FRESTRouter& Router)
{
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/level/info"),
		FRESTRouteHandler::CreateRaw(this, &FLevelHandler::HandleInfo));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/level/outliner"),
		FRESTRouteHandler::CreateRaw(this, &FLevelHandler::HandleOutliner));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/level/load"),
		FRESTRouteHandler::CreateRaw(this, &FLevelHandler::HandleLoad));

	UE_LOG(LogTemp, Log, TEXT("LevelHandler: Registered 3 routes at /level"));
}

FRESTResponse FLevelHandler::HandleInfo(const FRESTRequest& Request)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FRESTResponse::Error(400, TEXT("NO_LEVEL_LOADED"), TEXT("No level currently open in editor"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	TSharedPtr<FJsonObject> LevelInfo = MakeShared<FJsonObject>();
	LevelInfo->SetStringField(TEXT("name"), World->GetMapName());
	LevelInfo->SetStringField(TEXT("path"), World->GetOutermost()->GetPathName());

	// Actor count
	int32 ActorCount = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		ActorCount++;
	}
	LevelInfo->SetNumberField(TEXT("actor_count"), ActorCount);

	// World bounds (in centimeters)
	FBox WorldBounds = World->GetWorldSettings()->GetComponentsBoundingBox();
	if (WorldBounds.IsValid)
	{
		LevelInfo->SetObjectField(TEXT("bounds_min"), JsonHelpers::VectorToJson(WorldBounds.Min));
		LevelInfo->SetObjectField(TEXT("bounds_max"), JsonHelpers::VectorToJson(WorldBounds.Max));
	}

	Response->SetObjectField(TEXT("level"), LevelInfo);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FLevelHandler::HandleOutliner(const FRESTRequest& Request)
{
	UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (!World)
	{
		return FRESTResponse::Error(400, TEXT("NO_LEVEL_LOADED"), TEXT("No level currently open in editor"));
	}

	bool bHierarchical = true;
	const FString* FlatPtr = Request.QueryParams.Find(TEXT("flat"));
	if (FlatPtr && *FlatPtr == TEXT("true"))
	{
		bHierarchical = false;
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	if (bHierarchical)
	{
		// Build tree: only root actors (no parent)
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor && !Actor->GetAttachParentActor())
			{
				ActorsArray.Add(MakeShared<FJsonValueObject>(ActorToOutlinerJson(Actor, true)));
			}
		}
	}
	else
	{
		// Flat list
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor)
			{
				ActorsArray.Add(MakeShared<FJsonValueObject>(ActorToOutlinerJson(Actor, false)));
			}
		}
	}

	Response->SetArrayField(TEXT("actors"), ActorsArray);
	Response->SetNumberField(TEXT("count"), ActorsArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FLevelHandler::HandleLoad(const FRESTRequest& Request)
{
	FString Path;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("path"), Path, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load the map
	bool bSuccess = FEditorFileUtils::LoadMap(Path);

	if (!bSuccess)
	{
		return FRESTResponse::Error(404, TEXT("LEVEL_NOT_FOUND"),
			FString::Printf(TEXT("Failed to load level: %s"), *Path));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("loaded_level"), Path);

	return FRESTResponse::Ok(Response);
}

TSharedPtr<FJsonObject> FLevelHandler::ActorToOutlinerJson(AActor* Actor, bool bIncludeChildren)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
	Json->SetStringField(TEXT("label"), Actor->GetActorLabel());
	Json->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
	Json->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(Actor->GetActorLocation()));

	if (bIncludeChildren)
	{
		TArray<AActor*> Children;
		Actor->GetAttachedActors(Children);

		if (Children.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ChildrenArray;
			for (AActor* Child : Children)
			{
				ChildrenArray.Add(MakeShared<FJsonValueObject>(ActorToOutlinerJson(Child, true)));
			}
			Json->SetArrayField(TEXT("children"), ChildrenArray);
		}
	}

	return Json;
}

TArray<TSharedPtr<FJsonObject>> FLevelHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/level/info")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get current level information (name, path, actor count, bounds)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/level/outliner")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get world outliner actor hierarchy (query: flat=true for flat list)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/level/load")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Load a level by path (body: path)"));

	return Schemas;
}
