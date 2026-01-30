# Material Function Endpoints

Base path: `/api/v1/materials/function`

Endpoints for creating and editing Material Function assets. Material Functions are reusable node graphs that can be shared across multiple materials.

**IMPORTANT:** There are two categories of endpoints:

1. **Creation endpoints** (`/materials/function/create`) - Create new Material Function assets. Do not require an editor to be open.

2. **Editor endpoints** (`/materials/function/editor/*`) - Work with Material Function graphs and require the function to be open in the editor. If no editor is open for the target function, these endpoints will return `NO_FUNCTION_EDITOR` error. Use `/materials/function/editor/open` first.

---

## POST /materials/function/create

Create a new Material Function asset.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| name | string | Yes | - | Function name |
| path | string | No | /Game/Materials/Functions | Package path (auto-prefixed with /Game/ if needed) |
| description | string | No | "" | Function description text |
| expose_to_library | boolean | No | true | Whether to expose this function in the Material Editor function library |
| save | boolean | No | true | Save asset to disk after creation |

**Request:**
```json
{
  "name": "MF_Fresnel",
  "path": "/Game/Materials/Functions",
  "description": "Custom Fresnel effect for rim lighting",
  "expose_to_library": true
}
```

**Response:**
```json
{
  "success": true,
  "function_name": "MF_Fresnel",
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "package_path": "/Game/Materials/Functions",
  "expose_to_library": true
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing required parameters
- 500 - Creation failed

**Error Codes:**
- `CREATE_FAILED` - Failed to create material function asset

**Notes:**
- Path is automatically prefixed with `/Game/` if not present
- Default path is `/Game/Materials/Functions`
- Function is marked as dirty and registered with the Asset Registry
- Use `/materials/function/editor/open` to open the new function for editing

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/create" \
  -H "Content-Type: application/json" \
  -d '{"name": "MF_Fresnel", "path": "/Game/Materials/Functions", "description": "Custom Fresnel"}'
```

---

## POST /materials/function/editor/open

Open a Material Function in the editor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to Material Function asset |

