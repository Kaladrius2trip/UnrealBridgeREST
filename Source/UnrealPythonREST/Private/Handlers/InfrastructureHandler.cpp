// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/InfrastructureHandler.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Internationalization/Regex.h"

void FInfrastructureHandler::RegisterRoutes(FRESTRouter& Router)
{
	RouterRef = &Router;

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/health"),
		FRESTRouteHandler::CreateRaw(this, &FInfrastructureHandler::HandleHealth));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/schema"),
		FRESTRouteHandler::CreateRaw(this, &FInfrastructureHandler::HandleSchema));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/batch"),
		FRESTRouteHandler::CreateRaw(this, &FInfrastructureHandler::HandleBatch));

	UE_LOG(LogTemp, Log, TEXT("InfrastructureHandler: Registered /health, /schema, and /batch"));
}

FRESTResponse FInfrastructureHandler::HandleHealth(const FRESTRequest& Request)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("healthy"), true);
	Response->SetStringField(TEXT("status"), TEXT("running"));

	// Server info
	TSharedPtr<FJsonObject> Server = MakeShared<FJsonObject>();
	Server->SetStringField(TEXT("name"), TEXT("UnrealPythonREST"));
	Server->SetStringField(TEXT("version"), TEXT("2.0.0"));
	Response->SetObjectField(TEXT("server"), Server);

	// Engine info
	TSharedPtr<FJsonObject> Engine = MakeShared<FJsonObject>();
	Engine->SetStringField(TEXT("version"), FEngineVersion::Current().ToString());
	Engine->SetStringField(TEXT("project"), FApp::GetProjectName());
	Response->SetObjectField(TEXT("engine"), Engine);

	// Units documentation
	TSharedPtr<FJsonObject> Units = MakeShared<FJsonObject>();
	Units->SetStringField(TEXT("distance"), TEXT("centimeters"));
	Units->SetStringField(TEXT("rotation"), TEXT("degrees"));
	Units->SetStringField(TEXT("scale"), TEXT("multiplier (1.0 = normal)"));
	Response->SetObjectField(TEXT("units"), Units);

	// Coordinate system
	TSharedPtr<FJsonObject> Coords = MakeShared<FJsonObject>();
	Coords->SetStringField(TEXT("handedness"), TEXT("left-handed"));
	Coords->SetStringField(TEXT("x"), TEXT("forward (red)"));
	Coords->SetStringField(TEXT("y"), TEXT("right (green)"));
	Coords->SetStringField(TEXT("z"), TEXT("up (blue)"));
	Response->SetObjectField(TEXT("coordinate_system"), Coords);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FInfrastructureHandler::HandleSchema(const FRESTRequest& Request)
{
	if (!RouterRef)
	{
		return FRESTResponse::ServerError(TEXT("Router not available"));
	}

	// Check for handler filter first
	const FString* HandlerParam = Request.QueryParams.Find(TEXT("handler"));
	if (HandlerParam && !HandlerParam->IsEmpty())
	{
		TSharedPtr<FJsonObject> Schema = BuildHandlerSchema(*HandlerParam);
		return FRESTResponse::Ok(Schema);
	}

	// Check for endpoint filter
	const FString* EndpointParam = Request.QueryParams.Find(TEXT("endpoint"));
	if (EndpointParam && !EndpointParam->IsEmpty())
	{
		TSharedPtr<FJsonObject> Schema = BuildEndpointSchema(*EndpointParam);
		return FRESTResponse::Ok(Schema);
	}

	// Default: full schema
	TSharedPtr<FJsonObject> Schema = BuildFullSchema();
	return FRESTResponse::Ok(Schema);
}

