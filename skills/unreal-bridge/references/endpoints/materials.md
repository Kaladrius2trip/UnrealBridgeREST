# Materials Endpoints

Base path: `/api/v1/materials`

Material parameter access, replacement, and Material Editor manipulation endpoints. Provides both runtime material instance parameter control and base Material graph editing.

**IMPORTANT:** There are two categories of endpoints:

1. **Material Instance endpoints** (`/materials/param`, `/materials/replace`) - Work with runtime material instances (MaterialInstanceConstant, MaterialInstanceDynamic) and can be used without opening an editor.

2. **Material Editor endpoints** (`/materials/editor/*`) - Work with base Material assets and require a Material Editor to be open in Unreal Engine. If no Material Editor is open, these endpoints will return `NO_MATERIAL_EDITOR` error.

---

## GET /materials/param

Get a parameter value from a material instance.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to material instance (e.g., "/Game/Materials/MI_MyMaterial.MI_MyMaterial") |
| param_name | string | Yes | - | Parameter name to retrieve |

**Response (Scalar):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "Roughness",
  "material_type": "MaterialInstanceConstant",
  "param_type": "scalar",
  "value": 0.5
}
```

**Response (Vector/Color):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "BaseColor",
  "material_type": "MaterialInstanceConstant",
  "param_type": "vector",
  "value": {
    "r": 1.0,
    "g": 0.0,
    "b": 0.0,
    "a": 1.0
  }
}
```

**Response (Texture):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "BaseTexture",
  "material_type": "MaterialInstanceConstant",
  "param_type": "texture",
  "value": "/Game/Textures/T_Checker.T_Checker"
}
```

**Response (Base Material):**
```json
{
  "success": false,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_type": "Material",
  "message": "Base materials define parameters via expressions. Use a MaterialInstance to get/set parameter values."
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing required parameters
- 404 - Material or parameter not found

**Error Codes:**
- `PARAM_NOT_FOUND` - Parameter not found in material
- `MATERIAL_NOT_FOUND` - Material asset not found

**Notes:**
- Only works with MaterialInstanceConstant (not base Material assets)
- Base Materials define parameters via expressions, not simple values
- Supports scalar (float), vector (color), and texture parameters
- Returns parameter type with the value

**curl:**
```bash
# Get scalar parameter
curl -s "http://localhost:$PORT/api/v1/materials/param?material_path=/Game/Materials/MI_MyMaterial.MI_MyMaterial&param_name=Roughness"

# Get color parameter
curl -s "http://localhost:$PORT/api/v1/materials/param?material_path=/Game/Materials/MI_MyMaterial.MI_MyMaterial&param_name=BaseColor"
```

---

## POST /materials/param

Set a parameter value on a material instance.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to material instance |
| param_name | string | Yes | - | Parameter name to set |
| value | number\|object\|string | Yes | - | Parameter value (type depends on parameter) |
| save | boolean | No | true | Save asset to disk after modification |

**Request (Scalar):**
```json
{
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "Roughness",
  "value": 0.8
}
```

**Request (Vector/Color):**
```json
{
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "BaseColor",
  "value": {
    "r": 0.0,
    "g": 1.0,
    "b": 0.0,
    "a": 1.0
  }
}
```

**Request (Texture):**
```json
{
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "BaseTexture",
  "value": "/Game/Textures/T_Wood.T_Wood"
}
```

**Response (Scalar):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "Roughness",
  "param_type": "scalar",
  "old_value": 0.5,
  "new_value": 0.8
}
```

**Response (Vector/Color):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "param_name": "BaseColor",
  "param_type": "vector",
  "old_value": {
    "r": 1.0,
    "g": 0.0,
    "b": 0.0,
    "a": 1.0
  },
  "new_value": {
    "r": 0.0,
    "g": 1.0,
    "b": 0.0,
    "a": 1.0
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid value type, or cannot modify base Material
- 404 - Material or texture not found

**Error Codes:**
- `MATERIAL_NOT_FOUND` - Material asset not found
- `TEXTURE_NOT_FOUND` - Texture asset not found (for texture parameters)
- `CANNOT_MODIFY_BASE_MATERIAL` - Cannot set parameters on base Material (create MaterialInstance first)

**Notes:**
- Only works with MaterialInstanceConstant
- Value type must match parameter type:
  - Scalar: number (float)
  - Vector/Color: object with r, g, b, a fields (a is optional, defaults to 1.0)
  - Texture: string path to texture asset, or "None" to clear
- Material is marked as modified after setting
- Returns both old and new values

**curl:**
```bash
# Set scalar parameter
curl -s -X POST "http://localhost:$PORT/api/v1/materials/param" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial", "param_name": "Roughness", "value": 0.8}'

