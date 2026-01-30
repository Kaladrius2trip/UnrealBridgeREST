#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FRESTRouter;
class IRESTHandler;

class FUnrealPythonRESTModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static FUnrealPythonRESTModule& Get();
    static bool IsAvailable();

    TSharedPtr<FRESTRouter> GetRouter() const { return Router; }
    void RegisterHandler(TSharedPtr<IRESTHandler> Handler);

private:
    TSharedPtr<FRESTRouter> Router;
    TArray<TSharedPtr<IRESTHandler>> Handlers;
};
