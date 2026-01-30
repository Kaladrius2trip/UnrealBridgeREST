# Blueprints Endpoints

Base path: `/api/v1/blueprints`

Blueprint Editor node manipulation endpoints. Provides full access to Blueprint Editor for node inspection and editing.

**IMPORTANT:** Most Blueprint endpoints require a Blueprint Editor to be open in Unreal Engine with an active graph. If no Blueprint is open, endpoints will return `NO_BLUEPRINT_EDITOR` error.

---

## GET /blueprints/selection

Get selected nodes in the currently open Blueprint Editor.

**Parameters:** None

**Response:**
```json
{
  "success": true,
  "blueprint": "BP_MyActor",
  "blueprint_path": "/Game/Blueprints/BP_MyActor.BP_MyActor",
  "selected_nodes": [
    {
      "id": "12345678-1234-1234-1234-123456789012",
      "class": "K2Node_CallFunction",
      "title": "Print String",
      "compact_title": "Print String",
      "position": {"x": 100, "y": 200},
      "is_pure": false,
      "node_type": "FunctionCall",
      "function_name": "PrintString",
      "function_owner": "KismetSystemLibrary",
      "input_pin_count": 2,
      "output_pin_count": 1
    }
  ],
  "count": 1
}
```

**Status Codes:**
- 200 - Success
- 400 - Blueprint Editor not available

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open

