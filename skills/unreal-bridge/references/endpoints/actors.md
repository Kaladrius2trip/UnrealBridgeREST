# Actors Endpoints

Base path: `/api/v1/actors`

## GET /actors/list

List all actors in the currently open level.

**Parameters:**
None (query parameters not implemented in current version)

**Response:**
```json
{
  "success": true,
  "actors": [
    {
      "label": "Cube_1",
      "class": "StaticMeshActor",
      "location": {"x": 0.0, "y": 0.0, "z": 0.0}
    }
  ],
  "count": 1
}
```

**Status Codes:**
- 200 - Success
- 400 - No level currently open

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/actors/list"
```

**Python:**
```python
response = requests.get(f"{base_url}/actors/list")
actors = response.json()["actors"]
```

---

## GET /actors/details

Get detailed information about a specific actor by label.

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Actor label to look up |

**Response:**
```json
{
  "success": true,
  "actor": {
    "label": "Cube_1",
    "class": "StaticMeshActor",
    "location": {"x": 0.0, "y": 0.0, "z": 0.0},
    "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
    "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
    "transform": { ... },
    "path": "/Game/Level.Level:PersistentLevel.Cube_1"
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing label parameter
- 404 - Actor not found (includes suggestions for similar actor labels)

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/actors/details?label=Cube_1"
```

**Python:**
```python
response = requests.get(f"{base_url}/actors/details", params={"label": "Cube_1"})
actor_details = response.json()["actor"]
```

---

## POST /actors/spawn

Spawn a new actor at a specified transform.

**Request Body:**
```json
{
  "class_path": "/Script/Engine.StaticMeshActor",
  "location": {"x": 0.0, "y": 0.0, "z": 0.0},
  "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
  "scale": {"x": 1.0, "y": 1.0, "z": 1.0}
}
```

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| class_path | string | Yes | - | Full class path of the actor to spawn |
| location | object | No | {0,0,0} | World location (x, y, z) |
| rotation | object | No | {0,0,0} | Rotation (pitch, yaw, roll) |
| scale | object | No | {1,1,1} | 3D scale (x, y, z) |

**Response:**
```json
{
  "success": true,
  "actor_label": "StaticMeshActor_2",
  "actor_path": "/Game/Level.Level:PersistentLevel.StaticMeshActor_2",
  "location": {"x": 0.0, "y": 0.0, "z": 0.0}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing class_path or spawn failed

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/spawn" \
  -H "Content-Type: application/json" \
  -d '{"class_path": "/Script/Engine.StaticMeshActor", "location": {"x": 100, "y": 0, "z": 50}}'
```

**Python:**
```python
response = requests.post(f"{base_url}/actors/spawn", json={
    "class_path": "/Script/Engine.StaticMeshActor",
    "location": {"x": 100, "y": 0, "z": 50}
})
actor_label = response.json()["actor_label"]
```

---

## POST /actors/spawn_raycast

Spawn an actor by raycasting downward to find a surface. Useful for placing objects on the ground or other surfaces.

**Request Body:**
```json
{
  "class_path": "/Script/Engine.StaticMeshActor",
  "location": {"x": 0.0, "y": 0.0, "z": 1000.0},
  "rotation": {"pitch": 0.0, "yaw": 0.0, "roll": 0.0},
  "scale": {"x": 1.0, "y": 1.0, "z": 1.0},
  "max_distance": 100000
}
```

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| class_path | string | Yes | - | Full class path of the actor to spawn |
| location | object | No | {0,0,0} | Starting location for raycast |
| rotation | object | No | {0,0,0} | Rotation (pitch, yaw, roll) |
| scale | object | No | {1,1,1} | 3D scale (x, y, z) |
| max_distance | number | No | 100000 | Maximum raycast distance |

**Response:**
```json
{
  "success": true,
  "actor_label": "StaticMeshActor_3",
  "hit_location": {"x": 0.0, "y": 0.0, "z": -50.0},
  "hit_normal": {"x": 0.0, "y": 0.0, "z": 1.0}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing class_path, no surface found, or spawn failed

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/spawn_raycast" \
  -H "Content-Type: application/json" \
  -d '{"class_path": "/Script/Engine.StaticMeshActor", "location": {"x": 0, "y": 0, "z": 1000}}'
```

**Python:**
```python
response = requests.post(f"{base_url}/actors/spawn_raycast", json={
    "class_path": "/Script/Engine.StaticMeshActor",
    "location": {"x": 0, "y": 0, "z": 1000},
    "max_distance": 50000
})
hit_location = response.json()["hit_location"]
```

---

## POST /actors/duplicate

Duplicate an existing actor with an optional offset.

**Request Body:**
```json
{
  "label": "Cube_1",
  "offset": {"x": 100.0, "y": 0.0, "z": 0.0}
}
```

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Label of the actor to duplicate |
| offset | object | No | {100,0,0} | Position offset from source (x, y, z) |

**Response:**
```json
{
  "success": true,
  "source_label": "Cube_1",
  "new_label": "Cube_2",
  "location": {"x": 100.0, "y": 0.0, "z": 0.0}
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing label parameter
- 404 - Source actor not found (includes suggestions)
- 500 - Duplication failed

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/duplicate" \
  -H "Content-Type: application/json" \
  -d '{"label": "Cube_1", "offset": {"x": 200, "y": 0, "z": 0}}'
```

**Python:**
```python
response = requests.post(f"{base_url}/actors/duplicate", json={
    "label": "Cube_1",
    "offset": {"x": 200, "y": 0, "z": 0}
})
new_label = response.json()["new_label"]
```

---

## POST /actors/transform

Update the transform (location, rotation, and/or scale) of an existing actor.

**Request Body:**
```json
{
  "label": "Cube_1",
  "location": {"x": 100.0, "y": 50.0, "z": 25.0},
  "rotation": {"pitch": 0.0, "yaw": 45.0, "roll": 0.0},
  "scale": {"x": 2.0, "y": 2.0, "z": 2.0}
}
```

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Label of the actor to transform |
| location | object | No | - | New world location (x, y, z) |
| rotation | object | No | - | New rotation (pitch, yaw, roll) |
| scale | object | No | - | New 3D scale (x, y, z) |

**Response:**
```json
{
  "success": true,
  "label": "Cube_1",
  "modified": true,
  "transform": {
    "location": {"x": 100.0, "y": 50.0, "z": 25.0},
    "rotation": {"pitch": 0.0, "yaw": 45.0, "roll": 0.0},
    "scale": {"x": 2.0, "y": 2.0, "z": 2.0}
  }
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing label parameter
- 404 - Actor not found (includes suggestions)

**Notes:**
- You can specify any combination of location, rotation, and scale
- Omitted transform components remain unchanged
- The `modified` field indicates if any changes were applied

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/transform" \
  -H "Content-Type: application/json" \
  -d '{"label": "Cube_1", "location": {"x": 100, "y": 50, "z": 25}}'
```

**Python:**
```python
response = requests.post(f"{base_url}/actors/transform", json={
    "label": "Cube_1",
    "location": {"x": 100, "y": 50, "z": 25},
    "rotation": {"pitch": 0, "yaw": 45, "roll": 0}
})
```

---

## POST /actors/delete

Delete an actor from the level.

**Request Body:**
```json
{
  "label": "Cube_1"
}
```

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| label | string | Yes | - | Label of the actor to delete |

**Response:**
```json
{
  "success": true,
  "deleted_label": "Cube_1",
  "deleted_class": "StaticMeshActor"
}
```

**Status Codes:**
- 200 - Success
- 400 - Missing label parameter
- 404 - Actor not found (includes suggestions)

**curl:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/delete" \
  -H "Content-Type: application/json" \
  -d '{"label": "Cube_1"}'
```

**Python:**
```python
response = requests.post(f"{base_url}/actors/delete", json={
    "label": "Cube_1"
})
```

---

## GET /actors/in_view

Get all actors within a specified distance from the active viewport camera.

**Parameters:**
| Name | Type | Required | Default | Description |
|------|------|----------|---------|-------------|
| max_distance | number | No | 50000 | Maximum distance from camera (in cm) |

**Response:**
```json
{
  "success": true,
  "camera_location": {"x": 500.0, "y": 300.0, "z": 200.0},
  "actors": [
    {
      "label": "Cube_1",
      "class": "StaticMeshActor",
      "location": {"x": 100.0, "y": 0.0, "z": 0.0},
      "distance": 523.5
    }
  ],
  "count": 1
}
```

**Status Codes:**
- 200 - Success
- 400 - No level loaded or no active viewport

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/actors/in_view?max_distance=10000"
```

**Python:**
```python
response = requests.get(f"{base_url}/actors/in_view", params={"max_distance": 10000})
nearby_actors = response.json()["actors"]
```

---

## Common Error Responses

All endpoints may return the following error format:

```json
{
  "success": false,
  "error": "ERROR_CODE",
  "message": "Human-readable error message",
  "details": {
    "provided": "user_input",
    "hint": "Suggestion for fixing the issue",
    "similar": ["similar_value_1", "similar_value_2"]
  }
}
```

**Common Error Codes:**
- `NO_LEVEL_LOADED` - No level currently open in the editor
- `NO_VIEWPORT` - No active editor viewport
- `ACTOR_NOT_FOUND` - Specified actor label not found
- `SPAWN_FAILED` - Actor spawn operation failed
- `DUPLICATE_FAILED` - Actor duplication failed
- `NO_SURFACE_FOUND` - Raycast did not hit any surface

---

## Notes

### Coordinate System
- Unreal Engine uses a left-handed Z-up coordinate system
- Distances are in centimeters (100 units = 1 meter)
- Rotation angles are in degrees

### Actor Labels
- Actor labels are the display names shown in the World Outliner
- Labels are not guaranteed to be unique
- Use `/actors/details` to get the full actor path for precise identification

### Common Class Paths
- Static Mesh Actor: `/Script/Engine.StaticMeshActor`
- Point Light: `/Script/Engine.PointLight`
- Spot Light: `/Script/Engine.SpotLight`
- Camera Actor: `/Script/Engine.CameraActor`
- Empty Actor: `/Script/Engine.Actor`

### Transform Objects
All transform objects use this structure:

**Location (Vector):**
```json
{"x": 0.0, "y": 0.0, "z": 0.0}
```

**Rotation (Rotator):**
```json
{"pitch": 0.0, "yaw": 0.0, "roll": 0.0}
```

**Scale (Vector):**
```json
{"x": 1.0, "y": 1.0, "z": 1.0}
```
