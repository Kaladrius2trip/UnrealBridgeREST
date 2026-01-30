// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/ActorsHandler.h"
#include "Utils/JsonHelpers.h"
#include "Utils/ActorUtils.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "LevelEditorViewport.h"
#include "EditorViewportClient.h"

void FActorsHandler::RegisterRoutes(FRESTRouter& Router)
{
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/actors/list"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleList));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/actors/details"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleDetails));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/actors/spawn"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleSpawn));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/actors/spawn_raycast"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleSpawnRaycast));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/actors/duplicate"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleDuplicate));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/actors/transform"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleTransform));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/actors/delete"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleDelete));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/actors/in_view"),
		FRESTRouteHandler::CreateRaw(this, &FActorsHandler::HandleInView));

	UE_LOG(LogTemp, Log, TEXT("ActorsHandler: Registered 8 routes at /actors"));
}

FRESTResponse FActorsHandler::HandleList(const FRESTRequest& Request)
{
	UWorld* World = ActorUtils::GetEditorWorld();
	if (!World)
	{
		return FRESTResponse::Error(400, TEXT("NO_LEVEL_LOADED"), TEXT("No level currently open"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorJson->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(Actor->GetActorLocation()));
			ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Response->SetArrayField(TEXT("actors"), ActorsArray);
	Response->SetNumberField(TEXT("count"), ActorsArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleDetails(const FRESTRequest& Request)
{
	const FString* LabelPtr = Request.QueryParams.Find(TEXT("label"));
	if (!LabelPtr || LabelPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: label"));
	}

	AActor* Actor = ActorUtils::FindActorByLabel(*LabelPtr);
	if (!Actor)
	{
		TArray<FString> AllLabels = ActorUtils::GetAllActorLabels();
		TArray<FString> Similar = JsonHelpers::FindSimilarStrings(*LabelPtr, AllLabels);
		TSharedPtr<FJsonObject> Details = JsonHelpers::CreateErrorDetails(
			*LabelPtr,
			TEXT("Use GET /actors/list to see available actors"),
			Similar
		);
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), **LabelPtr));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("actor"), ActorUtils::ActorToDetailedJson(Actor));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleSpawn(const FRESTRequest& Request)
{
	FString ClassPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("class_path"), ClassPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Parse transform from JSON
	FTransform Transform;
	JsonHelpers::JsonToTransform(Request.JsonBody, Transform);

	// Spawn the actor
	AActor* SpawnedActor = ActorUtils::SpawnActorFromClass(ClassPath, Transform, Error);
	if (!SpawnedActor)
	{
		return FRESTResponse::Error(400, TEXT("SPAWN_FAILED"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("actor_label"), SpawnedActor->GetActorLabel());
	Response->SetStringField(TEXT("actor_path"), SpawnedActor->GetPathName());
	Response->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(SpawnedActor->GetActorLocation()));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleSpawnRaycast(const FRESTRequest& Request)
{
	FString ClassPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("class_path"), ClassPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get start location for raycast
	FVector StartLocation = FVector::ZeroVector;
	if (Request.JsonBody->HasField(TEXT("location")))
	{
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("location")), StartLocation);
	}

	// Raycast to find surface
	FVector HitLocation;
	FVector HitNormal;
	float MaxDistance = static_cast<float>(JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("max_distance"), 100000));

	if (!ActorUtils::RaycastToSurface(StartLocation, MaxDistance, HitLocation, HitNormal))
	{
		return FRESTResponse::Error(400, TEXT("NO_SURFACE_FOUND"),
			TEXT("No surface found below the specified location"));
	}

	// Parse rotation and scale
	FRotator Rotation = FRotator::ZeroRotator;
	if (Request.JsonBody->HasField(TEXT("rotation")))
	{
		JsonHelpers::JsonToRotator(Request.JsonBody->GetObjectField(TEXT("rotation")), Rotation);
	}

	FVector Scale = FVector::OneVector;
	if (Request.JsonBody->HasField(TEXT("scale")))
	{
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("scale")), Scale);
	}

	FTransform Transform(Rotation, HitLocation, Scale);

	// Spawn the actor
	AActor* SpawnedActor = ActorUtils::SpawnActorFromClass(ClassPath, Transform, Error);
	if (!SpawnedActor)
	{
		return FRESTResponse::Error(400, TEXT("SPAWN_FAILED"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("actor_label"), SpawnedActor->GetActorLabel());
	Response->SetObjectField(TEXT("hit_location"), JsonHelpers::VectorToJson(HitLocation));
	Response->SetObjectField(TEXT("hit_normal"), JsonHelpers::VectorToJson(HitNormal));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleDuplicate(const FRESTRequest& Request)
{
	FString Label;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("label"), Label, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	AActor* SourceActor = ActorUtils::FindActorByLabel(Label);
	if (!SourceActor)
	{
		TArray<FString> AllLabels = ActorUtils::GetAllActorLabels();
		TArray<FString> Similar = JsonHelpers::FindSimilarStrings(Label, AllLabels);
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), *Label));
	}

	// Get offset (default: 100 units on X axis)
	FVector Offset = FVector(100.0, 0.0, 0.0);
	if (Request.JsonBody->HasField(TEXT("offset")))
	{
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("offset")), Offset);
	}

	// Duplicate the actor
	UWorld* World = ActorUtils::GetEditorWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Template = SourceActor;

	AActor* DuplicatedActor = World->SpawnActor<AActor>(
		SourceActor->GetClass(),
		SourceActor->GetActorLocation() + Offset,
		SourceActor->GetActorRotation(),
		SpawnParams
	);

	if (!DuplicatedActor)
	{
		return FRESTResponse::Error(500, TEXT("DUPLICATE_FAILED"), TEXT("Failed to duplicate actor"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("source_label"), Label);
	Response->SetStringField(TEXT("new_label"), DuplicatedActor->GetActorLabel());
	Response->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(DuplicatedActor->GetActorLocation()));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleTransform(const FRESTRequest& Request)
{
	FString Label;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("label"), Label, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	AActor* Actor = ActorUtils::FindActorByLabel(Label);
	if (!Actor)
	{
		TArray<FString> AllLabels = ActorUtils::GetAllActorLabels();
		TArray<FString> Similar = JsonHelpers::FindSimilarStrings(Label, AllLabels);
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), *Label));
	}

	// Apply transforms
	bool bModified = false;

	if (Request.JsonBody->HasField(TEXT("location")))
	{
		FVector Location;
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("location")), Location);
		Actor->SetActorLocation(Location);
		bModified = true;
	}

	if (Request.JsonBody->HasField(TEXT("rotation")))
	{
		FRotator Rotation;
		JsonHelpers::JsonToRotator(Request.JsonBody->GetObjectField(TEXT("rotation")), Rotation);
		Actor->SetActorRotation(Rotation);
		bModified = true;
	}

	if (Request.JsonBody->HasField(TEXT("scale")))
	{
		FVector Scale;
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("scale")), Scale);
		Actor->SetActorScale3D(Scale);
		bModified = true;
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("label"), Label);
	Response->SetBoolField(TEXT("modified"), bModified);
	Response->SetObjectField(TEXT("transform"), JsonHelpers::TransformToJson(Actor->GetActorTransform()));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleDelete(const FRESTRequest& Request)
{
	FString Label;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("label"), Label, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	AActor* Actor = ActorUtils::FindActorByLabel(Label);
	if (!Actor)
	{
		TArray<FString> AllLabels = ActorUtils::GetAllActorLabels();
		TArray<FString> Similar = JsonHelpers::FindSimilarStrings(Label, AllLabels);
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), *Label));
	}

	// Store info before deletion
	FString ActorClass = Actor->GetClass()->GetName();

	// Delete the actor
	Actor->Destroy();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("deleted_label"), Label);
	Response->SetStringField(TEXT("deleted_class"), ActorClass);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FActorsHandler::HandleInView(const FRESTRequest& Request)
{
	UWorld* World = ActorUtils::GetEditorWorld();
	if (!World)
	{
		return FRESTResponse::Error(400, TEXT("NO_LEVEL_LOADED"), TEXT("No level currently open"));
	}

	// Get the active viewport
	FEditorViewportClient* ViewportClient = nullptr;
	if (GEditor && GEditor->GetActiveViewport())
	{
		ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	}

	if (!ViewportClient)
	{
		return FRESTResponse::Error(400, TEXT("NO_VIEWPORT"), TEXT("No active editor viewport"));
	}

	// Get camera location and max distance from query params
	FVector CameraLocation = ViewportClient->GetViewLocation();
	float MaxDistance = 50000.0f; // Default 500 meters

	const FString* DistancePtr = Request.QueryParams.Find(TEXT("max_distance"));
	if (DistancePtr)
	{
		MaxDistance = FCString::Atof(**DistancePtr);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("camera_location"), JsonHelpers::VectorToJson(CameraLocation));

	TArray<TSharedPtr<FJsonValue>> ActorsArray;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor)
		{
			// Distance-based visibility check
			float Distance = FVector::Dist(CameraLocation, Actor->GetActorLocation());
			if (Distance <= MaxDistance)
			{
				TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
				ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
				ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
				ActorJson->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(Actor->GetActorLocation()));
				ActorJson->SetNumberField(TEXT("distance"), Distance);
				ActorsArray.Add(MakeShared<FJsonValueObject>(ActorJson));
			}
		}
	}

	Response->SetArrayField(TEXT("actors"), ActorsArray);
	Response->SetNumberField(TEXT("count"), ActorsArray.Num());

	return FRESTResponse::Ok(Response);
}

