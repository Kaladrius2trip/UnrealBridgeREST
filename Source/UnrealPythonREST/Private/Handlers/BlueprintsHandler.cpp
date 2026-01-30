// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/BlueprintsHandler.h"
#include "Utils/JsonHelpers.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintEditor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Variable.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void FBlueprintsHandler::RegisterRoutes(FRESTRouter& Router)
{
	// Read endpoints
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/blueprints/selection"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleSelection));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/blueprints/node_info"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleNodeInfo));

	Router.RegisterRoute(ERESTMethod::GET, TEXT("/blueprints/nodes"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleListNodes));

	// Write endpoints
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/blueprints/node/position"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleSetNodePosition));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/blueprints/node/create"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleCreateNode));

	Router.RegisterRoute(ERESTMethod::DELETE, TEXT("/blueprints/node"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleDeleteNode));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/blueprints/connect"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleConnect));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/blueprints/disconnect"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleDisconnect));

	Router.RegisterRoute(ERESTMethod::POST, TEXT("/blueprints/pin/default"),
		FRESTRouteHandler::CreateRaw(this, &FBlueprintsHandler::HandleSetPinDefault));

	UE_LOG(LogTemp, Log, TEXT("BlueprintsHandler: Registered 9 routes at /blueprints"));
}

FBlueprintEditor* FBlueprintsHandler::FindActiveBlueprintEditor(UBlueprint*& OutBlueprint, FString& OutError)
{
	OutBlueprint = nullptr;

	if (!GEditor)
	{
		OutError = TEXT("NO_EDITOR");
		return nullptr;
	}

	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		OutError = TEXT("NO_SUBSYSTEM");
		return nullptr;
	}

	// Find open Blueprint editors
	TArray<UObject*> EditedAssets = AssetEditorSubsystem->GetAllEditedAssets();

	for (UObject* Asset : EditedAssets)
	{
		UBlueprint* Blueprint = Cast<UBlueprint>(Asset);
		if (Blueprint)
		{
			IAssetEditorInstance* EditorInstance = AssetEditorSubsystem->FindEditorForAsset(Blueprint, false);
			// Note: Blueprint assets are only opened by FBlueprintEditor in standard configurations
			if (EditorInstance && EditorInstance->GetEditorName() == TEXT("BlueprintEditor"))
			{
				OutBlueprint = Blueprint;
				return static_cast<FBlueprintEditor*>(EditorInstance);
			}
		}
	}

	OutError = TEXT("NO_BLUEPRINT_EDITOR");
	return nullptr;
}

FRESTResponse FBlueprintsHandler::HandleSelection(const FRESTRequest& Request)
{
	UBlueprint* EditedBlueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* BlueprintEditor = FindActiveBlueprintEditor(EditedBlueprint, ErrorCode);

	if (!BlueprintEditor)
	{
		if (ErrorCode == TEXT("NO_EDITOR"))
		{
			return FRESTResponse::Error(400, TEXT("NO_EDITOR"), TEXT("Editor not available"));
		}
		else if (ErrorCode == TEXT("NO_SUBSYSTEM"))
		{
			return FRESTResponse::Error(400, TEXT("NO_SUBSYSTEM"), TEXT("AssetEditorSubsystem not available"));
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("NO_BLUEPRINT_EDITOR"),
				TEXT("No Blueprint Editor is open. Open a Blueprint asset to use this endpoint."));
		}
	}

	// Get selected nodes from the Blueprint Editor
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint"), EditedBlueprint->GetName());
	Response->SetStringField(TEXT("blueprint_path"), EditedBlueprint->GetPathName());

	TArray<TSharedPtr<FJsonValue>> NodesArray;

	// Get selected nodes via editor's selection set
	const FGraphPanelSelectionSet& SelectedNodes = BlueprintEditor->GetSelectedNodes();
	for (UObject* SelectedObj : SelectedNodes)
	{
		UEdGraphNode* Node = Cast<UEdGraphNode>(SelectedObj);
		if (Node)
		{
			NodesArray.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
		}
	}

	Response->SetArrayField(TEXT("selected_nodes"), NodesArray);
	Response->SetNumberField(TEXT("count"), NodesArray.Num());

	return FRESTResponse::Ok(Response);
}

