// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "RESTRouter.h"

/**
 * Blueprint Editor node manipulation endpoints.
 *
 * Provides full access to Blueprint Editor for node inspection and editing.
 * Requires an open Blueprint Editor with an active graph.
 *
 * Endpoints:
 *   GET  /blueprints/selection      - Get selected nodes in Blueprint Editor
 *   GET  /blueprints/node_info      - Get details about a specific BP node
 *   GET  /blueprints/nodes          - List all nodes in a graph
 *   POST /blueprints/node/position  - Move node to new position
 *   POST /blueprints/node/create    - Create a new node
 *   DELETE /blueprints/node         - Delete a node
 *   POST /blueprints/connect        - Connect two pins
 *   POST /blueprints/disconnect     - Break pin connections
 *   POST /blueprints/pin/default    - Set pin default value
 */
class FBlueprintsHandler : public IRESTHandler
{
public:
	// IRESTHandler interface
	virtual FString GetBasePath() const override { return TEXT("/blueprints"); }
	virtual FString GetHandlerName() const override { return TEXT("Blueprints"); }
	virtual FString GetDescription() const override { return TEXT("Blueprint editor node inspection"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;

private:
	/** GET /blueprints/selection - Get selected nodes in Blueprint Editor */
	FRESTResponse HandleSelection(const FRESTRequest& Request);

	/** GET /blueprints/node_info - Get details about a specific BP node */
	FRESTResponse HandleNodeInfo(const FRESTRequest& Request);

	/** GET /blueprints/nodes - List all nodes in a graph */
	FRESTResponse HandleListNodes(const FRESTRequest& Request);

	/** POST /blueprints/node/position - Move node to new position */
	FRESTResponse HandleSetNodePosition(const FRESTRequest& Request);

	/** POST /blueprints/node/create - Create a new node */
	FRESTResponse HandleCreateNode(const FRESTRequest& Request);

	/** DELETE /blueprints/node - Delete a node */
	FRESTResponse HandleDeleteNode(const FRESTRequest& Request);

	/** POST /blueprints/connect - Connect two pins */
	FRESTResponse HandleConnect(const FRESTRequest& Request);

	/** POST /blueprints/disconnect - Break pin connections */
	FRESTResponse HandleDisconnect(const FRESTRequest& Request);

	/** POST /blueprints/pin/default - Set pin default value */
	FRESTResponse HandleSetPinDefault(const FRESTRequest& Request);

	/**
	 * Find the active Blueprint Editor and its edited Blueprint.
	 * @param OutBlueprint - The currently edited Blueprint (if found)
	 * @param OutError - Error message if editor not found
	 * @return The active FBlueprintEditor or nullptr
	 */
	class FBlueprintEditor* FindActiveBlueprintEditor(class UBlueprint*& OutBlueprint, FString& OutError);

	/** Convert UEdGraphNode to JSON representation */
	TSharedPtr<FJsonObject> NodeToJson(class UEdGraphNode* Node);

	/** Convert UEdGraphPin to JSON representation */
	TSharedPtr<FJsonObject> PinToJson(class UEdGraphPin* Pin);

	/** Find node by GUID in blueprint */
	class UEdGraphNode* FindNodeByGuid(class UBlueprint* Blueprint, const FGuid& NodeGuid);

	/** Find pin by name on a node */
	class UEdGraphPin* FindPinByName(class UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction = EGPD_MAX);

	/** Find graph by name in blueprint */
	class UEdGraph* FindGraphByName(class UBlueprint* Blueprint, const FString& GraphName);
};
