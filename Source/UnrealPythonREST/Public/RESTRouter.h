// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpServerResponse.h"

class IRESTHandler;

/** HTTP method types */
enum class ERESTMethod : uint8
{
    GET,
    POST,
    PUT,
    DELETE
};

/** Request context passed to route handlers */
struct FRESTRequest
{
    FString Path;
    ERESTMethod Method;
    TMap<FString, FString> QueryParams;
    TMap<FString, FString> PathParams;
    FString Body;
    TSharedPtr<FJsonObject> JsonBody;
};

/** Response builder */
struct FRESTResponse
{
    int32 StatusCode = 200;
    TSharedPtr<FJsonObject> JsonBody;
    FString RawBody;

    static FRESTResponse Ok(TSharedPtr<FJsonObject> Json);
    static FRESTResponse Error(int32 Code, const FString& ErrorCode, const FString& Message);
    static FRESTResponse NotFound(const FString& Message = TEXT("Not found"));
    static FRESTResponse BadRequest(const FString& Message);
    static FRESTResponse ServerError(const FString& Message);
};

/** Route handler callback */
DECLARE_DELEGATE_RetVal_OneParam(FRESTResponse, FRESTRouteHandler, const FRESTRequest&);

/**
 * REST Router - manages HTTP server and route dispatching.
 *
 * Handles incoming HTTP requests, parses them into FRESTRequest objects,
 * dispatches to registered route handlers, and converts responses back
 * to HTTP format.
 */
class UNREALPYTHONREST_API FRESTRouter : public TSharedFromThis<FRESTRouter>
{
public:
    FRESTRouter();
    ~FRESTRouter();

    /** Start the HTTP server on specified port */
    bool Start(int32 Port = 8080);

    /** Stop the HTTP server */
    void Stop();

    /** Check if server is running */
    bool IsRunning() const { return bIsRunning; }

    /** Get the current port */
    int32 GetPort() const { return CurrentPort; }

    /** Register a route with a handler callback */
    void RegisterRoute(ERESTMethod Method, const FString& Path, FRESTRouteHandler Handler);

    /** Register a handler (calls handler's RegisterRoutes) */
    void RegisterHandler(TSharedPtr<IRESTHandler> Handler);

    /** Get list of registered handlers */
    const TArray<TSharedPtr<IRESTHandler>>& GetHandlers() const { return RegisteredHandlers; }

    /** Dispatch a request internally (for batch operations) */
    FRESTResponse DispatchInternal(const FRESTRequest& Request);

private:
    /** Handle incoming HTTP request */
    bool HandleRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

    /** Parse request into FRESTRequest */
    FRESTRequest ParseRequest(const FHttpServerRequest& Request);

    /** Convert response to HTTP response */
    TUniquePtr<FHttpServerResponse> BuildResponse(const FRESTResponse& Response);

    /** Route table: Method+Path -> Handler */
    TMap<FString, FRESTRouteHandler> Routes;

    /** Registered handlers */
    TArray<TSharedPtr<IRESTHandler>> RegisteredHandlers;

    /** HTTP router from engine */
    TSharedPtr<IHttpRouter> HttpRouter;

    /** Server state */
    bool bIsRunning = false;
    int32 CurrentPort = 0;

    /** Route handle for cleanup */
    FHttpRouteHandle RouteHandle;
};