# Set color parameter
curl -s -X POST "http://localhost:$PORT/api/v1/materials/param" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial", "param_name": "BaseColor", "value": {"r": 0.0, "g": 1.0, "b": 0.0}}'
```

---

## POST /materials/recompile

Force recompile a material for rendering.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to material or material instance |

**Request:**
```json
{
  "material_path": "/Game/Materials/M_Master.M_Master"
}
```

**Response (Base Material):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_type": "Material",
  "message": "Material recompiled for rendering"
}
```

**Response (Material Instance):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/MI_MyMaterial.MI_MyMaterial",
  "material_type": "MaterialInstance",
  "parent_material": "/Game/Materials/M_Master.M_Master",
  "message": "Parent material recompiled for rendering"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters
- 404 - Material not found

**Error Codes:**
- `MATERIAL_NOT_FOUND` - Material asset not found

**Notes:**
- Works with both base Materials and MaterialInstances
- For MaterialInstances, recompiles the parent material
- Forces recompilation for rendering (updates shaders)
- Use after making structural changes to materials

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/recompile" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/M_Master.M_Master"}'
```

---

## POST /materials/replace

Replace material on actor(s) in the scene.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Path to new material to apply |
| label | string | No* | - | Single actor label |
| labels | array | No* | - | Array of actor labels |
| material_index | integer | No | -1 | Material slot index (-1 = all slots) |

*At least one of `label` or `labels` is required.

**Request (Single Actor, All Materials):**
```json
{
  "material_path": "/Game/Materials/M_NewMaterial.M_NewMaterial",
  "label": "StaticMeshActor_5"
}
```

**Request (Multiple Actors, Specific Slot):**
```json
{
  "material_path": "/Game/Materials/M_NewMaterial.M_NewMaterial",
  "labels": ["Cube_1", "Cube_2", "Cube_3"],
  "material_index": 0
}
```

**Response:**
```json
{
  "success": true,
  "new_material": "/Game/Materials/M_NewMaterial.M_NewMaterial",
  "actors_processed": 2,
  "actors": [
    {
      "label": "Cube_1",
      "materials_replaced": 3,
      "replaced": [
        {
          "component": "StaticMeshComponent0",
          "index": 0,
          "old_material": "/Game/Materials/M_OldMaterial.M_OldMaterial"
        },
        {
          "component": "StaticMeshComponent0",
          "index": 1,
          "old_material": "/Game/Materials/M_OldMaterial.M_OldMaterial"
        },
        {
          "component": "StaticMeshComponent0",
          "index": 2,
          "old_material": "/Game/Materials/M_OldMaterial.M_OldMaterial"
        }
      ]
    },
    {
      "label": "Cube_2",
      "materials_replaced": 3,
      "replaced": [...]
    }
  ]
}
```

**Response (With Not Found):**
```json
{
  "success": false,
  "new_material": "/Game/Materials/M_NewMaterial.M_NewMaterial",
  "actors_processed": 1,
  "actors": [...],
  "not_found": ["InvalidActorName"],
  "suggestions": ["SimilarActorName1", "SimilarActorName2"]
}
```

**Status Codes:**
- 200 - Success (even if some actors not found)
- 400 - Missing parameters
- 404 - Material not found

**Error Codes:**
- `MATERIAL_NOT_FOUND` - Material asset not found

**Notes:**
- Replaces materials on all PrimitiveComponents of the actor(s)
- If `material_index` is -1 (default), replaces all material slots
- If `material_index` is specified, only replaces that slot
- Returns details of all replaced materials per component
- If actors not found, provides similar actor name suggestions
- Supports both single `label` and array of `labels`

**curl:**
```bash
# Replace all materials on single actor
curl -s -X POST "http://localhost:$PORT/api/v1/materials/replace" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/M_NewMaterial.M_NewMaterial", "label": "StaticMeshActor_5"}'

