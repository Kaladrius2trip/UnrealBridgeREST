// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"
#include "Containers/Ticker.h"

/**
 * Camera animation state for smooth movement
 */
struct FCameraAnimation
{
	// Linear mode (direct position interpolation)
	FVector StartLocation;
	FVector EndLocation;
	FRotator StartRotation;
	FRotator EndRotation;

	// Orbit mode (interpolate angle around target)
	bool bOrbitMode;
	FVector OrbitTarget;
	FRotator StartAngle;  // Angle FROM target TO camera (start)
	FRotator EndAngle;    // Angle FROM target TO camera (end)
	float StartDistance;
	float EndDistance;

	// Common
	float Duration;
	float ElapsedTime;
	bool bIsActive;

	FCameraAnimation()
		: bOrbitMode(false)
		, StartDistance(1000.0f)
		, EndDistance(1000.0f)
		, Duration(0)
		, ElapsedTime(0)
		, bIsActive(false)
	{}
};

/**
 * Editor utility endpoints.
 * Provides screenshot, camera, selection, and console command features.
 *
 * Endpoints:
 *   GET  /editor/project       - Project metadata (name, path, engine version)
 *   POST /editor/screenshot    - Capture viewport to file
 *   POST /editor/camera        - Move viewport camera (instant or animated)
 *   GET  /editor/selection     - Get selected actors
 *   POST /editor/selection     - Set selected actors
 *   POST /editor/console       - Run console command
 *   POST /editor/replace_mesh  - Swap meshes on actors
 *   POST /editor/replace_with_bp - Replace actor with blueprint
 *   POST /editor/open          - Open asset in its editor (body: asset_path)
 *   POST /editor/close         - Close asset editor (body: asset_path or close_all)
 */
class FEditorHandler : public IRESTHandler
{
public:
	FEditorHandler();
	virtual ~FEditorHandler();

	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/editor"); }
	virtual FString GetHandlerName() const override { return TEXT("Editor"); }
	virtual FString GetDescription() const override { return TEXT("Editor utilities: screenshot, camera, selection, console"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;

private:
	/** Camera animation state */
	FCameraAnimation CameraAnim;
	FTSTicker::FDelegateHandle TickHandle;

	/** Tick function for camera animation */
	bool TickCameraAnimation(float DeltaTime);

	/** Ease in-out interpolation */
	static float EaseInOut(float T);

private:
	/** GET /editor/project - Get project metadata */
	FRESTResponse HandleProject(const FRESTRequest& Request);

	/** POST /editor/screenshot - Capture viewport to file */
	FRESTResponse HandleScreenshot(const FRESTRequest& Request);

	/** POST /editor/camera - Move viewport camera to location/rotation or focus on actor */
	FRESTResponse HandleCamera(const FRESTRequest& Request);

	/** GET /editor/selection - Get currently selected actors */
	FRESTResponse HandleGetSelection(const FRESTRequest& Request);

	/** POST /editor/selection - Set selected actors by label */
	FRESTResponse HandleSetSelection(const FRESTRequest& Request);

	/** POST /editor/console - Execute a console command */
	FRESTResponse HandleConsole(const FRESTRequest& Request);

	/** POST /editor/replace_mesh - Replace static mesh on an actor */
	FRESTResponse HandleReplaceMesh(const FRESTRequest& Request);

	/** POST /editor/replace_with_bp - Replace actor with a blueprint instance */
	FRESTResponse HandleReplaceWithBP(const FRESTRequest& Request);

	/** GET /editor/camera/status - Get camera animation status */
	FRESTResponse HandleCameraStatus(const FRESTRequest& Request);

	/** POST /editor/live_coding - Trigger Live Coding compile */
	FRESTResponse HandleLiveCoding(const FRESTRequest& Request);

	/** GET /editor/live_coding - Get Live Coding status */
	FRESTResponse HandleLiveCodingStatus(const FRESTRequest& Request);

	/** POST /editor/open - Open asset in its appropriate editor */
	FRESTResponse HandleOpenAsset(const FRESTRequest& Request);

	/** POST /editor/close - Close asset editor */
	FRESTResponse HandleCloseAsset(const FRESTRequest& Request);
};