**Request:**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel"
}
```

**Response:**
```json
{
  "success": true,
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "function_name": "MF_Fresnel",
  "expression_count": 3
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters
- 404 - Function not found
- 500 - Failed to open editor

**Error Codes:**
- `FUNCTION_NOT_FOUND` - Material function not found at specified path
- `EDITOR_SUBSYSTEM_UNAVAILABLE` - AssetEditorSubsystem not available
- `OPEN_FAILED` - Failed to open editor for the function

**Notes:**
- Must be called before using any `/materials/function/editor/*` endpoints
- Opens the Material Function Editor window in Unreal Engine
- Returns the current expression count in the function

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/open" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel"}'
```

---

## GET /materials/function/editor/nodes

List all expression nodes in an open Material Function.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor (query param) |

**Response:**
```json
{
  "success": true,
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "node_count": 3,
  "nodes": [
    {
      "name": "MaterialExpressionFunctionInput_0",
      "class": "MaterialExpressionFunctionInput",
      "description": "Normal input",
      "position": {"x": -400, "y": 0},
      "outputs": [
        {"index": 0, "name": ""}
      ]
    },
    {
      "name": "MaterialExpressionFunctionOutput_0",
      "class": "MaterialExpressionFunctionOutput",
      "description": "",
      "position": {"x": 200, "y": 0},
      "outputs": [
        {"index": 0, "name": ""}
      ]
    },
    {
      "name": "MaterialExpressionFresnel_0",
      "class": "MaterialExpressionFresnel",
      "description": "",
      "position": {"x": 0, "y": 0},
      "outputs": [
        {"index": 0, "name": ""}
      ]
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Function editor not available

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function

**Notes:**
- Requires the function to be open in the editor
- Returns all expression nodes using the same format as `/materials/editor/nodes`
- Includes FunctionInput and FunctionOutput nodes specific to Material Functions

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/function/editor/nodes?function_path=/Game/Materials/Functions/MF_Fresnel.MF_Fresnel"
```

---

## POST /materials/function/editor/node/create

Create a new expression node in a Material Function.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| expression_class | string | Yes | - | Expression class name (with or without "MaterialExpression" prefix) |
| position | object | No | {x:0, y:0} | Position with `x` and `y` fields |
| input_name | string | No | - | Input pin name (for FunctionInput expressions) |
| input_type | string | No | - | Input type (for FunctionInput): Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, StaticBool, Bool, MaterialAttributes |
| output_name | string | No | - | Output pin name (for FunctionOutput expressions) |
| description | string | No | - | Description (for FunctionInput/FunctionOutput) |
| sort_priority | integer | No | - | Sort priority for ordering inputs/outputs |
| param_name | string | No | - | Parameter name (for ScalarParameter, VectorParameter) |
| default_value | number\|object | No | - | Default value (for parameter expressions) |
| value | number\|object | No | - | Constant value (for Constant, Constant3Vector) |
| save | boolean | No | true | Save asset to disk after modification |

**Request (FunctionInput):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_class": "FunctionInput",
  "position": {"x": -400, "y": 0},
  "input_name": "Normal",
  "input_type": "Vector3",
  "description": "World space normal vector",
  "sort_priority": 0
}
```

**Request (FunctionOutput):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_class": "FunctionOutput",
  "position": {"x": 200, "y": 0},
  "output_name": "Result",
  "description": "Fresnel result value",
  "sort_priority": 0
}
```

**Request (ScalarParameter):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_class": "ScalarParameter",
  "position": {"x": -400, "y": 100},
  "param_name": "Exponent",
  "default_value": 5.0
}
```

**Request (Math node):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_class": "Multiply",
  "position": {"x": 0, "y": 50}
}
```

**Response:**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionFunctionInput_0",
  "expression_class": "MaterialExpressionFunctionInput",
  "created_via_editor_api": true,
  "expression": {
    "name": "MaterialExpressionFunctionInput_0",
    "class": "MaterialExpressionFunctionInput",
    "description": "World space normal vector",
    "position": {"x": -400, "y": 0},
    "outputs": [
      {"index": 0, "name": ""}
    ]
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid expression class, or function editor not available
- 500 - Creation failed

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `INVALID_EXPRESSION_CLASS` - Expression class not found or invalid
- `CREATE_FAILED` - Failed to create expression

**Notes:**
- Requires the function to be open in the editor
- Expression class can be specified with or without "MaterialExpression" prefix
- Key Material Function expression types:
  - `FunctionInput` - Defines an input pin for the function (use `input_name`, `input_type`)
  - `FunctionOutput` - Defines an output pin for the function (use `output_name`)
- All standard expression types (ScalarParameter, Multiply, Add, etc.) are also supported
- Valid `input_type` values: Scalar, Vector2, Vector3, Vector4, Texture2D, TextureCube, StaticBool, Bool, MaterialAttributes
- Uses a 3-tier creation strategy: Editor API, UMaterialEditingLibrary, manual NewObject fallback
- `created_via_editor_api` indicates if the editor API was used (provides better visual updates)

**curl:**
```bash
# Create function input
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/node/create" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_class": "FunctionInput", "position": {"x": -400, "y": 0}, "input_name": "Normal", "input_type": "Vector3"}'

# Create function output
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/node/create" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_class": "FunctionOutput", "position": {"x": 200, "y": 0}, "output_name": "Result"}'

# Create math node
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/node/create" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_class": "Multiply", "position": {"x": 0, "y": 0}}'
```

---

## POST /materials/function/editor/node/position

Move an expression node to a new position in the function graph.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| expression_name | string | Yes | - | Expression node name |
| position | object | Yes | - | New position with `x` and `y` fields |
| save | boolean | No | true | Save asset to disk after modification |

**Request:**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_name": "MaterialExpressionFunctionInput_0",
  "position": {"x": -500, "y": -100}
}
```

**Response:**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionFunctionInput_0",
  "old_position": {"x": -400, "y": 0},
  "new_position": {"x": -500, "y": -100}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or function editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `EXPRESSION_NOT_FOUND` - Expression not found in function

**Notes:**
- Requires the function to be open in the editor
- Position coordinates are in graph space
- Returns both old and new positions

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/node/position" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_name": "MaterialExpressionFunctionInput_0", "position": {"x": -500, "y": -100}}'
```

---

## POST /materials/function/editor/connect

Connect two expression nodes in a Material Function.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| source_expression | string | Yes | - | Source expression name |
| target_expression | string | Yes | - | Target expression name |
| output_index | integer | No | 0 | Output pin index on source expression |
| input_index | integer | No | 0 | Input pin index on target expression |
| save | boolean | No | true | Save asset to disk after modification |

**Request:**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "source_expression": "MaterialExpressionFresnel_0",
  "target_expression": "MaterialExpressionFunctionOutput_0",
  "output_index": 0,
  "input_index": 0
}
```