FRESTResponse FBlueprintsHandler::HandleNodeInfo(const FRESTRequest& Request)
{
	// Get node identifier from query params
	const FString* NodeIdPtr = Request.QueryParams.Find(TEXT("node_id"));
	const FString* NodeNamePtr = Request.QueryParams.Find(TEXT("node_name"));

	if ((!NodeIdPtr || NodeIdPtr->IsEmpty()) && (!NodeNamePtr || NodeNamePtr->IsEmpty()))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: node_id or node_name"));
	}

	UBlueprint* EditedBlueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* BlueprintEditor = FindActiveBlueprintEditor(EditedBlueprint, ErrorCode);

	if (!BlueprintEditor)
	{
		if (ErrorCode == TEXT("NO_EDITOR"))
		{
			return FRESTResponse::Error(400, TEXT("NO_EDITOR"), TEXT("Editor not available"));
		}
		else if (ErrorCode == TEXT("NO_SUBSYSTEM"))
		{
			return FRESTResponse::Error(400, TEXT("NO_SUBSYSTEM"), TEXT("AssetEditorSubsystem not available"));
		}
		else
		{
			return FRESTResponse::Error(400, TEXT("NO_BLUEPRINT_EDITOR"),
				TEXT("No Blueprint Editor is open. Open a Blueprint asset to use this endpoint."));
		}
	}

	// Search for the node in all graphs
	UEdGraphNode* FoundNode = nullptr;
	FGuid SearchGuid;

	// Try to parse as GUID first
	if (NodeIdPtr && !NodeIdPtr->IsEmpty())
	{
		FGuid::Parse(*NodeIdPtr, SearchGuid);
	}

	// Search through all graphs
	TArray<UEdGraph*> Graphs;
	EditedBlueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph)
		{
			continue;
		}

		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (!Node)
			{
				continue;
			}

			// Match by GUID
			if (SearchGuid.IsValid() && Node->NodeGuid == SearchGuid)
			{
				FoundNode = Node;
				break;
			}

			// Match by name (node title or class name)
			if (NodeNamePtr && !NodeNamePtr->IsEmpty())
			{
				FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
				FString NodeClassName = Node->GetClass()->GetName();

				if (NodeTitle.Contains(*NodeNamePtr) || NodeClassName.Contains(*NodeNamePtr))
				{
					FoundNode = Node;
					break;
				}
			}
		}

		if (FoundNode)
		{
			break;
		}
	}

	if (!FoundNode)
	{
		FString SearchTerm = NodeIdPtr && !NodeIdPtr->IsEmpty() ? *NodeIdPtr : *NodeNamePtr;
		return FRESTResponse::Error(404, TEXT("NODE_NOT_FOUND"),
			FString::Printf(TEXT("Node not found: %s"), *SearchTerm));
	}

	// Build detailed response
	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint"), EditedBlueprint->GetName());
	Response->SetObjectField(TEXT("node"), NodeToJson(FoundNode));

	// Add pins information
	TArray<TSharedPtr<FJsonValue>> InputPins;
	TArray<TSharedPtr<FJsonValue>> OutputPins;

	for (UEdGraphPin* Pin : FoundNode->Pins)
	{
		if (!Pin)
		{
			continue;
		}

		TSharedPtr<FJsonObject> PinJson = PinToJson(Pin);
		if (Pin->Direction == EGPD_Input)
		{
			InputPins.Add(MakeShared<FJsonValueObject>(PinJson));
		}
		else
		{
			OutputPins.Add(MakeShared<FJsonValueObject>(PinJson));
		}
	}

	Response->SetArrayField(TEXT("input_pins"), InputPins);
	Response->SetArrayField(TEXT("output_pins"), OutputPins);

	return FRESTResponse::Ok(Response);
}

