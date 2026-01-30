# Level Endpoints

Base path: `/api/v1/level`

Level and world management endpoints using GEditor for level operations. Provides level information, world outliner data, and level loading capabilities.

---

## GET /level/info

Get current level information including name, path, actor count, and world bounds.

**Parameters:**

None

**Response:**
```json
{
  "success": true,
  "level": {
    "name": "MyLevel",
    "path": "/Game/Maps/MyLevel",
    "actor_count": 42,
    "bounds_min": {"x": -1000.0, "y": -1000.0, "z": 0.0},
    "bounds_max": {"x": 1000.0, "y": 1000.0, "z": 500.0}
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - No level loaded

**Error Codes:**
- `NO_LEVEL_LOADED` - No level currently open in editor

**Notes:**
- Returns information about the currently open level in the editor
- Actor count includes all actors in the world (iterated with TActorIterator)
- Bounds are in Unreal units (centimeters)
- Bounds are calculated from the world settings component bounding box
- Bounds fields are omitted if world bounds are invalid

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/level/info"
```

---

## GET /level/outliner

Get world outliner actor hierarchy or flat list.

**Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| flat | string | No | false | Set to "true" for flat list, otherwise returns hierarchical tree |

**Response (hierarchical):**
```json
{
  "success": true,
  "actors": [
    {
      "label": "PlayerStart",
      "class": "PlayerStart",
      "location": {"x": 0.0, "y": 0.0, "z": 0.0},
      "children": [
        {
          "label": "ChildActor",
          "class": "StaticMeshActor",
          "location": {"x": 100.0, "y": 0.0, "z": 50.0}
        }
      ]
    },
    {
      "label": "DirectionalLight",
      "class": "DirectionalLight",
      "location": {"x": 0.0, "y": 0.0, "z": 300.0}
    }
  ],
  "count": 2
}
```

**Response (flat):**
```json
{
  "success": true,
  "actors": [
    {
      "label": "PlayerStart",
      "class": "PlayerStart",
      "location": {"x": 0.0, "y": 0.0, "z": 0.0}
    },
    {
      "label": "ChildActor",
      "class": "StaticMeshActor",
      "location": {"x": 100.0, "y": 0.0, "z": 50.0}
    },
    {
      "label": "DirectionalLight",
      "class": "DirectionalLight",
      "location": {"x": 0.0, "y": 0.0, "z": 300.0}
    }
  ],
  "count": 3
}
```

**Status Codes:**
- 200 - Success
- 400 - No level loaded

**Error Codes:**
- `NO_LEVEL_LOADED` - No level currently open in editor

**Notes:**
- Default behavior returns hierarchical tree structure
- Hierarchical mode only includes root actors (actors without parent attachment)
- Child actors are nested recursively under their parents in the `children` array
- Flat mode returns all actors in a single-level list
- Actor location is in world space (centimeters)
- Actor class is the native class name (e.g., "PlayerStart", "StaticMeshActor")
- Use `flat=true` query parameter for flat list

**curl (hierarchical):**
```bash
curl -s "http://localhost:$PORT/api/v1/level/outliner"
```

**curl (flat):**
```bash
curl -s "http://localhost:$PORT/api/v1/level/outliner?flat=true"
```

---

## POST /level/load

Load a level by path.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | Yes | - | Map path to load (e.g., /Game/Maps/MyLevel) |

**Request:**
```json
{
  "path": "/Game/Maps/MyLevel"
}
```

**Response:**
```json
{
  "success": true,
  "loaded_level": "/Game/Maps/MyLevel"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters
- 404 - Level not found

**Error Codes:**
- `INVALID_PARAMS` - Missing required parameter
- `LEVEL_NOT_FOUND` - Failed to load level at specified path

**Notes:**
- Uses FEditorFileUtils::LoadMap for loading
- Path should be in game content format (e.g., /Game/Maps/LevelName)
- Loading will close any currently open level
- Any unsaved changes to the current level will trigger a save dialog

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/level/load" \
  -H "Content-Type: application/json" \
  -d '{"path": "/Game/Maps/MyLevel"}'
```

---

## Actor Data Format

Actor information returned by `/level/outliner` uses this format:

```json
{
  "label": "PlayerStart",
  "class": "PlayerStart",
  "location": {"x": 0.0, "y": 0.0, "z": 0.0},
  "children": []
}
```

| Field | Description | Availability |
|-------|-------------|--------------|
| label | Actor label shown in outliner | Always |
| class | Native class name of the actor | Always |
| location | World space location (cm) | Always |
| children | Nested child actors | Hierarchical mode only |
