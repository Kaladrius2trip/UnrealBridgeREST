// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/MaterialsHandler.h"
#include "Utils/JsonHelpers.h"
#include "Utils/ActorUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Components/PrimitiveComponent.h"
#include "Components/MeshComponent.h"
#include "EngineUtils.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "Editor.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "MaterialGraph/MaterialGraphNode_Root.h"
#include "IMaterialEditor.h"
#include "MaterialEditorUtilities.h"
#include "MaterialEditingLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialShared.h"
#include "Materials/MaterialInterface.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialFunctionFactoryNew.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionCustom.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "XmlFile.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"

// Forward declaration - defined after RegisterRoutes/GetEndpointSchemas
static void SaveAssetIfRequested(UObject* Asset, bool bShouldSave);

void FMaterialsHandler::RegisterRoutes(FRESTRouter& Router)
{
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/param"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleGetParam));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/param"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleSetParam));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/recompile"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleRecompile));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/replace"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleReplace));

	// Material asset creation
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/create"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateMaterial));

	// Material Instance creation
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/instance/create"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateMaterialInstance));

	// Dynamic Material Instance creation (runtime)
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/instance/dynamic"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateDynamicMaterialInstance));

	// Material Editor open
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/open"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleOpenMaterialEditor));

	// Material Editor node manipulation
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/editor/nodes"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleListMaterialNodes));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/node/position"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleSetMaterialNodePosition));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/node/create"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateMaterialNode));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/connect"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleConnectMaterialNodes));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/editor/status"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleMaterialStatus));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/refresh"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleRefreshEditor));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/expression/set"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleSetExpressionProperty));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/editor/validate"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleValidateGraph));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/disconnect"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleDisconnect));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/editor/connections"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleGetConnections));

	Router.RegisterRoute(ERESTMethod::DELETE, TEXT("/materials/editor/node"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleDeleteExpression));

	// Material Graph XML Serialization
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/editor/export"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleExportGraph));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/editor/import"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleImportGraph));

	// Material Function endpoints
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/create"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateMaterialFunction));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/open"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleOpenMaterialFunctionEditor));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/function/editor/nodes"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleListMaterialFunctionNodes));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/node/create"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleCreateMaterialFunctionNode));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/node/position"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleSetMaterialFunctionNodePosition));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/connect"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleConnectMaterialFunctionNodes));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/disconnect"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleDisconnectMaterialFunction));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/expression/set"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleSetMaterialFunctionExpressionProperty));

	Router.RegisterRoute(ERESTMethod::DELETE, TEXT("/materials/function/editor/node"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleDeleteMaterialFunctionExpression));

	// Material Function Graph XML Serialization
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/materials/function/editor/export"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleExportMaterialFunctionGraph));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/materials/function/editor/import"),
		FRESTRouteHandler::CreateRaw(this, &FMaterialsHandler::HandleImportMaterialFunctionGraph));

	UE_LOG(LogTemp, Log, TEXT("MaterialsHandler: Registered 32 routes at /materials (v4)"));
}

FRESTResponse FMaterialsHandler::HandleGetParam(const FRESTRequest& Request)
{
	// Get required query parameters
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: material_path"));
	}

	const FString* ParamNamePtr = Request.QueryParams.Find(TEXT("param_name"));
	if (!ParamNamePtr || ParamNamePtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: param_name"));
	}

	const FString& MaterialPath = *MaterialPathPtr;
	const FName ParamName = FName(*(*ParamNamePtr));

	// Try to load as MaterialInstanceConstant first (most common for editable materials)
	UMaterialInstanceConstant* MatInstConst = Cast<UMaterialInstanceConstant>(
		StaticLoadObject(UMaterialInstanceConstant::StaticClass(), nullptr, *MaterialPath));

	if (MatInstConst)
	{
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("material_path"), MaterialPath);
		Response->SetStringField(TEXT("param_name"), ParamName.ToString());
		Response->SetStringField(TEXT("material_type"), TEXT("MaterialInstanceConstant"));

		// Try scalar parameter
		float ScalarValue;
		if (MatInstConst->GetScalarParameterValue(ParamName, ScalarValue))
		{
			Response->SetStringField(TEXT("param_type"), TEXT("scalar"));
			Response->SetNumberField(TEXT("value"), ScalarValue);
			return FRESTResponse::Ok(Response);
		}

		// Try vector parameter
		FLinearColor VectorValue;
		if (MatInstConst->GetVectorParameterValue(ParamName, VectorValue))
		{
			Response->SetStringField(TEXT("param_type"), TEXT("vector"));
			TSharedPtr<FJsonObject> ColorJson = MakeShared<FJsonObject>();
			ColorJson->SetNumberField(TEXT("r"), VectorValue.R);
			ColorJson->SetNumberField(TEXT("g"), VectorValue.G);
			ColorJson->SetNumberField(TEXT("b"), VectorValue.B);
			ColorJson->SetNumberField(TEXT("a"), VectorValue.A);
			Response->SetObjectField(TEXT("value"), ColorJson);
			return FRESTResponse::Ok(Response);
		}

		// Try texture parameter
		UTexture* TextureValue;
		if (MatInstConst->GetTextureParameterValue(ParamName, TextureValue))
		{
			Response->SetStringField(TEXT("param_type"), TEXT("texture"));
			Response->SetStringField(TEXT("value"), TextureValue ? TextureValue->GetPathName() : TEXT("None"));
			return FRESTResponse::Ok(Response);
		}

		return FRESTResponse::Error(404, TEXT("PARAM_NOT_FOUND"),
			FString::Printf(TEXT("Parameter '%s' not found in material"), *ParamName.ToString()));
	}

	// Try to load as base Material
	UMaterial* Material = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

	if (Material)
	{
		// For base materials, we can list available parameters but cannot get values
		// as they are defined by expressions, not simple values
		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), false);
		Response->SetStringField(TEXT("material_path"), MaterialPath);
		Response->SetStringField(TEXT("material_type"), TEXT("Material"));
		Response->SetStringField(TEXT("message"),
			TEXT("Base materials define parameters via expressions. Use a MaterialInstance to get/set parameter values."));
		return FRESTResponse::Ok(Response);
	}

	return FRESTResponse::Error(404, TEXT("MATERIAL_NOT_FOUND"),
		FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
}

FRESTResponse FMaterialsHandler::HandleSetParam(const FRESTRequest& Request)
{
	// material_path is optional when using actor_label for MID
	FString MaterialPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("material_path"), TEXT(""));

	FString ParamNameStr;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("param_name"), ParamNameStr, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	const FName ParamName = FName(*ParamNameStr);

	// Check that value field exists
	if (!Request.JsonBody->HasField(TEXT("value")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: value"));
	}

	// Try to load as MaterialInstanceConstant from disk (if material_path provided)
	UMaterialInstanceConstant* MatInstConst = nullptr;
	if (!MaterialPath.IsEmpty())
	{
		MatInstConst = Cast<UMaterialInstanceConstant>(
			StaticLoadObject(UMaterialInstanceConstant::StaticClass(), nullptr, *MaterialPath));
	}

	if (!MatInstConst)
	{
		// Check if actor_label is provided - user wants to modify MID on actor
		FString ActorLabel = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("actor_label"), TEXT(""));

		if (!ActorLabel.IsEmpty())
		{
			// Find actor and get its dynamic material
			AActor* Actor = ActorUtils::FindActorByLabel(ActorLabel);
			if (!Actor)
			{
				return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
					FString::Printf(TEXT("Actor with label '%s' not found"), *ActorLabel));
			}

			int32 MaterialIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("material_index"), 0);

			TArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

			if (PrimitiveComponents.Num() == 0)
			{
				return FRESTResponse::Error(400, TEXT("NO_PRIMITIVE_COMPONENT"),
					FString::Printf(TEXT("Actor '%s' has no primitive components"), *ActorLabel));
			}

			UPrimitiveComponent* PrimComp = PrimitiveComponents[0];
			UMaterialInterface* MatInterface = PrimComp->GetMaterial(MaterialIndex);
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MatInterface);

			if (!MID)
			{
				return FRESTResponse::Error(400, TEXT("NOT_DYNAMIC_MATERIAL"),
					FString::Printf(TEXT("Material at index %d on actor '%s' is not a MaterialInstanceDynamic. Use /materials/instance/dynamic to create one first."), MaterialIndex, *ActorLabel));
			}

			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("actor_label"), ActorLabel);
			Response->SetNumberField(TEXT("material_index"), MaterialIndex);
			Response->SetStringField(TEXT("param_name"), ParamName.ToString());
			Response->SetStringField(TEXT("material_type"), TEXT("MaterialInstanceDynamic"));

			// Determine value type and set parameter
			const TSharedPtr<FJsonValue>& ValueField = Request.JsonBody->Values.FindRef(TEXT("value"));

			if (!ValueField.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Missing or invalid 'value' field"));
			}

			if (ValueField->Type == EJson::Number)
			{
				float NewValue = static_cast<float>(ValueField->AsNumber());
				float OldValue = 0.0f;
				MID->GetScalarParameterValue(ParamName, OldValue);
				MID->SetScalarParameterValue(ParamName, NewValue);

				Response->SetStringField(TEXT("param_type"), TEXT("scalar"));
				Response->SetNumberField(TEXT("old_value"), OldValue);
				Response->SetNumberField(TEXT("new_value"), NewValue);
			}
			else if (ValueField->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
				FLinearColor NewColor;
				NewColor.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
				NewColor.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
				NewColor.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
				NewColor.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;

				FLinearColor OldColor;
				MID->GetVectorParameterValue(ParamName, OldColor);
				MID->SetVectorParameterValue(ParamName, NewColor);

				Response->SetStringField(TEXT("param_type"), TEXT("vector"));

				TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
				OldColorJson->SetNumberField(TEXT("r"), OldColor.R);
				OldColorJson->SetNumberField(TEXT("g"), OldColor.G);
				OldColorJson->SetNumberField(TEXT("b"), OldColor.B);
				OldColorJson->SetNumberField(TEXT("a"), OldColor.A);
				Response->SetObjectField(TEXT("old_value"), OldColorJson);

				TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
				NewColorJson->SetNumberField(TEXT("r"), NewColor.R);
				NewColorJson->SetNumberField(TEXT("g"), NewColor.G);
				NewColorJson->SetNumberField(TEXT("b"), NewColor.B);
				NewColorJson->SetNumberField(TEXT("a"), NewColor.A);
				Response->SetObjectField(TEXT("new_value"), NewColorJson);
			}
			else if (ValueField->Type == EJson::String)
			{
				FString TexturePath = ValueField->AsString();
				UTexture* NewTexture = Cast<UTexture>(
					StaticLoadObject(UTexture::StaticClass(), nullptr, *TexturePath));

				if (!NewTexture && !TexturePath.IsEmpty() && TexturePath != TEXT("None"))
				{
					return FRESTResponse::Error(404, TEXT("TEXTURE_NOT_FOUND"),
						FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
				}

				UTexture* OldTexture = nullptr;
				MID->GetTextureParameterValue(ParamName, OldTexture);
				MID->SetTextureParameterValue(ParamName, NewTexture);

				Response->SetStringField(TEXT("param_type"), TEXT("texture"));
				Response->SetStringField(TEXT("old_value"), OldTexture ? OldTexture->GetPathName() : TEXT("None"));
				Response->SetStringField(TEXT("new_value"), NewTexture ? NewTexture->GetPathName() : TEXT("None"));
			}
			else
			{
				return FRESTResponse::BadRequest(TEXT("Unsupported value type"));
			}

			// Force component to update its render state so changes are visible
			PrimComp->MarkRenderStateDirty();

			return FRESTResponse::Ok(Response);
		}

		// Check if it's a base material (original code)
		UMaterial* Material = Cast<UMaterial>(
			StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

		if (Material)
		{
			return FRESTResponse::Error(400, TEXT("CANNOT_MODIFY_BASE_MATERIAL"),
				TEXT("Cannot set parameters on base Material. Create a MaterialInstance first."));
		}

		return FRESTResponse::Error(404, TEXT("MATERIAL_NOT_FOUND"),
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("param_name"), ParamName.ToString());

	// Determine value type and set parameter
	const TSharedPtr<FJsonValue>& ValueField = Request.JsonBody->Values.FindRef(TEXT("value"));

	if (!ValueField.IsValid())
	{
		return FRESTResponse::BadRequest(TEXT("Invalid value field"));
	}

	// Create scoped transaction for undo support
	FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("Set Material Parameter: %s"), *ParamName.ToString())));

	// Mark object for modification (required for undo)
	MatInstConst->Modify();

	if (ValueField->Type == EJson::Number)
	{
		// Scalar parameter
		float NewValue = static_cast<float>(ValueField->AsNumber());

		// Get old value for response
		float OldValue = 0.0f;
		MatInstConst->GetScalarParameterValue(ParamName, OldValue);

		// Use MaterialEditingLibrary to properly enable override and set value
		UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MatInstConst, ParamName, NewValue);

		Response->SetStringField(TEXT("param_type"), TEXT("scalar"));
		Response->SetNumberField(TEXT("old_value"), OldValue);
		Response->SetNumberField(TEXT("new_value"), NewValue);
	}
	else if (ValueField->Type == EJson::Object)
	{
		// Vector/color parameter
		TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
		FLinearColor NewColor;
		NewColor.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
		NewColor.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
		NewColor.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
		NewColor.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;

		// Get old value
		FLinearColor OldColor;
		MatInstConst->GetVectorParameterValue(ParamName, OldColor);

		// Use MaterialEditingLibrary to properly enable override and set value
		UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MatInstConst, ParamName, NewColor);

		Response->SetStringField(TEXT("param_type"), TEXT("vector"));

		TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
		OldColorJson->SetNumberField(TEXT("r"), OldColor.R);
		OldColorJson->SetNumberField(TEXT("g"), OldColor.G);
		OldColorJson->SetNumberField(TEXT("b"), OldColor.B);
		OldColorJson->SetNumberField(TEXT("a"), OldColor.A);
		Response->SetObjectField(TEXT("old_value"), OldColorJson);

		TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
		NewColorJson->SetNumberField(TEXT("r"), NewColor.R);
		NewColorJson->SetNumberField(TEXT("g"), NewColor.G);
		NewColorJson->SetNumberField(TEXT("b"), NewColor.B);
		NewColorJson->SetNumberField(TEXT("a"), NewColor.A);
		Response->SetObjectField(TEXT("new_value"), NewColorJson);
	}
	else if (ValueField->Type == EJson::String)
	{
		// Texture parameter (path string)
		FString TexturePath = ValueField->AsString();
		UTexture* NewTexture = Cast<UTexture>(
			StaticLoadObject(UTexture::StaticClass(), nullptr, *TexturePath));

		if (!NewTexture && !TexturePath.IsEmpty() && TexturePath != TEXT("None"))
		{
			return FRESTResponse::Error(404, TEXT("TEXTURE_NOT_FOUND"),
				FString::Printf(TEXT("Texture not found: %s"), *TexturePath));
		}

		// Get old texture
		UTexture* OldTexture = nullptr;
		MatInstConst->GetTextureParameterValue(ParamName, OldTexture);

		// Use MaterialEditingLibrary to properly enable override and set value
		UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MatInstConst, ParamName, NewTexture);

		Response->SetStringField(TEXT("param_type"), TEXT("texture"));
		Response->SetStringField(TEXT("old_value"), OldTexture ? OldTexture->GetPathName() : TEXT("None"));
		Response->SetStringField(TEXT("new_value"), NewTexture ? NewTexture->GetPathName() : TEXT("None"));
	}
	else
	{
		return FRESTResponse::BadRequest(TEXT("Unsupported value type. Use number for scalar, object for vector/color, string for texture path."));
	}

	// Trigger visual update and notify listeners
	MatInstConst->PostEditChange();

	// Mark material as modified
	MatInstConst->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(MatInstConst, bSave);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleRecompile(const FRESTRequest& Request)
{
	FString MaterialPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Try to load as Material first
	UMaterial* Material = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

	if (Material)
	{
		// Force recompile for rendering
		Material->ForceRecompileForRendering();

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("material_path"), MaterialPath);
		Response->SetStringField(TEXT("material_type"), TEXT("Material"));
		Response->SetStringField(TEXT("message"), TEXT("Material recompiled for rendering"));

		return FRESTResponse::Ok(Response);
	}

	// Try MaterialInstance
	UMaterialInstance* MatInst = Cast<UMaterialInstance>(
		StaticLoadObject(UMaterialInstance::StaticClass(), nullptr, *MaterialPath));

	if (MatInst)
	{
		// For material instances, we need to get the parent material and recompile that
		UMaterial* ParentMaterial = MatInst->GetMaterial();
		if (ParentMaterial)
		{
			ParentMaterial->ForceRecompileForRendering();

			TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
			Response->SetBoolField(TEXT("success"), true);
			Response->SetStringField(TEXT("material_path"), MaterialPath);
			Response->SetStringField(TEXT("material_type"), TEXT("MaterialInstance"));
			Response->SetStringField(TEXT("parent_material"), ParentMaterial->GetPathName());
			Response->SetStringField(TEXT("message"), TEXT("Parent material recompiled for rendering"));

			return FRESTResponse::Ok(Response);
		}
	}

	return FRESTResponse::Error(404, TEXT("MATERIAL_NOT_FOUND"),
		FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
}