**Response:**
```json
{
  "success": true,
  "source_expression": "MaterialExpressionFresnel_0",
  "output_index": 0,
  "target_expression": "MaterialExpressionFunctionOutput_0",
  "input_index": 0,
  "connection_verified": true
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid indices, or function editor not available
- 404 - Expression not found
- 500 - Graph node or schema unavailable

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `SOURCE_NOT_FOUND` - Source expression not found
- `TARGET_NOT_FOUND` - Target expression not found
- `INVALID_OUTPUT_INDEX` - Output index out of range
- `INVALID_PIN` - Could not get output or input pin at specified indices
- `NO_GRAPH_NODE` - Source or target expression has no GraphNode
- `NO_MATERIAL_GRAPH` - Could not get MaterialGraph
- `NO_SCHEMA` - Could not get MaterialGraphSchema

**Notes:**
- Requires the function to be open in the editor
- Unlike material editor connect, this only supports expression-to-expression connections (no material property targets)
- Uses graph schema pin connections for proper editor synchronization
- Function is updated after connection via `UpdateMaterialFunction`
- `connection_verified` indicates whether the schema reported a successful connection

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/connect" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "source_expression": "MaterialExpressionFresnel_0", "target_expression": "MaterialExpressionFunctionOutput_0"}'
```

---

## POST /materials/function/editor/disconnect

Disconnect an input on an expression node in a Material Function.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| target_expression | string | Yes | - | Target expression name whose input to disconnect |
| input_index | integer | No | 0 | Input pin index to disconnect |
| save | boolean | No | true | Save asset to disk after modification |

**Request:**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "target_expression": "MaterialExpressionFunctionOutput_0",
  "input_index": 0
}
```

**Response:**
```json
{
  "success": true,
  "target_expression": "MaterialExpressionFunctionOutput_0",
  "input_index": 0,
  "was_connected": true
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid pin, or function editor not available
- 404 - Expression not found
- 500 - Graph node unavailable

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `TARGET_NOT_FOUND` - Target expression not found
- `NO_GRAPH_NODE` - Target expression has no GraphNode
- `INVALID_INPUT_PIN` - Could not get input pin at specified index

**Notes:**
- Requires the function to be open in the editor
- Unlike material editor disconnect, this only supports expression input disconnection (no material property targets)
- `was_connected` indicates whether the pin had a connection before the operation
- If the pin was not connected, the endpoint still returns success but `was_connected` will be false
- Function is updated after disconnection via `UpdateMaterialFunction`

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/disconnect" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "target_expression": "MaterialExpressionFunctionOutput_0", "input_index": 0}'
```

---

## POST /materials/function/editor/expression/set

Set a property value on an expression node in a Material Function.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| expression_name | string | Yes | - | Expression node name |
| property | string | Yes | - | Property name to set |
| value | number\|string\|object\|boolean | Yes | - | New value (type depends on property) |
| save | boolean | No | true | Save asset to disk after modification |

**Supported Properties by Expression Type:**

**FunctionInput:**
| Property | Type | Description |
|----------|------|-------------|
| InputName | string | Input pin name |
| InputType | string | Input type: Scalar, Vector2, Vector3, Vector4, Texture2D, StaticBool, Bool, MaterialAttributes |
| Description | string | Description text |
| SortPriority | integer | Sort order for input ordering |
| UsePreviewValueAsDefault | boolean | Use preview value as default |

**FunctionOutput:**
| Property | Type | Description |
|----------|------|-------------|
| OutputName | string | Output pin name |
| Description | string | Description text |
| SortPriority | integer | Sort order for output ordering |

**ScalarParameter:**
| Property | Type | Description |
|----------|------|-------------|
| DefaultValue / Value | number | Default scalar value |
| ParameterName | string | Parameter name |

**VectorParameter:**
| Property | Type | Description |
|----------|------|-------------|
| DefaultValue / Value | object {r,g,b,a} | Default color value |
| ParameterName | string | Parameter name |

**Request (FunctionInput InputName):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_name": "MaterialExpressionFunctionInput_0",
  "property": "InputName",
  "value": "WorldNormal"
}
```

**Request (FunctionInput InputType):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_name": "MaterialExpressionFunctionInput_0",
  "property": "InputType",
  "value": "Vector3"
}
```

**Request (FunctionOutput OutputName):**
```json
{
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "expression_name": "MaterialExpressionFunctionOutput_0",
  "property": "OutputName",
  "value": "FresnelResult"
}
```

**Response:**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionFunctionInput_0",
  "property": "InputName",
  "old_value": "Normal",
  "new_value": "WorldNormal"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid property, unsupported expression type, or function editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `EXPRESSION_NOT_FOUND` - Expression not found in function
- `INVALID_PROPERTY` - Property not supported for this expression type
- `UNSUPPORTED_EXPRESSION_TYPE` - Expression type not supported for property editing

**Notes:**
- Requires the function to be open in the editor
- Property names are case-insensitive
- "Value" can be used as alias for primary value properties (DefaultValue)
- Function is updated via `UpdateMaterialFunction` after setting
- Returns both old and new values when available

**curl:**
```bash
# Set function input name
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_name": "MaterialExpressionFunctionInput_0", "property": "InputName", "value": "WorldNormal"}'

