// Copyright Epic Games, Inc. All Rights Reserved.

#include "RESTRouter.h"
#include "IRESTHandler.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Static factory methods for FRESTResponse

FRESTResponse FRESTResponse::Ok(TSharedPtr<FJsonObject> Json)
{
	FRESTResponse Response;
	Response.StatusCode = 200;
	Response.JsonBody = Json;
	return Response;
}

FRESTResponse FRESTResponse::Error(int32 Code, const FString& ErrorCode, const FString& Message)
{
	FRESTResponse Response;
	Response.StatusCode = Code;
	Response.JsonBody = MakeShared<FJsonObject>();
	Response.JsonBody->SetBoolField(TEXT("success"), false);
	Response.JsonBody->SetStringField(TEXT("error"), ErrorCode);
	Response.JsonBody->SetStringField(TEXT("message"), Message);
	return Response;
}

FRESTResponse FRESTResponse::NotFound(const FString& Message)
{
	return Error(404, TEXT("NOT_FOUND"), Message);
}

FRESTResponse FRESTResponse::BadRequest(const FString& Message)
{
	return Error(400, TEXT("BAD_REQUEST"), Message);
}

FRESTResponse FRESTResponse::ServerError(const FString& Message)
{
	return Error(500, TEXT("SERVER_ERROR"), Message);
}

// FRESTRouter implementation

FRESTRouter::FRESTRouter()
	: bIsRunning(false)
	, CurrentPort(0)
{
}

FRESTRouter::~FRESTRouter()
{
	Stop();
}

bool FRESTRouter::Start(int32 Port)
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("RESTRouter: Server already running on port %d"), CurrentPort);
		return false;
	}

	// Get the HTTP router from the module
	FHttpServerModule& HttpServerModule = FHttpServerModule::Get();
	HttpRouter = HttpServerModule.GetHttpRouter(Port);

	if (!HttpRouter.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RESTRouter: Failed to get HTTP router for port %d"), Port);
		return false;
	}

	// Bind catch-all route at /api/v1/*
	FHttpPath ApiPath(TEXT("/api/v1"));

	RouteHandle = HttpRouter->BindRoute(
		ApiPath,
		EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_POST |
		EHttpServerRequestVerbs::VERB_PUT | EHttpServerRequestVerbs::VERB_DELETE,
		FHttpRequestHandler::CreateRaw(this, &FRESTRouter::HandleRequest)
	);

	if (!RouteHandle.IsValid())
	{
		UE_LOG(LogTemp, Error, TEXT("RESTRouter: Failed to bind route at /api/v1"));
		HttpRouter.Reset();
		return false;
	}

	// Register built-in endpoints
	RegisterRoute(ERESTMethod::GET, TEXT("/health"), FRESTRouteHandler::CreateLambda(
		[this](const FRESTRequest& Request) -> FRESTResponse
		{
			TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetBoolField(TEXT("success"), true);
			Json->SetStringField(TEXT("status"), TEXT("running"));
			Json->SetNumberField(TEXT("port"), CurrentPort);

			TArray<TSharedPtr<FJsonValue>> HandlersArray;
			for (const TSharedPtr<IRESTHandler>& Handler : RegisteredHandlers)
			{
				if (Handler.IsValid())
				{
					HandlersArray.Add(MakeShared<FJsonValueString>(Handler->GetHandlerName()));
				}
			}
			Json->SetArrayField(TEXT("handlers"), HandlersArray);

			return FRESTResponse::Ok(Json);
		}
	));

	RegisterRoute(ERESTMethod::GET, TEXT("/handlers"), FRESTRouteHandler::CreateLambda(
		[this](const FRESTRequest& Request) -> FRESTResponse
		{
			TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
			Json->SetBoolField(TEXT("success"), true);

			TArray<TSharedPtr<FJsonValue>> HandlersArray;
			for (const TSharedPtr<IRESTHandler>& Handler : RegisteredHandlers)
			{
				if (Handler.IsValid())
				{
					TSharedPtr<FJsonObject> HandlerJson = MakeShared<FJsonObject>();
					HandlerJson->SetStringField(TEXT("name"), Handler->GetHandlerName());
					HandlerJson->SetStringField(TEXT("path"), Handler->GetBasePath());
					HandlerJson->SetStringField(TEXT("description"), Handler->GetDescription());
					HandlersArray.Add(MakeShared<FJsonValueObject>(HandlerJson));
				}
			}
			Json->SetArrayField(TEXT("handlers"), HandlersArray);

			return FRESTResponse::Ok(Json);
		}
	));

	// Start the HTTP listener
	HttpServerModule.StartAllListeners();

	CurrentPort = Port;
	bIsRunning = true;

	UE_LOG(LogTemp, Log, TEXT("RESTRouter: Started on port %d"), Port);
	return true;
}