TSharedPtr<FJsonObject> FBlueprintsHandler::NodeToJson(UEdGraphNode* Node)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Node)
	{
		return Json;
	}

	// Basic node info
	Json->SetStringField(TEXT("id"), Node->NodeGuid.ToString());
	Json->SetStringField(TEXT("class"), Node->GetClass()->GetName());
	Json->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Json->SetStringField(TEXT("compact_title"), Node->GetNodeTitle(ENodeTitleType::MenuTitle).ToString());

	// Position
	TSharedPtr<FJsonObject> Position = MakeShared<FJsonObject>();
	Position->SetNumberField(TEXT("x"), Node->NodePosX);
	Position->SetNumberField(TEXT("y"), Node->NodePosY);
	Json->SetObjectField(TEXT("position"), Position);

	// Node comment
	if (!Node->NodeComment.IsEmpty())
	{
		Json->SetStringField(TEXT("comment"), Node->NodeComment);
	}

	// Check for K2Node specifics
	UK2Node* K2Node = Cast<UK2Node>(Node);
	if (K2Node)
	{
		Json->SetBoolField(TEXT("is_pure"), K2Node->IsNodePure());

		// Function call specifics
		UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node);
		if (FunctionNode)
		{
			Json->SetStringField(TEXT("node_type"), TEXT("FunctionCall"));
			UFunction* Function = FunctionNode->GetTargetFunction();
			if (Function)
			{
				Json->SetStringField(TEXT("function_name"), Function->GetName());
				Json->SetStringField(TEXT("function_owner"), Function->GetOwnerClass() ? Function->GetOwnerClass()->GetName() : TEXT("None"));
			}
		}

		// Variable node specifics
		UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node);
		if (VariableNode)
		{
			Json->SetStringField(TEXT("node_type"), TEXT("Variable"));
			Json->SetStringField(TEXT("variable_name"), VariableNode->GetVarName().ToString());
		}

		// Event node specifics
		UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
		if (EventNode)
		{
			Json->SetStringField(TEXT("node_type"), TEXT("Event"));
		}
	}

	// Add pin count summary
	int32 InputCount = 0;
	int32 OutputCount = 0;
	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin)
		{
			if (Pin->Direction == EGPD_Input)
			{
				InputCount++;
			}
			else
			{
				OutputCount++;
			}
		}
	}
	Json->SetNumberField(TEXT("input_pin_count"), InputCount);
	Json->SetNumberField(TEXT("output_pin_count"), OutputCount);

	return Json;
}

TSharedPtr<FJsonObject> FBlueprintsHandler::PinToJson(UEdGraphPin* Pin)
{
	TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();

	if (!Pin)
	{
		return Json;
	}

	Json->SetStringField(TEXT("name"), Pin->PinName.ToString());
	Json->SetStringField(TEXT("direction"), Pin->Direction == EGPD_Input ? TEXT("Input") : TEXT("Output"));
	Json->SetStringField(TEXT("type"), Pin->PinType.PinCategory.ToString());

	// Sub-category for object types
	if (!Pin->PinType.PinSubCategory.IsNone())
	{
		Json->SetStringField(TEXT("sub_type"), Pin->PinType.PinSubCategory.ToString());
	}

	// Object type class
	if (Pin->PinType.PinSubCategoryObject.IsValid())
	{
		Json->SetStringField(TEXT("object_class"), Pin->PinType.PinSubCategoryObject->GetName());
	}

	// Container type
	if (Pin->PinType.IsArray())
	{
		Json->SetStringField(TEXT("container"), TEXT("Array"));
	}
	else if (Pin->PinType.IsSet())
	{
		Json->SetStringField(TEXT("container"), TEXT("Set"));
	}
	else if (Pin->PinType.IsMap())
	{
		Json->SetStringField(TEXT("container"), TEXT("Map"));
	}

	// Default value
	if (!Pin->DefaultValue.IsEmpty())
	{
		Json->SetStringField(TEXT("default_value"), Pin->DefaultValue);
	}

	// Connection status
	Json->SetBoolField(TEXT("is_connected"), Pin->LinkedTo.Num() > 0);
	Json->SetNumberField(TEXT("connection_count"), Pin->LinkedTo.Num());

	// Hidden status
	Json->SetBoolField(TEXT("is_hidden"), Pin->bHidden);

	return Json;
}

// Helper: Find node by GUID
UEdGraphNode* FBlueprintsHandler::FindNodeByGuid(UBlueprint* Blueprint, const FGuid& NodeGuid)
{
	if (!Blueprint || !NodeGuid.IsValid())
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (!Graph) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && Node->NodeGuid == NodeGuid)
			{
				return Node;
			}
		}
	}
	return nullptr;
}

