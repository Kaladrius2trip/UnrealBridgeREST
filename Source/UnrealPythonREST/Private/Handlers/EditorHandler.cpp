// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/EditorHandler.h"
#include "Utils/JsonHelpers.h"
#include "Utils/ActorUtils.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "HighResScreenshot.h"
#include "UnrealClient.h"
#include "Selection.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "ILiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/UObjectIterator.h"

FEditorHandler::FEditorHandler()
{
	CameraAnim.bIsActive = false;
}

FEditorHandler::~FEditorHandler()
{
	// Use bIsActive flag instead of TickHandle.IsValid() for safety
	if (CameraAnim.bIsActive)
	{
		CameraAnim.bIsActive = false;
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	}
}

float FEditorHandler::EaseInOut(float T)
{
	// Smooth step: 3t^2 - 2t^3
	return T * T * (3.0f - 2.0f * T);
}

bool FEditorHandler::TickCameraAnimation(float DeltaTime)
{
	if (!CameraAnim.bIsActive)
	{
		return false; // Stop ticking
	}

	CameraAnim.ElapsedTime += DeltaTime;
	float Alpha = FMath::Clamp(CameraAnim.ElapsedTime / CameraAnim.Duration, 0.0f, 1.0f);
	float EasedAlpha = EaseInOut(Alpha);

	FVector CurrentLocation;
	FRotator CurrentRotation;

	if (CameraAnim.bOrbitMode)
	{
		// Orbit mode: SLERP the angle around the target, interpolate distance
		FQuat StartQuat = CameraAnim.StartAngle.Quaternion();
		FQuat EndQuat = CameraAnim.EndAngle.Quaternion();
		FQuat CurrentQuat = FQuat::Slerp(StartQuat, EndQuat, EasedAlpha);
		FRotator CurrentAngle = CurrentQuat.Rotator();

		// Interpolate distance
		float CurrentDistance = FMath::Lerp(CameraAnim.StartDistance, CameraAnim.EndDistance, EasedAlpha);

		// Calculate camera position from interpolated angle and distance
		FVector OffsetDirection = CurrentAngle.Vector();
		CurrentLocation = CameraAnim.OrbitTarget + (OffsetDirection * CurrentDistance);

		// Camera always looks at target
		FVector LookDirection = CameraAnim.OrbitTarget - CurrentLocation;
		CurrentRotation = LookDirection.Rotation();
	}
	else
	{
		// Linear mode: interpolate location directly, SLERP rotation
		CurrentLocation = FMath::Lerp(CameraAnim.StartLocation, CameraAnim.EndLocation, EasedAlpha);

		FQuat StartQuat = CameraAnim.StartRotation.Quaternion();
		FQuat EndQuat = CameraAnim.EndRotation.Quaternion();
		FQuat CurrentQuat = FQuat::Slerp(StartQuat, EndQuat, EasedAlpha);
		CurrentRotation = CurrentQuat.Rotator();
	}

	// Apply to viewport
	FEditorViewportClient* ViewportClient = nullptr;
	if (GEditor && GEditor->GetActiveViewport())
	{
		ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	}

	if (ViewportClient)
	{
		ViewportClient->SetViewLocation(CurrentLocation);
		ViewportClient->SetViewRotation(CurrentRotation);
	}

	// Check if animation is complete
	if (Alpha >= 1.0f)
	{
		CameraAnim.bIsActive = false;
		return false; // Stop ticking
	}

	return true; // Continue ticking
}

void FEditorHandler::RegisterRoutes(FRESTRouter& Router)
{
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/editor/project"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleProject));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/screenshot"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleScreenshot));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/camera"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleCamera));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/editor/camera/status"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleCameraStatus));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/editor/selection"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleGetSelection));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/selection"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleSetSelection));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/console"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleConsole));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/replace_mesh"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleReplaceMesh));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/replace_with_bp"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleReplaceWithBP));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/live_coding"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleLiveCoding));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/editor/live_coding"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleLiveCodingStatus));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/open"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleOpenAsset));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/editor/close"),
		FRESTRouteHandler::CreateRaw(this, &FEditorHandler::HandleCloseAsset));

	UE_LOG(LogTemp, Log, TEXT("EditorHandler: Registered 13 routes at /editor"));
}

