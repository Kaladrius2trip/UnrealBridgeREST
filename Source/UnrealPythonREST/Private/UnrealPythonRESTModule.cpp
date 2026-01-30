// Copyright Epic Games, Inc. All Rights Reserved.

#include "UnrealPythonREST.h"
#include "RESTRouter.h"
#include "ConfigWriter.h"
#include "Handlers/InfrastructureHandler.h"
#include "Handlers/AssetsHandler.h"
#include "Handlers/LevelHandler.h"
#include "Handlers/ActorsHandler.h"
#include "Handlers/EditorHandler.h"
#include "Handlers/PythonHandler.h"
#include "Handlers/MaterialsHandler.h"
#include "Handlers/BlueprintsHandler.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FUnrealPythonRESTModule"

DEFINE_LOG_CATEGORY_STATIC(LogUnrealPythonREST, Log, All);

void FUnrealPythonRESTModule::StartupModule()
{
	// Create and start the REST router
	Router = MakeShared<FRESTRouter>();

	if (!Router->Start())
	{
		UE_LOG(LogUnrealPythonREST, Error, TEXT("Failed to start REST server"));
		return;
	}

	// Register Infrastructure handler first (provides /health, /schema)
	TSharedPtr<FInfrastructureHandler> InfraHandler = MakeShared<FInfrastructureHandler>();
	RegisterHandler(InfraHandler);

	// Register Assets handler (provides /assets/*)
	TSharedPtr<FAssetsHandler> AssetsHandler = MakeShared<FAssetsHandler>();
	RegisterHandler(AssetsHandler);

	// Register Level handler (provides /level/*)
	TSharedPtr<FLevelHandler> LevelHandler = MakeShared<FLevelHandler>();
	RegisterHandler(LevelHandler);

	// Register Actors handler (provides /actors/*)
	TSharedPtr<FActorsHandler> ActorsHandler = MakeShared<FActorsHandler>();
	RegisterHandler(ActorsHandler);

	// Register Editor handler (provides /editor/*)
	TSharedPtr<FEditorHandler> EditorHandler = MakeShared<FEditorHandler>();
	RegisterHandler(EditorHandler);

	// Register Python handler (provides /python/*)
	TSharedPtr<FPythonHandler> PythonHandler = MakeShared<FPythonHandler>();
	RegisterHandler(PythonHandler);

	// Register Materials handler (provides /materials/*)
	TSharedPtr<FMaterialsHandler> MaterialsHandler = MakeShared<FMaterialsHandler>();
	RegisterHandler(MaterialsHandler);

	// Register Blueprints handler (provides /blueprints/*)
	TSharedPtr<FBlueprintsHandler> BlueprintsHandler = MakeShared<FBlueprintsHandler>();
	RegisterHandler(BlueprintsHandler);

	// Write discovery config file for clients
	if (!FConfigWriter::WriteConfig(*Router))
	{
		UE_LOG(LogUnrealPythonREST, Warning, TEXT("Failed to write discovery config file"));
	}

	UE_LOG(LogUnrealPythonREST, Log, TEXT("UnrealPythonREST started on port %d"), Router->GetPort());
}

void FUnrealPythonRESTModule::ShutdownModule()
{
	// Delete discovery config file
	FConfigWriter::DeleteConfig();

	// Shutdown all handlers
	for (const TSharedPtr<IRESTHandler>& Handler : Handlers)
	{
		if (Handler.IsValid())
		{
			Handler->Shutdown();
		}
	}
	Handlers.Empty();

	// Stop the router
	if (Router.IsValid())
	{
		Router->Stop();
		Router.Reset();
	}

	UE_LOG(LogUnrealPythonREST, Log, TEXT("UnrealPythonREST shutdown complete"));
}

FUnrealPythonRESTModule& FUnrealPythonRESTModule::Get()
{
	return FModuleManager::LoadModuleChecked<FUnrealPythonRESTModule>("UnrealPythonREST");
}

bool FUnrealPythonRESTModule::IsAvailable()
{
	return FModuleManager::Get().IsModuleLoaded("UnrealPythonREST");
}

void FUnrealPythonRESTModule::RegisterHandler(TSharedPtr<IRESTHandler> Handler)
{
	if (!Handler.IsValid())
	{
		UE_LOG(LogUnrealPythonREST, Warning, TEXT("Attempted to register invalid handler"));
		return;
	}

	if (!Router.IsValid())
	{
		UE_LOG(LogUnrealPythonREST, Error, TEXT("Cannot register handler: Router is not initialized"));
		return;
	}

	// Add to module's handlers array for shutdown management
	Handlers.Add(Handler);

	// Register the handler with the router (this also registers its routes)
	Router->RegisterHandler(Handler);

	UE_LOG(LogUnrealPythonREST, Log, TEXT("Registered handler: %s"), *Handler->GetHandlerName());
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealPythonRESTModule, UnrealPythonREST)