// Helper: Find pin by name
UEdGraphPin* FBlueprintsHandler::FindPinByName(UEdGraphNode* Node, const FString& PinName, EEdGraphPinDirection Direction)
{
	if (!Node)
	{
		return nullptr;
	}

	for (UEdGraphPin* Pin : Node->Pins)
	{
		if (Pin && Pin->PinName.ToString() == PinName)
		{
			if (Direction == EGPD_MAX || Pin->Direction == Direction)
			{
				return Pin;
			}
		}
	}
	return nullptr;
}

// Helper: Find graph by name
UEdGraph* FBlueprintsHandler::FindGraphByName(UBlueprint* Blueprint, const FString& GraphName)
{
	if (!Blueprint)
	{
		return nullptr;
	}

	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	for (UEdGraph* Graph : Graphs)
	{
		if (Graph && Graph->GetName() == GraphName)
		{
			return Graph;
		}
	}

	// Default to EventGraph if not found
	if (GraphName.IsEmpty() || GraphName == TEXT("EventGraph"))
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (Graph)
			{
				return Graph;
			}
		}
	}

	return nullptr;
}

// GET /blueprints/nodes - List all nodes in a graph
FRESTResponse FBlueprintsHandler::HandleListNodes(const FRESTRequest& Request)
{
	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	// Get optional graph name
	const FString* GraphNamePtr = Request.QueryParams.Find(TEXT("graph"));
	FString GraphName = GraphNamePtr ? *GraphNamePtr : TEXT("");

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("blueprint"), Blueprint->GetName());

	// Get all graphs
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);

	// List available graphs
	TArray<TSharedPtr<FJsonValue>> GraphsArray;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph)
		{
			GraphsArray.Add(MakeShared<FJsonValueString>(Graph->GetName()));
		}
	}
	Response->SetArrayField(TEXT("available_graphs"), GraphsArray);

	// Find the requested graph
	UEdGraph* TargetGraph = FindGraphByName(Blueprint, GraphName);
	if (!TargetGraph && Graphs.Num() > 0)
	{
		TargetGraph = Graphs[0];
	}

	if (TargetGraph)
	{
		Response->SetStringField(TEXT("current_graph"), TargetGraph->GetName());

		TArray<TSharedPtr<FJsonValue>> NodesArray;
		for (UEdGraphNode* Node : TargetGraph->Nodes)
		{
			if (Node)
			{
				NodesArray.Add(MakeShared<FJsonValueObject>(NodeToJson(Node)));
			}
		}
		Response->SetArrayField(TEXT("nodes"), NodesArray);
		Response->SetNumberField(TEXT("node_count"), NodesArray.Num());
	}

	return FRESTResponse::Ok(Response);
}

