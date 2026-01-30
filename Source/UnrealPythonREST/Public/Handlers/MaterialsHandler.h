// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Material parameter access and replacement endpoints.
 * Provides access to material parameters and allows material swapping on actors.
 *
 * Endpoints:
 *   GET  /materials/param                    - Get parameter value (query: material_path, param_name)
 *   POST /materials/param                    - Set parameter value (MIC: material_path; MID: actor_label, material_index)
 *   POST /materials/recompile                - Rebuild material (body: material_path)
 *   POST /materials/replace                  - Swap material on actors (body: label or labels[], material_path)
 *   POST /materials/create                   - Create new material asset
 *   POST /materials/instance/create          - Create MaterialInstanceConstant from parent material
 *   POST /materials/instance/dynamic         - Create MaterialInstanceDynamic on actor
 *   POST /materials/editor/open              - Open material in Material Editor
 *   GET  /materials/editor/nodes             - List nodes in open Material Editor
 *   POST /materials/editor/node/position     - Move material expression node
 *   POST /materials/editor/node/create       - Create new material expression node
 *   POST /materials/editor/connect           - Connect material expression outputs to inputs
 *   POST /materials/editor/disconnect        - Disconnect input from property or expression
 *   GET  /materials/editor/connections       - List all connections in material graph
 *   GET  /materials/editor/status            - Get material compilation status and errors
 *   POST /materials/editor/refresh           - Refresh Material Editor to show latest changes
 *   POST /materials/editor/expression/set    - Set expression property value on base Material
 *   GET  /materials/editor/validate          - Validate material graph for issues
 *   DELETE /materials/editor/node            - Delete expression and all its connections
 *   GET  /materials/editor/export            - Export material graph as XML
 *   POST /materials/editor/import            - Import material graph from XML
 */
class FMaterialsHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/materials"); }
	virtual FString GetHandlerName() const override { return TEXT("Materials"); }
	virtual FString GetDescription() const override { return TEXT("Material parameter access and replacement"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;

private:
	/** GET /materials/param - Get material parameter value */
	FRESTResponse HandleGetParam(const FRESTRequest& Request);

	/** POST /materials/param - Set material parameter value */
	FRESTResponse HandleSetParam(const FRESTRequest& Request);

	/** POST /materials/recompile - Force recompile material for rendering */
	FRESTResponse HandleRecompile(const FRESTRequest& Request);

	/** POST /materials/replace - Replace material on actor(s) */
	FRESTResponse HandleReplace(const FRESTRequest& Request);

	/** POST /materials/create - Create new material asset */
	FRESTResponse HandleCreateMaterial(const FRESTRequest& Request);

	/** POST /materials/instance/create - Create new MaterialInstanceConstant from parent */
	FRESTResponse HandleCreateMaterialInstance(const FRESTRequest& Request);

	/** POST /materials/instance/dynamic - Create runtime MaterialInstanceDynamic on actor */
	FRESTResponse HandleCreateDynamicMaterialInstance(const FRESTRequest& Request);

	/** POST /materials/editor/open - Open material in Material Editor */
	FRESTResponse HandleOpenMaterialEditor(const FRESTRequest& Request);

	/** GET /materials/editor/nodes - List all expression nodes in open Material Editor */
	FRESTResponse HandleListMaterialNodes(const FRESTRequest& Request);

	/** POST /materials/editor/node/position - Move material expression to new position */
	FRESTResponse HandleSetMaterialNodePosition(const FRESTRequest& Request);

	/** POST /materials/editor/node/create - Create new material expression node */
	FRESTResponse HandleCreateMaterialNode(const FRESTRequest& Request);

	/** POST /materials/editor/connect - Connect material expression output to input */
	FRESTResponse HandleConnectMaterialNodes(const FRESTRequest& Request);

	/** GET /materials/editor/status - Get material compilation status and errors */
	FRESTResponse HandleMaterialStatus(const FRESTRequest& Request);

	/** POST /materials/editor/refresh - Refresh Material Editor to show latest changes */
	FRESTResponse HandleRefreshEditor(const FRESTRequest& Request);

	/** POST /materials/editor/expression/set - Set expression property value on base Material */
	FRESTResponse HandleSetExpressionProperty(const FRESTRequest& Request);

	/** GET /materials/editor/validate - Validate material graph for issues */
	FRESTResponse HandleValidateGraph(const FRESTRequest& Request);

	/** POST /materials/editor/disconnect - Disconnect an input */
	FRESTResponse HandleDisconnect(const FRESTRequest& Request);

	/** GET /materials/editor/connections - List all connections in material */
	FRESTResponse HandleGetConnections(const FRESTRequest& Request);

	/** DELETE /materials/editor/node - Delete expression and its connections */
	FRESTResponse HandleDeleteExpression(const FRESTRequest& Request);

	/** Export material graph as XML */
	FRESTResponse HandleExportGraph(const FRESTRequest& Request);

	/** Import material graph from XML */
	FRESTResponse HandleImportGraph(const FRESTRequest& Request);

	// Material Function endpoints
	/** POST /materials/function/create - Create new material function asset */
	FRESTResponse HandleCreateMaterialFunction(const FRESTRequest& Request);

	/** POST /materials/function/editor/open - Open material function in editor */
	FRESTResponse HandleOpenMaterialFunctionEditor(const FRESTRequest& Request);

	/** GET /materials/function/editor/nodes - List expressions in function */
	FRESTResponse HandleListMaterialFunctionNodes(const FRESTRequest& Request);

	/** POST /materials/function/editor/node/create - Create expression in function */
	FRESTResponse HandleCreateMaterialFunctionNode(const FRESTRequest& Request);

	/** POST /materials/function/editor/node/position - Move expression in function */
	FRESTResponse HandleSetMaterialFunctionNodePosition(const FRESTRequest& Request);

	/** POST /materials/function/editor/connect - Connect expressions in function */
	FRESTResponse HandleConnectMaterialFunctionNodes(const FRESTRequest& Request);

	/** POST /materials/function/editor/disconnect - Disconnect expression input in function */
	FRESTResponse HandleDisconnectMaterialFunction(const FRESTRequest& Request);

	/** POST /materials/function/editor/expression/set - Set expression property in function */
	FRESTResponse HandleSetMaterialFunctionExpressionProperty(const FRESTRequest& Request);

	/** DELETE /materials/function/editor/node - Delete expression from function */
	FRESTResponse HandleDeleteMaterialFunctionExpression(const FRESTRequest& Request);

	/** GET /materials/function/editor/export - Export function graph as XML */
	FRESTResponse HandleExportMaterialFunctionGraph(const FRESTRequest& Request);

	/** POST /materials/function/editor/import - Import function graph from XML */
	FRESTResponse HandleImportMaterialFunctionGraph(const FRESTRequest& Request);

	/**
	 * Find the active Material Editor and its edited Material.
	 * @param OutMaterial - The currently edited Material (if found)
	 * @param OutError - Error message if editor not found
	 * @param MaterialPath - Optional specific material path to find (if empty, returns first open material)
	 * @return true if a Material Editor with material was found
	 */
	bool FindActiveMaterialEditor(class UMaterial*& OutMaterial, FString& OutError, const FString& MaterialPath = TEXT(""));

	/** Find an active material function editor for the given function path */
	bool FindActiveMaterialFunctionEditor(class UMaterialFunction*& OutFunction, FString& OutError, const FString& FunctionPath);

	/** Find expression by name in a material function */
	class UMaterialExpression* FindExpressionInFunctionByName(class UMaterialFunction* Function, const FString& Name);

	/** Refresh the Material Editor graph view after modifications */
	void RefreshMaterialEditorGraph(class UMaterial* Material);

	/** Convert UMaterialExpression to JSON representation */
	TSharedPtr<FJsonObject> ExpressionToJson(class UMaterialExpression* Expression);

	/** Find expression by name in material */
	class UMaterialExpression* FindExpressionByName(class UMaterial* Material, const FString& Name);

	/**
	 * Check if a connection is possible between source and target.
	 * Validates output/input indices exist, checks type compatibility if possible.
	 * @param Material - The material being edited
	 * @param SourceExpression - The source expression to connect from
	 * @param OutputIndex - Which output of the source to use
	 * @param TargetProperty - Material property name (BaseColor, Normal, etc.) or empty
	 * @param TargetExpression - Target expression to connect to, or nullptr if connecting to property
	 * @param InputIndex - Which input of target expression to connect to
	 * @param OutError - Error message if connection not possible
	 * @return true if connection is possible
	 */
	bool CanConnect(class UMaterial* Material, class UMaterialExpression* SourceExpression, int32 OutputIndex,
		const FString& TargetProperty, class UMaterialExpression* TargetExpression, int32 InputIndex, FString& OutError);

	/**
	 * Verify that a connection actually exists between source and target.
	 * Called after Connect() to confirm the operation succeeded.
	 * @param Material - The material being edited
	 * @param SourceExpression - The expected source expression
	 * @param OutputIndex - Expected output index
	 * @param TargetProperty - Material property name or empty
	 * @param TargetExpression - Target expression or nullptr if checking property
	 * @param InputIndex - Target input index (only used if TargetExpression is valid)
	 * @return true if the connection exists as expected
	 */
	bool VerifyConnection(class UMaterial* Material, class UMaterialExpression* SourceExpression, int32 OutputIndex,
		const FString& TargetProperty, class UMaterialExpression* TargetExpression = nullptr, int32 InputIndex = 0);

	/**
	 * Get information about what is currently connected to a material property.
	 * @param Material - The material to query
	 * @param PropertyName - Property name (BaseColor, Metallic, etc.)
	 * @return JSON with connected_expression name and output_index, or null fields if not connected
	 */
	TSharedPtr<FJsonObject> GetPropertyConnectionInfo(class UMaterial* Material, const FString& PropertyName);
};