TArray<TSharedPtr<FJsonObject>> FActorsHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	// GET /actors/list
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/list"));
		Endpoint->SetStringField(TEXT("description"), TEXT("List all actors in the current level"));
		Schemas.Add(Endpoint);
	}

	// GET /actors/details
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/details"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Get detailed information about a specific actor"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> LabelParam = MakeShared<FJsonObject>();
		LabelParam->SetStringField(TEXT("type"), TEXT("string"));
		LabelParam->SetBoolField(TEXT("required"), true);
		LabelParam->SetStringField(TEXT("description"), TEXT("Actor label to query"));
		Params->SetObjectField(TEXT("label"), LabelParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// POST /actors/spawn
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/spawn"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Spawn actor from class path with location, rotation, scale"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> ClassParam = MakeShared<FJsonObject>();
		ClassParam->SetStringField(TEXT("type"), TEXT("string"));
		ClassParam->SetBoolField(TEXT("required"), true);
		ClassParam->SetStringField(TEXT("description"), TEXT("Full class path (e.g., /Script/Engine.StaticMeshActor)"));
		Params->SetObjectField(TEXT("class_path"), ClassParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// POST /actors/spawn_raycast
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/spawn_raycast"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Spawn actor on surface using raycast"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> ClassParam = MakeShared<FJsonObject>();
		ClassParam->SetStringField(TEXT("type"), TEXT("string"));
		ClassParam->SetBoolField(TEXT("required"), true);
		ClassParam->SetStringField(TEXT("description"), TEXT("Full class path"));
		Params->SetObjectField(TEXT("class_path"), ClassParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// POST /actors/duplicate
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/duplicate"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Duplicate an existing actor with optional offset"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> LabelParam = MakeShared<FJsonObject>();
		LabelParam->SetStringField(TEXT("type"), TEXT("string"));
		LabelParam->SetBoolField(TEXT("required"), true);
		LabelParam->SetStringField(TEXT("description"), TEXT("Label of actor to duplicate"));
		Params->SetObjectField(TEXT("label"), LabelParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// POST /actors/transform
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/transform"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Update actor location, rotation, and/or scale"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> LabelParam = MakeShared<FJsonObject>();
		LabelParam->SetStringField(TEXT("type"), TEXT("string"));
		LabelParam->SetBoolField(TEXT("required"), true);
		LabelParam->SetStringField(TEXT("description"), TEXT("Label of actor to transform"));
		Params->SetObjectField(TEXT("label"), LabelParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// POST /actors/delete
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("POST"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/delete"));
		Endpoint->SetStringField(TEXT("description"), TEXT("Remove actor from level"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> LabelParam = MakeShared<FJsonObject>();
		LabelParam->SetStringField(TEXT("type"), TEXT("string"));
		LabelParam->SetBoolField(TEXT("required"), true);
		LabelParam->SetStringField(TEXT("description"), TEXT("Label of actor to delete"));
		Params->SetObjectField(TEXT("label"), LabelParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	// GET /actors/in_view
	{
		TSharedPtr<FJsonObject> Endpoint = MakeShared<FJsonObject>();
		Endpoint->SetStringField(TEXT("method"), TEXT("GET"));
		Endpoint->SetStringField(TEXT("path"), TEXT("/actors/in_view"));
		Endpoint->SetStringField(TEXT("description"), TEXT("List actors visible in editor viewport"));

		TSharedPtr<FJsonObject> Params = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> DistParam = MakeShared<FJsonObject>();
		DistParam->SetStringField(TEXT("type"), TEXT("number"));
		DistParam->SetBoolField(TEXT("required"), false);
		DistParam->SetStringField(TEXT("default"), TEXT("50000"));
		DistParam->SetStringField(TEXT("description"), TEXT("Maximum distance from camera"));
		Params->SetObjectField(TEXT("max_distance"), DistParam);
		Endpoint->SetObjectField(TEXT("parameters"), Params);

		Schemas.Add(Endpoint);
	}

	return Schemas;
}