// POST /blueprints/node/position - Move node
FRESTResponse FBlueprintsHandler::HandleSetNodePosition(const FRESTRequest& Request)
{
	FString NodeId;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("node_id"), NodeId, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	if (!Request.JsonBody->HasField(TEXT("position")))
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: position"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	FGuid SearchGuid;
	FGuid::Parse(NodeId, SearchGuid);

	UEdGraphNode* Node = FindNodeByGuid(Blueprint, SearchGuid);
	if (!Node)
	{
		return FRESTResponse::Error(404, TEXT("NODE_NOT_FOUND"),
			FString::Printf(TEXT("Node not found: %s"), *NodeId));
	}

	TSharedPtr<FJsonObject> PosObj = Request.JsonBody->GetObjectField(TEXT("position"));
	int32 NewX = static_cast<int32>(PosObj->GetNumberField(TEXT("x")));
	int32 NewY = static_cast<int32>(PosObj->GetNumberField(TEXT("y")));

	// Store old position
	int32 OldX = Node->NodePosX;
	int32 OldY = Node->NodePosY;

	// Move node
	Node->NodePosX = NewX;
	Node->NodePosY = NewY;

	// Mark as modified
	Node->GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("node_id"), NodeId);
	Response->SetStringField(TEXT("node_title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

	TSharedPtr<FJsonObject> OldPos = MakeShared<FJsonObject>();
	OldPos->SetNumberField(TEXT("x"), OldX);
	OldPos->SetNumberField(TEXT("y"), OldY);
	Response->SetObjectField(TEXT("old_position"), OldPos);

	TSharedPtr<FJsonObject> NewPos = MakeShared<FJsonObject>();
	NewPos->SetNumberField(TEXT("x"), NewX);
	NewPos->SetNumberField(TEXT("y"), NewY);
	Response->SetObjectField(TEXT("new_position"), NewPos);

	return FRESTResponse::Ok(Response);
}

// POST /blueprints/node/create - Create new node
FRESTResponse FBlueprintsHandler::HandleCreateNode(const FRESTRequest& Request)
{
	FString NodeType;
	FString Error;
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("node_type"), NodeType, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	// Find graph
	FString GraphName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("graph"), TEXT(""));
	UEdGraph* Graph = FindGraphByName(Blueprint, GraphName);

	if (!Graph)
	{
		return FRESTResponse::Error(404, TEXT("GRAPH_NOT_FOUND"), TEXT("Graph not found"));
	}

	// Get position
	int32 PosX = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("x"), 0);
	int32 PosY = JsonHelpers::GetOptionalInt(Request.JsonBody, TEXT("y"), 0);

	UEdGraphNode* NewNode = nullptr;

	// Create node based on type
	if (NodeType == TEXT("CallFunction") || NodeType == TEXT("K2Node_CallFunction"))
	{
		FString FunctionName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("function_name"), TEXT(""));
		FString ClassName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("class_name"), TEXT(""));

		if (FunctionName.IsEmpty())
		{
			return FRESTResponse::BadRequest(TEXT("function_name required for CallFunction node"));
		}

		// Find the function
		UFunction* Function = nullptr;
		if (!ClassName.IsEmpty())
		{
			UClass* TargetClass = FindObject<UClass>(nullptr, *ClassName);
			if (TargetClass)
			{
				Function = TargetClass->FindFunctionByName(FName(*FunctionName));
			}
		}
		else
		{
			// Search in common classes
			Function = UKismetMathLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName));
			if (!Function)
			{
				Function = UKismetSystemLibrary::StaticClass()->FindFunctionByName(FName(*FunctionName));
			}
		}

		if (Function)
		{
			UK2Node_CallFunction* FuncNode = NewObject<UK2Node_CallFunction>(Graph);
			FuncNode->SetFromFunction(Function);
			FuncNode->NodePosX = PosX;
			FuncNode->NodePosY = PosY;
			FuncNode->AllocateDefaultPins();
			Graph->AddNode(FuncNode, true, false);
			NewNode = FuncNode;
		}
		else
		{
			return FRESTResponse::Error(404, TEXT("FUNCTION_NOT_FOUND"),
				FString::Printf(TEXT("Function not found: %s"), *FunctionName));
		}
	}
	else if (NodeType == TEXT("CustomEvent") || NodeType == TEXT("K2Node_CustomEvent"))
	{
		FString EventName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("event_name"), TEXT("CustomEvent"));

		UK2Node_CustomEvent* EventNode = NewObject<UK2Node_CustomEvent>(Graph);
		EventNode->CustomFunctionName = FName(*EventName);
		EventNode->NodePosX = PosX;
		EventNode->NodePosY = PosY;
		EventNode->AllocateDefaultPins();
		Graph->AddNode(EventNode, true, false);
		NewNode = EventNode;
	}
	else if (NodeType == TEXT("VariableGet") || NodeType == TEXT("K2Node_VariableGet"))
	{
		FString VarName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("variable_name"), TEXT(""));
		if (VarName.IsEmpty())
		{
			return FRESTResponse::BadRequest(TEXT("variable_name required for VariableGet node"));
		}

		UK2Node_VariableGet* VarNode = NewObject<UK2Node_VariableGet>(Graph);
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->NodePosX = PosX;
		VarNode->NodePosY = PosY;
		VarNode->AllocateDefaultPins();
		Graph->AddNode(VarNode, true, false);
		NewNode = VarNode;
	}
	else if (NodeType == TEXT("VariableSet") || NodeType == TEXT("K2Node_VariableSet"))
	{
		FString VarName = JsonHelpers::GetOptionalString(Request.JsonBody, TEXT("variable_name"), TEXT(""));
		if (VarName.IsEmpty())
		{
			return FRESTResponse::BadRequest(TEXT("variable_name required for VariableSet node"));
		}

		UK2Node_VariableSet* VarNode = NewObject<UK2Node_VariableSet>(Graph);
		VarNode->VariableReference.SetSelfMember(FName(*VarName));
		VarNode->NodePosX = PosX;
		VarNode->NodePosY = PosY;
		VarNode->AllocateDefaultPins();
		Graph->AddNode(VarNode, true, false);
		NewNode = VarNode;
	}
	else
	{
		return FRESTResponse::Error(400, TEXT("UNKNOWN_NODE_TYPE"),
			FString::Printf(TEXT("Unknown node type: %s. Supported: CallFunction, CustomEvent, VariableGet, VariableSet"), *NodeType));
	}

	if (NewNode)
	{
		Graph->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetObjectField(TEXT("node"), NodeToJson(NewNode));
		return FRESTResponse::Ok(Response);
	}

	return FRESTResponse::Error(500, TEXT("CREATE_FAILED"), TEXT("Failed to create node"));
}