TSharedPtr<FJsonObject> FInfrastructureHandler::BuildSchema(const TArray<TSharedPtr<IRESTHandler>>& Handlers)
{
	TSharedPtr<FJsonObject> Schema = MakeShared<FJsonObject>();
	Schema->SetStringField(TEXT("api_version"), TEXT("v1"));
	Schema->SetStringField(TEXT("base_path"), TEXT("/api/v1"));

	// Document units
	TSharedPtr<FJsonObject> Units = MakeShared<FJsonObject>();
	Units->SetStringField(TEXT("distance"), TEXT("centimeters (100 = 1 meter)"));
	Units->SetStringField(TEXT("rotation"), TEXT("degrees (90 = quarter turn)"));
	Units->SetStringField(TEXT("scale"), TEXT("multiplier (1.0 = normal size)"));
	Schema->SetObjectField(TEXT("units"), Units);

	// Error codes
	TSharedPtr<FJsonObject> ErrorCodes = MakeShared<FJsonObject>();
	ErrorCodes->SetStringField(TEXT("INVALID_PARAMS"), TEXT("400 - Missing or malformed parameters"));
	ErrorCodes->SetStringField(TEXT("ASSET_NOT_FOUND"), TEXT("404 - Asset path doesn't exist"));
	ErrorCodes->SetStringField(TEXT("ACTOR_NOT_FOUND"), TEXT("404 - Actor label not in level"));
	ErrorCodes->SetStringField(TEXT("CLASS_NOT_FOUND"), TEXT("404 - Class path invalid"));
	ErrorCodes->SetStringField(TEXT("NO_LEVEL_LOADED"), TEXT("400 - No level currently open"));
	ErrorCodes->SetStringField(TEXT("EXECUTION_ERROR"), TEXT("500 - Runtime error"));
	Schema->SetObjectField(TEXT("error_codes"), ErrorCodes);

	// List handlers
	TArray<TSharedPtr<FJsonValue>> HandlersArray;
	for (const TSharedPtr<IRESTHandler>& Handler : Handlers)
	{
		if (Handler.IsValid())
		{
			TSharedPtr<FJsonObject> HandlerJson = MakeShared<FJsonObject>();
			HandlerJson->SetStringField(TEXT("name"), Handler->GetHandlerName());
			HandlerJson->SetStringField(TEXT("base_path"), Handler->GetBasePath());
			HandlerJson->SetStringField(TEXT("description"), Handler->GetDescription());
			HandlersArray.Add(MakeShared<FJsonValueObject>(HandlerJson));
		}
	}
	Schema->SetArrayField(TEXT("handlers"), HandlersArray);

	return Schema;
}

TSharedPtr<FJsonObject> FInfrastructureHandler::BuildFullSchema()
{
	TSharedPtr<FJsonObject> Schema = BuildSchema(RouterRef->GetHandlers());

	// Add endpoint details for each handler
	TArray<TSharedPtr<FJsonValue>> HandlersArray;
	for (const TSharedPtr<IRESTHandler>& Handler : RouterRef->GetHandlers())
	{
		if (Handler.IsValid())
		{
			TSharedPtr<FJsonObject> HandlerJson = MakeShared<FJsonObject>();
			HandlerJson->SetStringField(TEXT("name"), Handler->GetHandlerName());
			HandlerJson->SetStringField(TEXT("base_path"), Handler->GetBasePath());
			HandlerJson->SetStringField(TEXT("description"), Handler->GetDescription());

			// Add endpoint schemas
			TArray<TSharedPtr<FJsonObject>> EndpointSchemas = Handler->GetEndpointSchemas();
			TArray<TSharedPtr<FJsonValue>> EndpointsArray;
			for (const TSharedPtr<FJsonObject>& Endpoint : EndpointSchemas)
			{
				EndpointsArray.Add(MakeShared<FJsonValueObject>(Endpoint));
			}
			HandlerJson->SetArrayField(TEXT("endpoints"), EndpointsArray);

			HandlersArray.Add(MakeShared<FJsonValueObject>(HandlerJson));
		}
	}
	Schema->SetArrayField(TEXT("handlers"), HandlersArray);

	return Schema;
}