void FRESTRouter::Stop()
{
	if (!bIsRunning)
	{
		return;
	}

	// Shutdown all registered handlers
	for (TSharedPtr<IRESTHandler>& Handler : RegisteredHandlers)
	{
		if (Handler.IsValid())
		{
			Handler->Shutdown();
		}
	}

	// Unbind the route
	if (HttpRouter.IsValid() && RouteHandle.IsValid())
	{
		HttpRouter->UnbindRoute(RouteHandle);
	}

	// Clear state
	Routes.Empty();
	RegisteredHandlers.Empty();
	HttpRouter.Reset();
	RouteHandle.Reset();
	bIsRunning = false;
	CurrentPort = 0;

	UE_LOG(LogTemp, Log, TEXT("RESTRouter: Stopped"));
}

void FRESTRouter::RegisterRoute(ERESTMethod Method, const FString& Path, FRESTRouteHandler Handler)
{
	// Build route key as "METHOD:path"
	FString MethodString;
	switch (Method)
	{
	case ERESTMethod::GET:
		MethodString = TEXT("GET");
		break;
	case ERESTMethod::POST:
		MethodString = TEXT("POST");
		break;
	case ERESTMethod::PUT:
		MethodString = TEXT("PUT");
		break;
	case ERESTMethod::DELETE:
		MethodString = TEXT("DELETE");
		break;
	}

	FString RouteKey = FString::Printf(TEXT("%s:%s"), *MethodString, *Path);
	Routes.Add(RouteKey, MoveTemp(Handler));

	UE_LOG(LogTemp, Verbose, TEXT("RESTRouter: Registered route %s"), *RouteKey);
}

void FRESTRouter::RegisterHandler(TSharedPtr<IRESTHandler> Handler)
{
	if (!Handler.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("RESTRouter: Attempted to register invalid handler"));
		return;
	}

	RegisteredHandlers.Add(Handler);
	Handler->RegisterRoutes(*this);

	UE_LOG(LogTemp, Log, TEXT("RESTRouter: Registered handler '%s' at '%s'"),
		*Handler->GetHandlerName(), *Handler->GetBasePath());
}

FRESTResponse FRESTRouter::DispatchInternal(const FRESTRequest& Request)
{
	// Build route key as "METHOD:path"
	FString MethodStr;
	switch (Request.Method)
	{
	case ERESTMethod::GET:
		MethodStr = TEXT("GET");
		break;
	case ERESTMethod::POST:
		MethodStr = TEXT("POST");
		break;
	case ERESTMethod::PUT:
		MethodStr = TEXT("PUT");
		break;
	case ERESTMethod::DELETE:
		MethodStr = TEXT("DELETE");
		break;
	}

	FString RouteKey = FString::Printf(TEXT("%s:%s"), *MethodStr, *Request.Path);

	// Find handler
	FRESTRouteHandler* Handler = Routes.Find(RouteKey);
	if (!Handler)
	{
		return FRESTResponse::NotFound(FString::Printf(TEXT("Route not found: %s"), *RouteKey));
	}

	// Check if handler is bound
	if (!Handler->IsBound())
	{
		return FRESTResponse::ServerError(TEXT("Route handler not bound"));
	}

	// Execute handler
	return Handler->Execute(Request);
}