FRESTResponse FEditorHandler::HandleProject(const FRESTRequest& Request)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	TSharedPtr<FJsonObject> ProjectInfo = MakeShared<FJsonObject>();
	ProjectInfo->SetStringField(TEXT("name"), FApp::GetProjectName());
	ProjectInfo->SetStringField(TEXT("path"), FPaths::ProjectDir());
	ProjectInfo->SetStringField(TEXT("engine_version"), FEngineVersion::Current().ToString());
	ProjectInfo->SetStringField(TEXT("content_dir"), FPaths::ProjectContentDir());

	Response->SetObjectField(TEXT("project"), ProjectInfo);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleScreenshot(const FRESTRequest& Request)
{
	// Get output path
	FString OutputPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT(""));
	if (OutputPath.IsEmpty())
	{
		OutputPath = FPaths::ProjectSavedDir() / TEXT("Screenshots") / FString::Printf(TEXT("Screenshot_%s.png"), *FDateTime::Now().ToString());
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

	// Request screenshot
	FHighResScreenshotConfig& HighResConfig = GetHighResScreenshotConfig();
	HighResConfig.SetFilename(OutputPath);
	HighResConfig.bMaskEnabled = false;

	ViewportClient->TakeHighResScreenShot();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("path"), OutputPath);
	Response->SetStringField(TEXT("message"), TEXT("Screenshot requested - file will be saved asynchronously"));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleCamera(const FRESTRequest& Request)
{
	FEditorViewportClient* ViewportClient = nullptr;
	if (GEditor && GEditor->GetActiveViewport())
	{
		ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	}

	if (!ViewportClient)
	{
		return FRESTResponse::Error(400, TEXT("NO_VIEWPORT"), TEXT("No active editor viewport"));
	}

	// Validate JSON body
	if (!Request.JsonBody.IsValid())
	{
		return FRESTResponse::BadRequest(TEXT("Invalid or missing JSON body"));
	}

	// Check for animation duration (in seconds)
	float Duration = static_cast<float>(JsonHelpers::GetOptionalDouble(Request.JsonBody, TEXT("duration"), 0.0));
	bool bAnimate = Duration > 0.0f;

	// Get current camera state as start
	FVector StartLocation = ViewportClient->GetViewLocation();
	FRotator StartRotation = ViewportClient->GetViewRotation();

	// Parse target location
	FVector TargetLocation = StartLocation;
	if (Request.JsonBody->HasField(TEXT("location")))
	{
		JsonHelpers::JsonToVector(Request.JsonBody->GetObjectField(TEXT("location")), TargetLocation);
	}

	// Parse target rotation
	FRotator TargetRotation = StartRotation;
	if (Request.JsonBody->HasField(TEXT("rotation")))
	{
		JsonHelpers::JsonToRotator(Request.JsonBody->GetObjectField(TEXT("rotation")), TargetRotation);
	}

	// Orbit mode: specify target point, angle (direction from target to camera), and distance
	bool bOrbitMode = Request.JsonBody->HasField(TEXT("orbit"));
	FVector OrbitTarget = FVector::ZeroVector;
	FRotator StartAngle = FRotator::ZeroRotator;
	FRotator EndAngle = FRotator::ZeroRotator;
	float StartDistance = 1000.0f;
	float EndDistance = 1000.0f;

	if (bOrbitMode)
	{
		TSharedPtr<FJsonObject> OrbitObj = Request.JsonBody->GetObjectField(TEXT("orbit"));

		// Get orbit target - the point camera will look at (defaults to origin)
		if (OrbitObj->HasField(TEXT("target")))
		{
			JsonHelpers::JsonToVector(OrbitObj->GetObjectField(TEXT("target")), OrbitTarget);
		}

		// Get end distance from target (defaults to 1000 units)
		EndDistance = static_cast<float>(JsonHelpers::GetOptionalDouble(OrbitObj, TEXT("distance"), 1000.0));

		// Get end angle - direction FROM target TO camera (pitch/yaw)
		if (OrbitObj->HasField(TEXT("angle")))
		{
			JsonHelpers::JsonToRotator(OrbitObj->GetObjectField(TEXT("angle")), EndAngle);
		}

		// Calculate start angle and distance from current camera position
		FVector ToCamera = StartLocation - OrbitTarget;
		StartDistance = ToCamera.Size();
		if (StartDistance > KINDA_SMALL_NUMBER)
		{
			StartAngle = ToCamera.Rotation();
		}

		// Calculate final camera position for response
		FVector OffsetDirection = EndAngle.Vector();
		TargetLocation = OrbitTarget + (OffsetDirection * EndDistance);

		// Camera rotation: look back at target
		FVector LookDirection = OrbitTarget - TargetLocation;
		TargetRotation = LookDirection.Rotation();
	}

	// Focus on actor if specified (overrides location/rotation target)
	FString FocusLabel = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("focus_actor"), TEXT(""));
	FString FocusWarning;
	if (!FocusLabel.IsEmpty())
	{
		AActor* FocusActor = ActorUtils::FindActorByLabel(FocusLabel);
		if (FocusActor)
		{
			if (bAnimate)
			{
				// For animation, calculate target to frame the actor
				FBox BoundingBox = FocusActor->GetComponentsBoundingBox();
				TargetLocation = BoundingBox.GetCenter() + FVector(-500, 0, 300); // Offset from actor
				TargetRotation = (BoundingBox.GetCenter() - TargetLocation).Rotation();
			}
			else
			{
				// Instant focus
				ViewportClient->FocusViewportOnBox(FocusActor->GetComponentsBoundingBox());
			}
		}
		else
		{
			FocusWarning = FString::Printf(TEXT("Focus actor '%s' not found"), *FocusLabel);
		}
	}

	// Apply camera change
	if (bAnimate)
	{
		// Stop any existing animation - use bIsActive flag instead of TickHandle.IsValid()
		// because Live Coding can invalidate the weak pointer in TickHandle
		if (CameraAnim.bIsActive)
		{
			CameraAnim.bIsActive = false;
			// Try to remove the ticker, but don't crash if the handle is stale
			FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
			TickHandle.Reset();
		}

		// Set up animation state
		CameraAnim.bOrbitMode = bOrbitMode;
		CameraAnim.Duration = Duration;
		CameraAnim.ElapsedTime = 0.0f;
		CameraAnim.bIsActive = true;

		if (bOrbitMode)
		{
			// Orbit animation: SLERP the angle around target
			CameraAnim.OrbitTarget = OrbitTarget;
			CameraAnim.StartAngle = StartAngle;
			CameraAnim.EndAngle = EndAngle;
			CameraAnim.StartDistance = StartDistance;
			CameraAnim.EndDistance = EndDistance;
		}
		else
		{
			// Linear animation: interpolate position directly
			CameraAnim.StartLocation = StartLocation;
			CameraAnim.EndLocation = TargetLocation;
			CameraAnim.StartRotation = StartRotation;
			CameraAnim.EndRotation = TargetRotation;
		}

		// Register tick handler
		TickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateRaw(this, &FEditorHandler::TickCameraAnimation));

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetBoolField(TEXT("animating"), true);
		Response->SetBoolField(TEXT("orbit_mode"), bOrbitMode);
		Response->SetNumberField(TEXT("duration"), Duration);
		Response->SetObjectField(TEXT("start_location"), JsonHelpers::VectorToJson(StartLocation));
		Response->SetObjectField(TEXT("end_location"), JsonHelpers::VectorToJson(TargetLocation));
		Response->SetObjectField(TEXT("start_rotation"), JsonHelpers::RotatorToJson(StartRotation));
		Response->SetObjectField(TEXT("end_rotation"), JsonHelpers::RotatorToJson(TargetRotation));
		if (bOrbitMode)
		{
			Response->SetObjectField(TEXT("orbit_target"), JsonHelpers::VectorToJson(OrbitTarget));
			Response->SetObjectField(TEXT("start_angle"), JsonHelpers::RotatorToJson(StartAngle));
			Response->SetObjectField(TEXT("end_angle"), JsonHelpers::RotatorToJson(EndAngle));
		}
		if (!FocusWarning.IsEmpty())
		{
			Response->SetStringField(TEXT("warning"), FocusWarning);
		}
		return FRESTResponse::Ok(Response);
	}
	else
	{
		// Instant move (no focus_actor case, since that was handled above)
		if (Request.JsonBody->HasField(TEXT("location")))
		{
			ViewportClient->SetViewLocation(TargetLocation);
		}
		if (Request.JsonBody->HasField(TEXT("rotation")))
		{
			ViewportClient->SetViewRotation(TargetRotation);
		}

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetBoolField(TEXT("animating"), false);
		if (!FocusWarning.IsEmpty())
		{
			Response->SetStringField(TEXT("warning"), FocusWarning);
		}
		Response->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(ViewportClient->GetViewLocation()));
		Response->SetObjectField(TEXT("rotation"), JsonHelpers::RotatorToJson(ViewportClient->GetViewRotation()));
		return FRESTResponse::Ok(Response);
	}
}