TSharedPtr<FJsonObject> FInfrastructureHandler::BuildHandlerSchema(const FString& HandlerName)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	TSharedPtr<IRESTHandler> Handler = FindHandlerByName(HandlerName);
	if (!Handler.IsValid())
	{
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Handler '%s' not found"), *HandlerName));

		// List available handlers
		TArray<TSharedPtr<FJsonValue>> AvailableHandlers;
		for (const TSharedPtr<IRESTHandler>& H : RouterRef->GetHandlers())
		{
			if (H.IsValid())
			{
				AvailableHandlers.Add(MakeShared<FJsonValueString>(H->GetHandlerName()));
			}
		}
		Response->SetArrayField(TEXT("available_handlers"), AvailableHandlers);
		return Response;
	}

	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("name"), Handler->GetHandlerName());
	Response->SetStringField(TEXT("base_path"), Handler->GetBasePath());
	Response->SetStringField(TEXT("description"), Handler->GetDescription());

	// Add endpoint schemas
	TArray<TSharedPtr<FJsonObject>> EndpointSchemas = Handler->GetEndpointSchemas();
	TArray<TSharedPtr<FJsonValue>> EndpointsArray;
	for (const TSharedPtr<FJsonObject>& Endpoint : EndpointSchemas)
	{
		EndpointsArray.Add(MakeShared<FJsonValueObject>(Endpoint));
	}
	Response->SetArrayField(TEXT("endpoints"), EndpointsArray);

	return Response;
}

TSharedPtr<FJsonObject> FInfrastructureHandler::BuildEndpointSchema(const FString& EndpointPath)
{
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();

	// Search all handlers for matching endpoint
	for (const TSharedPtr<IRESTHandler>& Handler : RouterRef->GetHandlers())
	{
		if (!Handler.IsValid())
		{
			continue;
		}

		TArray<TSharedPtr<FJsonObject>> EndpointSchemas = Handler->GetEndpointSchemas();
		for (const TSharedPtr<FJsonObject>& Endpoint : EndpointSchemas)
		{
			FString Path;
			if (Endpoint->TryGetStringField(TEXT("path"), Path))
			{
				// Match exact path or with leading slash normalization
				if (Path.Equals(EndpointPath, ESearchCase::IgnoreCase) ||
					(TEXT("/") + Path).Equals(EndpointPath, ESearchCase::IgnoreCase) ||
					Path.Equals(TEXT("/") + EndpointPath, ESearchCase::IgnoreCase))
				{
					Response->SetBoolField(TEXT("success"), true);
					Response->SetStringField(TEXT("handler"), Handler->GetHandlerName());

					// Copy all fields from the endpoint schema
					for (const auto& Field : Endpoint->Values)
					{
						Response->SetField(Field.Key, Field.Value);
					}
					return Response;
				}
			}
		}
	}

	// Endpoint not found
	Response->SetBoolField(TEXT("success"), false);
	Response->SetStringField(TEXT("error"), FString::Printf(TEXT("Endpoint '%s' not found"), *EndpointPath));

	// List all available endpoints
	TArray<TSharedPtr<FJsonValue>> AvailableEndpoints;
	for (const TSharedPtr<IRESTHandler>& Handler : RouterRef->GetHandlers())
	{
		if (!Handler.IsValid())
		{
			continue;
		}

		TArray<TSharedPtr<FJsonObject>> EndpointSchemas = Handler->GetEndpointSchemas();
		for (const TSharedPtr<FJsonObject>& Endpoint : EndpointSchemas)
		{
			FString Path;
			if (Endpoint->TryGetStringField(TEXT("path"), Path))
			{
				AvailableEndpoints.Add(MakeShared<FJsonValueString>(Path));
			}
		}
	}
	Response->SetArrayField(TEXT("available_endpoints"), AvailableEndpoints);

	return Response;
}

TSharedPtr<IRESTHandler> FInfrastructureHandler::FindHandlerByName(const FString& Name)
{
	for (const TSharedPtr<IRESTHandler>& Handler : RouterRef->GetHandlers())
	{
		if (Handler.IsValid() && Handler->GetHandlerName().Equals(Name, ESearchCase::IgnoreCase))
		{
			return Handler;
		}
	}
	return nullptr;
}