bool FRESTRouter::HandleRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	// Parse the incoming request
	FRESTRequest ParsedRequest = ParseRequest(Request);

	// Build route key to find handler
	FString MethodString;
	switch (ParsedRequest.Method)
	{
	case ERESTMethod::GET:
		MethodString = TEXT("GET");
		break;
	case ERESTMethod::POST:
		MethodString = TEXT("POST");
		break;
	case ERESTMethod::PUT:
		MethodString = TEXT("PUT");
		break;
	case ERESTMethod::DELETE:
		MethodString = TEXT("DELETE");
		break;
	}

	FString RouteKey = FString::Printf(TEXT("%s:%s"), *MethodString, *ParsedRequest.Path);

	// Find and execute the route handler
	FRESTResponse Response;
	if (FRESTRouteHandler* Handler = Routes.Find(RouteKey))
	{
		if (Handler->IsBound())
		{
			Response = Handler->Execute(ParsedRequest);
		}
		else
		{
			Response = FRESTResponse::ServerError(TEXT("Route handler not bound"));
		}
	}
	else
	{
		Response = FRESTResponse::NotFound(FString::Printf(TEXT("No handler for %s"), *RouteKey));
	}

	// Build and send HTTP response
	TUniquePtr<FHttpServerResponse> HttpResponse = BuildResponse(Response);
	OnComplete(MoveTemp(HttpResponse));

	return true;
}

FRESTRequest FRESTRouter::ParseRequest(const FHttpServerRequest& Request)
{
	FRESTRequest ParsedRequest;

	// Extract path and remove /api/v1 prefix
	FString FullPath = Request.RelativePath.GetPath();

	// Remove leading /api/v1 prefix
	const FString ApiPrefix = TEXT("/api/v1");
	if (FullPath.StartsWith(ApiPrefix))
	{
		ParsedRequest.Path = FullPath.RightChop(ApiPrefix.Len());
	}
	else
	{
		ParsedRequest.Path = FullPath;
	}

	// Ensure path starts with /
	if (!ParsedRequest.Path.StartsWith(TEXT("/")))
	{
		ParsedRequest.Path = TEXT("/") + ParsedRequest.Path;
	}

	// If path is empty, default to root
	if (ParsedRequest.Path.IsEmpty())
	{
		ParsedRequest.Path = TEXT("/");
	}

	// Parse HTTP method
	EHttpServerRequestVerbs Verb = Request.Verb;
	if (EnumHasAnyFlags(Verb, EHttpServerRequestVerbs::VERB_GET))
	{
		ParsedRequest.Method = ERESTMethod::GET;
	}
	else if (EnumHasAnyFlags(Verb, EHttpServerRequestVerbs::VERB_POST))
	{
		ParsedRequest.Method = ERESTMethod::POST;
	}
	else if (EnumHasAnyFlags(Verb, EHttpServerRequestVerbs::VERB_PUT))
	{
		ParsedRequest.Method = ERESTMethod::PUT;
	}
	else if (EnumHasAnyFlags(Verb, EHttpServerRequestVerbs::VERB_DELETE))
	{
		ParsedRequest.Method = ERESTMethod::DELETE;
	}
	else
	{
		// Default to GET for unknown methods
		ParsedRequest.Method = ERESTMethod::GET;
	}

	// Parse query parameters
	for (const auto& Param : Request.QueryParams)
	{
		ParsedRequest.QueryParams.Add(Param.Key, Param.Value);
	}

	// Parse body
	if (Request.Body.Num() > 0)
	{
		// Convert body bytes to string
		ParsedRequest.Body = FString(ANSI_TO_TCHAR(reinterpret_cast<const char*>(Request.Body.GetData())));
		ParsedRequest.Body = ParsedRequest.Body.Left(Request.Body.Num());

		// Try to deserialize as JSON
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(ParsedRequest.Body);
		TSharedPtr<FJsonObject> JsonObject;
		if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
		{
			ParsedRequest.JsonBody = JsonObject;
		}
	}

	return ParsedRequest;
}

TUniquePtr<FHttpServerResponse> FRESTRouter::BuildResponse(const FRESTResponse& Response)
{
	FString ResponseBody;

	// Serialize JSON to string
	if (Response.JsonBody.IsValid())
	{
		TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&ResponseBody);
		FJsonSerializer::Serialize(Response.JsonBody.ToSharedRef(), JsonWriter);
	}
	else if (!Response.RawBody.IsEmpty())
	{
		ResponseBody = Response.RawBody;
	}
	else
	{
		// Empty response body
		ResponseBody = TEXT("{}");
	}

	// Create HTTP response
	TUniquePtr<FHttpServerResponse> HttpResponse = FHttpServerResponse::Create(ResponseBody, TEXT("application/json"));

	// Note: FHttpServerResponse::Create sets a 200 OK status by default
	// The actual status code handling depends on UE version - newer versions support custom status codes

	return HttpResponse;
}
