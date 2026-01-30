# Editor Endpoints

Base path: `/api/v1/editor`

Editor utility endpoints for project info, camera control, selection management, screenshot capture, console commands, and asset editors.

---

## GET /editor/project

Get project metadata including name, paths, and engine version.

**Parameters:** None

**Response:**
```json
{
  "success": true,
  "project": {
    "name": "MyProject",
    "path": "C:/Projects/MyProject/",
    "engine_version": "5.3.2-12345678",
    "content_dir": "C:/Projects/MyProject/Content/"
  }
}
```

**Status Codes:**
- 200 - Success

**Notes:**
- Always returns current project information
- Paths are platform-specific (Windows uses backslashes)
- Engine version includes full build number

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/editor/project"
```

---

## POST /editor/screenshot

Capture the active viewport to a PNG file.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| path | string | No | Auto-generated | Output file path (e.g., "C:/Screenshots/shot.png") |

**Request:**
```json
{
  "path": "C:/Screenshots/viewport.png"
}
```

**Response:**
```json
{
  "success": true,
  "path": "C:/Screenshots/viewport.png",
  "message": "Screenshot requested - file will be saved asynchronously"
}
```

**Status Codes:**
- 200 - Success
- 400 - No active viewport

**Error Codes:**
- `NO_VIEWPORT` - No active editor viewport available

**Notes:**
- Screenshot is saved asynchronously (file may not exist immediately)
- If path is omitted, auto-generates to `ProjectSaved/Screenshots/Screenshot_TIMESTAMP.png`
- Uses high-resolution screenshot system
- Requires an active viewport

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/screenshot" \
  -H "Content-Type: application/json" \
  -d '{"path": "C:/Screenshots/viewport.png"}'
```

---

## POST /editor/camera

Move or animate the viewport camera to a new location/rotation, orbit around a point, or focus on an actor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| location | object | No | Current | Target camera location `{x, y, z}` |
| rotation | object | No | Current | Target camera rotation `{pitch, yaw, roll}` |
| duration | number | No | 0 | Animation duration in seconds (0 = instant) |
| focus_actor | string | No | - | Actor label to focus on (overrides location/rotation) |
| orbit | object | No | - | Orbit mode configuration (see below) |

**Orbit Mode Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| target | object | No | {0,0,0} | Point to orbit around `{x, y, z}` |
| angle | object | No | Current | Direction from target to camera `{pitch, yaw, roll}` |
| distance | number | No | 1000 | Distance from target to camera |

**Request (Instant Move):**
```json
{
  "location": {"x": 1000, "y": 2000, "z": 500},
  "rotation": {"pitch": -15, "yaw": 90, "roll": 0}
}
```

**Request (Animated Move):**
```json
{
  "location": {"x": 1000, "y": 2000, "z": 500},
  "rotation": {"pitch": -15, "yaw": 90, "roll": 0},
  "duration": 2.0
}
```

**Request (Orbit Mode):**
```json
{
  "orbit": {
    "target": {"x": 0, "y": 0, "z": 100},
    "angle": {"pitch": -30, "yaw": 45, "roll": 0},
    "distance": 1500
  },
  "duration": 3.0
}
```

**Request (Focus on Actor):**
```json
{
  "focus_actor": "SM_Cube_1",
  "duration": 1.5
}
```

**Response (Instant):**
```json
{
  "success": true,
  "animating": false,
  "location": {"x": 1000, "y": 2000, "z": 500},
  "rotation": {"pitch": -15, "yaw": 90, "roll": 0}
}
```

**Response (Animated):**
```json
{
  "success": true,
  "animating": true,
  "orbit_mode": false,
  "duration": 2.0,
  "start_location": {"x": 0, "y": 0, "z": 300},
  "end_location": {"x": 1000, "y": 2000, "z": 500},
  "start_rotation": {"pitch": 0, "yaw": 0, "roll": 0},
  "end_rotation": {"pitch": -15, "yaw": 90, "roll": 0}
}
```