# Replace specific material slot on multiple actors
curl -s -X POST "http://localhost:$PORT/api/v1/materials/replace" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/M_NewMaterial.M_NewMaterial", "labels": ["Cube_1", "Cube_2"], "material_index": 0}'
```

---

## POST /materials/create

Create a new base Material asset.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| name | string | Yes | - | Material name |
| path | string | No | /Game/Materials | Package path (auto-prefixed with /Game/ if needed) |
| save | boolean | No | true | Save asset to disk after creation |

**Request:**
```json
{
  "name": "M_MyNewMaterial",
  "path": "/Game/Materials/Characters"
}
```

**Response:**
```json
{
  "success": true,
  "material_name": "M_MyNewMaterial",
  "material_path": "/Game/Materials/Characters/M_MyNewMaterial.M_MyNewMaterial",
  "package_path": "/Game/Materials/Characters"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing required parameters
- 500 - Creation failed

**Error Codes:**
- `CREATE_FAILED` - Failed to create material asset

**Notes:**
- Creates a new base Material asset (not MaterialInstance)
- Path is automatically prefixed with `/Game/` if not present
- Material is marked as dirty and ready to be saved
- Use `/materials/editor/open` to open the new material for editing
- Default path is `/Game/Materials`

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/create" \
  -H "Content-Type: application/json" \
  -d '{"name": "M_MyNewMaterial", "path": "/Game/Materials/Characters"}'
```

---

## POST /materials/editor/open

Open a base Material in the Material Editor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to base Material asset |

**Request:**
```json
{
  "material_path": "/Game/Materials/M_Master.M_Master"
}
```

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_name": "M_Master",
  "message": "Material Editor opened"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters
- 404 - Material not found
- 500 - Failed to open editor

**Error Codes:**
- `MATERIAL_NOT_FOUND` - Material asset not found
- `OPEN_FAILED` - Failed to open Material Editor

**Notes:**
- Only works with base Material assets (not MaterialInstances)
- Opens the Material Editor window in Unreal Engine
- Material must be open before using other `/materials/editor/*` endpoints
- Editor is opened on the game thread

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/open" \
  -H "Content-Type: application/json" \
  -d '{"material_path": "/Game/Materials/M_Master.M_Master"}'
```

---

## GET /materials/editor/nodes

List all expression nodes in the currently open Material Editor.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_name": "M_Master",
  "expression_count": 5,
  "expressions": [
    {
      "name": "MaterialExpressionScalarParameter_0",
      "class": "MaterialExpressionScalarParameter",
      "description": "A scalar parameter",
      "position": {"x": -400, "y": -100},
      "param_name": "Roughness",
      "default_value": 0.5,
      "outputs": [
        {"index": 0, "name": ""}
      ]
    },
    {
      "name": "MaterialExpressionVectorParameter_0",
      "class": "MaterialExpressionVectorParameter",
      "description": "A vector parameter",
      "position": {"x": -400, "y": 100},
      "param_name": "BaseColor",
      "default_value": {
        "r": 1.0,
        "g": 1.0,
        "b": 1.0,
        "a": 1.0
      },
      "outputs": [
        {"index": 0, "name": ""}
      ]
    },
    {
      "name": "MaterialExpressionConstant_0",
      "class": "MaterialExpressionConstant",
      "description": "",
      "position": {"x": -200, "y": 0},
      "value": 0.8,
      "outputs": [
        {"index": 0, "name": ""}
      ]
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open

**Notes:**
- Requires Material Editor to be open
- Returns all expression nodes in the material graph
- Includes position, class, description, and type-specific properties
- `material_path` is required to identify which material to operate on
- Use `/materials/editor/open` first to open a material

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/editor/nodes"

# Target specific material if multiple open
curl -s "http://localhost:$PORT/api/v1/materials/editor/nodes?material_path=/Game/Materials/M_Master.M_Master"
```

---

## POST /materials/editor/node/position

Move a material expression node to a new position.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| expression_name | string | Yes | - | Expression node name |
| position | object | Yes | - | New position with `x` and `y` fields |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| save | boolean | No | true | Save asset to disk after modification |

**Request:**
```json
{
  "expression_name": "MaterialExpressionScalarParameter_0",
  "position": {
    "x": -500,
    "y": -200
  }
}
```

**Response:**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionScalarParameter_0",
  "old_position": {"x": -400, "y": -100},
  "new_position": {"x": -500, "y": -200}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Material Editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `EXPRESSION_NOT_FOUND` - Expression not found in material

**Notes:**
- Requires Material Editor to be open
- Position coordinates are in graph space
- Material is marked as modified after moving
- Returns both old and new positions

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/node/position" \
  -H "Content-Type: application/json" \
  -d '{"expression_name": "MaterialExpressionScalarParameter_0", "position": {"x": -500, "y": -200}}'
```

---

## POST /materials/editor/node/create

Create a new material expression node in the Material Editor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| expression_class | string | Yes | - | Expression class name (with or without "MaterialExpression" prefix) |
| position | object | No | {x:0, y:0} | Position with `x` and `y` fields |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| param_name | string | Conditional* | - | Parameter name (for parameter expressions) |
| default_value | number\|object | Conditional* | - | Default value (for parameters and constants) |
| save | boolean | No | true | Save asset to disk after modification |

*Required for certain expression types

**Request (ScalarParameter):**
```json
{
  "expression_class": "ScalarParameter",
  "position": {"x": -400, "y": 0},
  "param_name": "Metallic",
  "default_value": 0.0
}
```

**Request (VectorParameter):**
```json
{
  "expression_class": "VectorParameter",
  "position": {"x": -400, "y": 100},
  "param_name": "EmissiveColor",
  "default_value": {
    "r": 1.0,
    "g": 0.5,
    "b": 0.0,
    "a": 1.0
  }
}
```

**Request (Constant):**
```json
{
  "expression_class": "Constant",
  "position": {"x": -200, "y": 0},
  "value": 0.5
}
```

**Request (Constant3Vector):**
```json
{
  "expression_class": "Constant3Vector",
  "position": {"x": -200, "y": 100},
  "value": {
    "r": 1.0,
    "g": 1.0,
    "b": 1.0
  }
}
```

**Request (Multiply/Add/etc.):**
```json
{
  "expression_class": "Multiply",
  "position": {"x": 0, "y": 0}
}
```

**Response:**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionScalarParameter_1",
  "expression_class": "MaterialExpressionScalarParameter",
  "created_via_editor_api": true,
  "expression": {
    "name": "MaterialExpressionScalarParameter_1",
    "class": "MaterialExpressionScalarParameter",
    "description": "A scalar parameter",
    "position": {"x": -400, "y": 0},
    "param_name": "Metallic",
    "default_value": 0.0,
    "outputs": [
      {"index": 0, "name": ""}
    ]
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid expression class, or Material Editor not available
- 500 - Creation failed

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `INVALID_EXPRESSION_CLASS` - Expression class not found or invalid
- `CREATE_FAILED` - Failed to create expression

**Notes:**
- Requires Material Editor to be open
- Expression class can be specified with or without "MaterialExpression" prefix
- Common types: ScalarParameter, VectorParameter, TextureSample, Add, Multiply, Constant, Constant3Vector
- Uses Material Editor API when possible for proper visual updates
- Material is marked as modified after creation
- `created_via_editor_api` indicates if editor API was used (provides better visual updates)

**curl:**
```bash
# Create scalar parameter
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/node/create" \
  -H "Content-Type: application/json" \
  -d '{"expression_class": "ScalarParameter", "position": {"x": -400, "y": 0}, "param_name": "Metallic", "default_value": 0.0}'

# Create math node
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/node/create" \
  -H "Content-Type: application/json" \
  -d '{"expression_class": "Multiply", "position": {"x": 0, "y": 0}}'
```

---

## POST /materials/editor/connect

Connect material expression outputs to inputs or material properties.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| source_expression | string | Yes | - | Source expression name |
| output_index | integer | No | 0 | Output pin index on source |
| target_property | string | No* | - | Material property (BaseColor, Metallic, etc.) |
| target_expression | string | No* | - | Target expression name |
| input_index | integer | No | 0 | Input pin index on target expression |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| save | boolean | No | true | Save asset to disk after modification |

*Exactly one of `target_property` or `target_expression` is required.

**Request (Connect to Material Property):**
```json
{
  "source_expression": "MaterialExpressionVectorParameter_0",
  "output_index": 0,
  "target_property": "BaseColor"
}
```

**Request (Connect to Another Expression):**
```json
{
  "source_expression": "MaterialExpressionScalarParameter_0",
  "output_index": 0,
  "target_expression": "MaterialExpressionMultiply_0",
  "input_index": 0
}
```

**Response (Material Property):**
```json
{
  "success": true,
  "source_expression": "MaterialExpressionVectorParameter_0",
  "output_index": 0,
  "target_property": "BaseColor"
}
```

**Response (Expression Input):**
```json
{
  "success": true,
  "source_expression": "MaterialExpressionScalarParameter_0",
  "output_index": 0,
  "target_expression": "MaterialExpressionMultiply_0",
  "input_index": 0
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid indices, or Material Editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `SOURCE_NOT_FOUND` - Source expression not found
- `TARGET_NOT_FOUND` - Target expression not found
- `INVALID_OUTPUT_INDEX` - Output index out of range
- `INVALID_INPUT_INDEX` - Input index out of range
- `INVALID_TARGET_PROPERTY` - Unknown material property

**Notes:**
- Requires Material Editor to be open
- Valid material properties: BaseColor, Metallic, Specular, Roughness, EmissiveColor, Normal, Opacity, OpacityMask, AmbientOcclusion
- Output index defaults to 0 if not specified
- Input index defaults to 0 if not specified
- Material is marked as modified after connection
- Graph is refreshed after connection

**curl:**
```bash
# Connect to material property
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/connect" \
  -H "Content-Type: application/json" \
  -d '{"source_expression": "MaterialExpressionVectorParameter_0", "target_property": "BaseColor"}'

# Connect to another expression
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/connect" \
  -H "Content-Type: application/json" \
  -d '{"source_expression": "MaterialExpressionScalarParameter_0", "target_expression": "MaterialExpressionMultiply_0", "input_index": 0}'
```

---

## GET /materials/editor/status

Get material compilation status, domain, blend mode, and errors.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_name": "M_Master",
  "domain": "Surface",
  "blend_mode": "Opaque",
  "expression_count": 5,
  "has_valid_shader": true,
  "has_errors": false,
  "compile_errors": []
}
```

**Response (With Errors):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_name": "M_Master",
  "domain": "Surface",
  "blend_mode": "Opaque",
  "expression_count": 5,
  "has_valid_shader": false,
  "has_errors": true,
  "compile_errors": [
    {"error": "[SM5] /Game/Materials/M_Master.M_Master: Missing BaseColor input"},
    {"error": "[SM5] /Game/Materials/M_Master.M_Master: Material has compilation errors"}
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open

**Notes:**
- Requires Material Editor to be open
- Domain values: Surface, DeferredDecal, LightFunction, Volume, PostProcess, UI
- Blend mode values: Opaque, Masked, Translucent, Additive, Modulate
- Returns shader compilation status and errors
- Use to verify material is valid before saving

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/editor/status"
```

---

## POST /materials/editor/refresh

Force refresh the Material Editor graph to show latest changes.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |

**Request:**
```json
{
  "material_path": "/Game/Materials/M_Master.M_Master"
}
```

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "message": "Material Editor graph refreshed"
}
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open

**Notes:**
- Requires Material Editor to be open
- Triggers graph rebuild and notification
- Updates Material Editor UI and expression previews
- Note: Visual graph updates may not appear until material is closed and reopened (known limitation)

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/refresh" \
  -H "Content-Type: application/json" \
  -d '{}'
```

---

## POST /materials/editor/expression/set

Set a property value on a material expression in the Material Editor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| expression_name | string | Yes | - | Expression node name |
| property | string | Yes | - | Property name to set |
| value | number\|string\|object | Yes | - | New value (type depends on property) |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| save | boolean | No | true | Save asset to disk after modification |

**Request (ScalarParameter DefaultValue):**
```json
{
  "expression_name": "MaterialExpressionScalarParameter_0",
  "property": "DefaultValue",
  "value": 0.75
}
```

**Request (ScalarParameter ParameterName):**
```json
{
  "expression_name": "MaterialExpressionScalarParameter_0",
  "property": "ParameterName",
  "value": "NewRoughnessParam"
}
```

**Request (VectorParameter DefaultValue):**
```json
{
  "expression_name": "MaterialExpressionVectorParameter_0",
  "property": "DefaultValue",
  "value": {
    "r": 0.5,
    "g": 0.5,
    "b": 1.0,
    "a": 1.0
  }
}
```

**Request (Constant Value):**
```json
{
  "expression_name": "MaterialExpressionConstant_0",
  "property": "R",
  "value": 0.9
}
```

**Request (Constant3Vector Value):**
```json
{
  "expression_name": "MaterialExpressionConstant3Vector_0",
  "property": "Constant",
  "value": {
    "r": 1.0,
    "g": 0.0,
    "b": 0.0
  }
}
```

**Response (Scalar):**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionScalarParameter_0",
  "property": "DefaultValue",
  "old_value": 0.5,
  "new_value": 0.75
}
```

**Response (Vector):**
```json
{
  "success": true,
  "expression_name": "MaterialExpressionVectorParameter_0",
  "property": "DefaultValue",
  "old_value": {
    "r": 1.0,
    "g": 1.0,
    "b": 1.0,
    "a": 1.0
  },
  "new_value": {
    "r": 0.5,
    "g": 0.5,
    "b": 1.0,
    "a": 1.0
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters, invalid property, unsupported expression type, or Material Editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `EXPRESSION_NOT_FOUND` - Expression not found in material
- `INVALID_PROPERTY` - Property not supported for this expression type
- `UNSUPPORTED_EXPRESSION_TYPE` - Expression type not supported for property editing

**Notes:**
- Requires Material Editor to be open
- Supported expression types:
  - ScalarParameter: DefaultValue (number), ParameterName (string)
  - VectorParameter: DefaultValue (object {r,g,b,a}), ParameterName (string)
  - Constant: R (number), Value (number)
  - Constant3Vector: Constant (object {r,g,b}), Value (object {r,g,b})
- Property names are case-insensitive
- "Value" can be used as alias for primary value properties
- Material is marked as modified after setting
- Editor preview is refreshed after setting
- Returns both old and new values

**curl:**
```bash
# Set scalar parameter default value
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"expression_name": "MaterialExpressionScalarParameter_0", "property": "DefaultValue", "value": 0.75}'

# Set vector parameter default color
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"expression_name": "MaterialExpressionVectorParameter_0", "property": "DefaultValue", "value": {"r": 0.5, "g": 0.5, "b": 1.0}}'

# Set constant value
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/expression/set" \
  -H "Content-Type: application/json" \
  -d '{"expression_name": "MaterialExpressionConstant_0", "property": "Value", "value": 0.9}'
```

---

## GET /materials/editor/connections

List all connections in the material graph.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |
| expression | string | No | - | Filter connections by expression name |

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "connection_count": 3,
  "connections": [
    {
      "source_expression": "MaterialExpressionVectorParameter_0",
      "output_index": 0,
      "target_type": "property",
      "target_property": "BaseColor"
    },
    {
      "source_expression": "MaterialExpressionScalarParameter_0",
      "output_index": 0,
      "target_type": "expression",
      "target_expression": "MaterialExpressionMultiply_0",
      "input_index": 0
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open

**Notes:**
- Requires Material Editor to be open
- Returns both property connections and expression-to-expression connections
- Use `expression` parameter to filter connections involving a specific node

**curl:**
```bash
# Get all connections
curl -s "http://localhost:$PORT/api/v1/materials/editor/connections"

# Filter by expression
curl -s "http://localhost:$PORT/api/v1/materials/editor/connections?expression=MaterialExpressionMultiply_0"
```

---

## POST /materials/editor/disconnect

Disconnect an input from a material property or expression.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| target_property | string | No* | - | Material property to disconnect |
| target_expression | string | No* | - | Target expression name |
| input_index | integer | No | 0 | Input pin index on target expression |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| save | boolean | No | true | Save asset to disk after modification |

*Exactly one of `target_property` or `target_expression` is required.

**Request (Disconnect Property):**
```json
{
  "target_property": "BaseColor"
}
```

**Request (Disconnect Expression Input):**
```json
{
  "target_expression": "MaterialExpressionMultiply_0",
  "input_index": 0
}
```

**Response:**
```json
{
  "success": true,
  "disconnected": true,
  "target_property": "BaseColor",
  "previous_source": "MaterialExpressionVectorParameter_0"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Material Editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `TARGET_NOT_FOUND` - Target expression not found
- `INVALID_INPUT_INDEX` - Input index out of range
- `INVALID_TARGET_PROPERTY` - Unknown material property

**Notes:**
- Requires Material Editor to be open
- Material is marked as modified after disconnection
- Graph is refreshed after disconnection

**curl:**
```bash
# Disconnect property
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/disconnect" \
  -H "Content-Type: application/json" \
  -d '{"target_property": "BaseColor"}'

# Disconnect expression input
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/disconnect" \
  -H "Content-Type: application/json" \
  -d '{"target_expression": "MaterialExpressionMultiply_0", "input_index": 0}'
```

---

## DELETE /materials/editor/node

Delete an expression node and all its connections from the material.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| expression_name | string | Yes | - | Expression node name to delete |
| material_path | string | Yes | - | Full path to the material asset open in editor |
| save | boolean | No | true | Save asset to disk after modification |

**Response:**
```json
{
  "success": true,
  "deleted_expression": "MaterialExpressionMultiply_0",
  "connections_removed": 2
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or Material Editor not available
- 404 - Expression not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `EXPRESSION_NOT_FOUND` - Expression not found in material

**Notes:**
- Requires Material Editor to be open
- Automatically removes all connections to/from the deleted node
- Material is marked as modified after deletion
- Graph is refreshed after deletion

**curl:**
```bash
curl -s -X DELETE "http://localhost:$PORT/api/v1/materials/editor/node?expression_name=MaterialExpressionMultiply_0"
```

---

## GET /materials/editor/validate

Validate material graph for disconnected nodes, missing connections, and compile errors.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |

**Response (Valid):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "is_valid": true,
  "warnings": [],
  "errors": []
}
```

**Response (With Issues):**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "is_valid": false,
  "warnings": [
    {"type": "disconnected_node", "expression": "MaterialExpressionConstant_0", "message": "Node has no connections"}
  ],
  "errors": [
    {"type": "compile_error", "message": "[SM5] Missing BaseColor input"}
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open

**Notes:**
- Requires Material Editor to be open
- Checks for disconnected/orphan nodes
- Checks for missing required connections
- Reports compilation errors
- Use before saving to catch issues

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/editor/validate"
```

---

## GET /materials/editor/export

Export material graph to XML format for backup, transfer, or analysis.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| material_path | string | Yes | - | Full path to the material asset open in editor |

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/M_Master.M_Master",
  "material_name": "M_Master",
  "xml": "<?xml version=\"1.0\"?>...",
  "node_count": 5,
  "connection_count": 4
}
```

**XML Format:**
```xml
<?xml version="1.0"?>
<material name="M_Master" blend_mode="Opaque" shading_model="DefaultLit">
  <nodes>
    <node class="MaterialExpressionVectorParameter" name="MaterialExpressionVectorParameter_0">
      <position x="-400" y="0"/>
      <param_name>BaseColor</param_name>
      <default_value r="1.0" g="1.0" b="1.0" a="1.0"/>
    </node>
    <node class="MaterialExpressionScalarParameter" name="MaterialExpressionScalarParameter_0">
      <position x="-400" y="100"/>
      <param_name>Roughness</param_name>
      <default_value>0.5</default_value>
    </node>
  </nodes>
  <connections>
    <connection>
      <source expression="MaterialExpressionVectorParameter_0" output="0"/>
      <target property="BaseColor"/>
    </connection>
    <connection>
      <source expression="MaterialExpressionScalarParameter_0" output="0"/>
      <target property="Roughness"/>
    </connection>
  </connections>
</material>
```

**Status Codes:**
- 200 - Success
- 400 - Material Editor not available
- 404 - Material not found

**Error Codes:**
- `NO_MATERIAL_EDITOR` - No Material Editor is currently open
- `MATERIAL_NOT_FOUND` - Material asset not found

**Notes:**
- Requires Material Editor to be open (or specify material_path to open it)
- Exports all nodes with positions, properties, and connections
- XML can be used with `/materials/editor/import` to recreate material
- Useful for material templates and backup

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/materials/editor/export"

# Export specific material
curl -s "http://localhost:$PORT/api/v1/materials/editor/export?material_path=/Game/Materials/M_Master.M_Master"
```

---

## POST /materials/editor/import

Import material from XML definition, creating a new material with nodes and connections.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| xml | string | Yes | - | XML material definition |
| path | string | No | /Game/Materials | Package path for new material |
| name | string | No | (from XML) | Override material name |
| save | boolean | No | true | Save asset to disk after import |

**Request:**
```json
{
  "xml": "<?xml version=\"1.0\"?><material name=\"M_Imported\" blend_mode=\"Opaque\">...</material>",
  "path": "/Game/Materials/Imported",
  "name": "M_CustomName"
}
```

**Response:**
```json
{
  "success": true,
  "material_path": "/Game/Materials/Imported/M_CustomName.M_CustomName",
  "material_name": "M_CustomName",
  "nodes_created": 5,
  "connections_created": 4
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing XML or invalid format
- 500 - Import failed

**Error Codes:**
- `INVALID_XML` - XML parsing failed
- `IMPORT_FAILED` - Failed to create material or nodes

**Notes:**
- Creates new material asset at specified path
- Opens Material Editor automatically for proper node creation
- Recreates all nodes with original positions
- Recreates all connections (property and expression-to-expression)
- Material is saved after successful import
- Use with `/materials/editor/export` for material templates

**curl:**
```bash
# Import from exported XML
curl -s -X POST "http://localhost:$PORT/api/v1/materials/editor/import" \
  -H "Content-Type: application/json" \
  -d '{"xml": "<?xml version=\"1.0\"?><material name=\"M_Test\">...</material>", "path": "/Game/Materials"}'
```

---

## Expression Data Format

Material expression nodes are returned in this common format:

```json
{
  "name": "MaterialExpressionScalarParameter_0",
  "class": "MaterialExpressionScalarParameter",
  "description": "A scalar parameter",
  "position": {"x": -400, "y": -100},
  "param_name": "Roughness",
  "default_value": 0.5,
  "outputs": [
    {"index": 0, "name": ""}
  ]
}
```

| Field | Description |
|-------|-------------|
| name | Expression node name (unique identifier) |
| class | Expression class name |
| description | Expression description text |
| position | Node position in graph (x, y coordinates) |
| param_name | Parameter name (for parameter expressions) |
| default_value | Default value (for parameters and constants) |
| value | Constant value (for constant expressions) |
| outputs | Array of output pins with index and name |

Type-specific fields:
- ScalarParameter: `param_name`, `default_value` (number)
- VectorParameter: `param_name`, `default_value` (object {r,g,b,a})
- Constant: `value` (number)
- Constant3Vector: `value` (object {r,g,b})

---

## Common Expression Classes

| Class | Description | Common Uses |
|-------|-------------|-------------|
| ScalarParameter | Scalar parameter (float) | Roughness, Metallic, Opacity values |
| VectorParameter | Vector parameter (color) | BaseColor, EmissiveColor |
| TextureSample | Texture sampler | Texture maps |
| Constant | Scalar constant | Fixed values |
| Constant3Vector | Vector constant | Fixed colors |
| Add | Add two values | Math operations |
| Multiply | Multiply two values | Modulating values |
| Lerp | Linear interpolation | Blending |
| OneMinus | 1 - value | Inverting values |
| Clamp | Clamp value to range | Limiting values |

---

## Material Property Names

Valid target properties for `/materials/editor/connect`:

| Property | Description |
|----------|-------------|
| BaseColor | Base color (albedo) |
| Metallic | Metallic value (0-1) |
| Specular | Specular intensity |
| Roughness | Surface roughness (0-1) |
| EmissiveColor | Emissive (glow) color |
| Normal | Normal map input |
| Opacity | Opacity (for translucent) |
| OpacityMask | Opacity mask (for masked) |
| AmbientOcclusion | Ambient occlusion |