FRESTResponse FEditorHandler::HandleCameraStatus(const FRESTRequest& Request)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetBoolField(TEXT("animating"), CameraAnim.bIsActive);

	if (CameraAnim.bIsActive)
	{
		float Progress = FMath::Clamp(CameraAnim.ElapsedTime / CameraAnim.Duration, 0.0f, 1.0f);
		Response->SetNumberField(TEXT("progress"), Progress);
		Response->SetNumberField(TEXT("elapsed"), CameraAnim.ElapsedTime);
		Response->SetNumberField(TEXT("duration"), CameraAnim.Duration);
	}

	// Get current camera position
	FEditorViewportClient* ViewportClient = nullptr;
	if (GEditor && GEditor->GetActiveViewport())
	{
		ViewportClient = static_cast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	}

	if (ViewportClient)
	{
		Response->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(ViewportClient->GetViewLocation()));
		Response->SetObjectField(TEXT("rotation"), JsonHelpers::RotatorToJson(ViewportClient->GetViewRotation()));
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleGetSelection(const FRESTRequest& Request)
{
	if (!GEditor)
	{
		return FRESTResponse::Error(400, TEXT("NO_EDITOR"), TEXT("Editor not available"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> SelectedArray;

	USelection* Selection = GEditor->GetSelectedActors();
	for (FSelectionIterator It(*Selection); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (Actor)
		{
			TSharedPtr<FJsonObject> ActorJson = MakeShared<FJsonObject>();
			ActorJson->SetStringField(TEXT("label"), Actor->GetActorLabel());
			ActorJson->SetStringField(TEXT("class"), Actor->GetClass()->GetName());
			ActorJson->SetObjectField(TEXT("location"), JsonHelpers::VectorToJson(Actor->GetActorLocation()));
			SelectedArray.Add(MakeShared<FJsonValueObject>(ActorJson));
		}
	}

	Response->SetArrayField(TEXT("selected"), SelectedArray);
	Response->SetNumberField(TEXT("count"), SelectedArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleSetSelection(const FRESTRequest& Request)
{
	if (!GEditor)
	{
		return FRESTResponse::Error(400, TEXT("NO_EDITOR"), TEXT("Editor not available"));
	}

	// Get labels array
	const TArray<TSharedPtr<FJsonValue>>* LabelsArray = nullptr;
	if (!Request.JsonBody->TryGetArrayField(TEXT("labels"), LabelsArray) || !LabelsArray)
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: labels (array of actor labels)"));
	}

	// Clear current selection
	GEditor->SelectNone(true, true, false);

	TArray<FString> FoundLabels;
	TArray<FString> NotFoundLabels;

	for (const TSharedPtr<FJsonValue>& LabelValue : *LabelsArray)
	{
		FString Label = LabelValue->AsString();
		AActor* Actor = ActorUtils::FindActorByLabel(Label);
		if (Actor)
		{
			GEditor->SelectActor(Actor, true, true, false, false);
			FoundLabels.Add(Label);
		}
		else
		{
			NotFoundLabels.Add(Label);
		}
	}

	// Notify selection change
	GEditor->NoteSelectionChange();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), NotFoundLabels.Num() == 0);
	Response->SetNumberField(TEXT("selected_count"), FoundLabels.Num());

	TArray<TSharedPtr<FJsonValue>> FoundArray;
	for (const FString& L : FoundLabels)
	{
		FoundArray.Add(MakeShared<FJsonValueString>(L));
	}
	Response->SetArrayField(TEXT("selected"), FoundArray);

	if (NotFoundLabels.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& L : NotFoundLabels)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(L));
		}
		Response->SetArrayField(TEXT("not_found"), NotFoundArray);
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleConsole(const FRESTRequest& Request)
{
	FString Command;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("command"), Command, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Execute the console command
	UWorld* World = ActorUtils::GetEditorWorld();
	if (World)
	{
		GEngine->Exec(World, *Command);
	}
	else
	{
		GEngine->Exec(nullptr, *Command);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("command"), Command);
	Response->SetStringField(TEXT("message"), TEXT("Command executed"));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleReplaceMesh(const FRESTRequest& Request)
{
	FString TargetLabel;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("label"), TargetLabel, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString NewMeshPath;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("mesh_path"), NewMeshPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Find the actor
	AActor* Actor = ActorUtils::FindActorByLabel(TargetLabel);
	if (!Actor)
	{
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), *TargetLabel));
	}

	// Load the new mesh
	UStaticMesh* NewMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *NewMeshPath));
	if (!NewMesh)
	{
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Static mesh not found: %s"), *NewMeshPath));
	}

	// Find static mesh component
	UStaticMeshComponent* MeshComponent = Actor->FindComponentByClass<UStaticMeshComponent>();
	if (!MeshComponent)
	{
		return FRESTResponse::Error(400, TEXT("NO_MESH_COMPONENT"),
			TEXT("Actor does not have a StaticMeshComponent"));
	}

	// Replace the mesh
	FString OldMeshPath = MeshComponent->GetStaticMesh() ? MeshComponent->GetStaticMesh()->GetPathName() : TEXT("None");
	MeshComponent->SetStaticMesh(NewMesh);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("label"), TargetLabel);
	Response->SetStringField(TEXT("old_mesh"), OldMeshPath);
	Response->SetStringField(TEXT("new_mesh"), NewMeshPath);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleReplaceWithBP(const FRESTRequest& Request)
{
	FString TargetLabel;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("label"), TargetLabel, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString BlueprintPath;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("blueprint_path"), BlueprintPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Find the original actor
	AActor* OriginalActor = ActorUtils::FindActorByLabel(TargetLabel);
	if (!OriginalActor)
	{
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found"), *TargetLabel));
	}

	// Store original transform
	FTransform OriginalTransform = OriginalActor->GetActorTransform();
	FString OriginalClass = OriginalActor->GetClass()->GetName();

	// Spawn the blueprint actor
	AActor* NewActor = ActorUtils::SpawnActorFromClass(BlueprintPath, OriginalTransform, Error);
	if (!NewActor)
	{
		return FRESTResponse::Error(400, TEXT("SPAWN_FAILED"), Error);
	}

	// Delete the original
	OriginalActor->Destroy();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("original_label"), TargetLabel);
	Response->SetStringField(TEXT("original_class"), OriginalClass);
	Response->SetStringField(TEXT("new_label"), NewActor->GetActorLabel());
	Response->SetStringField(TEXT("new_class"), NewActor->GetClass()->GetName());
	Response->SetObjectField(TEXT("transform"), JsonHelpers::TransformToJson(NewActor->GetActorTransform()));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleLiveCoding(const FRESTRequest& Request)
{
	// Check if Live Coding module is available
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding)
	{
		return FRESTResponse::Error(400, TEXT("LIVE_CODING_NOT_AVAILABLE"),
			TEXT("Live Coding module is not available on this platform"));
	}

	// Check if already compiling
	if (LiveCoding->IsCompiling())
	{
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("status"), TEXT("already_compiling"));
		Response->SetStringField(TEXT("message"), TEXT("A Live Coding compile is already in progress"));
		return FRESTResponse::Ok(Response);
	}

	// Check if Live Coding is enabled
	if (!LiveCoding->IsEnabledForSession())
	{
		// Try to enable it
		if (LiveCoding->CanEnableForSession())
		{
			LiveCoding->EnableForSession(true);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("LIVE_CODING_DISABLED"),
				TEXT("Live Coding is not enabled and cannot be enabled for this session"));
		}
	}

	// Get optional wait parameter
	bool bWaitForCompletion = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("wait"), false);

	// Trigger compile
	ELiveCodingCompileResult Result = ELiveCodingCompileResult::NotStarted;
	ELiveCodingCompileFlags Flags = bWaitForCompletion ? ELiveCodingCompileFlags::WaitForCompletion : ELiveCodingCompileFlags::None;

	bool bStarted = LiveCoding->Compile(Flags, &Result);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Convert result to string and determine success
	FString ResultStr;
	FString Message;
	bool bSuccess = false;

	switch (Result)
	{
		case ELiveCodingCompileResult::Success:
			ResultStr = TEXT("success");
			Message = TEXT("Compilation completed successfully");
			bSuccess = true;
			break;
		case ELiveCodingCompileResult::NoChanges:
			ResultStr = TEXT("no_changes");
			Message = TEXT("No code changes detected");
			bSuccess = true;
			break;
		case ELiveCodingCompileResult::InProgress:
			ResultStr = TEXT("in_progress");
			Message = TEXT("Compilation started (use wait=true to wait for completion)");
			bSuccess = true;
			break;
		case ELiveCodingCompileResult::CompileStillActive:
			ResultStr = TEXT("compile_still_active");
			Message = TEXT("A previous compilation is still active");
			bSuccess = false;
			break;
		case ELiveCodingCompileResult::NotStarted:
			ResultStr = TEXT("not_started");
			Message = TEXT("Compilation failed to start");
			bSuccess = false;
			break;
		case ELiveCodingCompileResult::Failure:
			ResultStr = TEXT("failure");
			Message = TEXT("Compilation FAILED - check Output Log in Unreal Editor for error details");
			bSuccess = false;
			break;
		case ELiveCodingCompileResult::Cancelled:
			ResultStr = TEXT("cancelled");
			Message = TEXT("Compilation was cancelled");
			bSuccess = false;
			break;
		default:
			ResultStr = TEXT("unknown");
			Message = TEXT("Unknown compilation result");
			bSuccess = false;
			break;
	}

	Response->SetBoolField(TEXT("success"), bSuccess);
	Response->SetStringField(TEXT("result"), ResultStr);
	Response->SetStringField(TEXT("message"), Message);
	Response->SetBoolField(TEXT("waited"), bWaitForCompletion);

	// If failed and waited, indicate that errors are in the Output Log
	if (!bSuccess && bWaitForCompletion && Result == ELiveCodingCompileResult::Failure)
	{
		Response->SetStringField(TEXT("error_location"), TEXT("Unreal Editor Output Log (Window > Developer Tools > Output Log)"));
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleLiveCodingStatus(const FRESTRequest& Request)
{
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	if (!LiveCoding)
	{
		Response->SetBoolField(TEXT("available"), false);
		Response->SetStringField(TEXT("message"), TEXT("Live Coding module not available on this platform"));
	}
	else
	{
		Response->SetBoolField(TEXT("available"), true);
		Response->SetBoolField(TEXT("enabled_by_default"), LiveCoding->IsEnabledByDefault());
		Response->SetBoolField(TEXT("enabled_for_session"), LiveCoding->IsEnabledForSession());
		Response->SetBoolField(TEXT("has_started"), LiveCoding->HasStarted());
		Response->SetBoolField(TEXT("is_compiling"), LiveCoding->IsCompiling());
		Response->SetBoolField(TEXT("can_enable"), LiveCoding->CanEnableForSession());
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleOpenAsset(const FRESTRequest& Request)
{
	// Required: asset_path (e.g., "/Game/Materials/M_Test")
	FString AssetPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("asset_path"), AssetPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load the asset
	UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
	if (!Asset)
	{
		return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
			FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
	}

	// Open the asset in its appropriate editor
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return FRESTResponse::Error(500, TEXT("SUBSYSTEM_ERROR"), TEXT("AssetEditorSubsystem not available"));
	}

	bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Asset);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), bOpened);
	Response->SetStringField(TEXT("asset_path"), AssetPath);
	Response->SetStringField(TEXT("asset_class"), Asset->GetClass()->GetName());

	if (bOpened)
	{
		Response->SetStringField(TEXT("message"), TEXT("Asset editor opened"));
	}
	else
	{
		Response->SetStringField(TEXT("message"), TEXT("Failed to open asset editor"));
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FEditorHandler::HandleCloseAsset(const FRESTRequest& Request)
{
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return FRESTResponse::Error(500, TEXT("SUBSYSTEM_ERROR"), TEXT("AssetEditorSubsystem not available"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Check if close_all is requested
	bool bCloseAll = false;
	if (Request.JsonBody.IsValid() && Request.JsonBody->HasField(TEXT("close_all")))
	{
		bCloseAll = Request.JsonBody->GetBoolField(TEXT("close_all"));
	}

	if (bCloseAll)
	{
		// Close all asset editors
		AssetEditorSubsystem->CloseAllAssetEditors();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("message"), TEXT("All asset editors closed"));
	}
	else
	{
		// Close specific asset
		FString AssetPath;
		FString Error;
		if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("asset_path"), AssetPath, Error))
		{
			return FRESTResponse::BadRequest(Error);
		}

		UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, *AssetPath);
		if (!Asset)
		{
			return FRESTResponse::Error(404, TEXT("ASSET_NOT_FOUND"),
				FString::Printf(TEXT("Asset not found: %s"), *AssetPath));
		}

		AssetEditorSubsystem->CloseAllEditorsForAsset(Asset);
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("asset_path"), AssetPath);
		Response->SetStringField(TEXT("message"), TEXT("Asset editor closed"));
	}

	return FRESTResponse::Ok(Response);
}

TArray<TSharedPtr<FJsonObject>> FEditorHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/project")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get project metadata (name, path, engine version)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/screenshot")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Capture viewport screenshot (body: path)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/camera")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Move viewport camera (body: location, rotation, duration, orbit, focus_actor)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/camera/status")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get camera animation status"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/selection")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get currently selected actors"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/selection")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Set selected actors (body: labels[])"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/console")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Execute console command (body: command)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/replace_mesh")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Replace static mesh on actor (body: label, mesh_path)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/replace_with_bp")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Replace actor with Blueprint instance (body: label, blueprint_path)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/live_coding")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Trigger Live Coding compile (body: wait=true/false)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/live_coding")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get Live Coding status"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/open")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Open asset in editor (body: asset_path)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/editor/close")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Close asset editor (body: asset_path or close_all=true)"));

	return Schemas;
}