TSharedPtr<IRESTHandler> FInfrastructureHandler::FindHandlerByEndpoint(const FString& EndpointPath)
{
	TSharedPtr<IRESTHandler> BestMatch = nullptr;
	int32 BestMatchLength = 0;

	for (const TSharedPtr<IRESTHandler>& Handler : RouterRef->GetHandlers())
	{
		if (!Handler.IsValid())
		{
			continue;
		}

		FString BasePath = Handler->GetBasePath();
		if (EndpointPath.StartsWith(BasePath, ESearchCase::IgnoreCase))
		{
			// Prefer longer (more specific) base paths
			if (BasePath.Len() > BestMatchLength)
			{
				BestMatch = Handler;
				BestMatchLength = BasePath.Len();
			}
		}
	}

	return BestMatch;
}

FRESTResponse FInfrastructureHandler::HandleBatch(const FRESTRequest& Request)
{
	if (!RouterRef)
	{
		return FRESTResponse::ServerError(TEXT("Router not available"));
	}

	// Parse requests array
	const TArray<TSharedPtr<FJsonValue>>* RequestsArray = nullptr;
	if (!Request.JsonBody.IsValid() || !Request.JsonBody->TryGetArrayField(TEXT("requests"), RequestsArray) || !RequestsArray)
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: requests (array)"));
	}

	// Get options
	bool bStopOnError = true;
	if (Request.JsonBody->HasField(TEXT("options")))
	{
		TSharedPtr<FJsonObject> Options = Request.JsonBody->GetObjectField(TEXT("options"));
		if (Options.IsValid() && Options->HasField(TEXT("stop_on_error")))
		{
			bStopOnError = Options->GetBoolField(TEXT("stop_on_error"));
		}
	}

	// Store results for variable resolution
	TArray<TSharedPtr<FJsonObject>> Results;
	int32 Completed = 0;
	int32 Failed = 0;

	for (int32 Index = 0; Index < RequestsArray->Num(); Index++)
	{
		TSharedPtr<FJsonObject> ReqObj = (*RequestsArray)[Index]->AsObject();
		if (!ReqObj.IsValid())
		{
			TSharedPtr<FJsonObject> ErrorResult = MakeShared<FJsonObject>();
			ErrorResult->SetNumberField(TEXT("index"), Index);
			ErrorResult->SetBoolField(TEXT("success"), false);
			ErrorResult->SetStringField(TEXT("error"), TEXT("Invalid request object"));
			Results.Add(ErrorResult);
			Failed++;

			if (bStopOnError)
			{
				break;
			}
			continue;
		}

		// Parse method
		FString MethodStr = ReqObj->GetStringField(TEXT("method")).ToUpper();
		ERESTMethod Method = ERESTMethod::GET;
		if (MethodStr == TEXT("POST"))
		{
			Method = ERESTMethod::POST;
		}
		else if (MethodStr == TEXT("PUT"))
		{
			Method = ERESTMethod::PUT;
		}
		else if (MethodStr == TEXT("DELETE"))
		{
			Method = ERESTMethod::DELETE;
		}

		FString Path = ReqObj->GetStringField(TEXT("path"));

		// Get and resolve body with variable references
		TSharedPtr<FJsonObject> Body;
		if (ReqObj->HasField(TEXT("body")))
		{
			Body = ReqObj->GetObjectField(TEXT("body"));
			Body = ResolveVariableReferences(Body, Results);
		}

		// Build internal request
		FRESTRequest InternalRequest;
		InternalRequest.Method = Method;
		InternalRequest.Path = Path;
		InternalRequest.JsonBody = Body;

		// Dispatch to router
		FRESTResponse InternalResponse = RouterRef->DispatchInternal(InternalRequest);

		// Build result
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("index"), Index);
		Result->SetStringField(TEXT("method"), MethodStr);
		Result->SetStringField(TEXT("path"), Path);
		Result->SetNumberField(TEXT("status"), InternalResponse.StatusCode);
		Result->SetBoolField(TEXT("success"), InternalResponse.StatusCode >= 200 && InternalResponse.StatusCode < 300);

		// Include response body
		if (InternalResponse.JsonBody.IsValid())
		{
			Result->SetObjectField(TEXT("data"), InternalResponse.JsonBody);
		}

		Results.Add(Result);

		if (InternalResponse.StatusCode >= 200 && InternalResponse.StatusCode < 300)
		{
			Completed++;
		}
		else
		{
			Failed++;
			if (bStopOnError)
			{
				break;
			}
		}
	}

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), Failed == 0);

	TArray<TSharedPtr<FJsonValue>> ResultsJsonArray;
	for (const TSharedPtr<FJsonObject>& Result : Results)
	{
		ResultsJsonArray.Add(MakeShared<FJsonValueObject>(Result));
	}
	Response->SetArrayField(TEXT("results"), ResultsJsonArray);
	Response->SetNumberField(TEXT("completed"), Completed);
	Response->SetNumberField(TEXT("failed"), Failed);

	return FRESTResponse::Ok(Response);
}