**Response (Orbit Animated):**
```json
{
  "success": true,
  "animating": true,
  "orbit_mode": true,
  "duration": 3.0,
  "start_location": {"x": 0, "y": 0, "z": 300},
  "end_location": {"x": 1060.66, "y": 1060.66, "z": 850},
  "start_rotation": {"pitch": 0, "yaw": 0, "roll": 0},
  "end_rotation": {"pitch": -30, "yaw": 45, "roll": 0},
  "orbit_target": {"x": 0, "y": 0, "z": 100},
  "start_angle": {"pitch": 0, "yaw": 0, "roll": 0},
  "end_angle": {"pitch": -30, "yaw": 45, "roll": 0}
}
```

**Response (Actor Not Found):**
```json
{
  "success": true,
  "animating": true,
  "duration": 1.5,
  "warning": "Focus actor 'InvalidActor' not found",
  ...
}
```

**Status Codes:**
- 200 - Success
- 400 - No active viewport or invalid JSON

**Error Codes:**
- `NO_VIEWPORT` - No active editor viewport available

**Notes:**
- Instant move: `duration` omitted or 0
- Animated move: Uses smooth ease-in-out interpolation (3t² - 2t³)
- Rotation uses SLERP for smooth interpolation
- Orbit mode: Camera SLERP around target point at specified angle/distance
- Focus actor: Automatically frames actor in viewport using bounding box
- Focus with animation: Offset -500 X, +300 Z from actor center
- Starting a new animation cancels any running animation
- If `focus_actor` not found, shows warning but continues with other parameters

**curl:**
```bash
# Instant move
curl -s -X POST "http://localhost:$PORT/api/v1/editor/camera" \
  -H "Content-Type: application/json" \
  -d '{"location": {"x": 1000, "y": 2000, "z": 500}}'

# Animated move
curl -s -X POST "http://localhost:$PORT/api/v1/editor/camera" \
  -H "Content-Type: application/json" \
  -d '{"location": {"x": 1000, "y": 2000, "z": 500}, "duration": 2.0}'

# Orbit around point
curl -s -X POST "http://localhost:$PORT/api/v1/editor/camera" \
  -H "Content-Type: application/json" \
  -d '{"orbit": {"target": {"x": 0, "y": 0, "z": 100}, "angle": {"pitch": -30, "yaw": 45, "roll": 0}, "distance": 1500}, "duration": 3.0}'

# Focus on actor
curl -s -X POST "http://localhost:$PORT/api/v1/editor/camera" \
  -H "Content-Type: application/json" \
  -d '{"focus_actor": "SM_Cube_1", "duration": 1.5}'
```

---

## GET /editor/camera/status

Get current camera animation status and position.

**Parameters:** None

**Response (Animating):**
```json
{
  "success": true,
  "animating": true,
  "progress": 0.65,
  "elapsed": 1.3,
  "duration": 2.0,
  "location": {"x": 650, "y": 1300, "z": 425},
  "rotation": {"pitch": -9.75, "yaw": 58.5, "roll": 0}
}
```

**Response (Not Animating):**
```json
{
  "success": true,
  "animating": false,
  "location": {"x": 1000, "y": 2000, "z": 500},
  "rotation": {"pitch": -15, "yaw": 90, "roll": 0}
}
```

**Status Codes:**
- 200 - Success

**Notes:**
- `progress` - Animation progress 0.0 to 1.0 (only when animating)
- `elapsed` - Time elapsed in seconds (only when animating)
- `duration` - Total animation duration (only when animating)
- `location` and `rotation` show current camera position
- Use to poll animation status or get current camera position

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/editor/camera/status"
```

---

## GET /editor/selection

Get currently selected actors in the editor.

**Parameters:** None

**Response:**
```json
{
  "success": true,
  "count": 2,
  "selected": [
    {
      "label": "SM_Cube_1",
      "class": "StaticMeshActor",
      "location": {"x": 100, "y": 200, "z": 50}
    },
    {
      "label": "SM_Sphere_2",
      "class": "StaticMeshActor",
      "location": {"x": 500, "y": 600, "z": 75}
    }
  ]
}
```

**Status Codes:**
- 200 - Success
- 400 - Editor not available

**Error Codes:**
- `NO_EDITOR` - Editor not available

**Notes:**
- Returns empty array if nothing is selected
- Each selected actor includes label, class, and location
- Only returns AActor-derived objects (not components)

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/editor/selection"
```

