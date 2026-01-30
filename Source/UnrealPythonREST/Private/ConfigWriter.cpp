// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConfigWriter.h"
#include "RESTRouter.h"
#include "IRESTHandler.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FString FConfigWriter::GetConfigPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("UnrealPythonREST.json"));
}

bool FConfigWriter::WriteConfig(const FRESTRouter& Router)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	// Version for future compatibility
	Json->SetNumberField(TEXT("version"), 1);

	// Server connection info
	Json->SetNumberField(TEXT("port"), Router.GetPort());
	Json->SetNumberField(TEXT("pid"), FPlatformProcess::GetCurrentProcessId());

	// Project info
	Json->SetStringField(TEXT("project"), FApp::GetProjectName());
	Json->SetStringField(TEXT("started_at"), FDateTime::UtcNow().ToIso8601());

	// List of registered handlers
	TArray<TSharedPtr<FJsonValue>> HandlerArray;
	for (const auto& Handler : Router.GetHandlers())
	{
		if (Handler.IsValid())
		{
			HandlerArray.Add(MakeShared<FJsonValueString>(Handler->GetHandlerName()));
		}
	}
	Json->SetArrayField(TEXT("handlers"), HandlerArray);

	// Serialize to string
	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Json.ToSharedRef(), Writer);

	// Write to file
	FString ConfigPath = GetConfigPath();
	if (FFileHelper::SaveStringToFile(JsonString, *ConfigPath))
	{
		UE_LOG(LogTemp, Log, TEXT("ConfigWriter: Wrote config to %s"), *ConfigPath);
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("ConfigWriter: Failed to write config to %s"), *ConfigPath);
	return false;
}

bool FConfigWriter::DeleteConfig()
{
	FString ConfigPath = GetConfigPath();

	if (FPaths::FileExists(ConfigPath))
	{
		if (IFileManager::Get().Delete(*ConfigPath))
		{
			UE_LOG(LogTemp, Log, TEXT("ConfigWriter: Deleted config %s"), *ConfigPath);
			return true;
		}

		UE_LOG(LogTemp, Warning, TEXT("ConfigWriter: Failed to delete config %s"), *ConfigPath);
		return false;
	}

	// File doesn't exist, nothing to delete
	return true;
}