TSharedPtr<FJsonObject> FInfrastructureHandler::ResolveVariableReferences(
	TSharedPtr<FJsonObject> Body,
	const TArray<TSharedPtr<FJsonObject>>& PreviousResults)
{
	if (!Body.IsValid())
	{
		return Body;
	}

	TSharedPtr<FJsonObject> Resolved = MakeShared<FJsonObject>();

	for (const auto& Field : Body->Values)
	{
		if (Field.Value->Type == EJson::String)
		{
			FString Value = Field.Value->AsString();
			FString ResolvedValue = ResolveStringVariables(Value, PreviousResults);
			Resolved->SetStringField(Field.Key, ResolvedValue);
		}
		else if (Field.Value->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> NestedObj = Field.Value->AsObject();
			Resolved->SetObjectField(Field.Key, ResolveVariableReferences(NestedObj, PreviousResults));
		}
		else
		{
			Resolved->SetField(Field.Key, Field.Value);
		}
	}

	return Resolved;
}

FString FInfrastructureHandler::ResolveStringVariables(
	const FString& Value,
	const TArray<TSharedPtr<FJsonObject>>& PreviousResults)
{
	FString Result = Value;

	// Pattern: $0 or $0.path.to.value
	FRegexPattern Pattern(TEXT("\\$(\\d+)(\\.([\\w\\.]+))?"));
	FRegexMatcher Matcher(Pattern, Value);

	while (Matcher.FindNext())
	{
		FString FullMatch = Matcher.GetCaptureGroup(0);
		int32 Index = FCString::Atoi(*Matcher.GetCaptureGroup(1));
		FString JsonPath = Matcher.GetCaptureGroup(3);

		if (Index >= 0 && Index < PreviousResults.Num())
		{
			TSharedPtr<FJsonObject> ResultData = PreviousResults[Index];
			if (ResultData.IsValid())
			{
				if (JsonPath.IsEmpty())
				{
					// Just $N - can't substitute entire object into string
					continue;
				}

				TSharedPtr<FJsonValue> Extracted = ExtractJsonPath(ResultData, JsonPath);
				if (Extracted.IsValid() && Extracted->Type == EJson::String)
				{
					Result = Result.Replace(*FullMatch, *Extracted->AsString());
				}
				else if (Extracted.IsValid() && Extracted->Type == EJson::Number)
				{
					Result = Result.Replace(*FullMatch, *FString::Printf(TEXT("%g"), Extracted->AsNumber()));
				}
			}
		}
	}

	return Result;
}

TSharedPtr<FJsonValue> FInfrastructureHandler::ExtractJsonPath(
	TSharedPtr<FJsonObject> Root,
	const FString& Path)
{
	TArray<FString> Parts;
	Path.ParseIntoArray(Parts, TEXT("."));

	TSharedPtr<FJsonObject> Current = Root;
	for (int32 i = 0; i < Parts.Num() - 1; i++)
	{
		if (!Current.IsValid() || !Current->HasField(Parts[i]))
		{
			return nullptr;
		}
		Current = Current->GetObjectField(Parts[i]);
	}

	if (Parts.Num() > 0 && Current.IsValid() && Current->HasField(Parts.Last()))
	{
		return Current->TryGetField(Parts.Last());
	}

	return nullptr;
}