---

## POST /editor/selection

Set selected actors in the editor by their labels.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| labels | array | Yes | - | Array of actor labels to select |

**Request:**
```json
{
  "labels": ["SM_Cube_1", "SM_Sphere_2", "SM_Cylinder_3"]
}
```

**Response (All Found):**
```json
{
  "success": true,
  "selected_count": 3,
  "selected": ["SM_Cube_1", "SM_Sphere_2", "SM_Cylinder_3"]
}
```

**Response (Some Not Found):**
```json
{
  "success": false,
  "selected_count": 2,
  "selected": ["SM_Cube_1", "SM_Sphere_2"],
  "not_found": ["InvalidActor"]
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters or editor not available

**Error Codes:**
- `NO_EDITOR` - Editor not available
- Missing `labels` parameter returns BadRequest

**Notes:**
- Clears current selection before applying new selection
- `success` is false if any actors were not found
- Actors must exist in the current level
- Selection change notification is triggered automatically

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/selection" \
  -H "Content-Type: application/json" \
  -d '{"labels": ["SM_Cube_1", "SM_Sphere_2"]}'
```

---

## POST /editor/console

Execute a console command in the Unreal Editor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| command | string | Yes | - | Console command to execute (e.g., "stat fps") |

**Request:**
```json
{
  "command": "stat fps"
}
```

**Response:**
```json
{
  "success": true,
  "command": "stat fps",
  "message": "Command executed"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing or invalid parameters

**Error Codes:**
- Missing `command` parameter returns BadRequest

**Notes:**
- Executes command via GEngine->Exec()
- No output capture - check Unreal Editor Output Log for results
- Commands execute in editor world context if available
- Use for toggling debug displays, running commands, etc.
- Examples: "stat fps", "stat unit", "r.SetRes 1920x1080", "ke * reload"

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/console" \
  -H "Content-Type: application/json" \
  -d '{"command": "stat fps"}'
```

---

## POST /editor/replace_mesh

Replace the static mesh on an actor.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Actor label to modify |
| mesh_path | string | Yes | - | Full path to new static mesh asset |

**Request:**
```json
{
  "label": "SM_Cube_1",
  "mesh_path": "/Game/Meshes/SM_Sphere.SM_Sphere"
}
```

**Response:**
```json
{
  "success": true,
  "label": "SM_Cube_1",
  "old_mesh": "/Game/Meshes/SM_Cube.SM_Cube",
  "new_mesh": "/Game/Meshes/SM_Sphere.SM_Sphere"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or actor has no mesh component
- 404 - Actor or mesh asset not found

**Error Codes:**
- Missing required parameters returns BadRequest
- `ACTOR_NOT_FOUND` - Actor with specified label not found
- `ASSET_NOT_FOUND` - Static mesh asset not found at path
- `NO_MESH_COMPONENT` - Actor does not have a StaticMeshComponent

**Notes:**
- Actor must have a UStaticMeshComponent
- Mesh asset must exist and be loaded
- Transform and other properties are preserved
- `old_mesh` returns "None" if actor had no mesh assigned

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/replace_mesh" \
  -H "Content-Type: application/json" \
  -d '{"label": "SM_Cube_1", "mesh_path": "/Game/Meshes/SM_Sphere.SM_Sphere"}'
```

---

## POST /editor/replace_with_bp

Replace an actor with a blueprint instance, preserving the transform.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Actor label to replace |
| blueprint_path | string | Yes | - | Full path to blueprint class asset |

**Request:**
```json
{
  "label": "SM_Cube_1",
  "blueprint_path": "/Game/Blueprints/BP_Interactive.BP_Interactive_C"
}
```