# Set function output name
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_name": "MaterialExpressionFunctionOutput_0", "property": "OutputName", "value": "Result"}'

# Set scalar parameter default
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel", "expression_name": "MaterialExpressionScalarParameter_0", "property": "DefaultValue", "value": 5.0}'
```

---

## DELETE /materials/function/editor/node

Delete an expression node from a Material Function.

**Parameters (query or body):**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor |
| expression_name | string | Yes | - | Expression node name to delete |
| save | boolean | No | true | Save asset to disk after modification |

**Response:**
```json
{
  "success": true,
  "deleted_expression": "MaterialExpressionMultiply_0",
  "expression_class": "MaterialExpressionMultiply",
  "remaining_expressions": 4
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or function editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function
- `EXPRESSION_NOT_FOUND` - Expression not found in function

**Notes:**
- Requires the function to be open in the editor
- Supports both query parameters (for DELETE) and body parameters
- Uses `UMaterialEditingLibrary::DeleteMaterialExpressionInFunction` which automatically removes connections
- Function is updated via `UpdateMaterialFunction` after deletion
- Returns the class of the deleted expression and remaining expression count

**curl:**
```bash
# Using query parameters
curl -s -X DELETE "http://localhost:$PORT/api/v1/materials/function/editor/node?function_path=/Game/Materials/Functions/MF_Fresnel.MF_Fresnel&expression_name=MaterialExpressionMultiply_0"
```

---

## GET /materials/function/editor/export

Export a Material Function graph to XML format.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| function_path | string | Yes | - | Full path to the material function open in editor (query param) |

**Response:**
```json
{
  "success": true,
  "function_path": "/Game/Materials/Functions/MF_Fresnel.MF_Fresnel",
  "function_name": "MF_Fresnel",
  "xml": "<?xml version=\"1.0\" encoding=\"UTF-8\"?>...",
  "node_count": 5
}
```

**XML Format:**
```xml
<?xml version="1.0" encoding="UTF-8"?>
<MaterialFunctionGraph name="MF_Fresnel" path="/Game/Materials/Functions/MF_Fresnel.MF_Fresnel">
  <FunctionSettings>
    <Description>Custom Fresnel effect</Description>
    <ExposeToLibrary>true</ExposeToLibrary>
  </FunctionSettings>
  <Nodes>
    <Node id="MaterialExpressionFunctionInput_0" class="MaterialExpressionFunctionInput">
      <Position x="-400" y="0"/>
      <Properties>
        <InputName>Normal</InputName>
        <InputType>2</InputType>
        <Description>World normal</Description>
        <SortPriority>0</SortPriority>
        <UsePreviewValueAsDefault>false</UsePreviewValueAsDefault>
      </Properties>
      <Outputs>
        <Output index="0" name=""/>
      </Outputs>
      <Inputs>
        <Input index="0" name="Preview"/>
      </Inputs>
    </Node>
    <Node id="MaterialExpressionFunctionOutput_0" class="MaterialExpressionFunctionOutput">
      <Position x="200" y="0"/>
      <Properties>
        <OutputName>Result</OutputName>
        <Description></Description>
        <SortPriority>0</SortPriority>
      </Properties>
      <Outputs>
        <Output index="0" name=""/>
      </Outputs>
      <Inputs>
        <Input index="0" name=""/>
      </Inputs>
    </Node>
  </Nodes>
  <Connections>
    <Connection source="MaterialExpressionFresnel_0" output="0" target="MaterialExpressionFunctionOutput_0" input="0"/>
  </Connections>
</MaterialFunctionGraph>
```

**Status Codes:**
- 200 - Success
- 400 - Function editor not available

**Error Codes:**
- `NO_FUNCTION_EDITOR` - No editor is open for the specified function

**Notes:**
- Requires the function to be open in the editor
- Exports all nodes with positions, properties, outputs, inputs, and connections
- Root element is `<MaterialFunctionGraph>` (distinct from Material export which uses `<material>`)
- Includes `<FunctionSettings>` with Description and ExposeToLibrary
- `InputType` in XML is exported as integer enum value, not string
- XML can be used with `/materials/function/editor/import` to recreate the function

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/function/editor/export?function_path=/Game/Materials/Functions/MF_Fresnel.MF_Fresnel"
```

---

## POST /materials/function/editor/import

Import a Material Function from XML definition, creating a new function with nodes and connections.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| xml | string | Yes | - | XML material function definition (root element must be `<MaterialFunctionGraph>`) |
| name | string | No | (from XML) | Override function name |
| path | string | No | /Game/Materials/Functions | Package path for new function |
| save | boolean | No | true | Save asset to disk after import |

**Request:**
```json
{
  "xml": "<?xml version=\"1.0\" encoding=\"UTF-8\"?><MaterialFunctionGraph name=\"MF_Fresnel\">...</MaterialFunctionGraph>",
  "path": "/Game/Materials/Functions/Imported",
  "name": "MF_FresnelCopy"
}
```

**Response:**
```json
{
  "success": true,
  "function_path": "/Game/Materials/Functions/Imported/MF_FresnelCopy.MF_FresnelCopy",
  "function_name": "MF_FresnelCopy",
  "nodes_created": 5,
  "connections_created": 4
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing XML or invalid format
- 500 - Import failed

**Error Codes:**
- `INVALID_XML` - XML parsing failed or missing `MaterialFunctionGraph` root element
- `CREATE_FAILED` - Failed to create material function asset
- `FUNCTION_RELOAD_FAILED` - Failed to reload function after opening editor

**Notes:**
- Creates a new Material Function asset at the specified path
- If `name` is not provided, uses the `name` attribute from the XML root element, falling back to "ImportedFunction"
- Automatically opens the function in the editor for proper node creation
- Applies `<FunctionSettings>` (Description, ExposeToLibrary) from XML
- Recreates all nodes with original positions and properties
- Recreates all connections using graph schema pin connections with expression input fallback
- Unknown expression classes in the XML are silently skipped
- Use with `/materials/function/editor/export` for function templates and duplication

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/function/editor/import" \
  -H "Content-Type: application/json" \
  -d '{"xml": "<?xml version=\"1.0\"?><MaterialFunctionGraph name=\"MF_Test\">...</MaterialFunctionGraph>", "path": "/Game/Materials/Functions"}'
```

---

## Function-Specific Expression Types

Material Functions use two special expression types not available in regular Materials:

| Class | Description | Key Properties |
|-------|-------------|----------------|
| FunctionInput | Defines an input pin for the function | input_name, input_type, description, sort_priority |
| FunctionOutput | Defines an output pin for the function | output_name, description, sort_priority |

### FunctionInput Types

| Type | Description |
|------|-------------|
| Scalar | Single float value |
| Vector2 | 2-component vector |
| Vector3 | 3-component vector (e.g., normals, colors without alpha) |
| Vector4 | 4-component vector (e.g., colors with alpha) |
| Texture2D | 2D texture reference |
| TextureCube | Cube texture reference |
| StaticBool | Static boolean (compile-time switch) |
| Bool | Runtime boolean |
| MaterialAttributes | Full material attributes struct |

All standard expression classes (ScalarParameter, VectorParameter, Constant, Multiply, Add, Lerp, etc.) listed in the [Materials documentation](materials.md) are also available within Material Functions.
