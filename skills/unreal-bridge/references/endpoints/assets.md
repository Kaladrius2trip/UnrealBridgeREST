# Assets Endpoints

Base path: `/api/v1/assets`

Asset management endpoints using Unreal Engine's Asset Registry for queries. All paths use game content format: `/Game/Path/To/Asset`.

---

## GET /assets/list

List assets in a directory with optional filters.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | No | /Game | Content path to search (e.g., /Game/MyFolder) |
| type | string | No | - | Asset type filter (e.g., Material, StaticMesh, or full path /Script/Engine.Material) |
| limit | integer | No | 1000 | Maximum number of assets to return (hard limit: 10000) |

**Response:**
```json
{
  "success": true,
  "total": 150,
  "assets": [
    {
      "name": "M_Base",
      "path": "/Game/Materials/M_Base.M_Base",
      "class": "Material",
      "package": "/Game/Materials/M_Base"
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Invalid asset type

**Error Codes:**
- `INVALID_TYPE` - Invalid asset type specified

**Notes:**
- Searches recursively from the specified path
- Hard limit of 10000 assets prevents server hangs
- Progress logged every 100 assets during enumeration
- Type can be simple name or full path (e.g., "Material" or "/Script/Engine.Material")

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/assets/list?path=/Game/Materials&type=Material&limit=100"
```

---

## POST /assets/search

Search assets by name pattern.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| query | string | Yes | - | Search string to match against asset names (case-sensitive contains) |
| type | string | No | - | Filter by asset class name |
| limit | integer | No | 100 | Maximum number of results to return |

**Request:**
```json
{
  "query": "Base",
  "type": "Material",
  "limit": 50
}
```

**Response:**
```json
{
  "success": true,
  "count": 3,
  "assets": [
    {
      "name": "M_Base",
      "path": "/Game/Materials/M_Base.M_Base",
      "class": "Material",
      "package": "/Game/Materials/M_Base"
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter or invalid format

**Notes:**
- Case-sensitive substring match on asset names
- Searches all assets in the registry
- Stops searching once limit is reached

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/assets/search" \
  -H "Content-Type: application/json" \
  -d '{"query": "Base", "type": "Material", "limit": 50}'
```

---

## GET /assets/info

Get detailed information about a specific asset.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Full asset path (e.g., /Game/Materials/M_Basic.M_Basic) |

**Response:**
```json
{
  "success": true,
  "asset": {
    "name": "M_Base",
    "path": "/Game/Materials/M_Base.M_Base",
    "class": "Material",
    "package": "/Game/Materials/M_Base"
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing path parameter
- 404 - Asset not found

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter
- `ASSET_NOT_FOUND` - Asset does not exist at specified path

**Notes:**
- Uses asset registry for fast lookup without loading
- Suggestion to use `/assets/list` or `/assets/search` to find valid paths

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/assets/info?path=/Game/Materials/M_Base.M_Base"
```

---

## GET /assets/refs

Get asset references (dependencies) and referencers (what uses this asset).

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Package path of the asset (e.g., /Game/Materials/M_Basic) |

**Response:**
```json
{
  "success": true,
  "asset": "/Game/Materials/M_Base",
  "referencers": [
    "/Game/Blueprints/BP_Player",
    "/Game/Materials/M_Variant"
  ],
  "dependencies": [
    "/Game/Textures/T_BaseColor",
    "/Game/Textures/T_Normal"
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing path parameter

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter

**Notes:**
- `referencers` - Assets that reference this asset (what uses it)
- `dependencies` - Assets that this asset references (what it uses)
- Uses package name format (without the asset name suffix)

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/assets/refs?path=/Game/Materials/M_Base"
```

---

## POST /assets/export

Export an asset to text format (T3D-like output).

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Full asset path to export |

**Request:**
```json
{
  "path": "/Game/Materials/M_Base.M_Base"
}
```

**Response:**
```json
{
  "success": true,
  "path": "/Game/Materials/M_Base.M_Base",
  "exported_text": "Begin Object Class=/Script/Engine.Material Name=\"M_Base\" ...\nEnd Object"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters
- 404 - Asset not found

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter
- `ASSET_NOT_FOUND` - Asset does not exist or cannot be loaded

**Notes:**
- Loads the asset into memory
- Exports using UExporter::ExportToOutputDevice (T3D format)
- Useful for inspecting asset properties and structure

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/assets/export" \
  -H "Content-Type: application/json" \
  -d '{"path": "/Game/Materials/M_Base.M_Base"}'
```

---

## POST /assets/validate

Validate asset integrity.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Full asset path to validate |

**Request:**
```json
{
  "path": "/Game/Materials/M_Base.M_Base"
}
```

**Response:**
```json
{
  "success": true,
  "valid": true,
  "path": "/Game/Materials/M_Base.M_Base",
  "errors": []
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters
- 404 - Asset not found

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter
- `ASSET_NOT_FOUND` - Asset does not exist or cannot be loaded

**Notes:**
- Loads the asset to verify it can be accessed
- Performs basic validation using UObject::IsAsset()
- Returns empty errors array if valid

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/assets/validate" \
  -H "Content-Type: application/json" \
  -d '{"path": "/Game/Materials/M_Base.M_Base"}'
```

---

## GET /assets/mesh_details

Get detailed geometry information for a static mesh (LODs, vertices, triangles, bounds).

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Full path to the static mesh asset |

**Response:**
```json
{
  "success": true,
  "path": "/Game/Meshes/SM_Cube.SM_Cube",
  "mesh": {
    "lod_count": 1,
    "bounds_origin": {"x": 0.0, "y": 0.0, "z": 0.0},
    "bounds_extent": {"x": 50.0, "y": 50.0, "z": 50.0},
    "bounds_radius": 86.6,
    "lods": [
      {
        "index": 0,
        "vertices": 24,
        "triangles": 12,
        "sections": 1
      }
    ]
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing path parameter
- 404 - Static mesh not found

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter
- `ASSET_NOT_FOUND` - Static mesh does not exist or is not a valid static mesh

**Notes:**
- Only works with StaticMesh assets
- Bounds are in Unreal units (centimeters)
- Returns LOD count, bounding box, and per-LOD geometry details
- Vertices, triangles, and sections are per-LOD

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/assets/mesh_details?path=/Game/Meshes/SM_Cube.SM_Cube"
```

---

## Asset Data Format

All endpoints that return asset information use this common format:

```json
{
  "name": "M_Base",
  "path": "/Game/Materials/M_Base.M_Base",
  "class": "Material",
  "package": "/Game/Materials/M_Base"
}
```

| Field | Description |
|-------|-------------|
| name | Asset name (without path) |
| path | Full object path (used for loading) |
| class | Asset class type (Material, StaticMesh, etc.) |
| package | Package path (used for references) |