**Response:**
```json
{
  "success": true,
  "original_label": "SM_Cube_1",
  "original_class": "StaticMeshActor",
  "new_label": "BP_Interactive_1",
  "new_class": "BP_Interactive_C",
  "transform": {
    "location": {"x": 100, "y": 200, "z": 50},
    "rotation": {"pitch": 0, "yaw": 90, "roll": 0},
    "scale": {"x": 1, "y": 1, "z": 1}
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters or spawn failed
- 404 - Actor not found

**Error Codes:**
- Missing required parameters returns BadRequest
- `ACTOR_NOT_FOUND` - Original actor not found
- `SPAWN_FAILED` - Failed to spawn blueprint instance (see error message for details)

**Notes:**
- Original actor is destroyed after successful spawn
- Transform (location, rotation, scale) is preserved
- New actor may get different label (Unreal auto-numbering)
- Blueprint path must include `_C` suffix for the class
- Original actor's properties are NOT copied to new instance

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/replace_with_bp" \
  -H "Content-Type: application/json" \
  -d '{"label": "SM_Cube_1", "blueprint_path": "/Game/Blueprints/BP_Interactive.BP_Interactive_C"}'
```

---

## POST /editor/live_coding

Trigger a Live Coding compile to hot-reload C++ code changes.

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| wait | boolean | No | false | Wait for compilation to complete before returning |

**Request:**
```json
{
  "wait": true
}
```

**Response (Success):**
```json
{
  "success": true,
  "result": "success",
  "message": "Compilation completed successfully",
  "waited": true
}
```

**Response (No Changes):**
```json
{
  "success": true,
  "result": "no_changes",
  "message": "No code changes detected",
  "waited": true
}
```

**Response (In Progress):**
```json
{
  "success": true,
  "result": "in_progress",
  "message": "Compilation started (use wait=true to wait for completion)",
  "waited": false
}
```

**Response (Failure):**
```json
{
  "success": false,
  "result": "failure",
  "message": "Compilation FAILED - check Output Log in Unreal Editor for error details",
  "waited": true,
  "error_location": "Unreal Editor Output Log (Window > Developer Tools > Output Log)"
}
```

**Response (Already Compiling):**
```json
{
  "success": false,
  "status": "already_compiling",
  "message": "A Live Coding compile is already in progress"
}
```

**Status Codes:**
- 200 - Success or compile in progress/already compiling
- 400 - Live Coding not available or disabled

**Error Codes:**
- `LIVE_CODING_NOT_AVAILABLE` - Module not available on this platform
- `LIVE_CODING_DISABLED` - Live Coding not enabled and cannot be enabled

**Result Values:**
- `success` - Compilation completed successfully
- `no_changes` - No code changes detected
- `in_progress` - Compilation started asynchronously
- `compile_still_active` - Previous compilation still active
- `not_started` - Compilation failed to start
- `failure` - Compilation failed with errors
- `cancelled` - Compilation was cancelled

**Notes:**
- Automatically enables Live Coding if disabled but available
- `wait=false` (default): Returns immediately with `in_progress` status
- `wait=true`: Blocks until compilation completes
- Check `/editor/live_coding` (GET) for status while compiling
- Compilation errors appear in Unreal Editor Output Log, not in response
- Only available on platforms that support Live Coding (Windows)

**curl:**
```bash
# Start compilation (async)
curl -s -X POST "http://localhost:$PORT/api/v1/editor/live_coding" \
  -H "Content-Type: application/json" \
  -d '{}'

# Compile and wait
curl -s -X POST "http://localhost:$PORT/api/v1/editor/live_coding" \
  -H "Content-Type: application/json" \
  -d '{"wait": true}'
```

---

## GET /editor/live_coding

Get Live Coding status and availability.

**Parameters:** None

**Response (Available):**
```json
{
  "success": true,
  "available": true,
  "enabled_by_default": true,
  "enabled_for_session": true,
  "has_started": true,
  "is_compiling": false,
  "can_enable": true
}
```

**Response (Not Available):**
```json
{
  "success": true,
  "available": false,
  "message": "Live Coding module not available on this platform"
}
```

**Status Codes:**
- 200 - Success