**Notes:**
- Returns empty array if no nodes are selected
- Node data includes position, class, title, and type-specific information
- Selection is from the active Blueprint Editor window

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/blueprints/selection"
```

---

## GET /blueprints/node_info

Get detailed information about a specific node, including all pins.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_id | string | No* | - | Node GUID (e.g., "12345678-1234-1234-1234-123456789012") |
| node_name | string | No* | - | Node name or title to search for (partial match) |

*At least one of `node_id` or `node_name` is required.

**Response:**
```json
{
  "success": true,
  "blueprint": "BP_MyActor",
  "node": {
    "id": "12345678-1234-1234-1234-123456789012",
    "class": "K2Node_CallFunction",
    "title": "Print String",
    "compact_title": "Print String",
    "position": {"x": 100, "y": 200},
    "is_pure": false,
    "node_type": "FunctionCall",
    "function_name": "PrintString",
    "function_owner": "KismetSystemLibrary",
    "input_pin_count": 2,
    "output_pin_count": 1
  },
  "input_pins": [
    {
      "name": "execute",
      "direction": "Input",
      "type": "exec",
      "is_connected": true,
      "connection_count": 1,
      "is_hidden": false
    },
    {
      "name": "InString",
      "direction": "Input",
      "type": "string",
      "default_value": "Hello World",
      "is_connected": false,
      "connection_count": 0,
      "is_hidden": false
    }
  ],
  "output_pins": [
    {
      "name": "then",
      "direction": "Output",
      "type": "exec",
      "is_connected": false,
      "connection_count": 0,
      "is_hidden": false
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Blueprint Editor not available
- 404 - Node not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `NODE_NOT_FOUND` - Node not found with the given ID or name

**Notes:**
- Search by GUID is exact match
- Search by name does partial matching on node title or class name
- Returns full pin information including types, connections, and default values
- Object pins include `object_class` field with the class name
- Container pins (arrays, sets, maps) include `container` field

**curl:**
```bash
# By node ID
curl -s "http://localhost:$PORT/api/v1/blueprints/node_info?node_id=12345678-1234-1234-1234-123456789012"

# By node name
curl -s "http://localhost:$PORT/api/v1/blueprints/node_info?node_name=PrintString"
```

---

## GET /blueprints/nodes

List all nodes in a graph of the currently open Blueprint.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| graph | string | No | EventGraph | Name of the graph to list nodes from |

**Response:**
```json
{
  "success": true,
  "blueprint": "BP_MyActor",
  "available_graphs": [
    "EventGraph",
    "ConstructionScript",
    "MyFunction"
  ],
  "current_graph": "EventGraph",
  "nodes": [
    {
      "id": "12345678-1234-1234-1234-123456789012",
      "class": "K2Node_Event",
      "title": "Event BeginPlay",
      "compact_title": "BeginPlay",
      "position": {"x": 0, "y": 0},
      "node_type": "Event",
      "input_pin_count": 0,
      "output_pin_count": 1
    }
  ],
  "node_count": 1
}
```

**Status Codes:**
- 200 - Success
- 400 - Blueprint Editor not available

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open

**Notes:**
- If no graph name specified, defaults to EventGraph
- Lists all available graphs in the Blueprint
- Returns basic node information without detailed pin data
- Use `/blueprints/node_info` to get detailed info for specific nodes

**curl:**
```bash
# Default graph
curl -s "http://localhost:$PORT/api/v1/blueprints/nodes"

# Specific graph
curl -s "http://localhost:$PORT/api/v1/blueprints/nodes?graph=ConstructionScript"
```

---

## POST /blueprints/node/position

Move a node to a new position in the graph.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_id | string | Yes | - | Node GUID to move |
| position | object | Yes | - | New position with `x` and `y` fields |

**Request:**
```json
{
  "node_id": "12345678-1234-1234-1234-123456789012",
  "position": {
    "x": 500,
    "y": 300
  }
}
```

**Response:**
```json
{
  "success": true,
  "node_id": "12345678-1234-1234-1234-123456789012",
  "node_title": "Print String",
  "old_position": {"x": 100, "y": 200},
  "new_position": {"x": 500, "y": 300}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Blueprint Editor not available
- 404 - Node not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `NODE_NOT_FOUND` - Node not found with the given ID

**Notes:**
- Position coordinates are in graph space (not screen pixels)
- Blueprint is marked as modified after moving
- Graph is notified of changes

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/node/position" \
  -H "Content-Type: application/json" \
  -d '{"node_id": "12345678-1234-1234-1234-123456789012", "position": {"x": 500, "y": 300}}'
```

---

## POST /blueprints/node/create

Create a new node in the Blueprint graph.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_type | string | Yes | - | Type of node to create (CallFunction, CustomEvent, VariableGet, VariableSet) |
| graph | string | No | EventGraph | Graph name to create node in |
| x | integer | No | 0 | X position in graph |
| y | integer | No | 0 | Y position in graph |
| function_name | string | Conditional* | - | Function name (required for CallFunction) |
| class_name | string | No | - | Class name for function (optional, searches common classes if not provided) |
| event_name | string | No | CustomEvent | Event name (for CustomEvent type) |
| variable_name | string | Conditional* | - | Variable name (required for VariableGet/VariableSet) |

*Required depending on `node_type`

**Request (CallFunction):**
```json
{
  "node_type": "CallFunction",
  "function_name": "PrintString",
  "class_name": "/Script/Engine.KismetSystemLibrary",
  "graph": "EventGraph",
  "x": 100,
  "y": 200
}
```

**Request (CustomEvent):**
```json
{
  "node_type": "CustomEvent",
  "event_name": "OnPlayerDamaged",
  "x": 100,
  "y": 200
}
```

**Request (VariableGet):**
```json
{
  "node_type": "VariableGet",
  "variable_name": "Health",
  "x": 100,
  "y": 200
}
```

**Response:**
```json
{
  "success": true,
  "node": {
    "id": "87654321-4321-4321-4321-210987654321",
    "class": "K2Node_CallFunction",
    "title": "Print String",
    "position": {"x": 100, "y": 200},
    "is_pure": false,
    "node_type": "FunctionCall",
    "function_name": "PrintString",
    "function_owner": "KismetSystemLibrary",
    "input_pin_count": 2,
    "output_pin_count": 1
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid node type, or Blueprint Editor not available
- 404 - Graph or function not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `GRAPH_NOT_FOUND` - Graph not found in Blueprint
- `FUNCTION_NOT_FOUND` - Function not found in specified class
- `UNKNOWN_NODE_TYPE` - Unsupported node type

**Notes:**
- Supported node types: `CallFunction`, `CustomEvent`, `VariableGet`, `VariableSet`
- For CallFunction without class_name, searches in KismetMathLibrary and KismetSystemLibrary
- Variable nodes require the variable to exist in the Blueprint
- Blueprint is marked as modified after creation
- Node is automatically added to the graph with default pins allocated

**curl:**
```bash
# Create function call node
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/node/create" \
  -H "Content-Type: application/json" \
  -d '{"node_type": "CallFunction", "function_name": "PrintString", "x": 100, "y": 200}'

# Create custom event
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/node/create" \
  -H "Content-Type: application/json" \
  -d '{"node_type": "CustomEvent", "event_name": "OnPlayerDamaged", "x": 100, "y": 200}'
```

---

## DELETE /blueprints/node

Delete a node from the Blueprint graph.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_id | string | Yes | - | Node GUID to delete |

**Response:**
```json
{
  "success": true,
  "deleted_node_id": "12345678-1234-1234-1234-123456789012",
  "deleted_node_title": "Print String"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Blueprint Editor not available
- 404 - Node not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `NODE_NOT_FOUND` - Node not found with the given ID

**Notes:**
- Removes the node and all its connections
- Blueprint is marked as modified after deletion
- Uses FBlueprintEditorUtils::RemoveNode for safe removal

**curl:**
```bash
curl -s -X DELETE "http://localhost:$PORT/api/v1/blueprints/node?node_id=12345678-1234-1234-1234-123456789012"
```

---

## POST /blueprints/connect

Connect two pins between nodes.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| source_node_id | string | Yes | - | Source node GUID |
| source_pin | string | Yes | - | Source pin name (typically output pin) |
| target_node_id | string | Yes | - | Target node GUID |
| target_pin | string | Yes | - | Target pin name (typically input pin) |

**Request:**
```json
{
  "source_node_id": "12345678-1234-1234-1234-123456789012",
  "source_pin": "then",
  "target_node_id": "87654321-4321-4321-4321-210987654321",
  "target_pin": "execute"
}
```

**Response:**
```json
{
  "success": true,
  "source_node": "Event BeginPlay",
  "source_pin": "then",
  "target_node": "Print String",
  "target_pin": "execute"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, Blueprint Editor not available, or connection failed
- 404 - Node or pin not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `SOURCE_NODE_NOT_FOUND` - Source node not found
- `TARGET_NODE_NOT_FOUND` - Target node not found
- `SOURCE_PIN_NOT_FOUND` - Source pin not found
- `TARGET_PIN_NOT_FOUND` - Target pin not found
- `CONNECTION_FAILED` - Failed to connect pins (incompatible types)

**Notes:**
- Source pin is typically an output pin, target pin is typically an input pin
- Schema validates pin type compatibility before connecting
- Blueprint is marked as modified after connection
- If pin direction is not specified in search, searches both directions

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/connect" \
  -H "Content-Type: application/json" \
  -d '{
    "source_node_id": "12345678-1234-1234-1234-123456789012",
    "source_pin": "then",
    "target_node_id": "87654321-4321-4321-4321-210987654321",
    "target_pin": "execute"
  }'
```

---

## POST /blueprints/disconnect

Break all connections on a specific pin.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_id | string | Yes | - | Node GUID |
| pin | string | Yes | - | Pin name to disconnect |

**Request:**
```json
{
  "node_id": "12345678-1234-1234-1234-123456789012",
  "pin": "InString"
}
```

**Response:**
```json
{
  "success": true,
  "node": "Print String",
  "pin": "InString",
  "connections_broken": 1
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Blueprint Editor not available
- 404 - Node or pin not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `NODE_NOT_FOUND` - Node not found
- `PIN_NOT_FOUND` - Pin not found on the node

**Notes:**
- Breaks all connections on the specified pin
- Works for both input and output pins
- Blueprint is marked as modified after disconnection
- Returns count of connections broken

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/disconnect" \
  -H "Content-Type: application/json" \
  -d '{"node_id": "12345678-1234-1234-1234-123456789012", "pin": "InString"}'
```

---

## POST /blueprints/pin/default

Set the default value for an input pin.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| node_id | string | Yes | - | Node GUID |
| pin | string | Yes | - | Input pin name |
| value | string | Yes | - | Default value to set (as string) |

**Request:**
```json
{
  "node_id": "12345678-1234-1234-1234-123456789012",
  "pin": "InString",
  "value": "Hello Blueprint!"
}
```

**Response:**
```json
{
  "success": true,
  "node": "Print String",
  "pin": "InString",
  "old_value": "Hello World",
  "new_value": "Hello Blueprint!"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Blueprint Editor not available
- 404 - Node or pin not found

**Error Codes:**
- `NO_EDITOR` - Editor not available
- `NO_SUBSYSTEM` - AssetEditorSubsystem not available
- `NO_BLUEPRINT_EDITOR` - No Blueprint Editor is open
- `NODE_NOT_FOUND` - Node not found
- `PIN_NOT_FOUND` - Pin not found (searches input pins only)

**Notes:**
- Only works on input pins
- Value is provided as string and converted by schema based on pin type
- For boolean values, use "true" or "false"
- For numeric values, use string representation (e.g., "42", "3.14")
- For object references, use asset path format
- Blueprint is marked as modified after setting value

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/blueprints/pin/default" \
  -H "Content-Type: application/json" \
  -d '{"node_id": "12345678-1234-1234-1234-123456789012", "pin": "InString", "value": "Hello Blueprint!"}'
```

---

## Node Data Format

All endpoints that return node information use this common format:

```json
{
  "id": "12345678-1234-1234-1234-123456789012",
  "class": "K2Node_CallFunction",
  "title": "Print String",
  "compact_title": "Print String",
  "position": {"x": 100, "y": 200},
  "comment": "Optional node comment",
  "is_pure": false,
  "node_type": "FunctionCall",
  "function_name": "PrintString",
  "function_owner": "KismetSystemLibrary",
  "input_pin_count": 2,
  "output_pin_count": 1
}
```

| Field | Description |
|-------|-------------|
| id | Node GUID (unique identifier) |
| class | Node class name (e.g., K2Node_CallFunction, K2Node_Event) |
| title | Full display title |
| compact_title | Compact/menu title |
| position | Node position in graph (x, y coordinates) |
| comment | Node comment (only present if not empty) |
| is_pure | Whether the node is pure (no execution pins) |
| node_type | Categorized type (FunctionCall, Variable, Event) |
| function_name | Function name (for CallFunction nodes) |
| function_owner | Owning class name (for CallFunction nodes) |
| variable_name | Variable name (for Variable nodes) |
| input_pin_count | Number of input pins |
| output_pin_count | Number of output pins |

---

## Pin Data Format

Pin information is returned with detailed type and connection data:

```json
{
  "name": "InString",
  "direction": "Input",
  "type": "string",
  "sub_type": "Optional subtype",
  "object_class": "UObject class for object pins",
  "container": "Array/Set/Map for container types",
  "default_value": "Default value if set",
  "is_connected": true,
  "connection_count": 1,
  "is_hidden": false
}
```

| Field | Description |
|-------|-------------|
| name | Pin name |
| direction | "Input" or "Output" |
| type | Pin type category (exec, bool, int, float, string, object, etc.) |
| sub_type | Pin subcategory (optional) |
| object_class | Object class name for object-type pins |
| container | Container type if pin is array/set/map |
| default_value | Default value (only for input pins) |
| is_connected | Whether the pin has connections |
| connection_count | Number of connections |
| is_hidden | Whether the pin is hidden in UI |
