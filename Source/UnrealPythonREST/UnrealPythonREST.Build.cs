using UnrealBuildTool;

public class UnrealPythonREST : ModuleRules
{
    public UnrealPythonREST(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "HTTP",
            "HTTPServer",
            "Json",
            "JsonUtilities",
            "XmlParser",
            "RHI"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "PythonScriptPlugin",
            "Slate",
            "SlateCore",
            "DeveloperSettings",
            "AssetRegistry"
        });

        // Editor-only dependencies for InfrastructureHandler and future handlers
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "UnrealEd",
                "BlueprintGraph",
                "Kismet",
                "MaterialEditor",
                "AssetTools",
                "EditorFramework"
            });

            // Live Coding is Windows-only
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDependencyModuleNames.Add("LiveCoding");
            }
        }
    }
}