**Notes:**
- `available` - Whether Live Coding module is available on platform
- `enabled_by_default` - Whether enabled in project settings
- `enabled_for_session` - Whether currently enabled for this session
- `has_started` - Whether Live Coding has been initialized
- `is_compiling` - Whether a compilation is currently in progress
- `can_enable` - Whether it can be enabled for this session
- Live Coding is typically only available on Windows

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/editor/live_coding"
```

---

## POST /editor/open

Open an asset in its appropriate editor (Material Editor, Blueprint Editor, etc.).

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| asset_path | string | Yes | - | Full path to asset (e.g., "/Game/Materials/M_Base.M_Base") |

**Request:**
```json
{
  "asset_path": "/Game/Materials/M_Base.M_Base"
}
```

**Response (Success):**
```json
{
  "success": true,
  "asset_path": "/Game/Materials/M_Base.M_Base",
  "asset_class": "Material",
  "message": "Asset editor opened"
}
```

**Response (Failed to Open):**
```json
{
  "success": false,
  "asset_path": "/Game/Materials/M_Base.M_Base",
  "asset_class": "Material",
  "message": "Failed to open asset editor"
}
```

**Status Codes:**
- 200 - Success or failed to open
- 400 - Missing or invalid parameters
- 404 - Asset not found
- 500 - AssetEditorSubsystem not available

**Error Codes:**
- Missing `asset_path` parameter returns BadRequest
- `ASSET_NOT_FOUND` - Asset does not exist at path
- `SUBSYSTEM_ERROR` - AssetEditorSubsystem not available

**Notes:**
- Automatically selects appropriate editor based on asset type
- Material → Material Editor, Blueprint → Blueprint Editor, etc.
- If asset is already open, brings it to front
- Asset must exist and be loadable
- Editor window opens in Unreal Editor UI

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/editor/open" \
  -H "Content-Type: application/json" \
  -d '{"asset_path": "/Game/Materials/M_Base.M_Base"}'
```

---

## POST /editor/close

Close asset editor(s).

**Body Parameters:**

| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| asset_path | string | No* | - | Full path to asset to close |
| close_all | boolean | No* | false | Close all asset editors |

*One of `asset_path` or `close_all` must be provided.

**Request (Close Specific Asset):**
```json
{
  "asset_path": "/Game/Materials/M_Base.M_Base"
}
```

**Request (Close All):**
```json
{
  "close_all": true
}
```

**Response (Specific Asset):**
```json
{
  "success": true,
  "asset_path": "/Game/Materials/M_Base.M_Base",
  "message": "Asset editor closed"
}
```

**Response (Close All):**
```json
{
  "success": true,
  "message": "All asset editors closed"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing parameters
- 404 - Asset not found
- 500 - AssetEditorSubsystem not available

**Error Codes:**
- Missing required parameters returns BadRequest
- `ASSET_NOT_FOUND` - Asset does not exist at path (when closing specific asset)
- `SUBSYSTEM_ERROR` - AssetEditorSubsystem not available

**Notes:**
- `close_all` takes precedence over `asset_path` if both provided
- Closes all editor windows for the specified asset
- Does not save changes - user will be prompted if unsaved changes exist
- Safe to call even if asset is not currently open

**curl:**
```bash
# Close specific asset
curl -s -X POST "http://localhost:$PORT/api/v1/editor/close" \
  -H "Content-Type: application/json" \
  -d '{"asset_path": "/Game/Materials/M_Base.M_Base"}'

# Close all asset editors
curl -s -X POST "http://localhost:$PORT/api/v1/editor/close" \
  -H "Content-Type: application/json" \
  -d '{"close_all": true}'
```

---

## Common Data Formats

### Vector Format
```json
{
  "x": 100.0,
  "y": 200.0,
  "z": 50.0
}
```

### Rotator Format
```json
{
  "pitch": -15.0,
  "yaw": 90.0,
  "roll": 0.0
}
```

### Transform Format
```json
{
  "location": {"x": 100, "y": 200, "z": 50},
  "rotation": {"pitch": 0, "yaw": 90, "roll": 0},
  "scale": {"x": 1, "y": 1, "z": 1}
}
```

All coordinates and rotations use Unreal Engine's coordinate system:
- Distances in centimeters
- Rotations in degrees
- X = Forward, Y = Right, Z = Up