FRESTResponse FMaterialsHandler::HandleReplace(const FRESTRequest& Request)
{
	FString MaterialPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load the material
	UMaterialInterface* NewMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *MaterialPath));

	if (!NewMaterial)
	{
		return FRESTResponse::Error(404, TEXT("MATERIAL_NOT_FOUND"),
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	// Collect target actor labels
	TArray<FString> Labels;

	// Check for single label
	if (Request.JsonBody->HasField(TEXT("label")))
	{
		Labels.Add(Request.JsonBody->GetStringField(TEXT("label")));
	}

	// Check for labels array
	const TArray<TSharedPtr<FJsonValue>>* LabelsArray = nullptr;
	if (Request.JsonBody->TryGetArrayField(TEXT("labels"), LabelsArray) && LabelsArray)
	{
		for (const TSharedPtr<FJsonValue>& LabelValue : *LabelsArray)
		{
			Labels.Add(LabelValue->AsString());
		}
	}

	if (Labels.Num() == 0)
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: label or labels[]"));
	}

	// Get optional material index
	int32 MaterialIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("material_index"), -1);

	TArray<TSharedPtr<FJsonValue>> ResultsArray;
	TArray<FString> NotFoundLabels;

	for (const FString& Label : Labels)
	{
		AActor* Actor = ActorUtils::FindActorByLabel(Label);
		if (!Actor)
		{
			NotFoundLabels.Add(Label);
			continue;
		}

		TSharedPtr<FJsonObject> ActorResult = MakeShared<FJsonObject>();
		ActorResult->SetStringField(TEXT("label"), Label);

		// Find all primitive components (mesh, etc.)
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		int32 MaterialsReplaced = 0;
		TArray<TSharedPtr<FJsonValue>> ReplacedMaterials;

		for (UPrimitiveComponent* PrimComp : PrimitiveComponents)
		{
			if (!PrimComp)
			{
				continue;
			}

			int32 NumMaterials = PrimComp->GetNumMaterials();

			if (MaterialIndex >= 0)
			{
				// Replace specific material index
				if (MaterialIndex < NumMaterials)
				{
					UMaterialInterface* OldMat = PrimComp->GetMaterial(MaterialIndex);
					PrimComp->SetMaterial(MaterialIndex, NewMaterial);

					TSharedPtr<FJsonObject> MatInfo = MakeShared<FJsonObject>();
					MatInfo->SetStringField(TEXT("component"), PrimComp->GetName());
					MatInfo->SetNumberField(TEXT("index"), MaterialIndex);
					MatInfo->SetStringField(TEXT("old_material"), OldMat ? OldMat->GetPathName() : TEXT("None"));
					ReplacedMaterials.Add(MakeShared<FJsonValueObject>(MatInfo));
					MaterialsReplaced++;
				}
			}
			else
			{
				// Replace all materials
				for (int32 i = 0; i < NumMaterials; i++)
				{
					UMaterialInterface* OldMat = PrimComp->GetMaterial(i);
					PrimComp->SetMaterial(i, NewMaterial);

					TSharedPtr<FJsonObject> MatInfo = MakeShared<FJsonObject>();
					MatInfo->SetStringField(TEXT("component"), PrimComp->GetName());
					MatInfo->SetNumberField(TEXT("index"), i);
					MatInfo->SetStringField(TEXT("old_material"), OldMat ? OldMat->GetPathName() : TEXT("None"));
					ReplacedMaterials.Add(MakeShared<FJsonValueObject>(MatInfo));
					MaterialsReplaced++;
				}
			}
		}

		ActorResult->SetNumberField(TEXT("materials_replaced"), MaterialsReplaced);
		ActorResult->SetArrayField(TEXT("replaced"), ReplacedMaterials);
		ResultsArray.Add(MakeShared<FJsonValueObject>(ActorResult));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), NotFoundLabels.Num() == 0);
	Response->SetStringField(TEXT("new_material"), MaterialPath);
	Response->SetArrayField(TEXT("actors"), ResultsArray);
	Response->SetNumberField(TEXT("actors_processed"), ResultsArray.Num());

	if (NotFoundLabels.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> NotFoundArray;
		for (const FString& Label : NotFoundLabels)
		{
			NotFoundArray.Add(MakeShared<FJsonValueString>(Label));
		}
		Response->SetArrayField(TEXT("not_found"), NotFoundArray);

		// Add suggestions for not found actors
		TArray<FString> AllLabels = ActorUtils::GetAllActorLabels();
		TArray<FString> Similar = JsonHelpers::FindSimilarStrings(NotFoundLabels[0], AllLabels);
		if (Similar.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> SuggestionsArray;
			for (const FString& Suggestion : Similar)
			{
				SuggestionsArray.Add(MakeShared<FJsonValueString>(Suggestion));
			}
			Response->SetArrayField(TEXT("suggestions"), SuggestionsArray);
		}
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleCreateMaterial(const FRESTRequest& Request)
{
	// Get required material name
	FString MaterialName;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("name"), MaterialName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get optional path (default to /Game/Materials)
	FString PackagePath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT("/Game/Materials"));

	// Ensure path starts with /Game/
	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		if (PackagePath.StartsWith(TEXT("/")))
		{
			PackagePath = TEXT("/Game") + PackagePath;
		}
		else
		{
			PackagePath = TEXT("/Game/") + PackagePath;
		}
	}

	// Get AssetTools module
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	// Create material factory
	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();

	// Create the material asset
	UObject* NewAsset = AssetTools.CreateAsset(MaterialName, PackagePath, UMaterial::StaticClass(), MaterialFactory);

	if (!NewAsset)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			FString::Printf(TEXT("Failed to create material '%s' at '%s'"), *MaterialName, *PackagePath));
	}

	UMaterial* NewMaterial = Cast<UMaterial>(NewAsset);
	if (!NewMaterial)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Created asset is not a Material"));
	}

	// Mark as dirty so it can be saved
	NewMaterial->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(NewMaterial, bSave);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_name"), MaterialName);
	Response->SetStringField(TEXT("material_path"), NewMaterial->GetPathName());
	Response->SetStringField(TEXT("package_path"), PackagePath);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleCreateMaterialInstance(const FRESTRequest& Request)
{
	// Get required instance name
	FString InstanceName;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("name"), InstanceName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get required parent material path
	FString ParentPath;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("parent_material"), ParentPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load parent material
	UMaterialInterface* ParentMaterial = Cast<UMaterialInterface>(
		StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *ParentPath));

	if (!ParentMaterial)
	{
		return FRESTResponse::Error(404, TEXT("PARENT_NOT_FOUND"),
			FString::Printf(TEXT("Parent material not found: %s"), *ParentPath));
	}

	// Get optional path (default to /Game/Materials/Instances)
	FString PackagePath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT("/Game/Materials/Instances"));

	// Ensure path starts with /Game/
	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		if (PackagePath.StartsWith(TEXT("/")))
		{
			PackagePath = TEXT("/Game") + PackagePath;
		}
		else
		{
			PackagePath = TEXT("/Game/") + PackagePath;
		}
	}

	// Create package for the new instance
	FString PackageName = PackagePath / InstanceName;
	UPackage* Package = CreatePackage(*PackageName);
	if (!Package)
	{
		return FRESTResponse::Error(500, TEXT("PACKAGE_FAILED"),
			FString::Printf(TEXT("Failed to create package: %s"), *PackageName));
	}

	// Create scoped transaction for undo support
	FScopedTransaction Transaction(FText::FromString(FString::Printf(TEXT("Create Material Instance: %s"), *InstanceName)));

	// Create the material instance constant
	UMaterialInstanceConstant* NewInstance = NewObject<UMaterialInstanceConstant>(
		Package, *InstanceName, RF_Public | RF_Standalone);

	if (!NewInstance)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			FString::Printf(TEXT("Failed to create MaterialInstanceConstant: %s"), *InstanceName));
	}

	// Mark object for modification (required for undo)
	NewInstance->Modify();

	// Set the parent material
	NewInstance->SetParentEditorOnly(ParentMaterial);

	// Apply initial parameter overrides if provided
	const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
	if (Request.JsonBody->TryGetObjectField(TEXT("parameters"), ParamsObject) && ParamsObject)
	{
		for (const auto& Param : (*ParamsObject)->Values)
		{
			const FName ParamName = FName(*Param.Key);
			const TSharedPtr<FJsonValue>& Value = Param.Value;

			if (Value->Type == EJson::Number)
			{
				// Scalar parameter - use MaterialEditingLibrary to properly enable override
				UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(NewInstance, ParamName, static_cast<float>(Value->AsNumber()));
			}
			else if (Value->Type == EJson::Object)
			{
				// Vector/color parameter
				TSharedPtr<FJsonObject> ColorJson = Value->AsObject();
				FLinearColor Color;
				Color.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
				Color.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
				Color.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
				Color.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;
				// Use MaterialEditingLibrary to properly enable override
				UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(NewInstance, ParamName, Color);
			}
			else if (Value->Type == EJson::String)
			{
				// Texture parameter
				FString TexturePath = Value->AsString();
				UTexture* Texture = Cast<UTexture>(
					StaticLoadObject(UTexture::StaticClass(), nullptr, *TexturePath));
				if (Texture)
				{
					// Use MaterialEditingLibrary to properly enable override
					UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(NewInstance, ParamName, Texture);
				}
			}
		}
	}

	// Trigger visual update and notify listeners
	NewInstance->PostEditChange();

	// Mark package dirty for saving
	NewInstance->MarkPackageDirty();
	Package->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(NewInstance, bSave);

	// Notify asset registry
	FAssetRegistryModule::AssetCreated(NewInstance);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("instance_name"), InstanceName);
	Response->SetStringField(TEXT("instance_path"), NewInstance->GetPathName());
	Response->SetStringField(TEXT("parent_material"), ParentPath);
	Response->SetStringField(TEXT("package_path"), PackagePath);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleCreateDynamicMaterialInstance(const FRESTRequest& Request)
{
	// Get required actor label
	FString ActorLabel;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("actor_label"), ActorLabel, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Find the actor
	AActor* Actor = ActorUtils::FindActorByLabel(ActorLabel);
	if (!Actor)
	{
		return FRESTResponse::Error(404, TEXT("ACTOR_NOT_FOUND"),
			FString::Printf(TEXT("Actor with label '%s' not found. Use GET /actors/list to see available actors."), *ActorLabel));
	}

	// Get optional material index (default 0)
	int32 MaterialIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("material_index"), 0);

	// Get optional source material (if not provided, uses existing material)
	FString SourceMaterialPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("source_material"), TEXT(""));
	UMaterialInterface* SourceMaterial = nullptr;
	if (!SourceMaterialPath.IsEmpty())
	{
		SourceMaterial = Cast<UMaterialInterface>(
			StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, *SourceMaterialPath));
		if (!SourceMaterial)
		{
			return FRESTResponse::Error(404, TEXT("SOURCE_NOT_FOUND"),
				FString::Printf(TEXT("Source material not found: %s"), *SourceMaterialPath));
		}
	}

	// Get optional instance name
	FString InstanceName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("name"), TEXT(""));
	FName OptionalName = InstanceName.IsEmpty() ? NAME_None : FName(*InstanceName);

	// Find primitive component
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	if (PrimitiveComponents.Num() == 0)
	{
		return FRESTResponse::Error(400, TEXT("NO_PRIMITIVE_COMPONENT"),
			FString::Printf(TEXT("Actor '%s' has no primitive components"), *ActorLabel));
	}

	// Use first primitive component (most common case)
	UPrimitiveComponent* PrimComp = PrimitiveComponents[0];

	// Validate material index
	int32 NumMaterials = PrimComp->GetNumMaterials();
	if (MaterialIndex < 0 || MaterialIndex >= NumMaterials)
	{
		return FRESTResponse::Error(400, TEXT("INVALID_MATERIAL_INDEX"),
			FString::Printf(TEXT("Material index %d out of range [0, %d)"), MaterialIndex, NumMaterials));
	}

	// Create dynamic material instance
	UMaterialInstanceDynamic* DynamicInstance = PrimComp->CreateDynamicMaterialInstance(
		MaterialIndex, SourceMaterial, OptionalName);

	if (!DynamicInstance)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			TEXT("Failed to create MaterialInstanceDynamic"));
	}

	// Apply initial parameter overrides if provided
	const TSharedPtr<FJsonObject>* ParamsObject = nullptr;
	if (Request.JsonBody->TryGetObjectField(TEXT("parameters"), ParamsObject) && ParamsObject)
	{
		for (const auto& Param : (*ParamsObject)->Values)
		{
			const FName ParamName = FName(*Param.Key);
			const TSharedPtr<FJsonValue>& Value = Param.Value;

			if (Value->Type == EJson::Number)
			{
				DynamicInstance->SetScalarParameterValue(ParamName, static_cast<float>(Value->AsNumber()));
			}
			else if (Value->Type == EJson::Object)
			{
				TSharedPtr<FJsonObject> ColorJson = Value->AsObject();
				FLinearColor Color;
				Color.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
				Color.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
				Color.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
				Color.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;
				DynamicInstance->SetVectorParameterValue(ParamName, Color);
			}
			else if (Value->Type == EJson::String)
			{
				FString TexturePath = Value->AsString();
				UTexture* Texture = Cast<UTexture>(
					StaticLoadObject(UTexture::StaticClass(), nullptr, *TexturePath));
				if (Texture)
				{
					DynamicInstance->SetTextureParameterValue(ParamName, Texture);
				}
			}
		}
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("actor_label"), ActorLabel);
	Response->SetStringField(TEXT("component"), PrimComp->GetName());
	Response->SetNumberField(TEXT("material_index"), MaterialIndex);
	Response->SetStringField(TEXT("instance_name"), DynamicInstance->GetName());
	Response->SetStringField(TEXT("parent_material"), DynamicInstance->Parent ? DynamicInstance->Parent->GetPathName() : TEXT("None"));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleOpenMaterialEditor(const FRESTRequest& Request)
{
	// Get required material path
	FString MaterialPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Load the material
	UMaterial* Material = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

	if (!Material)
	{
		return FRESTResponse::Error(404, TEXT("MATERIAL_NOT_FOUND"),
			FString::Printf(TEXT("Material not found: %s"), *MaterialPath));
	}

	// Open the material in the editor - MUST be done on game thread
	bool bOpened = false;

	if (IsInGameThread())
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			bOpened = AssetEditorSubsystem->OpenEditorForAsset(Material);
		}
	}
	else
	{
		// Schedule on game thread and wait
		FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&]()
		{
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			if (AssetEditorSubsystem)
			{
				bOpened = AssetEditorSubsystem->OpenEditorForAsset(Material);
			}
		}, TStatId(), nullptr, ENamedThreads::GameThread);

		FTaskGraphInterface::Get().WaitUntilTaskCompletes(Task);
	}

	if (!bOpened)
	{
		return FRESTResponse::Error(500, TEXT("OPEN_FAILED"),
			FString::Printf(TEXT("Failed to open Material Editor for: %s"), *MaterialPath));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), MaterialPath);
	Response->SetStringField(TEXT("material_name"), Material->GetName());
	Response->SetStringField(TEXT("message"), TEXT("Material Editor opened"));

	return FRESTResponse::Ok(Response);
}

// ============================================================================
// Asset Save Helper
// ============================================================================

/** Save asset package to disk if requested */
static void SaveAssetIfRequested(UObject* Asset, bool bShouldSave)
{
	if (!bShouldSave || !Asset) return;

	UPackage* Package = Asset->GetOutermost();
	if (Package)
	{
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Standalone;
		UPackage::Save(Package, Asset, *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), SaveArgs);
	}
}

// ============================================================================
// Material Editor Node Manipulation Helpers
// ============================================================================

namespace
{
	/** Map property name string to EMaterialProperty enum value */
	EMaterialProperty GetMaterialPropertyFromName(const FString& PropertyName)
	{
		if (PropertyName.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
		{
			return MP_BaseColor;
		}
		else if (PropertyName.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase))
		{
			return MP_Metallic;
		}
		else if (PropertyName.Equals(TEXT("Specular"), ESearchCase::IgnoreCase))
		{
			return MP_Specular;
		}
		else if (PropertyName.Equals(TEXT("Roughness"), ESearchCase::IgnoreCase))
		{
			return MP_Roughness;
		}
		else if (PropertyName.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase))
		{
			return MP_EmissiveColor;
		}
		else if (PropertyName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
		{
			return MP_Normal;
		}
		else if (PropertyName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase))
		{
			return MP_Opacity;
		}
		else if (PropertyName.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase))
		{
			return MP_OpacityMask;
		}
		else if (PropertyName.Equals(TEXT("AmbientOcclusion"), ESearchCase::IgnoreCase))
		{
			return MP_AmbientOcclusion;
		}
		return MP_MAX; // Invalid
	}

	/** Map EMaterialProperty enum value to property name string (reverse of GetMaterialPropertyFromName) */
	FString GetMaterialPropertyName(EMaterialProperty Property)
	{
		switch (Property)
		{
			case MP_BaseColor: return TEXT("BaseColor");
			case MP_Metallic: return TEXT("Metallic");
			case MP_Specular: return TEXT("Specular");
			case MP_Roughness: return TEXT("Roughness");
			case MP_EmissiveColor: return TEXT("EmissiveColor");
			case MP_Normal: return TEXT("Normal");
			case MP_Opacity: return TEXT("Opacity");
			case MP_OpacityMask: return TEXT("OpacityMask");
			case MP_AmbientOcclusion: return TEXT("AmbientOcclusion");
			case MP_WorldPositionOffset: return TEXT("WorldPositionOffset");
			case MP_SubsurfaceColor: return TEXT("SubsurfaceColor");
			case MP_Tangent: return TEXT("Tangent");
			case MP_Anisotropy: return TEXT("Anisotropy");
			case MP_ShadingModel: return TEXT("ShadingModel");
			case MP_FrontMaterial: return TEXT("FrontMaterial");
			case MP_SurfaceThickness: return TEXT("SurfaceThickness");
			case MP_Displacement: return TEXT("Displacement");
			case MP_PixelDepthOffset: return TEXT("PixelDepthOffset");
			default: return TEXT("");
		}
	}

	/** Get all valid material properties for export */
	TArray<TPair<FString, EMaterialProperty>> GetAllMaterialProperties()
	{
		return {
			{ TEXT("BaseColor"), MP_BaseColor },
			{ TEXT("Metallic"), MP_Metallic },
			{ TEXT("Specular"), MP_Specular },
			{ TEXT("Roughness"), MP_Roughness },
			{ TEXT("EmissiveColor"), MP_EmissiveColor },
			{ TEXT("Normal"), MP_Normal },
			{ TEXT("Opacity"), MP_Opacity },
			{ TEXT("OpacityMask"), MP_OpacityMask },
			{ TEXT("AmbientOcclusion"), MP_AmbientOcclusion },
			{ TEXT("WorldPositionOffset"), MP_WorldPositionOffset },
			{ TEXT("Refraction"), MP_Refraction },
			{ TEXT("PixelDepthOffset"), MP_PixelDepthOffset },
			{ TEXT("SubsurfaceColor"), MP_SubsurfaceColor },
		};
	}

	/** Parse blend mode from string */
	EBlendMode ParseBlendModeString(const FString& ModeStr)
	{
		if (ModeStr.Equals(TEXT("Masked"), ESearchCase::IgnoreCase)) return BLEND_Masked;
		if (ModeStr.Equals(TEXT("Translucent"), ESearchCase::IgnoreCase)) return BLEND_Translucent;
		if (ModeStr.Equals(TEXT("Additive"), ESearchCase::IgnoreCase)) return BLEND_Additive;
		if (ModeStr.Equals(TEXT("Modulate"), ESearchCase::IgnoreCase)) return BLEND_Modulate;
		return BLEND_Opaque;
	}
}

bool FMaterialsHandler::FindActiveMaterialEditor(UMaterial*& OutMaterial, FString& OutError, const FString& MaterialPath)
{
	OutMaterial = nullptr;

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		OutError = TEXT("AssetEditorSubsystem not available");
		return false;
	}

	if (MaterialPath.IsEmpty())
	{
		OutError = TEXT("material_path is required. Specify the full path to the material asset.");
		return false;
	}

	UMaterial* Material = Cast<UMaterial>(
		StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));

	if (Material)
	{
		// Check if it's open in an editor
		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Material, false);
		if (EditorInstance)
		{
			OutMaterial = Material;
			return true;
		}
		else
		{
			OutError = FString::Printf(TEXT("Material '%s' is not open in editor. Use /materials/editor/open first."), *MaterialPath);
			return false;
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Material not found: %s"), *MaterialPath);
		return false;
	}
}

bool FMaterialsHandler::FindActiveMaterialFunctionEditor(UMaterialFunction*& OutFunction, FString& OutError, const FString& FunctionPath)
{
	OutFunction = nullptr;

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		OutError = TEXT("AssetEditorSubsystem not available");
		return false;
	}

	if (FunctionPath.IsEmpty())
	{
		OutError = TEXT("function_path is required. Specify the full path to the material function asset.");
		return false;
	}

	UMaterialFunction* Function = Cast<UMaterialFunction>(
		StaticLoadObject(UMaterialFunction::StaticClass(), nullptr, *FunctionPath));

	if (Function)
	{
		IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Function, false);
		if (EditorInstance)
		{
			OutFunction = Function;
			return true;
		}
		else
		{
			OutError = FString::Printf(TEXT("Material function '%s' is not open in editor. Use /materials/function/editor/open first."), *FunctionPath);
			return false;
		}
	}
	else
	{
		OutError = FString::Printf(TEXT("Material function not found: %s"), *FunctionPath);
		return false;
	}
}

UMaterialExpression* FMaterialsHandler::FindExpressionInFunctionByName(UMaterialFunction* Function, const FString& Name)
{
	if (!Function) return nullptr;

	for (UMaterialExpression* Expression : Function->GetExpressions())
	{
		if (Expression && Expression->GetName() == Name)
		{
			return Expression;
		}
	}
	return nullptr;
}