// DELETE /blueprints/node - Delete node
FRESTResponse FBlueprintsHandler::HandleDeleteNode(const FRESTRequest& Request)
{
	const FString* NodeIdPtr = Request.QueryParams.Find(TEXT("node_id"));
	if (!NodeIdPtr || NodeIdPtr->IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required query parameter: node_id"));
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	FGuid SearchGuid;
	FGuid::Parse(*NodeIdPtr, SearchGuid);

	UEdGraphNode* Node = FindNodeByGuid(Blueprint, SearchGuid);
	if (!Node)
	{
		return FRESTResponse::Error(404, TEXT("NODE_NOT_FOUND"),
			FString::Printf(TEXT("Node not found: %s"), **NodeIdPtr));
	}

	FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();

	// Remove the node
	FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("deleted_node_id"), *NodeIdPtr);
	Response->SetStringField(TEXT("deleted_node_title"), NodeTitle);

	return FRESTResponse::Ok(Response);
}

// POST /blueprints/connect - Connect two pins
FRESTResponse FBlueprintsHandler::HandleConnect(const FRESTRequest& Request)
{
	FString SourceNodeId, TargetNodeId, SourcePinName, TargetPinName;
	FString Error;

	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("source_node_id"), SourceNodeId, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("source_pin"), SourcePinName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("target_node_id"), TargetNodeId, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("target_pin"), TargetPinName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	// Find nodes
	FGuid SourceGuid, TargetGuid;
	FGuid::Parse(SourceNodeId, SourceGuid);
	FGuid::Parse(TargetNodeId, TargetGuid);

	UEdGraphNode* SourceNode = FindNodeByGuid(Blueprint, SourceGuid);
	UEdGraphNode* TargetNode = FindNodeByGuid(Blueprint, TargetGuid);

	if (!SourceNode)
	{
		return FRESTResponse::Error(404, TEXT("SOURCE_NODE_NOT_FOUND"), TEXT("Source node not found"));
	}
	if (!TargetNode)
	{
		return FRESTResponse::Error(404, TEXT("TARGET_NODE_NOT_FOUND"), TEXT("Target node not found"));
	}

	// Find pins - source is typically output, target is typically input
	UEdGraphPin* SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_Output);
	if (!SourcePin)
	{
		SourcePin = FindPinByName(SourceNode, SourcePinName, EGPD_MAX);
	}

	UEdGraphPin* TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_Input);
	if (!TargetPin)
	{
		TargetPin = FindPinByName(TargetNode, TargetPinName, EGPD_MAX);
	}

	if (!SourcePin)
	{
		return FRESTResponse::Error(404, TEXT("SOURCE_PIN_NOT_FOUND"),
			FString::Printf(TEXT("Source pin not found: %s"), *SourcePinName));
	}
	if (!TargetPin)
	{
		return FRESTResponse::Error(404, TEXT("TARGET_PIN_NOT_FOUND"),
			FString::Printf(TEXT("Target pin not found: %s"), *TargetPinName));
	}

	// Try to connect
	const UEdGraphSchema* Schema = SourceNode->GetGraph()->GetSchema();
	if (Schema->TryCreateConnection(SourcePin, TargetPin))
	{
		SourceNode->GetGraph()->NotifyGraphChanged();
		FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetBoolField(TEXT("success"), true);
		Response->SetStringField(TEXT("source_node"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		Response->SetStringField(TEXT("source_pin"), SourcePinName);
		Response->SetStringField(TEXT("target_node"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
		Response->SetStringField(TEXT("target_pin"), TargetPinName);
		return FRESTResponse::Ok(Response);
	}
	else
	{
		return FRESTResponse::Error(400, TEXT("CONNECTION_FAILED"),
			TEXT("Failed to connect pins. Pins may be incompatible types."));
	}
}

// POST /blueprints/disconnect - Break connections
FRESTResponse FBlueprintsHandler::HandleDisconnect(const FRESTRequest& Request)
{
	FString NodeId, PinName;
	FString Error;

	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("node_id"), NodeId, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("pin"), PinName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	FGuid SearchGuid;
	FGuid::Parse(NodeId, SearchGuid);

	UEdGraphNode* Node = FindNodeByGuid(Blueprint, SearchGuid);
	if (!Node)
	{
		return FRESTResponse::Error(404, TEXT("NODE_NOT_FOUND"), TEXT("Node not found"));
	}

	UEdGraphPin* Pin = FindPinByName(Node, PinName, EGPD_MAX);
	if (!Pin)
	{
		return FRESTResponse::Error(404, TEXT("PIN_NOT_FOUND"),
			FString::Printf(TEXT("Pin not found: %s"), *PinName));
	}

	int32 BrokenCount = Pin->LinkedTo.Num();

	// Break all links
	Pin->BreakAllPinLinks();

	Node->GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Response->SetStringField(TEXT("pin"), PinName);
	Response->SetNumberField(TEXT("connections_broken"), BrokenCount);

	return FRESTResponse::Ok(Response);
}

// POST /blueprints/pin/default - Set pin default value
FRESTResponse FBlueprintsHandler::HandleSetPinDefault(const FRESTRequest& Request)
{
	FString NodeId, PinName, Value;
	FString Error;

	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("node_id"), NodeId, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("pin"), PinName, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}
	if (!JsonHelpers::GetRequiredString(Request.JsonBody, TEXT("value"), Value, Error))
	{
		return FRESTResponse::BadRequest(Error);
	}

	UBlueprint* Blueprint = nullptr;
	FString ErrorCode;
	FBlueprintEditor* Editor = FindActiveBlueprintEditor(Blueprint, ErrorCode);

	if (!Editor)
	{
		return FRESTResponse::Error(400, ErrorCode, TEXT("No Blueprint Editor open"));
	}

	FGuid SearchGuid;
	FGuid::Parse(NodeId, SearchGuid);

	UEdGraphNode* Node = FindNodeByGuid(Blueprint, SearchGuid);
	if (!Node)
	{
		return FRESTResponse::Error(404, TEXT("NODE_NOT_FOUND"), TEXT("Node not found"));
	}

	UEdGraphPin* Pin = FindPinByName(Node, PinName, EGPD_Input);
	if (!Pin)
	{
		return FRESTResponse::Error(404, TEXT("PIN_NOT_FOUND"),
			FString::Printf(TEXT("Pin not found: %s"), *PinName));
	}

	FString OldValue = Pin->DefaultValue;

	// Set default value
	const UEdGraphSchema* Schema = Node->GetGraph()->GetSchema();
	Schema->TrySetDefaultValue(*Pin, Value);

	Node->GetGraph()->NotifyGraphChanged();
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetBoolField(TEXT("success"), true);
	Response->SetStringField(TEXT("node"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
	Response->SetStringField(TEXT("pin"), PinName);
	Response->SetStringField(TEXT("old_value"), OldValue);
	Response->SetStringField(TEXT("new_value"), Pin->DefaultValue);

	return FRESTResponse::Ok(Response);
}

TArray<TSharedPtr<FJsonObject>> FBlueprintsHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/selection")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get selected nodes in active Blueprint Editor"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/node_info")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get detailed node information (query: node_id or node_name)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/nodes")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("List all nodes in Blueprint graph"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/node/position")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Move node to new position"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/node/create")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Create Blueprint node (CallFunction, CustomEvent, VariableGet/Set)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("DELETE")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/node")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Delete a node (query: node_id)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/connect")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Connect two Blueprint pins"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/disconnect")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Break pin connections"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/blueprints/pin/default")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Set pin default value"));

	return Schemas;
}