FRESTResponse FMaterialsHandler::HandleCreateMaterialFunction(const FRESTRequest& Request)
{
	FString FunctionName;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("name"), FunctionName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString PackagePath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT("/Game/Materials/Functions"));

	if (!PackagePath.StartsWith(TEXT("/Game/")))
	{
		if (PackagePath.StartsWith(TEXT("/")))
		{
			PackagePath = TEXT("/Game") + PackagePath;
		}
		else
		{
			PackagePath = TEXT("/Game/") + PackagePath;
		}
	}

	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialFunctionFactoryNew* FunctionFactory = NewObject<UMaterialFunctionFactoryNew>();

	UObject* NewAsset = AssetTools.CreateAsset(FunctionName, PackagePath, UMaterialFunction::StaticClass(), FunctionFactory);

	if (!NewAsset)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			FString::Printf(TEXT("Failed to create material function '%s' at '%s'"), *FunctionName, *PackagePath));
	}

	UMaterialFunction* NewFunction = Cast<UMaterialFunction>(NewAsset);
	if (!NewFunction)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Created asset is not a MaterialFunction"));
	}

	// Set optional properties
	FString Description = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("description"), TEXT(""));
	if (!Description.IsEmpty())
	{
		NewFunction->Description = Description;
	}

	bool bExposeToLibrary = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("expose_to_library"), true);
	NewFunction->bExposeToLibrary = bExposeToLibrary;

	NewFunction->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(NewFunction, bSave);

	FAssetRegistryModule::AssetCreated(NewFunction);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("function_name"), FunctionName);
	Response->SetStringField(TEXT("function_path"), NewFunction->GetPathName());
	Response->SetStringField(TEXT("package_path"), PackagePath);
	Response->SetBoolField(TEXT("expose_to_library"), bExposeToLibrary);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleOpenMaterialFunctionEditor(const FRESTRequest& Request)
{
	FString FunctionPath;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UMaterialFunction* Function = Cast<UMaterialFunction>(
		StaticLoadObject(UMaterialFunction::StaticClass(), nullptr, *FunctionPath));

	if (!Function)
	{
		return FRESTResponse::Error(404, TEXT("FUNCTION_NOT_FOUND"),
			FString::Printf(TEXT("Material function not found: %s"), *FunctionPath));
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return FRESTResponse::Error(500, TEXT("EDITOR_SUBSYSTEM_UNAVAILABLE"), TEXT("AssetEditorSubsystem not available"));
	}

	bool bOpened = AssetEditorSubsystem->OpenEditorForAsset(Function);

	if (!bOpened)
	{
		return FRESTResponse::Error(500, TEXT("OPEN_FAILED"),
			FString::Printf(TEXT("Failed to open editor for material function: %s"), *FunctionPath));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("function_path"), Function->GetPathName());
	Response->SetStringField(TEXT("function_name"), Function->GetName());
	Response->SetNumberField(TEXT("expression_count"), Function->GetExpressions().Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleListMaterialFunctionNodes(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	const FString* FunctionPathPtr = Request.QueryParams.Find(TEXT("function_path"));
	FString FunctionPath = FunctionPathPtr ? *FunctionPathPtr : TEXT("");

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("function_path"), Function->GetPathName());

	TArray<TSharedPtr<FJsonValue>> NodesArray;
	for (UMaterialExpression* Expression : Function->GetExpressions())
	{
		if (!Expression) continue;
		NodesArray.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Expression)));
	}

	Response->SetArrayField(TEXT("nodes"), NodesArray);
	Response->SetNumberField(TEXT("node_count"), NodesArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleCreateMaterialFunctionNode(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	FString FunctionPath;
	FString FnPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, FnPathError))
	{
		return FRESTResponse::BadRequest(FnPathError);
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString ExpressionClass;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_class"), ExpressionClass, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Find the class - try Engine module first, then other modules
	UClass* ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClass));
	if (!ExpClass)
	{
		ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *ExpressionClass));
	}

	if (!ExpClass || !ExpClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		return FRESTResponse::Error(400, TEXT("INVALID_EXPRESSION_CLASS"),
			FString::Printf(TEXT("Invalid expression class: %s"), *ExpressionClass));
	}

	float PosX = 0.0f;
	float PosY = 0.0f;
	if (Request.JsonBody->HasField(TEXT("position")))
	{
		TSharedPtr<FJsonObject> PosObj = Request.JsonBody->GetObjectField(TEXT("position"));
		PosX = static_cast<float>(PosObj->GetNumberField(TEXT("x")));
		PosY = static_cast<float>(PosObj->GetNumberField(TEXT("y")));
	}

	UMaterialExpression* NewExpression = nullptr;
	bool bCreatedViaEditor = false;

	// TIER 1: Try editor API
	TSharedPtr<IToolkit> FoundEditor = FToolkitManager::Get().FindEditorForAsset(Function);
	TSharedPtr<IMaterialEditor> MaterialEditor;
	if (FoundEditor.IsValid())
	{
		MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundEditor);
	}

	if (MaterialEditor.IsValid())
	{
		NewExpression = MaterialEditor->CreateNewMaterialExpression(
			ExpClass,
			FVector2D(PosX, PosY),
			false,
			true,
			Function->MaterialGraph
		);

		if (NewExpression)
		{
			bCreatedViaEditor = true;

			bool bFoundInFunction = false;
			for (UMaterialExpression* Expr : Function->GetExpressions())
			{
				if (Expr == NewExpression) { bFoundInFunction = true; break; }
			}
			if (!bFoundInFunction)
			{
				Function->GetExpressionCollection().AddExpression(NewExpression);
			}
		}
	}

	// TIER 2: Use UMaterialEditingLibrary
	if (!NewExpression)
	{
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionInFunction(
			Function, ExpClass);

		if (NewExpression)
		{
			NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
			NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);
		}
	}

	// TIER 3: Manual creation
	if (!NewExpression)
	{
		NewExpression = NewObject<UMaterialExpression>(Function, ExpClass, NAME_None, RF_Transactional);
		if (NewExpression)
		{
			NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
			NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);
			NewExpression->Function = Function;
			Function->GetExpressionCollection().AddExpression(NewExpression);

			if (Function->MaterialGraph)
			{
				Function->MaterialGraph->AddExpression(NewExpression, true);
			}
		}
	}

	if (!NewExpression)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Failed to create expression"));
	}

	// Handle FunctionInput-specific properties
	if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(NewExpression))
	{
		if (Request.JsonBody->HasField(TEXT("input_name")))
		{
			FuncInput->InputName = FName(*Request.JsonBody->GetStringField(TEXT("input_name")));
		}
		if (Request.JsonBody->HasField(TEXT("input_type")))
		{
			// Map string to EFunctionInputType
			FString InputTypeStr = Request.JsonBody->GetStringField(TEXT("input_type"));
			if (InputTypeStr.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Scalar;
			else if (InputTypeStr.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector2;
			else if (InputTypeStr.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector3;
			else if (InputTypeStr.Equals(TEXT("Vector4"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector4;
			else if (InputTypeStr.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Texture2D;
			else if (InputTypeStr.Equals(TEXT("TextureCube"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_TextureCube;
			else if (InputTypeStr.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_StaticBool;
			else if (InputTypeStr.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Bool;
			else if (InputTypeStr.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_MaterialAttributes;
		}
		if (Request.JsonBody->HasField(TEXT("description")))
		{
			FuncInput->Description = Request.JsonBody->GetStringField(TEXT("description"));
		}
		if (Request.JsonBody->HasField(TEXT("sort_priority")))
		{
			FuncInput->SortPriority = static_cast<int32>(Request.JsonBody->GetNumberField(TEXT("sort_priority")));
		}
		FuncInput->ConditionallyGenerateId(false);
	}

	// Handle FunctionOutput-specific properties
	if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(NewExpression))
	{
		if (Request.JsonBody->HasField(TEXT("output_name")))
		{
			FuncOutput->OutputName = FName(*Request.JsonBody->GetStringField(TEXT("output_name")));
		}
		if (Request.JsonBody->HasField(TEXT("description")))
		{
			FuncOutput->Description = Request.JsonBody->GetStringField(TEXT("description"));
		}
		if (Request.JsonBody->HasField(TEXT("sort_priority")))
		{
			FuncOutput->SortPriority = static_cast<int32>(Request.JsonBody->GetNumberField(TEXT("sort_priority")));
		}
		FuncOutput->ConditionallyGenerateId(false);
	}

	// Handle parameter expressions (same as material endpoint)
	if (Request.JsonBody->HasField(TEXT("param_name")))
	{
		FString ParamName = Request.JsonBody->GetStringField(TEXT("param_name"));
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpression))
		{
			ScalarParam->ParameterName = FName(*ParamName);
			if (Request.JsonBody->HasField(TEXT("default_value")))
			{
				ScalarParam->DefaultValue = static_cast<float>(Request.JsonBody->GetNumberField(TEXT("default_value")));
			}
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpression))
		{
			VectorParam->ParameterName = FName(*ParamName);
			if (Request.JsonBody->HasField(TEXT("default_value")))
			{
				TSharedPtr<FJsonObject> ColorJson = Request.JsonBody->GetObjectField(TEXT("default_value"));
				VectorParam->DefaultValue.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
				VectorParam->DefaultValue.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
				VectorParam->DefaultValue.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
				VectorParam->DefaultValue.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;
			}
		}
	}

	// Handle constant values
	if (Request.JsonBody->HasField(TEXT("value")))
	{
		if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpression))
		{
			ConstExpr->R = static_cast<float>(Request.JsonBody->GetNumberField(TEXT("value")));
		}
		else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(NewExpression))
		{
			TSharedPtr<FJsonObject> ColorJson = Request.JsonBody->GetObjectField(TEXT("value"));
			Const3Vec->Constant.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			Const3Vec->Constant.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			Const3Vec->Constant.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
		}
	}

	Function->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Function, bSave);

	if (!bCreatedViaEditor)
	{
		UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), NewExpression->GetName());
	Response->SetStringField(TEXT("expression_class"), ExpClass->GetName());
	Response->SetBoolField(TEXT("created_via_editor_api"), bCreatedViaEditor);
	Response->SetObjectField(TEXT("expression"), ExpressionToJson(NewExpression));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleSetMaterialFunctionNodePosition(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	FString FunctionPath;
	FString FnPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, FnPathError))
	{
		return FRESTResponse::BadRequest(FnPathError);
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString ExpressionName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_name"), ExpressionName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	if (!Request.JsonBody->HasField(TEXT("position")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: position"));
	}

	TSharedPtr<FJsonObject> PosObj = Request.JsonBody->GetObjectField(TEXT("position"));
	int32 NewX = static_cast<int32>(PosObj->GetNumberField(TEXT("x")));
	int32 NewY = static_cast<int32>(PosObj->GetNumberField(TEXT("y")));

	UMaterialExpression* Expression = FindExpressionInFunctionByName(Function, ExpressionName);
	if (!Expression)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in function"), *ExpressionName));
	}

	int32 OldX = Expression->MaterialExpressionEditorX;
	int32 OldY = Expression->MaterialExpressionEditorY;

	Expression->MaterialExpressionEditorX = NewX;
	Expression->MaterialExpressionEditorY = NewY;

	Function->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Function, bSave);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), ExpressionName);

	TSharedPtr<FJsonObject> OldPosJson = MakeShared<FJsonObject>();
	OldPosJson->SetNumberField(TEXT("x"), OldX);
	OldPosJson->SetNumberField(TEXT("y"), OldY);
	Response->SetObjectField(TEXT("old_position"), OldPosJson);

	TSharedPtr<FJsonObject> NewPosJson = MakeShared<FJsonObject>();
	NewPosJson->SetNumberField(TEXT("x"), NewX);
	NewPosJson->SetNumberField(TEXT("y"), NewY);
	Response->SetObjectField(TEXT("new_position"), NewPosJson);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleConnectMaterialFunctionNodes(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	FString FunctionPath;
	FString FnPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, FnPathError))
	{
		return FRESTResponse::BadRequest(FnPathError);
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString SourceName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("source_expression"), SourceName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString TargetName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("target_expression"), TargetName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UMaterialExpression* SourceExpression = FindExpressionInFunctionByName(Function, SourceName);
	if (!SourceExpression)
	{
		return FRESTResponse::Error(404, TEXT("SOURCE_NOT_FOUND"),
			FString::Printf(TEXT("Source expression '%s' not found"), *SourceName));
	}

	UMaterialExpression* TargetExpression = FindExpressionInFunctionByName(Function, TargetName);
	if (!TargetExpression)
	{
		return FRESTResponse::Error(404, TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("Target expression '%s' not found"), *TargetName));
	}

	int32 OutputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("output_index"), 0);
	int32 InputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("input_index"), 0);

	TArray<FExpressionOutput>& Outputs = SourceExpression->GetOutputs();
	if (OutputIndex < 0 || OutputIndex >= Outputs.Num())
	{
		return FRESTResponse::Error(400, TEXT("INVALID_OUTPUT_INDEX"),
			FString::Printf(TEXT("Output index %d invalid. Expression has %d outputs."), OutputIndex, Outputs.Num()));
	}

	// Get graph nodes for pin-based connections
	UMaterialGraphNode* SourceGraphNode = Cast<UMaterialGraphNode>(SourceExpression->GraphNode);
	UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpression->GraphNode);

	if (!SourceGraphNode || !TargetGraphNode)
	{
		return FRESTResponse::Error(500, TEXT("NO_GRAPH_NODE"),
			TEXT("Source or target expression has no GraphNode. Ensure function is open in editor."));
	}

	UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(SourceGraphNode->GetGraph());
	if (!MaterialGraph)
	{
		return FRESTResponse::Error(500, TEXT("NO_MATERIAL_GRAPH"), TEXT("Could not get MaterialGraph"));
	}

	const UMaterialGraphSchema* Schema = Cast<const UMaterialGraphSchema>(MaterialGraph->GetSchema());
	if (!Schema)
	{
		return FRESTResponse::Error(500, TEXT("NO_SCHEMA"), TEXT("Could not get MaterialGraphSchema"));
	}

	UEdGraphPin* OutputPin = SourceGraphNode->GetOutputPin(OutputIndex);
	UEdGraphPin* InputPin = TargetGraphNode->GetInputPin(InputIndex);

	if (!OutputPin || !InputPin)
	{
		return FRESTResponse::Error(400, TEXT("INVALID_PIN"),
			TEXT("Could not get output or input pin at specified indices"));
	}

	FScopedTransaction Transaction(FText::FromString(FString::Printf(
		TEXT("Connect %s to %s"), *SourceName, *TargetName)));

	bool bConnectionMade = Schema->TryCreateConnection(OutputPin, InputPin);

	MaterialGraph->LinkMaterialExpressionsFromGraph();

	Function->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Function, bSave);

	UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("source_expression"), SourceName);
	Response->SetNumberField(TEXT("output_index"), OutputIndex);
	Response->SetStringField(TEXT("target_expression"), TargetName);
	Response->SetNumberField(TEXT("input_index"), InputIndex);
	Response->SetBoolField(TEXT("connection_verified"), bConnectionMade);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleDisconnectMaterialFunction(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	FString FunctionPath;
	FString FnPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, FnPathError))
	{
		return FRESTResponse::BadRequest(FnPathError);
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString TargetName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("target_expression"), TargetName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	int32 InputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("input_index"), 0);

	UMaterialExpression* TargetExpression = FindExpressionInFunctionByName(Function, TargetName);
	if (!TargetExpression)
	{
		return FRESTResponse::Error(404, TEXT("TARGET_NOT_FOUND"),
			FString::Printf(TEXT("Target expression '%s' not found"), *TargetName));
	}

	UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpression->GraphNode);
	if (!TargetGraphNode)
	{
		return FRESTResponse::Error(500, TEXT("NO_GRAPH_NODE"),
			TEXT("Target expression has no GraphNode"));
	}

	UEdGraphPin* InputPin = TargetGraphNode->GetInputPin(InputIndex);
	if (!InputPin)
	{
		return FRESTResponse::Error(400, TEXT("INVALID_INPUT_PIN"),
			FString::Printf(TEXT("Could not get input pin %d"), InputIndex));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("target_expression"), TargetName);
	Response->SetNumberField(TEXT("input_index"), InputIndex);

	bool bWasConnected = InputPin->LinkedTo.Num() > 0;

	if (bWasConnected)
	{
		UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(TargetGraphNode->GetGraph());
		const UMaterialGraphSchema* Schema = Cast<const UMaterialGraphSchema>(MaterialGraph->GetSchema());

		FScopedTransaction Transaction(FText::FromString(FString::Printf(
			TEXT("Disconnect input %d on %s"), InputIndex, *TargetName)));

		Schema->BreakPinLinks(*InputPin, true);

		MaterialGraph->LinkMaterialExpressionsFromGraph();

		Function->MarkPackageDirty();

		bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
		SaveAssetIfRequested(Function, bSave);

		UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);
	}

	Response->SetBoolField(TEXT("was_connected"), bWasConnected);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleSetMaterialFunctionExpressionProperty(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	FString FunctionPath;
	FString FnPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("function_path"), FunctionPath, FnPathError))
	{
		return FRESTResponse::BadRequest(FnPathError);
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString ExpressionName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_name"), ExpressionName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString PropertyName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("property"), PropertyName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	if (!Request.JsonBody->HasField(TEXT("value")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: value"));
	}

	UMaterialExpression* Expression = FindExpressionInFunctionByName(Function, ExpressionName);
	if (!Expression)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in function"), *ExpressionName));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), ExpressionName);
	Response->SetStringField(TEXT("property"), PropertyName);

	const TSharedPtr<FJsonValue>& ValueField = Request.JsonBody->Values.FindRef(TEXT("value"));

	// Handle FunctionInput properties
	if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expression))
	{
		if (PropertyName.Equals(TEXT("InputName"), ESearchCase::IgnoreCase))
		{
			FString OldName = FuncInput->InputName.ToString();
			FuncInput->InputName = FName(*ValueField->AsString());
			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else if (PropertyName.Equals(TEXT("InputType"), ESearchCase::IgnoreCase))
		{
			FString TypeStr = ValueField->AsString();
			if (TypeStr.Equals(TEXT("Scalar"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Scalar;
			else if (TypeStr.Equals(TEXT("Vector2"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector2;
			else if (TypeStr.Equals(TEXT("Vector3"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector3;
			else if (TypeStr.Equals(TEXT("Vector4"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Vector4;
			else if (TypeStr.Equals(TEXT("Texture2D"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Texture2D;
			else if (TypeStr.Equals(TEXT("StaticBool"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_StaticBool;
			else if (TypeStr.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_Bool;
			else if (TypeStr.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase))
				FuncInput->InputType = FunctionInput_MaterialAttributes;
			Response->SetStringField(TEXT("new_value"), TypeStr);
		}
		else if (PropertyName.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
		{
			FString OldDesc = FuncInput->Description;
			FuncInput->Description = ValueField->AsString();
			Response->SetStringField(TEXT("old_value"), OldDesc);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else if (PropertyName.Equals(TEXT("SortPriority"), ESearchCase::IgnoreCase))
		{
			int32 OldPriority = FuncInput->SortPriority;
			FuncInput->SortPriority = static_cast<int32>(ValueField->AsNumber());
			Response->SetNumberField(TEXT("old_value"), OldPriority);
			Response->SetNumberField(TEXT("new_value"), FuncInput->SortPriority);
		}
		else if (PropertyName.Equals(TEXT("UsePreviewValueAsDefault"), ESearchCase::IgnoreCase))
		{
			FuncInput->bUsePreviewValueAsDefault = ValueField->AsBool();
			Response->SetBoolField(TEXT("new_value"), FuncInput->bUsePreviewValueAsDefault);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for FunctionInput. Valid: InputName, InputType, Description, SortPriority, UsePreviewValueAsDefault"), *PropertyName));
		}
	}
	// Handle FunctionOutput properties
	else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
	{
		if (PropertyName.Equals(TEXT("OutputName"), ESearchCase::IgnoreCase))
		{
			FString OldName = FuncOutput->OutputName.ToString();
			FuncOutput->OutputName = FName(*ValueField->AsString());
			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else if (PropertyName.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
		{
			FString OldDesc = FuncOutput->Description;
			FuncOutput->Description = ValueField->AsString();
			Response->SetStringField(TEXT("old_value"), OldDesc);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else if (PropertyName.Equals(TEXT("SortPriority"), ESearchCase::IgnoreCase))
		{
			int32 OldPriority = FuncOutput->SortPriority;
			FuncOutput->SortPriority = static_cast<int32>(ValueField->AsNumber());
			Response->SetNumberField(TEXT("old_value"), OldPriority);
			Response->SetNumberField(TEXT("new_value"), FuncOutput->SortPriority);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for FunctionOutput. Valid: OutputName, Description, SortPriority"), *PropertyName));
		}
	}
	// Handle ScalarParameter, VectorParameter, Constant, etc.
	else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		if (PropertyName.Equals(TEXT("DefaultValue"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			float OldValue = ScalarParam->DefaultValue;
			ScalarParam->DefaultValue = static_cast<float>(ValueField->AsNumber());
			Response->SetNumberField(TEXT("old_value"), OldValue);
			Response->SetNumberField(TEXT("new_value"), ScalarParam->DefaultValue);
		}
		else if (PropertyName.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
		{
			FString OldName = ScalarParam->ParameterName.ToString();
			ScalarParam->ParameterName = FName(*ValueField->AsString());
			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for ScalarParameter. Valid: DefaultValue, ParameterName"), *PropertyName));
		}
	}
	else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		if (PropertyName.Equals(TEXT("DefaultValue"), ESearchCase::IgnoreCase) || PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
			if (!ColorJson.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Value must be an object with r, g, b fields"));
			}
			FLinearColor NewValue;
			NewValue.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			NewValue.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			NewValue.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
			NewValue.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : VectorParam->DefaultValue.A;
			VectorParam->DefaultValue = NewValue;
			Response->SetStringField(TEXT("new_value"), FString::Printf(TEXT("(%f,%f,%f,%f)"), NewValue.R, NewValue.G, NewValue.B, NewValue.A));
		}
		else if (PropertyName.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
		{
			FString OldName = VectorParam->ParameterName.ToString();
			VectorParam->ParameterName = FName(*ValueField->AsString());
			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), ValueField->AsString());
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for VectorParameter"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionComponentMask
	else if (UMaterialExpressionComponentMask* Mask = Cast<UMaterialExpressionComponentMask>(Expression))
	{
		if (PropertyName.Equals(TEXT("R"), ESearchCase::IgnoreCase))
		{
			Mask->R = ValueField->AsBool();
			Response->SetBoolField(TEXT("new_value"), Mask->R);
		}
		else if (PropertyName.Equals(TEXT("G"), ESearchCase::IgnoreCase))
		{
			Mask->G = ValueField->AsBool();
			Response->SetBoolField(TEXT("new_value"), Mask->G);
		}
		else if (PropertyName.Equals(TEXT("B"), ESearchCase::IgnoreCase))
		{
			Mask->B = ValueField->AsBool();
			Response->SetBoolField(TEXT("new_value"), Mask->B);
		}
		else if (PropertyName.Equals(TEXT("A"), ESearchCase::IgnoreCase))
		{
			Mask->A = ValueField->AsBool();
			Response->SetBoolField(TEXT("new_value"), Mask->A);
		}
		else if (PropertyName.Equals(TEXT("Channels"), ESearchCase::IgnoreCase))
		{
			TSharedPtr<FJsonObject> Obj = ValueField->AsObject();
			if (!Obj.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Channels value must be an object with r, g, b, a boolean fields"));
			}
			Mask->R = Obj->GetBoolField(TEXT("r"));
			Mask->G = Obj->GetBoolField(TEXT("g"));
			Mask->B = Obj->GetBoolField(TEXT("b"));
			Mask->A = Obj->GetBoolField(TEXT("a"));

			TSharedPtr<FJsonObject> ChannelsJson = MakeShared<FJsonObject>();
			ChannelsJson->SetBoolField(TEXT("r"), Mask->R);
			ChannelsJson->SetBoolField(TEXT("g"), Mask->G);
			ChannelsJson->SetBoolField(TEXT("b"), Mask->B);
			ChannelsJson->SetBoolField(TEXT("a"), Mask->A);
			Response->SetObjectField(TEXT("new_value"), ChannelsJson);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for ComponentMask. Valid: R, G, B, A, Channels"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionConstant
	else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
	{
		if (PropertyName.Equals(TEXT("R"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			float OldValue = ConstExpr->R;
			float NewValue = static_cast<float>(ValueField->AsNumber());
			ConstExpr->R = NewValue;

			Response->SetNumberField(TEXT("old_value"), OldValue);
			Response->SetNumberField(TEXT("new_value"), NewValue);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Constant. Valid: R, Value"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionConstant3Vector
	else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(Expression))
	{
		if (PropertyName.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			FLinearColor OldValue = Const3Vec->Constant;

			TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
			if (!ColorJson.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Value must be an object with r, g, b fields"));
			}
			FLinearColor NewColor;
			NewColor.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			NewColor.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			NewColor.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
			NewColor.A = 1.0f;

			Const3Vec->Constant = NewColor;

			TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
			OldColorJson->SetNumberField(TEXT("r"), OldValue.R);
			OldColorJson->SetNumberField(TEXT("g"), OldValue.G);
			OldColorJson->SetNumberField(TEXT("b"), OldValue.B);
			Response->SetObjectField(TEXT("old_value"), OldColorJson);

			TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
			NewColorJson->SetNumberField(TEXT("r"), NewColor.R);
			NewColorJson->SetNumberField(TEXT("g"), NewColor.G);
			NewColorJson->SetNumberField(TEXT("b"), NewColor.B);
			Response->SetObjectField(TEXT("new_value"), NewColorJson);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Constant3Vector. Valid: Constant, Value"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionConstant4Vector
	else if (UMaterialExpressionConstant4Vector* Const4Vec = Cast<UMaterialExpressionConstant4Vector>(Expression))
	{
		if (PropertyName.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			FLinearColor OldValue = Const4Vec->Constant;

			TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
			if (!ColorJson.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Value must be an object with r, g, b, a fields"));
			}
			FLinearColor NewColor;
			NewColor.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			NewColor.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			NewColor.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
			NewColor.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;

			Const4Vec->Constant = NewColor;

			TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
			OldColorJson->SetNumberField(TEXT("r"), OldValue.R);
			OldColorJson->SetNumberField(TEXT("g"), OldValue.G);
			OldColorJson->SetNumberField(TEXT("b"), OldValue.B);
			OldColorJson->SetNumberField(TEXT("a"), OldValue.A);
			Response->SetObjectField(TEXT("old_value"), OldColorJson);

			TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
			NewColorJson->SetNumberField(TEXT("r"), NewColor.R);
			NewColorJson->SetNumberField(TEXT("g"), NewColor.G);
			NewColorJson->SetNumberField(TEXT("b"), NewColor.B);
			NewColorJson->SetNumberField(TEXT("a"), NewColor.A);
			Response->SetObjectField(TEXT("new_value"), NewColorJson);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Constant4Vector. Valid: Constant, Value"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionMaterialFunctionCall
	else if (UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
	{
		if (PropertyName.Equals(TEXT("MaterialFunction"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Function"), ESearchCase::IgnoreCase))
		{
			FString FuncPath = ValueField->AsString();
			UMaterialFunctionInterface* MatFunc = LoadObject<UMaterialFunctionInterface>(nullptr, *FuncPath);
			if (!MatFunc)
			{
				return FRESTResponse::Error(404, TEXT("FUNCTION_NOT_FOUND"),
					FString::Printf(TEXT("Material function '%s' not found"), *FuncPath));
			}

			FuncCall->SetMaterialFunction(MatFunc);

			Response->SetStringField(TEXT("new_value"), FuncPath);
			Response->SetNumberField(TEXT("input_count"), FuncCall->FunctionInputs.Num());
			Response->SetNumberField(TEXT("output_count"), FuncCall->FunctionOutputs.Num());

			TArray<TSharedPtr<FJsonValue>> InputsArray;
			for (const auto& Input : FuncCall->FunctionInputs)
			{
				TSharedPtr<FJsonObject> InputJson = MakeShared<FJsonObject>();
				InputJson->SetStringField(TEXT("name"), Input.ExpressionInput->InputName.ToString());
				InputsArray.Add(MakeShared<FJsonValueObject>(InputJson));
			}
			Response->SetArrayField(TEXT("inputs"), InputsArray);

			TArray<TSharedPtr<FJsonValue>> OutputsArray;
			for (const auto& Output : FuncCall->FunctionOutputs)
			{
				TSharedPtr<FJsonObject> OutputJson = MakeShared<FJsonObject>();
				OutputJson->SetStringField(TEXT("name"), Output.ExpressionOutput->OutputName.ToString());
				OutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
			}
			Response->SetArrayField(TEXT("outputs"), OutputsArray);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for MaterialFunctionCall. Valid: MaterialFunction, Function"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionCustom
	else if (UMaterialExpressionCustom* CustomExpr = Cast<UMaterialExpressionCustom>(Expression))
	{
		if (PropertyName.Equals(TEXT("Code"), ESearchCase::IgnoreCase))
		{
			FString OldCode = CustomExpr->Code;
			CustomExpr->Code = ValueField->AsString();
			Response->SetStringField(TEXT("old_value"), OldCode);
			Response->SetStringField(TEXT("new_value"), CustomExpr->Code);
		}
		else if (PropertyName.Equals(TEXT("OutputType"), ESearchCase::IgnoreCase))
		{
			FString TypeStr = ValueField->AsString();
			ECustomMaterialOutputType NewType = CMOT_Float1;
			if (TypeStr.Equals(TEXT("Float1"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("CMOT_Float1"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("Float"), ESearchCase::IgnoreCase))
				NewType = CMOT_Float1;
			else if (TypeStr.Equals(TEXT("Float2"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("CMOT_Float2"), ESearchCase::IgnoreCase))
				NewType = CMOT_Float2;
			else if (TypeStr.Equals(TEXT("Float3"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("CMOT_Float3"), ESearchCase::IgnoreCase))
				NewType = CMOT_Float3;
			else if (TypeStr.Equals(TEXT("Float4"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("CMOT_Float4"), ESearchCase::IgnoreCase))
				NewType = CMOT_Float4;
			else if (TypeStr.Equals(TEXT("MaterialAttributes"), ESearchCase::IgnoreCase) || TypeStr.Equals(TEXT("CMOT_MaterialAttributes"), ESearchCase::IgnoreCase))
				NewType = CMOT_MaterialAttributes;
			else
			{
				return FRESTResponse::Error(400, TEXT("INVALID_VALUE"),
					FString::Printf(TEXT("Invalid OutputType '%s'. Valid: Float1, Float2, Float3, Float4, MaterialAttributes"), *TypeStr));
			}
			CustomExpr->OutputType = NewType;
			Response->SetStringField(TEXT("new_value"), TypeStr);
		}
		else if (PropertyName.Equals(TEXT("Description"), ESearchCase::IgnoreCase))
		{
			CustomExpr->Description = ValueField->AsString();
			Response->SetStringField(TEXT("new_value"), CustomExpr->Description);
		}
		else if (PropertyName.Equals(TEXT("Inputs"), ESearchCase::IgnoreCase))
		{
			// Value is an array of objects: [{"name": "Q"}, {"name": "P"}]
			TArray<TSharedPtr<FJsonValue>> InputsArr = ValueField->AsArray();
			CustomExpr->Inputs.Empty();
			for (const auto& InputVal : InputsArr)
			{
				TSharedPtr<FJsonObject> InputObj = InputVal->AsObject();
				FCustomInput NewInput;
				NewInput.InputName = FName(*InputObj->GetStringField(TEXT("name")));
				CustomExpr->Inputs.Add(NewInput);
			}
			CustomExpr->RebuildOutputs();
			Response->SetNumberField(TEXT("input_count"), CustomExpr->Inputs.Num());
		}
		else if (PropertyName.Equals(TEXT("AdditionalOutputs"), ESearchCase::IgnoreCase))
		{
			TArray<TSharedPtr<FJsonValue>> OutputsArr = ValueField->AsArray();
			CustomExpr->AdditionalOutputs.Empty();
			for (const auto& OutputVal : OutputsArr)
			{
				TSharedPtr<FJsonObject> OutputObj = OutputVal->AsObject();
				FCustomOutput NewOutput;
				NewOutput.OutputName = FName(*OutputObj->GetStringField(TEXT("name")));
				FString OutTypeStr = OutputObj->HasField(TEXT("type")) ? OutputObj->GetStringField(TEXT("type")) : TEXT("Float3");
				if (OutTypeStr.Contains(TEXT("4"))) NewOutput.OutputType = CMOT_Float4;
				else if (OutTypeStr.Contains(TEXT("3"))) NewOutput.OutputType = CMOT_Float3;
				else if (OutTypeStr.Contains(TEXT("2"))) NewOutput.OutputType = CMOT_Float2;
				else NewOutput.OutputType = CMOT_Float1;
				CustomExpr->AdditionalOutputs.Add(NewOutput);
			}
			CustomExpr->RebuildOutputs();
			Response->SetNumberField(TEXT("output_count"), CustomExpr->AdditionalOutputs.Num());
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Custom. Valid: Code, OutputType, Description, Inputs, AdditionalOutputs"), *PropertyName));
		}
	}
	else
	{
		return FRESTResponse::Error(400, TEXT("UNSUPPORTED_EXPRESSION_TYPE"),
			FString::Printf(TEXT("Expression type '%s' not supported for property editing"),
				*Expression->GetClass()->GetName()));
	}

	Function->MarkPackageDirty();
	UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Function, bSave);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleDeleteMaterialFunctionExpression(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	// Support both query params and body
	FString FunctionPath;
	const FString* FunctionPathPtr = Request.QueryParams.Find(TEXT("function_path"));
	if (FunctionPathPtr && !FunctionPathPtr->IsEmpty())
	{
		FunctionPath = *FunctionPathPtr;
	}
	else if (Request.JsonBody.IsValid())
	{
		FunctionPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("function_path"), TEXT(""));
	}

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString ExpressionName;
	const FString* ExprNamePtr = Request.QueryParams.Find(TEXT("expression_name"));
	if (ExprNamePtr && !ExprNamePtr->IsEmpty())
	{
		ExpressionName = *ExprNamePtr;
	}
	else if (Request.JsonBody.IsValid())
	{
		ExpressionName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("expression_name"), TEXT(""));
	}

	if (ExpressionName.IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: expression_name"));
	}

	UMaterialExpression* ExpressionToDelete = FindExpressionInFunctionByName(Function, ExpressionName);
	if (!ExpressionToDelete)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in function"), *ExpressionName));
	}

	FString ExpressionClass = ExpressionToDelete->GetClass()->GetName();

	UMaterialEditingLibrary::DeleteMaterialExpressionInFunction(Function, ExpressionToDelete);

	Function->MarkPackageDirty();

	bool bSave = false;
	if (Request.JsonBody.IsValid())
	{
		bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	}
	else
	{
		const FString* SavePtr = Request.QueryParams.Find(TEXT("save"));
		bSave = !SavePtr || !SavePtr->Equals(TEXT("false"), ESearchCase::IgnoreCase);
	}
	SaveAssetIfRequested(Function, bSave);

	UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("deleted_expression"), ExpressionName);
	Response->SetStringField(TEXT("expression_class"), ExpressionClass);
	Response->SetNumberField(TEXT("remaining_expressions"), Function->GetExpressions().Num());

	return FRESTResponse::Ok(Response);
}

void FMaterialsHandler::RefreshMaterialEditorGraph(UMaterial* Material)
{
	if (!Material)
	{
		return;
	}

	// TODO: Visual graph refresh not yet working - nodes are created in data but don't appear
	// visually until the material is closed and reopened. See research prompt:
	// docs/prompts/material-editor-graph-refresh-research.md

	// Ensure we're on the game thread for UI updates
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [Material]()
		{
			if (Material && Material->IsValidLowLevel())
			{
				// LinkGraphNodesFromMaterial reads material data and creates visual pin links
				if (Material->MaterialGraph)
				{
					Material->MaterialGraph->LinkGraphNodesFromMaterial();
				}

				// Update editor
				TSharedPtr<IMaterialEditor> MaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(Material);
				if (MaterialEditor.IsValid())
				{
					MaterialEditor->UpdateMaterialAfterGraphChange();
					MaterialEditor->ForceRefreshExpressionPreviews();
				}
			}
		});
		return;
	}

	// Already on game thread
	if (Material->MaterialGraph)
	{
		// LinkGraphNodesFromMaterial reads material data and creates visual pin links
		Material->MaterialGraph->LinkGraphNodesFromMaterial();
	}

	TSharedPtr<IMaterialEditor> MaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(Material);
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
		MaterialEditor->ForceRefreshExpressionPreviews();
	}
}

TSharedPtr<FJsonObject> FMaterialsHandler::ExpressionToJson(UMaterialExpression* Expression)
{
	TSharedPtr<FJsonObject> ExprJson = MakeShared<FJsonObject>();

	if (!Expression)
	{
		return ExprJson;
	}

	ExprJson->SetStringField(TEXT("name"), Expression->GetName());
	ExprJson->SetStringField(TEXT("class"), Expression->GetClass()->GetName());
	ExprJson->SetStringField(TEXT("description"), Expression->GetDescription());
	ExprJson->SetBoolField(TEXT("has_graph_node"), Expression->GraphNode != nullptr);

	// Position
	TSharedPtr<FJsonObject> PosJson = MakeShared<FJsonObject>();
	PosJson->SetNumberField(TEXT("x"), Expression->MaterialExpressionEditorX);
	PosJson->SetNumberField(TEXT("y"), Expression->MaterialExpressionEditorY);
	ExprJson->SetObjectField(TEXT("position"), PosJson);

	// Check for parameter name
	if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		ExprJson->SetStringField(TEXT("param_name"), ScalarParam->ParameterName.ToString());
		ExprJson->SetNumberField(TEXT("default_value"), ScalarParam->DefaultValue);
	}
	else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		ExprJson->SetStringField(TEXT("param_name"), VectorParam->ParameterName.ToString());
		TSharedPtr<FJsonObject> DefaultColor = MakeShared<FJsonObject>();
		DefaultColor->SetNumberField(TEXT("r"), VectorParam->DefaultValue.R);
		DefaultColor->SetNumberField(TEXT("g"), VectorParam->DefaultValue.G);
		DefaultColor->SetNumberField(TEXT("b"), VectorParam->DefaultValue.B);
		DefaultColor->SetNumberField(TEXT("a"), VectorParam->DefaultValue.A);
		ExprJson->SetObjectField(TEXT("default_value"), DefaultColor);
	}
	else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
	{
		ExprJson->SetNumberField(TEXT("value"), ConstExpr->R);
	}
	else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(Expression))
	{
		TSharedPtr<FJsonObject> ColorJson = MakeShared<FJsonObject>();
		ColorJson->SetNumberField(TEXT("r"), Const3Vec->Constant.R);
		ColorJson->SetNumberField(TEXT("g"), Const3Vec->Constant.G);
		ColorJson->SetNumberField(TEXT("b"), Const3Vec->Constant.B);
		ExprJson->SetObjectField(TEXT("value"), ColorJson);
	}

	// Output info
	TArray<TSharedPtr<FJsonValue>> OutputsArray;
	TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
	for (int32 i = 0; i < Outputs.Num(); i++)
	{
		TSharedPtr<FJsonObject> OutputJson = MakeShared<FJsonObject>();
		OutputJson->SetNumberField(TEXT("index"), i);
		OutputJson->SetStringField(TEXT("name"), Outputs[i].OutputName.ToString());
		OutputsArray.Add(MakeShared<FJsonValueObject>(OutputJson));
	}
	ExprJson->SetArrayField(TEXT("outputs"), OutputsArray);

	return ExprJson;
}

UMaterialExpression* FMaterialsHandler::FindExpressionByName(UMaterial* Material, const FString& Name)
{
	if (!Material)
	{
		return nullptr;
	}

	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (Expression && Expression->GetName() == Name)
		{
			return Expression;
		}
	}

	return nullptr;
}

// ============================================================================
// Connection Safety Helpers
// ============================================================================

bool FMaterialsHandler::CanConnect(UMaterial* Material, UMaterialExpression* SourceExpression, int32 OutputIndex,
	const FString& TargetProperty, UMaterialExpression* TargetExpression, int32 InputIndex, FString& OutError)
{
	// Validate source expression
	if (!SourceExpression)
	{
		OutError = TEXT("Source expression is null");
		return false;
	}

	// Validate output index
	TArray<FExpressionOutput>& Outputs = SourceExpression->GetOutputs();
	if (OutputIndex < 0 || OutputIndex >= Outputs.Num())
	{
		OutError = FString::Printf(TEXT("Output index %d is invalid. Expression '%s' has %d outputs (valid: 0-%d)."),
			OutputIndex, *SourceExpression->GetName(), Outputs.Num(), FMath::Max(0, Outputs.Num() - 1));
		return false;
	}

	// Check if connecting to material property
	if (!TargetProperty.IsEmpty())
	{
		// Validate property name
		static const TArray<FString> ValidProperties = {
			TEXT("BaseColor"), TEXT("Metallic"), TEXT("Specular"), TEXT("Roughness"),
			TEXT("EmissiveColor"), TEXT("Normal"), TEXT("Opacity"), TEXT("OpacityMask"),
			TEXT("AmbientOcclusion")
		};

		bool bValidProperty = false;
		for (const FString& ValidProp : ValidProperties)
		{
			if (TargetProperty.Equals(ValidProp, ESearchCase::IgnoreCase))
			{
				bValidProperty = true;
				break;
			}
		}

		if (!bValidProperty)
		{
			OutError = FString::Printf(TEXT("Invalid target property: '%s'. Valid properties: BaseColor, Metallic, Specular, Roughness, EmissiveColor, Normal, Opacity, OpacityMask, AmbientOcclusion"),
				*TargetProperty);
			return false;
		}

		return true;
	}

	// Check if connecting to another expression
	if (TargetExpression)
	{
		// Validate input index by iterating GetInput until nullptr
		int32 InputCount = 0;
		while (TargetExpression->GetInput(InputCount) != nullptr)
		{
			InputCount++;
		}

		if (InputIndex < 0 || InputIndex >= InputCount)
		{
			OutError = FString::Printf(TEXT("Input index %d is invalid. Expression '%s' has %d inputs (valid: 0-%d)."),
				InputIndex, *TargetExpression->GetName(), InputCount, FMath::Max(0, InputCount - 1));
			return false;
		}

		return true;
	}

	OutError = TEXT("No target specified. Provide either target_property or target_expression.");
	return false;
}

bool FMaterialsHandler::VerifyConnection(UMaterial* Material, UMaterialExpression* SourceExpression, int32 OutputIndex,
	const FString& TargetProperty, UMaterialExpression* TargetExpression, int32 InputIndex)
{
	if (!Material || !SourceExpression)
	{
		return false;
	}

	// Check connection to material property
	if (!TargetProperty.IsEmpty())
	{
		FExpressionInput* Input = nullptr;

		if (TargetProperty.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->BaseColor;
		}
		else if (TargetProperty.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->Metallic;
		}
		else if (TargetProperty.Equals(TEXT("Specular"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->Specular;
		}
		else if (TargetProperty.Equals(TEXT("Roughness"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->Roughness;
		}
		else if (TargetProperty.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->EmissiveColor;
		}
		else if (TargetProperty.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->Normal;
		}
		else if (TargetProperty.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->Opacity;
		}
		else if (TargetProperty.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->OpacityMask;
		}
		else if (TargetProperty.Equals(TEXT("AmbientOcclusion"), ESearchCase::IgnoreCase))
		{
			Input = &Material->GetEditorOnlyData()->AmbientOcclusion;
		}

		if (Input)
		{
			return Input->Expression == SourceExpression && Input->OutputIndex == OutputIndex;
		}

		return false;
	}

	// Check connection to another expression
	if (TargetExpression)
	{
		FExpressionInput* Input = TargetExpression->GetInput(InputIndex);
		if (Input)
		{
			return Input->Expression == SourceExpression && Input->OutputIndex == OutputIndex;
		}
	}

	return false;
}

TSharedPtr<FJsonObject> FMaterialsHandler::GetPropertyConnectionInfo(UMaterial* Material, const FString& PropertyName)
{
	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();

	if (!Material)
	{
		Result->SetField(TEXT("connected_expression"), MakeShared<FJsonValueNull>());
		Result->SetField(TEXT("output_index"), MakeShared<FJsonValueNull>());
		Result->SetStringField(TEXT("error"), TEXT("Material is null"));
		return Result;
	}

	FExpressionInput* Input = nullptr;

	if (PropertyName.Equals(TEXT("BaseColor"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->BaseColor;
	}
	else if (PropertyName.Equals(TEXT("Metallic"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->Metallic;
	}
	else if (PropertyName.Equals(TEXT("Specular"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->Specular;
	}
	else if (PropertyName.Equals(TEXT("Roughness"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->Roughness;
	}
	else if (PropertyName.Equals(TEXT("EmissiveColor"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->EmissiveColor;
	}
	else if (PropertyName.Equals(TEXT("Normal"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->Normal;
	}
	else if (PropertyName.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->Opacity;
	}
	else if (PropertyName.Equals(TEXT("OpacityMask"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->OpacityMask;
	}
	else if (PropertyName.Equals(TEXT("AmbientOcclusion"), ESearchCase::IgnoreCase))
	{
		Input = &Material->GetEditorOnlyData()->AmbientOcclusion;
	}

	if (!Input)
	{
		Result->SetField(TEXT("connected_expression"), MakeShared<FJsonValueNull>());
		Result->SetField(TEXT("output_index"), MakeShared<FJsonValueNull>());
		Result->SetStringField(TEXT("error"), FString::Printf(TEXT("Invalid property name: %s"), *PropertyName));
		return Result;
	}

	Result->SetStringField(TEXT("property"), PropertyName);

	if (Input->Expression)
	{
		Result->SetStringField(TEXT("connected_expression"), Input->Expression->GetName());
		Result->SetNumberField(TEXT("output_index"), Input->OutputIndex);
		Result->SetBoolField(TEXT("is_connected"), true);
	}
	else
	{
		Result->SetField(TEXT("connected_expression"), MakeShared<FJsonValueNull>());
		Result->SetField(TEXT("output_index"), MakeShared<FJsonValueNull>());
		Result->SetBoolField(TEXT("is_connected"), false);
	}

	return Result;
}

// ============================================================================
// Material Editor Node Manipulation Endpoints
// ============================================================================

FRESTResponse FMaterialsHandler::HandleListMaterialNodes(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}
	FString MaterialPath = *MaterialPathPtr;

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("material_name"), Material->GetName());

	// List all expressions
	TArray<TSharedPtr<FJsonValue>> ExpressionsArray;
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (Expression)
		{
			ExpressionsArray.Add(MakeShared<FJsonValueObject>(ExpressionToJson(Expression)));
		}
	}
	Response->SetArrayField(TEXT("expressions"), ExpressionsArray);
	Response->SetNumberField(TEXT("expression_count"), ExpressionsArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleSetMaterialNodePosition(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get expression name
	FString ExpressionName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_name"), ExpressionName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get position
	if (!Request.JsonBody->HasField(TEXT("position")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: position"));
	}

	TSharedPtr<FJsonObject> PosObj = Request.JsonBody->GetObjectField(TEXT("position"));
	int32 NewX = static_cast<int32>(PosObj->GetNumberField(TEXT("x")));
	int32 NewY = static_cast<int32>(PosObj->GetNumberField(TEXT("y")));

	// Find the expression
	UMaterialExpression* Expression = FindExpressionByName(Material, ExpressionName);
	if (!Expression)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in material"), *ExpressionName));
	}

	// Store old position
	int32 OldX = Expression->MaterialExpressionEditorX;
	int32 OldY = Expression->MaterialExpressionEditorY;

	// Move expression
	Expression->MaterialExpressionEditorX = NewX;
	Expression->MaterialExpressionEditorY = NewY;

	// Mark material as modified
	Material->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	Material->PreEditChange(nullptr);
	Material->PostEditChange();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), ExpressionName);

	TSharedPtr<FJsonObject> OldPosJson = MakeShared<FJsonObject>();
	OldPosJson->SetNumberField(TEXT("x"), OldX);
	OldPosJson->SetNumberField(TEXT("y"), OldY);
	Response->SetObjectField(TEXT("old_position"), OldPosJson);

	TSharedPtr<FJsonObject> NewPosJson = MakeShared<FJsonObject>();
	NewPosJson->SetNumberField(TEXT("x"), NewX);
	NewPosJson->SetNumberField(TEXT("y"), NewY);
	Response->SetObjectField(TEXT("new_position"), NewPosJson);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleCreateMaterialNode(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get expression class name
	FString ExpressionClass;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_class"), ExpressionClass, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Find the class
	UClass* ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClass));
	if (!ExpClass)
	{
		// Try with MaterialExpression prefix if not included
		ExpClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.MaterialExpression%s"), *ExpressionClass));
	}

	if (!ExpClass || !ExpClass->IsChildOf(UMaterialExpression::StaticClass()))
	{
		// List some common expression types
		return FRESTResponse::Error(400, TEXT("INVALID_EXPRESSION_CLASS"),
			FString::Printf(TEXT("Invalid expression class: %s. Common types: ScalarParameter, VectorParameter, TextureSample, Add, Multiply, Constant, Constant3Vector"), *ExpressionClass));
	}

	// Get optional position
	float PosX = 0.0f;
	float PosY = 0.0f;
	if (Request.JsonBody->HasField(TEXT("position")))
	{
		TSharedPtr<FJsonObject> PosObj = Request.JsonBody->GetObjectField(TEXT("position"));
		PosX = static_cast<float>(PosObj->GetNumberField(TEXT("x")));
		PosY = static_cast<float>(PosObj->GetNumberField(TEXT("y")));
	}

	UMaterialExpression* NewExpression = nullptr;
	bool bCreatedViaEditor = false;

	// Try to use Material Editor API for proper visual updates
	// Use FToolkitManager directly to find editor for the Material asset
	TSharedPtr<IMaterialEditor> MaterialEditor;
	TSharedPtr<IToolkit> FoundEditor = FToolkitManager::Get().FindEditorForAsset(Material);
	if (FoundEditor.IsValid())
	{
		MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundEditor);
	}

	if (MaterialEditor.IsValid())
	{
		// Use IMaterialEditor::CreateNewMaterialExpression - handles visual graph updates automatically
		NewExpression = MaterialEditor->CreateNewMaterialExpression(
			ExpClass,
			FVector2D(PosX, PosY),
			false,  // bAutoSelect
			true,   // bAutoAssignResource
			Material->MaterialGraph  // Graph - editor uses its internal graph if nullptr
		);

		if (NewExpression)
		{
			bCreatedViaEditor = true;

			// Ensure expression is also in Material's expression collection
			// Editor API creates in graph but may not add to Material->GetExpressions()
			bool bFoundInMaterial = false;
			for (UMaterialExpression* Expr : Material->GetExpressions())
			{
				if (Expr == NewExpression)
				{
					bFoundInMaterial = true;
					break;
				}
			}
			if (!bFoundInMaterial)
			{
				Material->GetExpressionCollection().AddExpression(NewExpression);
			}
		}
	}

	// Fallback: Use UMaterialEditingLibrary if Material Editor API didn't work
	if (!NewExpression)
	{
		NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
			Material,
			nullptr,  // MaterialFunction
			ExpClass,
			nullptr,  // SelectedAsset
			static_cast<int32>(PosX),
			static_cast<int32>(PosY)
		);

		if (NewExpression)
		{
			// Try to refresh the editor using the newly created expression
			TSharedPtr<IMaterialEditor> MatEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(NewExpression);
			if (MatEditor.IsValid())
			{
				MatEditor->UpdateMaterialAfterGraphChange();
				MatEditor->ForceRefreshExpressionPreviews();
			}
		}
	}

	// Fallback: Create expression manually if editor API isn't available
	if (!NewExpression)
	{
		NewExpression = NewObject<UMaterialExpression>(Material, ExpClass, NAME_None, RF_Transactional);
		if (NewExpression)
		{
			// Set position
			NewExpression->MaterialExpressionEditorX = static_cast<int32>(PosX);
			NewExpression->MaterialExpressionEditorY = static_cast<int32>(PosY);

			// Add to material expression collection
			Material->GetExpressionCollection().AddExpression(NewExpression);

			// Add to material graph - this creates the UMaterialGraphNode
			if (Material->MaterialGraph)
			{
				Material->MaterialGraph->AddExpression(NewExpression, true);
			}
		}
	}

	if (!NewExpression)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Failed to create expression"));
	}

	// Handle parameter name for parameter expressions
	if (Request.JsonBody->HasField(TEXT("param_name")))
	{
		FString ParamName = Request.JsonBody->GetStringField(TEXT("param_name"));

		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpression))
		{
			ScalarParam->ParameterName = FName(*ParamName);
			if (Request.JsonBody->HasField(TEXT("default_value")))
			{
				ScalarParam->DefaultValue = static_cast<float>(Request.JsonBody->GetNumberField(TEXT("default_value")));
			}
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpression))
		{
			VectorParam->ParameterName = FName(*ParamName);
			if (Request.JsonBody->HasField(TEXT("default_value")))
			{
				TSharedPtr<FJsonObject> ColorJson = Request.JsonBody->GetObjectField(TEXT("default_value"));
				VectorParam->DefaultValue.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
				VectorParam->DefaultValue.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
				VectorParam->DefaultValue.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
				VectorParam->DefaultValue.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : 1.0f;
			}
		}
	}

	// Handle constant values
	if (Request.JsonBody->HasField(TEXT("value")))
	{
		if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpression))
		{
			ConstExpr->R = static_cast<float>(Request.JsonBody->GetNumberField(TEXT("value")));
		}
		else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(NewExpression))
		{
			TSharedPtr<FJsonObject> ColorJson = Request.JsonBody->GetObjectField(TEXT("value"));
			Const3Vec->Constant.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			Const3Vec->Constant.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			Const3Vec->Constant.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
		}
	}

	// Mark material as modified
	Material->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	// Only manually recompile if not created via editor API (editor handles its own updates)
	if (!bCreatedViaEditor)
	{
		UMaterialEditingLibrary::RecompileMaterial(Material);
	}
	RefreshMaterialEditorGraph(Material);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), NewExpression->GetName());
	Response->SetStringField(TEXT("expression_class"), ExpClass->GetName());
	Response->SetBoolField(TEXT("created_via_editor_api"), bCreatedViaEditor);
	Response->SetObjectField(TEXT("expression"), ExpressionToJson(NewExpression));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleConnectMaterialNodes(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get source expression
	FString SourceName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("source_expression"), SourceName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UMaterialExpression* SourceExpression = FindExpressionByName(Material, SourceName);
	if (!SourceExpression)
	{
		return FRESTResponse::Error(404, TEXT("SOURCE_NOT_FOUND"),
			FString::Printf(TEXT("Source expression '%s' not found"), *SourceName));
	}

	// Get output index (default 0)
	int32 OutputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("output_index"), 0);

	// Check output index is valid
	TArray<FExpressionOutput>& Outputs = SourceExpression->GetOutputs();
	if (OutputIndex < 0 || OutputIndex >= Outputs.Num())
	{
		return FRESTResponse::Error(400, TEXT("INVALID_OUTPUT_INDEX"),
			FString::Printf(TEXT("Output index %d invalid. Expression has %d outputs."), OutputIndex, Outputs.Num()));
	}

	// Check target - can be a material property or another expression
	FString TargetProperty;
	FString TargetExpression;

	bool bHasTargetProperty = Request.JsonBody->TryGetStringField(TEXT("target_property"), TargetProperty);
	bool bHasTargetExpression = Request.JsonBody->TryGetStringField(TEXT("target_expression"), TargetExpression);

	if (!bHasTargetProperty && !bHasTargetExpression)
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: target_property or target_expression"));
	}

	// Validate connection is possible before attempting
	FString CanConnectError;
	bool bCanConnect = CanConnect(Material, SourceExpression, OutputIndex,
		bHasTargetProperty ? TargetProperty : TEXT(""),
		bHasTargetExpression ? FindExpressionByName(Material, TargetExpression) : nullptr,
		JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("input_index"), 0),
		CanConnectError);

	if (!bCanConnect)
	{
		return FRESTResponse::Error(400, TEXT("CONNECTION_NOT_POSSIBLE"), CanConnectError);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("source_expression"), SourceName);
	Response->SetNumberField(TEXT("output_index"), OutputIndex);

	// ====================================================================
	// Use Graph Pin connections via Schema (proper Material Editor approach)
	// This creates visual wire connections that sync to material data
	// ====================================================================

	// Get source expression's graph node first - we'll get the graph from it
	UMaterialGraphNode* SourceGraphNode = Cast<UMaterialGraphNode>(SourceExpression->GraphNode);
	if (!SourceGraphNode)
	{
		return FRESTResponse::Error(500, TEXT("NO_SOURCE_GRAPH_NODE"),
			FString::Printf(TEXT("Source expression '%s' has no GraphNode. It may not be properly registered in the editor."), *SourceName));
	}

	// Get the material graph from the expression's graph node
	// This is more reliable than Material->MaterialGraph which may not be set
	UMaterialGraph* MaterialGraph = Cast<UMaterialGraph>(SourceGraphNode->GetGraph());
	if (!MaterialGraph)
	{
		return FRESTResponse::Error(500, TEXT("NO_MATERIAL_GRAPH"), TEXT("Could not get MaterialGraph from expression's GraphNode."));
	}

	// Get the graph schema for creating connections
	const UMaterialGraphSchema* Schema = Cast<const UMaterialGraphSchema>(MaterialGraph->GetSchema());
	if (!Schema)
	{
		return FRESTResponse::Error(500, TEXT("NO_SCHEMA"), TEXT("Could not get MaterialGraphSchema"));
	}

	UEdGraphPin* OutputPin = SourceGraphNode->GetOutputPin(OutputIndex);
	if (!OutputPin)
	{
		return FRESTResponse::Error(400, TEXT("INVALID_OUTPUT_PIN"),
			FString::Printf(TEXT("Could not get output pin %d from source expression '%s'"), OutputIndex, *SourceName));
	}

	UEdGraphPin* InputPin = nullptr;

	if (bHasTargetProperty)
	{
		// Connect to material property via RootNode
		EMaterialProperty MatProperty = GetMaterialPropertyFromName(TargetProperty);
		if (MatProperty == MP_MAX)
		{
			return FRESTResponse::Error(400, TEXT("INVALID_TARGET_PROPERTY"),
				FString::Printf(TEXT("Unknown material property: %s. Valid: BaseColor, Metallic, Specular, Roughness, EmissiveColor, Normal, Opacity, OpacityMask, AmbientOcclusion"), *TargetProperty));
		}

		// Get input index for this property on the material graph
		int32 PropertyInputIndex = MaterialGraph->GetInputIndexForProperty(MatProperty);
		if (PropertyInputIndex == INDEX_NONE)
		{
			return FRESTResponse::Error(400, TEXT("PROPERTY_NOT_SUPPORTED"),
				FString::Printf(TEXT("Property '%s' is not available on this material (may not be supported for current material domain)"), *TargetProperty));
		}

		// Get root node's input pin for this property
		if (!MaterialGraph->RootNode)
		{
			return FRESTResponse::Error(500, TEXT("NO_ROOT_NODE"), TEXT("Material graph has no root node"));
		}

		InputPin = MaterialGraph->RootNode->GetInputPin(PropertyInputIndex);
		if (!InputPin)
		{
			return FRESTResponse::Error(500, TEXT("NO_INPUT_PIN"),
				FString::Printf(TEXT("Could not get input pin for property '%s'"), *TargetProperty));
		}

		Response->SetStringField(TEXT("target_property"), TargetProperty);
	}
	else
	{
		// Connect to another expression's input
		UMaterialExpression* TargetExpr = FindExpressionByName(Material, TargetExpression);
		if (!TargetExpr)
		{
			return FRESTResponse::Error(404, TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("Target expression '%s' not found"), *TargetExpression));
		}

		int32 InputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("input_index"), 0);

		// Get target expression's graph node
		UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpr->GraphNode);
		if (!TargetGraphNode)
		{
			return FRESTResponse::Error(500, TEXT("NO_TARGET_GRAPH_NODE"),
				FString::Printf(TEXT("Target expression '%s' has no GraphNode. It may not be properly registered in the editor."), *TargetExpression));
		}

		// Count available inputs for error message
		int32 InputCount = 0;
		while (TargetExpr->GetInput(InputCount) != nullptr)
		{
			InputCount++;
		}

		if (InputIndex < 0 || InputIndex >= InputCount)
		{
			return FRESTResponse::Error(400, TEXT("INVALID_INPUT_INDEX"),
				FString::Printf(TEXT("Input index %d invalid. Expression has %d inputs."), InputIndex, InputCount));
		}

		InputPin = TargetGraphNode->GetInputPin(InputIndex);
		if (!InputPin)
		{
			return FRESTResponse::Error(500, TEXT("NO_INPUT_PIN"),
				FString::Printf(TEXT("Could not get input pin %d from target expression '%s'"), InputIndex, *TargetExpression));
		}

		Response->SetStringField(TEXT("target_expression"), TargetExpression);
		Response->SetNumberField(TEXT("input_index"), InputIndex);
	}

	// Undo support
	FScopedTransaction Transaction(FText::FromString(FString::Printf(
		TEXT("Connect %s to %s"), *SourceName,
		bHasTargetProperty ? *TargetProperty : *TargetExpression)));

	Material->PreEditChange(nullptr);

	// Write through editor graph only (graph-first architecture)
	bool bConnectionMade = Schema->TryCreateConnection(OutputPin, InputPin);

	// Sync graph → data model (equivalent to Apply in editor)
	MaterialGraph->LinkMaterialExpressionsFromGraph();

	// Trigger recompile
	Material->PostEditChange();
	Material->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	Response->SetBoolField(TEXT("connection_verified"), bConnectionMade);

	// Check for compile errors
	FMaterialResource* MaterialResource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
	if (MaterialResource)
	{
		const TArray<FString>& CompileErrors = MaterialResource->GetCompileErrors();
		if (CompileErrors.Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> ErrorsArray;
			for (const FString& Err : CompileErrors)
			{
				ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
			}
			Response->SetArrayField(TEXT("compile_errors"), ErrorsArray);
		}
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleMaterialStatus(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}
	FString MaterialPath = *MaterialPathPtr;

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("material_name"), Material->GetName());

	// Get material domain
	FString DomainStr;
	switch (Material->MaterialDomain)
	{
		case MD_Surface: DomainStr = TEXT("Surface"); break;
		case MD_DeferredDecal: DomainStr = TEXT("DeferredDecal"); break;
		case MD_LightFunction: DomainStr = TEXT("LightFunction"); break;
		case MD_Volume: DomainStr = TEXT("Volume"); break;
		case MD_PostProcess: DomainStr = TEXT("PostProcess"); break;
		case MD_UI: DomainStr = TEXT("UI"); break;
		default: DomainStr = TEXT("Unknown"); break;
	}
	Response->SetStringField(TEXT("domain"), DomainStr);

	// Get blend mode
	FString BlendModeStr;
	switch (Material->BlendMode)
	{
		case BLEND_Opaque: BlendModeStr = TEXT("Opaque"); break;
		case BLEND_Masked: BlendModeStr = TEXT("Masked"); break;
		case BLEND_Translucent: BlendModeStr = TEXT("Translucent"); break;
		case BLEND_Additive: BlendModeStr = TEXT("Additive"); break;
		case BLEND_Modulate: BlendModeStr = TEXT("Modulate"); break;
		default: BlendModeStr = TEXT("Unknown"); break;
	}
	Response->SetStringField(TEXT("blend_mode"), BlendModeStr);

	// Get cached expression data
	Response->SetNumberField(TEXT("expression_count"), Material->GetExpressions().Num());

	// Check compilation status and errors
	TArray<TSharedPtr<FJsonValue>> ErrorsArray;

	// Get material resource for current platform (use SP_PCD3D_SM5 as default for editor)
	FMaterialResource* Resource = Material->GetMaterialResource(SP_PCD3D_SM5);

	bool bHasValidResources = Resource && Resource->GetGameThreadShaderMap() != nullptr;
	Response->SetBoolField(TEXT("has_valid_shader"), bHasValidResources);

	if (Resource)
	{
		// GetCompileErrors returns a const reference
		const TArray<FString>& CompileErrors = Resource->GetCompileErrors();

		for (const FString& CompileError : CompileErrors)
		{
			TSharedPtr<FJsonObject> ErrorObj = MakeShared<FJsonObject>();
			ErrorObj->SetStringField(TEXT("error"), CompileError);
			ErrorsArray.Add(MakeShared<FJsonValueObject>(ErrorObj));
		}
	}

	Response->SetArrayField(TEXT("compile_errors"), ErrorsArray);
	Response->SetBoolField(TEXT("has_errors"), ErrorsArray.Num() > 0);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleRefreshEditor(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Force refresh the Material Editor graph
	RefreshMaterialEditorGraph(Material);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("message"), TEXT("Material Editor graph refreshed"));

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleSetExpressionProperty(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get required fields
	FString ExpressionName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("expression_name"), ExpressionName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	FString PropertyName;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("property"), PropertyName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	if (!Request.JsonBody->HasField(TEXT("value")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: value"));
	}

	// Find the expression
	UMaterialExpression* Expression = FindExpressionByName(Material, ExpressionName);
	if (!Expression)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in material"), *ExpressionName));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("expression_name"), ExpressionName);
	Response->SetStringField(TEXT("property"), PropertyName);

	const TSharedPtr<FJsonValue>& ValueField = Request.JsonBody->Values.FindRef(TEXT("value"));

	// Handle UMaterialExpressionScalarParameter
	if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
	{
		if (PropertyName.Equals(TEXT("DefaultValue"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			float OldValue = ScalarParam->DefaultValue;
			float NewValue = static_cast<float>(ValueField->AsNumber());
			ScalarParam->DefaultValue = NewValue;

			Response->SetNumberField(TEXT("old_value"), OldValue);
			Response->SetNumberField(TEXT("new_value"), NewValue);
		}
		else if (PropertyName.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
		{
			FString OldName = ScalarParam->ParameterName.ToString();
			FString NewName = ValueField->AsString();
			ScalarParam->ParameterName = FName(*NewName);

			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), NewName);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for ScalarParameter. Valid: DefaultValue, ParameterName"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionVectorParameter
	else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
	{
		if (PropertyName.Equals(TEXT("DefaultValue"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			FLinearColor OldValue = VectorParam->DefaultValue;

			TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
			if (!ColorJson.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Value must be an object with r, g, b fields for color/vector properties"));
			}
			FLinearColor NewValue;
			NewValue.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			NewValue.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			NewValue.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
			NewValue.A = ColorJson->HasField(TEXT("a")) ? static_cast<float>(ColorJson->GetNumberField(TEXT("a"))) : OldValue.A;

			VectorParam->DefaultValue = NewValue;

			TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
			OldColorJson->SetNumberField(TEXT("r"), OldValue.R);
			OldColorJson->SetNumberField(TEXT("g"), OldValue.G);
			OldColorJson->SetNumberField(TEXT("b"), OldValue.B);
			OldColorJson->SetNumberField(TEXT("a"), OldValue.A);
			Response->SetObjectField(TEXT("old_value"), OldColorJson);

			TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
			NewColorJson->SetNumberField(TEXT("r"), NewValue.R);
			NewColorJson->SetNumberField(TEXT("g"), NewValue.G);
			NewColorJson->SetNumberField(TEXT("b"), NewValue.B);
			NewColorJson->SetNumberField(TEXT("a"), NewValue.A);
			Response->SetObjectField(TEXT("new_value"), NewColorJson);
		}
		else if (PropertyName.Equals(TEXT("ParameterName"), ESearchCase::IgnoreCase))
		{
			FString OldName = VectorParam->ParameterName.ToString();
			FString NewName = ValueField->AsString();
			VectorParam->ParameterName = FName(*NewName);

			Response->SetStringField(TEXT("old_value"), OldName);
			Response->SetStringField(TEXT("new_value"), NewName);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for VectorParameter. Valid: DefaultValue, ParameterName"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionConstant
	else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
	{
		if (PropertyName.Equals(TEXT("R"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			float OldValue = ConstExpr->R;
			float NewValue = static_cast<float>(ValueField->AsNumber());
			ConstExpr->R = NewValue;

			Response->SetNumberField(TEXT("old_value"), OldValue);
			Response->SetNumberField(TEXT("new_value"), NewValue);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Constant. Valid: R, Value"), *PropertyName));
		}
	}
	// Handle UMaterialExpressionConstant3Vector
	else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(Expression))
	{
		if (PropertyName.Equals(TEXT("Constant"), ESearchCase::IgnoreCase) ||
			PropertyName.Equals(TEXT("Value"), ESearchCase::IgnoreCase))
		{
			FLinearColor OldValue = Const3Vec->Constant;

			TSharedPtr<FJsonObject> ColorJson = ValueField->AsObject();
			if (!ColorJson.IsValid())
			{
				return FRESTResponse::BadRequest(TEXT("Value must be an object with r, g, b fields for color/vector properties"));
			}
			FLinearColor NewValue;
			NewValue.R = static_cast<float>(ColorJson->GetNumberField(TEXT("r")));
			NewValue.G = static_cast<float>(ColorJson->GetNumberField(TEXT("g")));
			NewValue.B = static_cast<float>(ColorJson->GetNumberField(TEXT("b")));
			NewValue.A = 1.0f;

			Const3Vec->Constant = NewValue;

			TSharedPtr<FJsonObject> OldColorJson = MakeShared<FJsonObject>();
			OldColorJson->SetNumberField(TEXT("r"), OldValue.R);
			OldColorJson->SetNumberField(TEXT("g"), OldValue.G);
			OldColorJson->SetNumberField(TEXT("b"), OldValue.B);
			Response->SetObjectField(TEXT("old_value"), OldColorJson);

			TSharedPtr<FJsonObject> NewColorJson = MakeShared<FJsonObject>();
			NewColorJson->SetNumberField(TEXT("r"), NewValue.R);
			NewColorJson->SetNumberField(TEXT("g"), NewValue.G);
			NewColorJson->SetNumberField(TEXT("b"), NewValue.B);
			Response->SetObjectField(TEXT("new_value"), NewColorJson);
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("INVALID_PROPERTY"),
				FString::Printf(TEXT("Property '%s' not supported for Constant3Vector. Valid: Constant, Value"), *PropertyName));
		}
	}
	else
	{
		return FRESTResponse::Error(400, TEXT("UNSUPPORTED_EXPRESSION_TYPE"),
			FString::Printf(TEXT("Expression type '%s' not supported for property editing. Supported: ScalarParameter, VectorParameter, Constant, Constant3Vector"),
				*Expression->GetClass()->GetName()));
	}

	// Recompile material using proper API
	Material->MarkPackageDirty();
	UMaterialEditingLibrary::RecompileMaterial(Material);
	RefreshMaterialEditorGraph(Material);

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleValidateGraph(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}
	FString MaterialPath = *MaterialPathPtr;

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("material_name"), Material->GetName());

	// Get all expressions
	auto Expressions = Material->GetExpressions();
	int32 ExpressionCount = Expressions.Num();
	Response->SetNumberField(TEXT("expression_count"), ExpressionCount);

	// Track issues
	TArray<TSharedPtr<FJsonValue>> IssuesArray;
	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;
	int32 DisconnectedCount = 0;
	int32 ConnectionCount = 0;

	// Build a set of expressions that are used as inputs somewhere
	TSet<UMaterialExpression*> UsedExpressions;

	// Check material property connections
	auto CheckPropertyConnection = [&](FExpressionInput& Input, const FString& PropertyName)
	{
		if (Input.Expression)
		{
			UsedExpressions.Add(Input.Expression);
			ConnectionCount++;

			TSharedPtr<FJsonObject> ConnJson = MakeShared<FJsonObject>();
			ConnJson->SetStringField(TEXT("source"), Input.Expression->GetName());
			ConnJson->SetStringField(TEXT("target_property"), PropertyName);
			ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
		}
	};

	// Check main material properties
	CheckPropertyConnection(Material->GetEditorOnlyData()->BaseColor, TEXT("BaseColor"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->Metallic, TEXT("Metallic"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->Specular, TEXT("Specular"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->Roughness, TEXT("Roughness"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->EmissiveColor, TEXT("EmissiveColor"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->Normal, TEXT("Normal"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->Opacity, TEXT("Opacity"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->OpacityMask, TEXT("OpacityMask"));
	CheckPropertyConnection(Material->GetEditorOnlyData()->AmbientOcclusion, TEXT("AmbientOcclusion"));

	// Check expression-to-expression connections
	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression)
		{
			continue;
		}

		// Iterate through all inputs of this expression
		int32 InputIndex = 0;
		while (FExpressionInput* Input = Expression->GetInput(InputIndex))
		{
			if (Input->Expression)
			{
				UsedExpressions.Add(Input->Expression);
				ConnectionCount++;

				TSharedPtr<FJsonObject> ConnJson = MakeShared<FJsonObject>();
				ConnJson->SetStringField(TEXT("source"), Input->Expression->GetName());
				ConnJson->SetStringField(TEXT("target_expression"), Expression->GetName());
				ConnJson->SetNumberField(TEXT("target_input"), InputIndex);
				ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
			}
			InputIndex++;
		}
	}

	Response->SetNumberField(TEXT("connection_count"), ConnectionCount);
	Response->SetArrayField(TEXT("connections"), ConnectionsArray);

	// Find disconnected nodes (not used anywhere)
	// Exclude parameters since they are meant for material instances
	for (UMaterialExpression* Expression : Expressions)
	{
		if (!Expression)
		{
			continue;
		}

		// Skip parameter expressions - they are intentionally not connected inside the graph
		if (Expression->IsA(UMaterialExpressionScalarParameter::StaticClass()) ||
			Expression->IsA(UMaterialExpressionVectorParameter::StaticClass()))
		{
			continue;
		}

		// Check if this expression is used anywhere
		if (!UsedExpressions.Contains(Expression))
		{
			DisconnectedCount++;

			TSharedPtr<FJsonObject> IssueJson = MakeShared<FJsonObject>();
			IssueJson->SetStringField(TEXT("type"), TEXT("disconnected_node"));
			IssueJson->SetStringField(TEXT("expression"), Expression->GetName());
			IssueJson->SetStringField(TEXT("expression_class"), Expression->GetClass()->GetName());
			IssueJson->SetStringField(TEXT("message"),
				FString::Printf(TEXT("Expression '%s' (%s) is not connected to anything"),
					*Expression->GetName(), *Expression->GetClass()->GetName()));
			IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
		}
	}

	Response->SetNumberField(TEXT("disconnected_nodes"), DisconnectedCount);

	// Check required connections - BaseColor should have something connected for standard materials
	if (!Material->GetEditorOnlyData()->BaseColor.Expression)
	{
		TSharedPtr<FJsonObject> IssueJson = MakeShared<FJsonObject>();
		IssueJson->SetStringField(TEXT("type"), TEXT("missing_required"));
		IssueJson->SetStringField(TEXT("property"), TEXT("BaseColor"));
		IssueJson->SetStringField(TEXT("message"), TEXT("BaseColor has no connected expression"));
		IssuesArray.Add(MakeShared<FJsonValueObject>(IssueJson));
	}

	Response->SetNumberField(TEXT("issue_count"), IssuesArray.Num());
	Response->SetArrayField(TEXT("issues"), IssuesArray);

	// Get compile errors from material resource
	TArray<TSharedPtr<FJsonValue>> CompileErrorsArray;
	FMaterialResource* Resource = Material->GetMaterialResource(SP_PCD3D_SM5);

	bool bHasValidShader = Resource && Resource->GetGameThreadShaderMap() != nullptr;
	Response->SetBoolField(TEXT("has_valid_shader"), bHasValidShader);

	if (Resource)
	{
		const TArray<FString>& CompileErrors = Resource->GetCompileErrors();
		for (const FString& CompileError : CompileErrors)
		{
			CompileErrorsArray.Add(MakeShared<FJsonValueString>(CompileError));
		}
	}

	Response->SetArrayField(TEXT("compile_errors"), CompileErrorsArray);

	// Overall validity
	bool bIsValid = (IssuesArray.Num() == 0) && (CompileErrorsArray.Num() == 0) && bHasValidShader;
	Response->SetBoolField(TEXT("is_valid"), bIsValid);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleDisconnect(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from body
	FString MaterialPath;
	FString MatPathError;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
	{
		return FRESTResponse::BadRequest(MatPathError);
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Check for target - can be a material property or expression
	FString TargetProperty;
	FString TargetExpressionName;

	bool bHasTargetProperty = Request.JsonBody->TryGetStringField(TEXT("target_property"), TargetProperty);
	bool bHasTargetExpression = Request.JsonBody->TryGetStringField(TEXT("target_expression"), TargetExpressionName);

	if (!bHasTargetProperty && !bHasTargetExpression)
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: target_property or target_expression"));
	}

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);

	// ====================================================================
	// Disconnect at both data and graph levels (dual-write approach)
	// ====================================================================

	UEdGraphPin* InputPin = nullptr;
	UMaterialGraph* MaterialGraph = nullptr;

	// Find MaterialGraph from first expression's GraphNode (Material->MaterialGraph may be null)
	{
		auto Expressions = Material->GetExpressions();
		for (UMaterialExpression* Expr : Expressions)
		{
			if (Expr && Expr->GraphNode)
			{
				MaterialGraph = Cast<UMaterialGraph>(Expr->GraphNode->GetGraph());
				if (MaterialGraph) break;
			}
		}
	}
	if (!MaterialGraph)
	{
		// Fallback
		MaterialGraph = Material->MaterialGraph;
	}
	if (!MaterialGraph)
	{
		return FRESTResponse::Error(500, TEXT("NO_MATERIAL_GRAPH"), TEXT("Could not find MaterialGraph"));
	}

	if (bHasTargetProperty)
	{
		// Disconnect from material property via RootNode
		EMaterialProperty MatProperty = GetMaterialPropertyFromName(TargetProperty);
		if (MatProperty == MP_MAX)
		{
			return FRESTResponse::Error(400, TEXT("INVALID_TARGET_PROPERTY"),
				FString::Printf(TEXT("Unknown material property: %s. Valid: BaseColor, Metallic, Specular, Roughness, EmissiveColor, Normal, Opacity, OpacityMask, AmbientOcclusion"), *TargetProperty));
		}

		if (!MaterialGraph->RootNode)
		{
			return FRESTResponse::Error(500, TEXT("NO_ROOT_NODE"), TEXT("Material graph has no root node"));
		}

		int32 PropertyInputIndex = MaterialGraph->GetInputIndexForProperty(MatProperty);
		if (PropertyInputIndex == INDEX_NONE)
		{
			return FRESTResponse::Error(400, TEXT("PROPERTY_NOT_SUPPORTED"),
				FString::Printf(TEXT("Property '%s' is not available on this material"), *TargetProperty));
		}

		InputPin = MaterialGraph->RootNode->GetInputPin(PropertyInputIndex);
		if (!InputPin)
		{
			return FRESTResponse::Error(500, TEXT("NO_INPUT_PIN"),
				FString::Printf(TEXT("Could not get input pin for property '%s'"), *TargetProperty));
		}

		Response->SetStringField(TEXT("target_property"), TargetProperty);
	}
	else
	{
		// Disconnect from expression input
		UMaterialExpression* TargetExpr = FindExpressionByName(Material, TargetExpressionName);
		if (!TargetExpr)
		{
			return FRESTResponse::Error(404, TEXT("TARGET_NOT_FOUND"),
				FString::Printf(TEXT("Target expression '%s' not found"), *TargetExpressionName));
		}

		int32 InputIndex = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("input_index"), 0);

		// Get target expression's graph node
		UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpr->GraphNode);
		if (!TargetGraphNode)
		{
			return FRESTResponse::Error(500, TEXT("NO_TARGET_GRAPH_NODE"),
				FString::Printf(TEXT("Target expression '%s' has no GraphNode"), *TargetExpressionName));
		}

		// Count available inputs for validation
		int32 InputCount = 0;
		while (TargetExpr->GetInput(InputCount) != nullptr)
		{
			InputCount++;
		}

		if (InputIndex < 0 || InputIndex >= InputCount)
		{
			return FRESTResponse::Error(400, TEXT("INVALID_INPUT_INDEX"),
				FString::Printf(TEXT("Input index %d invalid. Expression has %d inputs."), InputIndex, InputCount));
		}

		InputPin = TargetGraphNode->GetInputPin(InputIndex);
		if (!InputPin)
		{
			return FRESTResponse::Error(500, TEXT("NO_INPUT_PIN"),
				FString::Printf(TEXT("Could not get input pin %d from target expression '%s'"), InputIndex, *TargetExpressionName));
		}

		Response->SetStringField(TEXT("target_expression"), TargetExpressionName);
		Response->SetNumberField(TEXT("input_index"), InputIndex);
	}

	// Check if pin is connected
	bool bWasConnected = InputPin->LinkedTo.Num() > 0;
	FString PreviousConnection;
	int32 PreviousOutputIndex = 0;

	if (bWasConnected)
	{
		// Get info about what was connected before breaking
		UEdGraphPin* LinkedPin = InputPin->LinkedTo[0];
		if (LinkedPin)
		{
			UMaterialGraphNode* LinkedNode = Cast<UMaterialGraphNode>(LinkedPin->GetOwningNode());
			if (LinkedNode && LinkedNode->MaterialExpression)
			{
				PreviousConnection = LinkedNode->MaterialExpression->GetName();
				PreviousOutputIndex = LinkedPin->SourceIndex;
			}
		}
	}

	// Disconnect through editor graph only (graph-first architecture)
	if (bWasConnected)
	{
		FScopedTransaction Transaction(FText::FromString(FString::Printf(
			TEXT("Disconnect %s"), bHasTargetProperty ? *TargetProperty : *TargetExpressionName)));

		Material->PreEditChange(nullptr);

		// Write through editor graph only
		InputPin->BreakAllPinLinks();

		// Sync graph → data model (equivalent to Apply in editor)
		MaterialGraph->LinkMaterialExpressionsFromGraph();

		// Trigger recompile
		Material->PostEditChange();
		Material->MarkPackageDirty();

		bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
		SaveAssetIfRequested(Material, bSave);
	}

	Response->SetBoolField(TEXT("was_connected"), bWasConnected);
	if (bWasConnected)
	{
		Response->SetStringField(TEXT("previous_connection"), PreviousConnection);
		Response->SetNumberField(TEXT("previous_output_index"), PreviousOutputIndex);

		// Check for compile errors
		FMaterialResource* MaterialResource = Material->GetMaterialResource(GMaxRHIShaderPlatform);
		if (MaterialResource)
		{
			const TArray<FString>& CompileErrors = MaterialResource->GetCompileErrors();
			if (CompileErrors.Num() > 0)
			{
				TArray<TSharedPtr<FJsonValue>> ErrorsArray;
				for (const FString& Err : CompileErrors)
				{
					ErrorsArray.Add(MakeShared<FJsonValueString>(Err));
				}
				Response->SetArrayField(TEXT("compile_errors"), ErrorsArray);
			}
		}
	}

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleGetConnections(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}
	FString MaterialPath = *MaterialPathPtr;

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get optional expression filter
	const FString* ExpressionFilterPtr = Request.QueryParams.Find(TEXT("expression"));
	FString ExpressionFilter = ExpressionFilterPtr ? *ExpressionFilterPtr : TEXT("");

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());

	TArray<TSharedPtr<FJsonValue>> ConnectionsArray;

	// Lambda to check if connection matches filter
	auto MatchesFilter = [&ExpressionFilter](const FString& ExprName) -> bool
	{
		if (ExpressionFilter.IsEmpty())
		{
			return true;
		}
		return ExprName.Equals(ExpressionFilter, ESearchCase::IgnoreCase);
	};

	// ====================================================================
	// Read connections from editor graph pins (graph-first architecture)
	// ====================================================================

	// Find MaterialGraph from expression GraphNodes
	UMaterialGraph* MaterialGraph = nullptr;
	for (UMaterialExpression* Expr : Material->GetExpressions())
	{
		if (Expr && Expr->GraphNode)
		{
			MaterialGraph = Cast<UMaterialGraph>(Expr->GraphNode->GetGraph());
			if (MaterialGraph) break;
		}
	}
	if (!MaterialGraph)
	{
		MaterialGraph = Material->MaterialGraph;
	}
	if (!MaterialGraph)
	{
		return FRESTResponse::Error(500, TEXT("NO_MATERIAL_GRAPH"), TEXT("Could not find MaterialGraph"));
	}

	// Property connections: read from RootNode input pins
	if (MaterialGraph->RootNode)
	{
		for (UEdGraphPin* Pin : MaterialGraph->RootNode->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
			{
				UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
				UMaterialGraphNode* SourceNode = Cast<UMaterialGraphNode>(LinkedPin->GetOwningNode());
				if (!SourceNode || !SourceNode->MaterialExpression) continue;

				FString SourceName = SourceNode->MaterialExpression->GetName();

				// Map pin back to property name
				FString PropertyName;
				if (Pin->SourceIndex >= 0 && Pin->SourceIndex < MaterialGraph->MaterialInputs.Num())
				{
					EMaterialProperty MatProp = MaterialGraph->MaterialInputs[Pin->SourceIndex].GetProperty();
					PropertyName = GetMaterialPropertyName(MatProp);
				}

				if (PropertyName.IsEmpty()) continue;

				if (MatchesFilter(SourceName))
				{
					TSharedPtr<FJsonObject> ConnJson = MakeShared<FJsonObject>();
					ConnJson->SetStringField(TEXT("type"), TEXT("property"));
					ConnJson->SetStringField(TEXT("source_expression"), SourceName);
					ConnJson->SetNumberField(TEXT("source_output"), LinkedPin->SourceIndex);
					ConnJson->SetStringField(TEXT("target_property"), PropertyName);
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
				}
			}
		}
	}

	// Expression-to-expression connections: read from expression node input pins
	for (UEdGraphNode* Node : MaterialGraph->Nodes)
	{
		UMaterialGraphNode* MatNode = Cast<UMaterialGraphNode>(Node);
		if (!MatNode || static_cast<UEdGraphNode*>(MatNode) == static_cast<UEdGraphNode*>(MaterialGraph->RootNode.Get()) || !MatNode->MaterialExpression) continue;

		FString TargetExprName = MatNode->MaterialExpression->GetName();

		for (UEdGraphPin* Pin : MatNode->Pins)
		{
			if (Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() > 0)
			{
				UEdGraphPin* LinkedPin = Pin->LinkedTo[0];
				UMaterialGraphNode* SourceNode = Cast<UMaterialGraphNode>(LinkedPin->GetOwningNode());
				if (!SourceNode || !SourceNode->MaterialExpression) continue;

				FString SourceExprName = SourceNode->MaterialExpression->GetName();

				if (MatchesFilter(SourceExprName) || MatchesFilter(TargetExprName))
				{
					TSharedPtr<FJsonObject> ConnJson = MakeShared<FJsonObject>();
					ConnJson->SetStringField(TEXT("type"), TEXT("expression"));
					ConnJson->SetStringField(TEXT("source_expression"), SourceExprName);
					ConnJson->SetNumberField(TEXT("source_output"), LinkedPin->SourceIndex);
					ConnJson->SetStringField(TEXT("target_expression"), TargetExprName);
					ConnJson->SetNumberField(TEXT("target_input"), Pin->SourceIndex);
					ConnectionsArray.Add(MakeShared<FJsonValueObject>(ConnJson));
				}
			}
		}
	}

	Response->SetNumberField(TEXT("connection_count"), ConnectionsArray.Num());
	Response->SetArrayField(TEXT("connections"), ConnectionsArray);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleDeleteExpression(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params or body
	FString MaterialPath;
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (MaterialPathPtr && !MaterialPathPtr->IsEmpty())
	{
		MaterialPath = *MaterialPathPtr;
	}
	else if (Request.JsonBody.IsValid())
	{
		FString MatPathError;
		if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("material_path"), MaterialPath, MatPathError))
		{
			return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
		}
	}
	else
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Get expression_name from query params or body
	FString ExpressionName;
	const FString* ExpressionNamePtr = Request.QueryParams.Find(TEXT("expression_name"));
	if (ExpressionNamePtr && !ExpressionNamePtr->IsEmpty())
	{
		ExpressionName = *ExpressionNamePtr;
	}
	else if (Request.JsonBody.IsValid())
	{
		ExpressionName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("expression_name"), TEXT(""));
	}

	if (ExpressionName.IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: expression_name"));
	}

	// Find the expression to delete
	UMaterialExpression* ExpressionToDelete = FindExpressionByName(Material, ExpressionName);
	if (!ExpressionToDelete)
	{
		return FRESTResponse::Error(404, TEXT("EXPRESSION_NOT_FOUND"),
			FString::Printf(TEXT("Expression '%s' not found in material"), *ExpressionName));
	}

	// Store expression info before deletion
	FString ExpressionClass = ExpressionToDelete->GetClass()->GetName();

	// Disconnect all connections TO this expression (from material properties)
	auto DisconnectProperty = [ExpressionToDelete](FExpressionInput& Input)
	{
		if (Input.Expression == ExpressionToDelete)
		{
			Input.Expression = nullptr;
			Input.OutputIndex = 0;
		}
	};

	DisconnectProperty(Material->GetEditorOnlyData()->BaseColor);
	DisconnectProperty(Material->GetEditorOnlyData()->Metallic);
	DisconnectProperty(Material->GetEditorOnlyData()->Specular);
	DisconnectProperty(Material->GetEditorOnlyData()->Roughness);
	DisconnectProperty(Material->GetEditorOnlyData()->EmissiveColor);
	DisconnectProperty(Material->GetEditorOnlyData()->Normal);
	DisconnectProperty(Material->GetEditorOnlyData()->Opacity);
	DisconnectProperty(Material->GetEditorOnlyData()->OpacityMask);
	DisconnectProperty(Material->GetEditorOnlyData()->AmbientOcclusion);

	// Disconnect all connections TO this expression (from other expressions' inputs)
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression || Expression == ExpressionToDelete)
		{
			continue;
		}

		// Iterate through all inputs of this expression and disconnect if pointing to our target
		int32 InputIndex = 0;
		while (FExpressionInput* Input = Expression->GetInput(InputIndex))
		{
			if (Input->Expression == ExpressionToDelete)
			{
				Input->Expression = nullptr;
				Input->OutputIndex = 0;
			}
			InputIndex++;
		}
	}

	// Get the MaterialGraph from the expression's GraphNode (more reliable)
	UMaterialGraph* MaterialGraph = nullptr;
	UEdGraphNode* GraphNodeToRemove = ExpressionToDelete->GraphNode;

	if (GraphNodeToRemove)
	{
		MaterialGraph = Cast<UMaterialGraph>(GraphNodeToRemove->GetGraph());
	}

	if (!MaterialGraph)
	{
		MaterialGraph = Material->MaterialGraph;
	}

	// Use the Material Editor to delete if available - this properly handles UI cleanup
	TSharedPtr<IMaterialEditor> MaterialEditor;
	TSharedPtr<IToolkit> FoundEditor = FToolkitManager::Get().FindEditorForAsset(Material);
	if (FoundEditor.IsValid())
	{
		MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundEditor);
	}

	if (MaterialEditor.IsValid() && GraphNodeToRemove)
	{
		// Use editor's delete functionality which handles UI widget cleanup
		TArray<UEdGraphNode*> NodesToDelete;
		NodesToDelete.Add(GraphNodeToRemove);
		MaterialEditor->DeleteNodes(NodesToDelete);
	}
	else
	{
		// Fallback: Manual deletion when editor not available
		if (MaterialGraph && GraphNodeToRemove)
		{
			MaterialGraph->RemoveNode(GraphNodeToRemove);
		}

		// Remove from material expression collection
		Material->GetExpressionCollection().RemoveExpression(ExpressionToDelete);

		// Mark expression for garbage collection
		ExpressionToDelete->MarkAsGarbage();
	}

	// Notify editor and recompile
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
	}

	Material->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	// Count remaining expressions
	int32 RemainingExpressions = Material->GetExpressions().Num();

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("deleted_expression"), ExpressionName);
	Response->SetStringField(TEXT("expression_class"), ExpressionClass);
	Response->SetNumberField(TEXT("remaining_expressions"), RemainingExpressions);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleExportGraph(const FRESTRequest& Request)
{
	UMaterial* Material = nullptr;
	FString Error;

	// Get required material_path from query params
	const FString* MaterialPathPtr = Request.QueryParams.Find(TEXT("material_path"));
	if (!MaterialPathPtr || MaterialPathPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required parameter: material_path"));
	}
	FString MaterialPath = *MaterialPathPtr;

	if (!FindActiveMaterialEditor(Material, Error, MaterialPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_MATERIAL_EDITOR"), Error);
	}

	// Build XML string manually (UE's XmlParser is read-only)
	FString Xml;
	Xml += TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	Xml += FString::Printf(TEXT("<MaterialGraph name=\"%s\" path=\"%s\">\n"),
		*Material->GetName(), *Material->GetPathName());

	// Settings section
	Xml += TEXT("  <Settings>\n");
	Xml += FString::Printf(TEXT("    <BlendMode>%s</BlendMode>\n"), *GetBlendModeString(Material->BlendMode));
	Xml += FString::Printf(TEXT("    <ShadingModel>%s</ShadingModel>\n"),
		*GetShadingModelString(Material->GetShadingModels().GetFirstShadingModel()));
	Xml += FString::Printf(TEXT("    <TwoSided>%s</TwoSided>\n"), Material->IsTwoSided() ? TEXT("true") : TEXT("false"));
	Xml += TEXT("  </Settings>\n");

	// Nodes section
	Xml += TEXT("  <Nodes>\n");
	TMap<UMaterialExpression*, FString> ExpressionIdMap;

	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression) continue;

		FString NodeId = Expression->GetName();
		ExpressionIdMap.Add(Expression, NodeId);

		Xml += FString::Printf(TEXT("    <Node id=\"%s\" class=\"%s\">\n"),
			*NodeId, *Expression->GetClass()->GetName());

		// Position
		Xml += FString::Printf(TEXT("      <Position x=\"%d\" y=\"%d\"/>\n"),
			Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);

		// Properties (expression-specific)
		Xml += TEXT("      <Properties>\n");

		// Handle specific expression types
		if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			Xml += FString::Printf(TEXT("        <ParameterName>%s</ParameterName>\n"),
				*ScalarParam->ParameterName.ToString());
			Xml += FString::Printf(TEXT("        <DefaultValue>%f</DefaultValue>\n"), ScalarParam->DefaultValue);
		}
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			Xml += FString::Printf(TEXT("        <ParameterName>%s</ParameterName>\n"),
				*VectorParam->ParameterName.ToString());
			Xml += FString::Printf(TEXT("        <DefaultValue r=\"%f\" g=\"%f\" b=\"%f\" a=\"%f\"/>\n"),
				VectorParam->DefaultValue.R, VectorParam->DefaultValue.G,
				VectorParam->DefaultValue.B, VectorParam->DefaultValue.A);
		}
		else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
		{
			Xml += FString::Printf(TEXT("        <Value>%f</Value>\n"), ConstExpr->R);
		}
		else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			Xml += FString::Printf(TEXT("        <Constant r=\"%f\" g=\"%f\" b=\"%f\"/>\n"),
				Const3Vec->Constant.R, Const3Vec->Constant.G, Const3Vec->Constant.B);
		}
		else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(Expression))
		{
			if (TexSample->Texture)
			{
				Xml += FString::Printf(TEXT("        <Texture>%s</Texture>\n"), *TexSample->Texture->GetPathName());
			}
		}

		Xml += TEXT("      </Properties>\n");

		// Outputs
		Xml += TEXT("      <Outputs>\n");
		TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
		for (int32 i = 0; i < Outputs.Num(); i++)
		{
			Xml += FString::Printf(TEXT("        <Output index=\"%d\" name=\"%s\"/>\n"),
				i, *Outputs[i].OutputName.ToString());
		}
		Xml += TEXT("      </Outputs>\n");

		// Inputs (count them)
		int32 InputCount = 0;
		while (Expression->GetInput(InputCount) != nullptr)
		{
			InputCount++;
		}

		if (InputCount > 0)
		{
			Xml += TEXT("      <Inputs>\n");
			for (int32 i = 0; i < InputCount; i++)
			{
				Xml += FString::Printf(TEXT("        <Input index=\"%d\" name=\"%s\"/>\n"),
					i, *Expression->GetInputName(i).ToString());
			}
			Xml += TEXT("      </Inputs>\n");
		}

		Xml += TEXT("    </Node>\n");
	}
	Xml += TEXT("  </Nodes>\n");

	// Connections section
	Xml += TEXT("  <Connections>\n");

	// Add property connections lambda
	auto AddPropertyConnection = [&](FExpressionInput& Input, const FString& PropertyName)
	{
		if (Input.Expression && ExpressionIdMap.Contains(Input.Expression))
		{
			Xml += TEXT("    <Connection>\n");
			Xml += FString::Printf(TEXT("      <Source node=\"%s\" output=\"%d\"/>\n"),
				*ExpressionIdMap[Input.Expression], Input.OutputIndex);
			Xml += FString::Printf(TEXT("      <Target property=\"%s\"/>\n"), *PropertyName);
			Xml += TEXT("    </Connection>\n");
		}
	};

	// Check all material property connections
	AddPropertyConnection(Material->GetEditorOnlyData()->BaseColor, TEXT("BaseColor"));
	AddPropertyConnection(Material->GetEditorOnlyData()->Metallic, TEXT("Metallic"));
	AddPropertyConnection(Material->GetEditorOnlyData()->Specular, TEXT("Specular"));
	AddPropertyConnection(Material->GetEditorOnlyData()->Roughness, TEXT("Roughness"));
	AddPropertyConnection(Material->GetEditorOnlyData()->EmissiveColor, TEXT("EmissiveColor"));
	AddPropertyConnection(Material->GetEditorOnlyData()->Normal, TEXT("Normal"));
	AddPropertyConnection(Material->GetEditorOnlyData()->Opacity, TEXT("Opacity"));
	AddPropertyConnection(Material->GetEditorOnlyData()->OpacityMask, TEXT("OpacityMask"));
	AddPropertyConnection(Material->GetEditorOnlyData()->AmbientOcclusion, TEXT("AmbientOcclusion"));

	// Add expression-to-expression connections
	for (UMaterialExpression* Expression : Material->GetExpressions())
	{
		if (!Expression || !ExpressionIdMap.Contains(Expression)) continue;

		FString TargetNodeId = ExpressionIdMap[Expression];

		int32 InputIndex = 0;
		while (FExpressionInput* Input = Expression->GetInput(InputIndex))
		{
			if (Input->Expression && ExpressionIdMap.Contains(Input->Expression))
			{
				Xml += TEXT("    <Connection>\n");
				Xml += FString::Printf(TEXT("      <Source node=\"%s\" output=\"%d\"/>\n"),
					*ExpressionIdMap[Input->Expression], Input->OutputIndex);
				Xml += FString::Printf(TEXT("      <Target node=\"%s\" input=\"%d\"/>\n"),
					*TargetNodeId, InputIndex);
				Xml += TEXT("    </Connection>\n");
			}
			InputIndex++;
		}
	}

	Xml += TEXT("  </Connections>\n");
	Xml += TEXT("</MaterialGraph>\n");

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("material_name"), Material->GetName());
	Response->SetStringField(TEXT("xml"), Xml);
	Response->SetNumberField(TEXT("node_count"), Material->GetExpressions().Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleImportGraph(const FRESTRequest& Request)
{
	// Get required XML content
	FString XmlContent;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("xml"), XmlContent, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get optional target path (where to create the material)
	FString TargetPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT("/Game/Materials"));

	// Get optional name override
	FString NameOverride = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("name"), TEXT(""));

	// Parse XML using FXmlFile (ConstructFromBuffer parses from string)
	FXmlFile XmlFile(XmlContent, EConstructMethod::ConstructFromBuffer);
	if (!XmlFile.IsValid())
	{
		return FRESTResponse::Error(400, TEXT("INVALID_XML"),
			FString::Printf(TEXT("Failed to parse XML: %s"), *XmlFile.GetLastError()));
	}

	// Get root element
	FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode || RootNode->GetTag() != TEXT("MaterialGraph"))
	{
		return FRESTResponse::Error(400, TEXT("INVALID_XML"), TEXT("Missing MaterialGraph root element"));
	}

	// Get material name from XML or override
	FString XmlName = RootNode->GetAttribute(TEXT("name"));
	FString MaterialName = NameOverride.IsEmpty() ?
		(XmlName.IsEmpty() ? TEXT("ImportedMaterial") : XmlName) :
		NameOverride;

	// Create the material
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(MaterialName, TargetPath, UMaterial::StaticClass(), MaterialFactory);

	if (!NewAsset)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			FString::Printf(TEXT("Failed to create material '%s' at '%s'"), *MaterialName, *TargetPath));
	}

	UMaterial* Material = Cast<UMaterial>(NewAsset);
	if (!Material)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Created asset is not a Material"));
	}

	// Apply settings
	FXmlNode* SettingsNode = RootNode->FindChildNode(TEXT("Settings"));
	if (SettingsNode)
	{
		if (FXmlNode* BlendNode = SettingsNode->FindChildNode(TEXT("BlendMode")))
		{
			FString BlendText = BlendNode->GetContent();
			if (!BlendText.IsEmpty())
			{
				Material->BlendMode = ParseBlendModeString(BlendText);
			}
		}

		if (FXmlNode* TwoSidedNode = SettingsNode->FindChildNode(TEXT("TwoSided")))
		{
			FString TwoSidedText = TwoSidedNode->GetContent();
			Material->TwoSided = TwoSidedText.Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
	}

	// Open material editor for proper graph manipulation
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Material);
	}

	// Wait for editor to initialize
	FPlatformProcess::Sleep(0.1f);

	// Get Material Editor and refresh our Material pointer from it
	// (important: editor may use a different instance than what we created)
	TSharedPtr<IMaterialEditor> MaterialEditor;
	TSharedPtr<IToolkit> FoundEditor = FToolkitManager::Get().FindEditorForAsset(Material);
	if (FoundEditor.IsValid())
	{
		MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundEditor);
	}

	// Reload Material to use the same instance as the editor
	FString MaterialPath = Material->GetPathName();
	Material = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr, *MaterialPath));
	if (!Material)
	{
		return FRESTResponse::Error(500, TEXT("MATERIAL_RELOAD_FAILED"),
			FString::Printf(TEXT("Failed to reload material after opening editor: %s"), *MaterialPath));
	}

	// Create nodes
	TMap<FString, UMaterialExpression*> NodeIdToExpression;
	int32 NodesCreated = 0;

	FXmlNode* NodesNode = RootNode->FindChildNode(TEXT("Nodes"));
	if (NodesNode)
	{
		for (FXmlNode* NodeElem : NodesNode->GetChildrenNodes())
		{
			if (NodeElem->GetTag() != TEXT("Node")) continue;

			FString NodeId = NodeElem->GetAttribute(TEXT("id"));
			FString NodeClass = NodeElem->GetAttribute(TEXT("class"));

			if (NodeId.IsEmpty() || NodeClass.IsEmpty()) continue;

			// Find the class
			UClass* ExpressionClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *NodeClass));
			if (!ExpressionClass)
			{
				// Try without prefix
				ExpressionClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.U%s"), *NodeClass));
			}

			if (!ExpressionClass)
			{
				continue; // Skip unknown classes
			}

			// Get position from XML
			int32 PosX = 0;
			int32 PosY = 0;
			if (FXmlNode* PosNode = NodeElem->FindChildNode(TEXT("Position")))
			{
				PosX = FCString::Atoi(*PosNode->GetAttribute(TEXT("x")));
				PosY = FCString::Atoi(*PosNode->GetAttribute(TEXT("y")));
			}

			UMaterialExpression* NewExpression = nullptr;

			// Try IMaterialEditor::CreateNewMaterialExpression first (creates proper GraphNode)
			if (MaterialEditor.IsValid())
			{
				NewExpression = MaterialEditor->CreateNewMaterialExpression(
					ExpressionClass,
					FVector2D(PosX, PosY),
					false,  // bAutoSelect
					true,   // bAutoAssignResource
					Material->MaterialGraph
				);

				// Ensure expression is also in Material's expression collection
				// (Editor API creates in graph but may not add to Material->GetExpressions())
				if (NewExpression)
				{
					bool bFoundInMaterial = false;
					for (UMaterialExpression* Expr : Material->GetExpressions())
					{
						if (Expr == NewExpression)
						{
							bFoundInMaterial = true;
							break;
						}
					}
					if (!bFoundInMaterial)
					{
						Material->GetExpressionCollection().AddExpression(NewExpression);
					}
				}
			}

			// Fallback to UMaterialEditingLibrary
			if (!NewExpression)
			{
				NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
					Material,
					nullptr,  // MaterialFunction
					ExpressionClass,
					nullptr,  // SelectedAsset
					PosX,
					PosY
				);
			}

			// Final fallback to NewObject
			if (!NewExpression)
			{
				NewExpression = NewObject<UMaterialExpression>(Material, ExpressionClass);
				if (!NewExpression)
				{
					continue;
				}

				// Add to material
				Material->GetExpressionCollection().AddExpression(NewExpression);
				NewExpression->Material = Material;
				NewExpression->MaterialExpressionEditorX = PosX;
				NewExpression->MaterialExpressionEditorY = PosY;

				// Add to graph to create GraphNode
				if (Material->MaterialGraph)
				{
					Material->MaterialGraph->AddExpression(NewExpression, true);
				}
			}

			// Set properties
			if (FXmlNode* PropsNode = NodeElem->FindChildNode(TEXT("Properties")))
			{
				if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpression))
				{
					if (FXmlNode* ParamNameNode = PropsNode->FindChildNode(TEXT("ParameterName")))
					{
						FString ParamNameText = ParamNameNode->GetContent();
						if (!ParamNameText.IsEmpty())
						{
							ScalarParam->ParameterName = FName(*ParamNameText);
						}
					}
					if (FXmlNode* DefaultValueNode = PropsNode->FindChildNode(TEXT("DefaultValue")))
					{
						ScalarParam->DefaultValue = FCString::Atof(*DefaultValueNode->GetContent());
					}
				}
				else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpression))
				{
					if (FXmlNode* ParamNameNode = PropsNode->FindChildNode(TEXT("ParameterName")))
					{
						FString ParamNameText = ParamNameNode->GetContent();
						if (!ParamNameText.IsEmpty())
						{
							VectorParam->ParameterName = FName(*ParamNameText);
						}
					}
					if (FXmlNode* DefaultValueNode = PropsNode->FindChildNode(TEXT("DefaultValue")))
					{
						FString AttrR = DefaultValueNode->GetAttribute(TEXT("r"));
						FString AttrG = DefaultValueNode->GetAttribute(TEXT("g"));
						FString AttrB = DefaultValueNode->GetAttribute(TEXT("b"));
						FString AttrA = DefaultValueNode->GetAttribute(TEXT("a"));
						VectorParam->DefaultValue.R = AttrR.IsEmpty() ? 0.0f : FCString::Atof(*AttrR);
						VectorParam->DefaultValue.G = AttrG.IsEmpty() ? 0.0f : FCString::Atof(*AttrG);
						VectorParam->DefaultValue.B = AttrB.IsEmpty() ? 0.0f : FCString::Atof(*AttrB);
						VectorParam->DefaultValue.A = AttrA.IsEmpty() ? 1.0f : FCString::Atof(*AttrA);
					}
				}
				else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpression))
				{
					if (FXmlNode* ValueNode = PropsNode->FindChildNode(TEXT("Value")))
					{
						ConstExpr->R = FCString::Atof(*ValueNode->GetContent());
					}
				}
				else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(NewExpression))
				{
					if (FXmlNode* ConstantNode = PropsNode->FindChildNode(TEXT("Constant")))
					{
						FString AttrR = ConstantNode->GetAttribute(TEXT("r"));
						FString AttrG = ConstantNode->GetAttribute(TEXT("g"));
						FString AttrB = ConstantNode->GetAttribute(TEXT("b"));
						Const3Vec->Constant.R = AttrR.IsEmpty() ? 0.0f : FCString::Atof(*AttrR);
						Const3Vec->Constant.G = AttrG.IsEmpty() ? 0.0f : FCString::Atof(*AttrG);
						Const3Vec->Constant.B = AttrB.IsEmpty() ? 0.0f : FCString::Atof(*AttrB);
					}
				}
				else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression))
				{
					if (FXmlNode* TextureNode = PropsNode->FindChildNode(TEXT("Texture")))
					{
						FString TexturePath = TextureNode->GetContent();
						if (!TexturePath.IsEmpty())
						{
							TexSample->Texture = LoadObject<UTexture>(nullptr, *TexturePath);
						}
					}
				}
			}

			NodeIdToExpression.Add(NodeId, NewExpression);
			NodesCreated++;
		}
	}

	// Rebuild graph and update editor to ensure GraphNodes are created for all expressions
	if (!MaterialEditor.IsValid())
	{
		MaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(Material);
	}
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
	}
	else if (Material->MaterialGraph)
	{
		Material->MaterialGraph->RebuildGraph();
	}

	// Create connections
	int32 ConnectionsCreated = 0;

	FXmlNode* ConnsNode = RootNode->FindChildNode(TEXT("Connections"));
	if (ConnsNode && NodeIdToExpression.Num() > 0)
	{
		// Get MaterialGraph from one of the created expressions' GraphNode (more reliable than Material->MaterialGraph)
		UMaterialGraph* MaterialGraph = nullptr;
		const UMaterialGraphSchema* Schema = nullptr;

		for (auto& Pair : NodeIdToExpression)
		{
			if (Pair.Value && Pair.Value->GraphNode)
			{
				UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Pair.Value->GraphNode);
				if (GraphNode)
				{
					MaterialGraph = Cast<UMaterialGraph>(GraphNode->GetGraph());
					if (MaterialGraph)
					{
						Schema = Cast<const UMaterialGraphSchema>(MaterialGraph->GetSchema());
						break;
					}
				}
			}
		}

		for (FXmlNode* ConnElem : ConnsNode->GetChildrenNodes())
		{
			if (ConnElem->GetTag() != TEXT("Connection")) continue;

			FXmlNode* SourceNode = ConnElem->FindChildNode(TEXT("Source"));
			FXmlNode* TargetNode = ConnElem->FindChildNode(TEXT("Target"));

			if (!SourceNode || !TargetNode) continue;

			FString SourceNodeId = SourceNode->GetAttribute(TEXT("node"));
			FString SourceOutputStr = SourceNode->GetAttribute(TEXT("output"));
			int32 SourceOutput = SourceOutputStr.IsEmpty() ? 0 : FCString::Atoi(*SourceOutputStr);

			if (SourceNodeId.IsEmpty()) continue;

			UMaterialExpression* SourceExpr = NodeIdToExpression.FindRef(SourceNodeId);
			if (!SourceExpr) continue;

			// Check if target is a property or another node
			FString TargetProperty = TargetNode->GetAttribute(TEXT("property"));
			FString TargetNodeId = TargetNode->GetAttribute(TEXT("node"));

			if (!TargetProperty.IsEmpty() && MaterialGraph && Schema)
			{
				// Connection to material property
				EMaterialProperty MatProp = GetMaterialPropertyFromName(TargetProperty);

				if (MatProp != MP_MAX && MaterialGraph->RootNode)
				{
					// Get source output pin
					UMaterialGraphNode* SourceGraphNode = Cast<UMaterialGraphNode>(SourceExpr->GraphNode);
					if (SourceGraphNode)
					{
						UEdGraphPin* OutputPin = SourceGraphNode->GetOutputPin(SourceOutput);

						// Find the correct input pin on root node
						int32 PropertyInputIndex = MaterialGraph->GetInputIndexForProperty(MatProp);
						if (PropertyInputIndex != INDEX_NONE)
						{
							UEdGraphPin* InputPin = MaterialGraph->RootNode->GetInputPin(PropertyInputIndex);
							if (OutputPin && InputPin)
							{
								Schema->TryCreateConnection(OutputPin, InputPin);

								// Also set the material property directly (graph pin connection may not sync)
								FExpressionInput* PropertyInput = Material->GetExpressionInputForProperty(MatProp);
								if (PropertyInput)
								{
									PropertyInput->Expression = SourceExpr;
									PropertyInput->OutputIndex = SourceOutput;
								}

								ConnectionsCreated++;
							}
						}
					}
				}
			}
			else if (!TargetNodeId.IsEmpty() && MaterialGraph && Schema)
			{
				// Connection to another expression
				FString TargetInputStr = TargetNode->GetAttribute(TEXT("input"));
				int32 TargetInput = TargetInputStr.IsEmpty() ? 0 : FCString::Atoi(*TargetInputStr);

				UMaterialExpression* TargetExpr = NodeIdToExpression.FindRef(TargetNodeId);
				if (TargetExpr && TargetExpr->GraphNode)
				{
					UMaterialGraphNode* SourceGraphNode = Cast<UMaterialGraphNode>(SourceExpr->GraphNode);
					UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpr->GraphNode);

					if (SourceGraphNode && TargetGraphNode)
					{
						UEdGraphPin* OutputPin = SourceGraphNode->GetOutputPin(SourceOutput);
						UEdGraphPin* InputPin = TargetGraphNode->GetInputPin(TargetInput);

						if (OutputPin && InputPin)
						{
							Schema->TryCreateConnection(OutputPin, InputPin);

							// Also set the expression input directly (graph pin connection may not sync)
							FExpressionInput* ExprInput = TargetExpr->GetInput(TargetInput);
							if (ExprInput)
							{
								ExprInput->Expression = SourceExpr;
								ExprInput->OutputIndex = SourceOutput;
							}

							ConnectionsCreated++;
						}
					}
				}
			}
		}
	}

	// Update material after all connections
	Material->PreEditChange(nullptr);
	Material->PostEditChange();
	Material->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Material, bSave);

	// Refresh editor (reuse MaterialEditor from earlier)
	if (!MaterialEditor.IsValid())
	{
		MaterialEditor = FMaterialEditorUtilities::GetIMaterialEditorForObject(Material);
	}
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
		MaterialEditor->ForceRefreshExpressionPreviews();
	}

	// Save the material asset to persist all changes
	UMaterialEditingLibrary::RecompileMaterial(Material);

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("material_path"), Material->GetPathName());
	Response->SetStringField(TEXT("material_name"), Material->GetName());
	Response->SetNumberField(TEXT("nodes_created"), NodesCreated);
	Response->SetNumberField(TEXT("connections_created"), ConnectionsCreated);

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleExportMaterialFunctionGraph(const FRESTRequest& Request)
{
	UMaterialFunction* Function = nullptr;
	FString Error;

	const FString* FunctionPathPtr = Request.QueryParams.Find(TEXT("function_path"));
	FString FunctionPath = FunctionPathPtr ? *FunctionPathPtr : TEXT("");

	if (!FindActiveMaterialFunctionEditor(Function, Error, FunctionPath))
	{
		return FRESTResponse::Error(400, TEXT("NO_FUNCTION_EDITOR"), Error);
	}

	FString Xml;
	Xml += TEXT("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
	Xml += FString::Printf(TEXT("<MaterialFunctionGraph name=\"%s\" path=\"%s\">\n"),
		*Function->GetName(), *Function->GetPathName());

	// Function settings
	Xml += TEXT("  <FunctionSettings>\n");
	Xml += FString::Printf(TEXT("    <Description>%s</Description>\n"), *Function->Description);
	Xml += FString::Printf(TEXT("    <ExposeToLibrary>%s</ExposeToLibrary>\n"),
		Function->bExposeToLibrary ? TEXT("true") : TEXT("false"));
	Xml += TEXT("  </FunctionSettings>\n");

	// Nodes section
	Xml += TEXT("  <Nodes>\n");

	for (UMaterialExpression* Expression : Function->GetExpressions())
	{
		if (!Expression) continue;

		FString NodeId = Expression->GetName();
		Xml += FString::Printf(TEXT("    <Node id=\"%s\" class=\"%s\">\n"),
			*NodeId, *Expression->GetClass()->GetName());

		Xml += FString::Printf(TEXT("      <Position x=\"%d\" y=\"%d\"/>\n"),
			Expression->MaterialExpressionEditorX, Expression->MaterialExpressionEditorY);

		Xml += TEXT("      <Properties>\n");

		// FunctionInput-specific
		if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(Expression))
		{
			Xml += FString::Printf(TEXT("        <InputName>%s</InputName>\n"), *FuncInput->InputName.ToString());
			Xml += FString::Printf(TEXT("        <InputType>%d</InputType>\n"), static_cast<int32>(FuncInput->InputType));
			Xml += FString::Printf(TEXT("        <Description>%s</Description>\n"), *FuncInput->Description);
			Xml += FString::Printf(TEXT("        <SortPriority>%d</SortPriority>\n"), FuncInput->SortPriority);
			Xml += FString::Printf(TEXT("        <UsePreviewValueAsDefault>%s</UsePreviewValueAsDefault>\n"),
				FuncInput->bUsePreviewValueAsDefault ? TEXT("true") : TEXT("false"));
		}
		// FunctionOutput-specific
		else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(Expression))
		{
			Xml += FString::Printf(TEXT("        <OutputName>%s</OutputName>\n"), *FuncOutput->OutputName.ToString());
			Xml += FString::Printf(TEXT("        <Description>%s</Description>\n"), *FuncOutput->Description);
			Xml += FString::Printf(TEXT("        <SortPriority>%d</SortPriority>\n"), FuncOutput->SortPriority);
		}
		// ScalarParameter
		else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(Expression))
		{
			Xml += FString::Printf(TEXT("        <ParameterName>%s</ParameterName>\n"), *ScalarParam->ParameterName.ToString());
			Xml += FString::Printf(TEXT("        <DefaultValue>%f</DefaultValue>\n"), ScalarParam->DefaultValue);
		}
		// VectorParameter
		else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(Expression))
		{
			Xml += FString::Printf(TEXT("        <ParameterName>%s</ParameterName>\n"), *VectorParam->ParameterName.ToString());
			Xml += FString::Printf(TEXT("        <DefaultValue r=\"%f\" g=\"%f\" b=\"%f\" a=\"%f\"/>\n"),
				VectorParam->DefaultValue.R, VectorParam->DefaultValue.G,
				VectorParam->DefaultValue.B, VectorParam->DefaultValue.A);
		}
		// Constant
		else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(Expression))
		{
			Xml += FString::Printf(TEXT("        <R>%f</R>\n"), ConstExpr->R);
		}
		// Constant3Vector
		else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(Expression))
		{
			Xml += FString::Printf(TEXT("        <Constant r=\"%f\" g=\"%f\" b=\"%f\"/>\n"),
				Const3Vec->Constant.R, Const3Vec->Constant.G, Const3Vec->Constant.B);
		}

		Xml += TEXT("      </Properties>\n");

		// Outputs
		Xml += TEXT("      <Outputs>\n");
		TArray<FExpressionOutput>& Outputs = Expression->GetOutputs();
		for (int32 i = 0; i < Outputs.Num(); i++)
		{
			Xml += FString::Printf(TEXT("        <Output index=\"%d\" name=\"%s\"/>\n"),
				i, *Outputs[i].OutputName.ToString());
		}
		Xml += TEXT("      </Outputs>\n");

		// Inputs
		int32 InputCount = 0;
		while (Expression->GetInput(InputCount) != nullptr) InputCount++;
		if (InputCount > 0)
		{
			Xml += TEXT("      <Inputs>\n");
			for (int32 i = 0; i < InputCount; i++)
			{
				Xml += FString::Printf(TEXT("        <Input index=\"%d\" name=\"%s\"/>\n"),
					i, *Expression->GetInputName(i).ToString());
			}
			Xml += TEXT("      </Inputs>\n");
		}

		Xml += TEXT("    </Node>\n");
	}
	Xml += TEXT("  </Nodes>\n");

	// Connections section
	Xml += TEXT("  <Connections>\n");
	for (UMaterialExpression* Expression : Function->GetExpressions())
	{
		if (!Expression) continue;

		int32 InputCount = 0;
		while (Expression->GetInput(InputCount) != nullptr) InputCount++;

		for (int32 InputIdx = 0; InputIdx < InputCount; InputIdx++)
		{
			FExpressionInput* Input = Expression->GetInput(InputIdx);
			if (Input && Input->Expression)
			{
				Xml += FString::Printf(TEXT("    <Connection source=\"%s\" output=\"%d\" target=\"%s\" input=\"%d\"/>\n"),
					*Input->Expression->GetName(), Input->OutputIndex,
					*Expression->GetName(), InputIdx);
			}
		}
	}
	Xml += TEXT("  </Connections>\n");
	Xml += TEXT("</MaterialFunctionGraph>\n");

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("function_path"), Function->GetPathName());
	Response->SetStringField(TEXT("function_name"), Function->GetName());
	Response->SetStringField(TEXT("xml"), Xml);
	Response->SetNumberField(TEXT("node_count"), Function->GetExpressions().Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FMaterialsHandler::HandleImportMaterialFunctionGraph(const FRESTRequest& Request)
{
	// Get required XML content
	FString XmlContent;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("xml"), XmlContent, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	// Get optional target path
	FString TargetPath = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("path"), TEXT("/Game/Materials/Functions"));

	// Get optional name override
	FString NameOverride = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("name"), TEXT(""));

	// Parse XML
	FXmlFile XmlFile(XmlContent, EConstructMethod::ConstructFromBuffer);
	if (!XmlFile.IsValid())
	{
		return FRESTResponse::Error(400, TEXT("INVALID_XML"),
			FString::Printf(TEXT("Failed to parse XML: %s"), *XmlFile.GetLastError()));
	}

	// Get root element
	FXmlNode* RootNode = XmlFile.GetRootNode();
	if (!RootNode || RootNode->GetTag() != TEXT("MaterialFunctionGraph"))
	{
		return FRESTResponse::Error(400, TEXT("INVALID_XML"), TEXT("Missing MaterialFunctionGraph root element"));
	}

	// Get function name from XML or override
	FString XmlName = RootNode->GetAttribute(TEXT("name"));
	FString FunctionName = NameOverride.IsEmpty() ?
		(XmlName.IsEmpty() ? TEXT("ImportedFunction") : XmlName) :
		NameOverride;

	// Create the material function
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	IAssetTools& AssetTools = AssetToolsModule.Get();

	UMaterialFunctionFactoryNew* FunctionFactory = NewObject<UMaterialFunctionFactoryNew>();
	UObject* NewAsset = AssetTools.CreateAsset(FunctionName, TargetPath, UMaterialFunction::StaticClass(), FunctionFactory);

	if (!NewAsset)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"),
			FString::Printf(TEXT("Failed to create material function '%s' at '%s'"), *FunctionName, *TargetPath));
	}

	UMaterialFunction* Function = Cast<UMaterialFunction>(NewAsset);
	if (!Function)
	{
		return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Created asset is not a MaterialFunction"));
	}

	// Apply function settings
	FXmlNode* SettingsNode = RootNode->FindChildNode(TEXT("FunctionSettings"));
	if (SettingsNode)
	{
		if (FXmlNode* DescNode = SettingsNode->FindChildNode(TEXT("Description")))
		{
			Function->Description = DescNode->GetContent();
		}

		if (FXmlNode* ExposeNode = SettingsNode->FindChildNode(TEXT("ExposeToLibrary")))
		{
			Function->bExposeToLibrary = ExposeNode->GetContent().Equals(TEXT("true"), ESearchCase::IgnoreCase);
		}
	}

	// Open function editor for proper graph manipulation
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (AssetEditorSubsystem)
	{
		AssetEditorSubsystem->OpenEditorForAsset(Function);
	}

	// Wait for editor to initialize
	FPlatformProcess::Sleep(0.1f);

	// Get Material Editor
	TSharedPtr<IMaterialEditor> MaterialEditor;
	TSharedPtr<IToolkit> FoundEditor = FToolkitManager::Get().FindEditorForAsset(Function);
	if (FoundEditor.IsValid())
	{
		MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(FoundEditor);
	}

	// Reload function to use the same instance as the editor
	FString FunctionPath = Function->GetPathName();
	Function = Cast<UMaterialFunction>(StaticLoadObject(UMaterialFunction::StaticClass(), nullptr, *FunctionPath));
	if (!Function)
	{
		return FRESTResponse::Error(500, TEXT("FUNCTION_RELOAD_FAILED"),
			FString::Printf(TEXT("Failed to reload function after opening editor: %s"), *FunctionPath));
	}

	// Create nodes
	TMap<FString, UMaterialExpression*> NodeIdToExpression;
	int32 NodesCreated = 0;

	FXmlNode* NodesNode = RootNode->FindChildNode(TEXT("Nodes"));
	if (NodesNode)
	{
		for (FXmlNode* NodeElem : NodesNode->GetChildrenNodes())
		{
			if (NodeElem->GetTag() != TEXT("Node")) continue;

			FString NodeId = NodeElem->GetAttribute(TEXT("id"));
			FString NodeClass = NodeElem->GetAttribute(TEXT("class"));

			if (NodeId.IsEmpty() || NodeClass.IsEmpty()) continue;

			// Find the class
			UClass* ExpressionClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.%s"), *NodeClass));
			if (!ExpressionClass)
			{
				ExpressionClass = FindObject<UClass>(nullptr, *FString::Printf(TEXT("/Script/Engine.U%s"), *NodeClass));
			}

			if (!ExpressionClass)
			{
				continue; // Skip unknown classes
			}

			// Get position from XML
			int32 PosX = 0;
			int32 PosY = 0;
			if (FXmlNode* PosNode = NodeElem->FindChildNode(TEXT("Position")))
			{
				PosX = FCString::Atoi(*PosNode->GetAttribute(TEXT("x")));
				PosY = FCString::Atoi(*PosNode->GetAttribute(TEXT("y")));
			}

			UMaterialExpression* NewExpression = nullptr;

			// Try IMaterialEditor::CreateNewMaterialExpression first
			if (MaterialEditor.IsValid())
			{
				NewExpression = MaterialEditor->CreateNewMaterialExpression(
					ExpressionClass,
					FVector2D(PosX, PosY),
					false,  // bAutoSelect
					true,   // bAutoAssignResource
					Function->MaterialGraph
				);

				if (NewExpression)
				{
					bool bFoundInFunction = false;
					for (UMaterialExpression* Expr : Function->GetExpressions())
					{
						if (Expr == NewExpression)
						{
							bFoundInFunction = true;
							break;
						}
					}
					if (!bFoundInFunction)
					{
						Function->GetExpressionCollection().AddExpression(NewExpression);
					}
				}
			}

			// Fallback to UMaterialEditingLibrary
			if (!NewExpression)
			{
				NewExpression = UMaterialEditingLibrary::CreateMaterialExpressionEx(
					nullptr,    // Material
					Function,   // MaterialFunction
					ExpressionClass,
					nullptr,    // SelectedAsset
					PosX,
					PosY
				);
			}

			// Final fallback to NewObject
			if (!NewExpression)
			{
				NewExpression = NewObject<UMaterialExpression>(Function, ExpressionClass);
				if (!NewExpression)
				{
					continue;
				}

				Function->GetExpressionCollection().AddExpression(NewExpression);
				NewExpression->Function = Function;
				NewExpression->MaterialExpressionEditorX = PosX;
				NewExpression->MaterialExpressionEditorY = PosY;
			}

			// Set properties
			if (FXmlNode* PropsNode = NodeElem->FindChildNode(TEXT("Properties")))
			{
				if (UMaterialExpressionFunctionInput* FuncInput = Cast<UMaterialExpressionFunctionInput>(NewExpression))
				{
					if (FXmlNode* InputNameNode = PropsNode->FindChildNode(TEXT("InputName")))
					{
						FString InputNameText = InputNameNode->GetContent();
						if (!InputNameText.IsEmpty())
						{
							FuncInput->InputName = FName(*InputNameText);
						}
					}
					if (FXmlNode* InputTypeNode = PropsNode->FindChildNode(TEXT("InputType")))
					{
						FuncInput->InputType = static_cast<EFunctionInputType>(FCString::Atoi(*InputTypeNode->GetContent()));
					}
					if (FXmlNode* DescNode = PropsNode->FindChildNode(TEXT("Description")))
					{
						FuncInput->Description = DescNode->GetContent();
					}
					if (FXmlNode* SortNode = PropsNode->FindChildNode(TEXT("SortPriority")))
					{
						FuncInput->SortPriority = FCString::Atoi(*SortNode->GetContent());
					}
					if (FXmlNode* PreviewNode = PropsNode->FindChildNode(TEXT("UsePreviewValueAsDefault")))
					{
						FuncInput->bUsePreviewValueAsDefault = PreviewNode->GetContent().Equals(TEXT("true"), ESearchCase::IgnoreCase);
					}
				}
				else if (UMaterialExpressionFunctionOutput* FuncOutput = Cast<UMaterialExpressionFunctionOutput>(NewExpression))
				{
					if (FXmlNode* OutputNameNode = PropsNode->FindChildNode(TEXT("OutputName")))
					{
						FString OutputNameText = OutputNameNode->GetContent();
						if (!OutputNameText.IsEmpty())
						{
							FuncOutput->OutputName = FName(*OutputNameText);
						}
					}
					if (FXmlNode* DescNode = PropsNode->FindChildNode(TEXT("Description")))
					{
						FuncOutput->Description = DescNode->GetContent();
					}
					if (FXmlNode* SortNode = PropsNode->FindChildNode(TEXT("SortPriority")))
					{
						FuncOutput->SortPriority = FCString::Atoi(*SortNode->GetContent());
					}
				}
				else if (UMaterialExpressionScalarParameter* ScalarParam = Cast<UMaterialExpressionScalarParameter>(NewExpression))
				{
					if (FXmlNode* ParamNameNode = PropsNode->FindChildNode(TEXT("ParameterName")))
					{
						FString ParamNameText = ParamNameNode->GetContent();
						if (!ParamNameText.IsEmpty())
						{
							ScalarParam->ParameterName = FName(*ParamNameText);
						}
					}
					if (FXmlNode* DefaultValueNode = PropsNode->FindChildNode(TEXT("DefaultValue")))
					{
						ScalarParam->DefaultValue = FCString::Atof(*DefaultValueNode->GetContent());
					}
				}
				else if (UMaterialExpressionVectorParameter* VectorParam = Cast<UMaterialExpressionVectorParameter>(NewExpression))
				{
					if (FXmlNode* ParamNameNode = PropsNode->FindChildNode(TEXT("ParameterName")))
					{
						FString ParamNameText = ParamNameNode->GetContent();
						if (!ParamNameText.IsEmpty())
						{
							VectorParam->ParameterName = FName(*ParamNameText);
						}
					}
					if (FXmlNode* DefaultValueNode = PropsNode->FindChildNode(TEXT("DefaultValue")))
					{
						FString AttrR = DefaultValueNode->GetAttribute(TEXT("r"));
						FString AttrG = DefaultValueNode->GetAttribute(TEXT("g"));
						FString AttrB = DefaultValueNode->GetAttribute(TEXT("b"));
						FString AttrA = DefaultValueNode->GetAttribute(TEXT("a"));
						VectorParam->DefaultValue.R = AttrR.IsEmpty() ? 0.0f : FCString::Atof(*AttrR);
						VectorParam->DefaultValue.G = AttrG.IsEmpty() ? 0.0f : FCString::Atof(*AttrG);
						VectorParam->DefaultValue.B = AttrB.IsEmpty() ? 0.0f : FCString::Atof(*AttrB);
						VectorParam->DefaultValue.A = AttrA.IsEmpty() ? 1.0f : FCString::Atof(*AttrA);
					}
				}
				else if (UMaterialExpressionConstant* ConstExpr = Cast<UMaterialExpressionConstant>(NewExpression))
				{
					if (FXmlNode* ValueNode = PropsNode->FindChildNode(TEXT("R")))
					{
						ConstExpr->R = FCString::Atof(*ValueNode->GetContent());
					}
				}
				else if (UMaterialExpressionConstant3Vector* Const3Vec = Cast<UMaterialExpressionConstant3Vector>(NewExpression))
				{
					if (FXmlNode* ConstantNode = PropsNode->FindChildNode(TEXT("Constant")))
					{
						FString AttrR = ConstantNode->GetAttribute(TEXT("r"));
						FString AttrG = ConstantNode->GetAttribute(TEXT("g"));
						FString AttrB = ConstantNode->GetAttribute(TEXT("b"));
						Const3Vec->Constant.R = AttrR.IsEmpty() ? 0.0f : FCString::Atof(*AttrR);
						Const3Vec->Constant.G = AttrG.IsEmpty() ? 0.0f : FCString::Atof(*AttrG);
						Const3Vec->Constant.B = AttrB.IsEmpty() ? 0.0f : FCString::Atof(*AttrB);
					}
				}
				else if (UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(NewExpression))
				{
					if (FXmlNode* TextureNode = PropsNode->FindChildNode(TEXT("Texture")))
					{
						FString TexturePath = TextureNode->GetContent();
						if (!TexturePath.IsEmpty())
						{
							TexSample->Texture = LoadObject<UTexture>(nullptr, *TexturePath);
						}
					}
				}
			}

			NodeIdToExpression.Add(NodeId, NewExpression);
			NodesCreated++;
		}
	}

	// Rebuild graph and update editor
	if (!MaterialEditor.IsValid())
	{
		TSharedPtr<IToolkit> EditorFound = FToolkitManager::Get().FindEditorForAsset(Function);
		if (EditorFound.IsValid())
		{
			MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(EditorFound);
		}
	}
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
	}

	// Create connections
	int32 ConnectionsCreated = 0;

	FXmlNode* ConnsNode = RootNode->FindChildNode(TEXT("Connections"));
	if (ConnsNode && NodeIdToExpression.Num() > 0)
	{
		// Get MaterialGraph and schema from one of the created expressions
		UMaterialGraph* MaterialGraph = nullptr;
		const UMaterialGraphSchema* Schema = nullptr;

		for (auto& Pair : NodeIdToExpression)
		{
			if (Pair.Value && Pair.Value->GraphNode)
			{
				UMaterialGraphNode* GraphNode = Cast<UMaterialGraphNode>(Pair.Value->GraphNode);
				if (GraphNode)
				{
					MaterialGraph = Cast<UMaterialGraph>(GraphNode->GetGraph());
					if (MaterialGraph)
					{
						Schema = Cast<const UMaterialGraphSchema>(MaterialGraph->GetSchema());
						break;
					}
				}
			}
		}

		for (FXmlNode* ConnElem : ConnsNode->GetChildrenNodes())
		{
			if (ConnElem->GetTag() != TEXT("Connection")) continue;

			// Parse attribute-style connections (matches export format)
			FString SourceNodeId = ConnElem->GetAttribute(TEXT("source"));
			FString SourceOutputStr = ConnElem->GetAttribute(TEXT("output"));
			int32 SourceOutput = SourceOutputStr.IsEmpty() ? 0 : FCString::Atoi(*SourceOutputStr);

			FString TargetNodeId = ConnElem->GetAttribute(TEXT("target"));
			FString TargetInputStr = ConnElem->GetAttribute(TEXT("input"));
			int32 TargetInput = TargetInputStr.IsEmpty() ? 0 : FCString::Atoi(*TargetInputStr);

			if (SourceNodeId.IsEmpty() || TargetNodeId.IsEmpty()) continue;

			UMaterialExpression* SourceExpr = NodeIdToExpression.FindRef(SourceNodeId);
			UMaterialExpression* TargetExpr = NodeIdToExpression.FindRef(TargetNodeId);

			if (!SourceExpr || !TargetExpr) continue;

			// Try graph schema connection first
			if (MaterialGraph && Schema && SourceExpr->GraphNode && TargetExpr->GraphNode)
			{
				UMaterialGraphNode* SourceGraphNode = Cast<UMaterialGraphNode>(SourceExpr->GraphNode);
				UMaterialGraphNode* TargetGraphNode = Cast<UMaterialGraphNode>(TargetExpr->GraphNode);

				if (SourceGraphNode && TargetGraphNode)
				{
					UEdGraphPin* OutputPin = SourceGraphNode->GetOutputPin(SourceOutput);
					UEdGraphPin* InputPin = TargetGraphNode->GetInputPin(TargetInput);

					if (OutputPin && InputPin)
					{
						Schema->TryCreateConnection(OutputPin, InputPin);

						// Also set the expression input directly
						FExpressionInput* ExprInput = TargetExpr->GetInput(TargetInput);
						if (ExprInput)
						{
							ExprInput->Expression = SourceExpr;
							ExprInput->OutputIndex = SourceOutput;
						}

						ConnectionsCreated++;
					}
				}
			}
			else
			{
				// Fallback: set expression input directly
				FExpressionInput* ExprInput = TargetExpr->GetInput(TargetInput);
				if (ExprInput)
				{
					ExprInput->Expression = SourceExpr;
					ExprInput->OutputIndex = SourceOutput;
					ConnectionsCreated++;
				}
			}
		}
	}

	// Update function after all changes
	Function->PreEditChange(nullptr);
	Function->PostEditChange();
	Function->MarkPackageDirty();

	bool bSave = JsonHelpers::GetOptionalBool(Request.JsonBody, TEXT("save"), true);
	SaveAssetIfRequested(Function, bSave);

	// Refresh editor
	if (!MaterialEditor.IsValid())
	{
		TSharedPtr<IToolkit> EditorFound = FToolkitManager::Get().FindEditorForAsset(Function);
		if (EditorFound.IsValid())
		{
			MaterialEditor = StaticCastSharedPtr<IMaterialEditor>(EditorFound);
		}
	}
	if (MaterialEditor.IsValid())
	{
		MaterialEditor->UpdateMaterialAfterGraphChange();
		MaterialEditor->ForceRefreshExpressionPreviews();
	}

	// Update the material function
	UMaterialEditingLibrary::UpdateMaterialFunction(Function, nullptr);

	// Build response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("function_path"), Function->GetPathName());
	Response->SetStringField(TEXT("function_name"), Function->GetName());
	Response->SetNumberField(TEXT("nodes_created"), NodesCreated);
	Response->SetNumberField(TEXT("connections_created"), ConnectionsCreated);

	return FRESTResponse::Ok(Response);
}

TArray<TSharedPtr<FJsonObject>> FMaterialsHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/param")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get material parameter value (query: material_path, param_name)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/param")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Set material parameter value (scalar, vector, texture). For MIC: material_path. For MID: actor_label, material_index"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/recompile")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Force recompile material for rendering"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/replace")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Swap material on actor(s) (body: label/labels[], material_path)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/create")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Create new material asset (body: name, path)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/instance/create")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Create MaterialInstanceConstant from parent material (body: name, parent_material, path?, parameters?)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/instance/dynamic")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Create MaterialInstanceDynamic on actor (body: actor_label, material_index?, source_material?, name?, parameters?)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/open")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Open material in Material Editor"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/nodes")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("List material expression nodes"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/node/position")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Move material expression node"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/node/create")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Create material expression (ScalarParameter, VectorParameter, Constant, etc.)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/connect")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Connect expression output to input or material property (validates before connecting)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/status")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get material compilation status and errors"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/refresh")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Refresh Material Editor graph"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/expression/set")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Set expression property (DefaultValue, ParameterName, etc.)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/validate")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Validate material graph for disconnected nodes, missing connections, and compile errors"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/disconnect")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Disconnect input from property or expression"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/connections")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("List all connections in material graph (query: expression to filter)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("DELETE")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/node")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Delete expression and all its connections from material"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/export")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Export material graph to XML format (query: material_path)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/editor/import")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Import material from XML definition (body: xml, path?, name?)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/function/editor/export")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Export material function graph to XML format (query: function_path)"));

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/materials/function/editor/import")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Import material function from XML definition (body: xml, path?, name?)"));

	return Schemas;
}
