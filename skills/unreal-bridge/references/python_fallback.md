# Python Fallback Patterns

This document covers using Python execution when no REST endpoint exists, including threading considerations.

## When to Use Python

Use `POST /python/execute` when:
- No REST endpoint exists for the operation
- Custom/rare operation not worth adding endpoint
- Complex logic requiring loops/conditionals
- Operations on custom asset types

## Standard Template

```python
import unreal
import json

# Perform operation
result = your_operation_here()

# Return structured data (captured in response)
print(json.dumps({
    "success": True,
    "data": result
}))
```

## Common Fallback Scenarios

| Scenario | Why No Endpoint | Python Approach |
|----------|-----------------|-----------------|
| Custom asset type queries | Too specific | `AssetRegistry.GetAssets()` with custom filter |
| Widget manipulation | Editor internals | `unreal.get_editor_subsystem()` |
| Animation operations | Large API surface | `AnimationLibrary` calls |
| Sequencer control | Complex state | `LevelSequenceEditorSubsystem` |
| Custom Blueprint logic | Infinite variations | Direct K2Node manipulation |

---

## Output Patterns

### Single Value
```python
print(json.dumps({"count": 42}))
```

### List of Items
```python
print(json.dumps({"items": [{"name": "A"}, {"name": "B"}]}))
```

### Error Handling
```python
try:
    result = risky_operation()
    print(json.dumps({"success": True, "result": result}))
except Exception as e:
    print(json.dumps({"success": False, "error": str(e)}))
```

---

## Performance Considerations

- Python execution has ~50-100ms overhead per call
- For loops over many items, do loop in Python (1 call) not multiple API calls
- Large data returns may hit response limits - paginate if needed

### Good: One Python Call with Loop
```python
import unreal
import json

actors = unreal.EditorLevelLibrary.get_all_level_actors()
result = []
for actor in actors[:100]:  # Limit results
    result.append({"name": actor.get_name(), "class": actor.get_class().get_name()})
print(json.dumps({"actors": result}))
```

### Bad: Multiple API Calls
```bash
# Don't do this - each call has overhead
for i in {1..100}; do
  curl "http://localhost:$PORT/api/v1/actors/details?label=Actor_$i"
done
```

---

## Threading Context

### Thread Safety

```
REST Request (HTTP Thread)
         ↓
Python executes (Worker Thread)
         ↓
UE API call - which thread?
         ↓
    ┌────────────────────┬────────────────────┐
    │ Game Thread Needed │    Any Thread OK   │
    ├────────────────────┼────────────────────┤
    │ Asset loading      │ Math operations    │
    │ Actor spawning     │ String processing  │
    │ Editor UI changes  │ File I/O           │
    │ BP compilation     │ AssetRegistry query│
    │ Material graph     │ Path operations    │
    └────────────────────┴────────────────────┘
```

### Operations Requiring Game Thread

| Category | Examples |
|----------|----------|
| Actor operations | Spawn, destroy, modify transforms |
| Asset modification | Save, rename, duplicate |
| Editor UI | Open editors, modify selections |
| Blueprint/Material graphs | Node creation, connections |
| Level operations | Load, stream, modify |

### Game Thread Execution Pattern

```python
import unreal

def do_on_game_thread():
    # This runs on game thread
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
        unreal.StaticMeshActor,
        unreal.Vector(0, 0, 0)
    )
    return actor.get_name()

# Queue for game thread execution
result = unreal.PythonBridge.execute_on_game_thread(do_on_game_thread)
```

---

## Combining REST + Python

Mix REST and Python in sequence:

1. `GET /assets/list` to find assets (fast, structured)
2. `POST /python/execute` for custom processing (flexible)

```bash
# First: Get list of materials via REST
MATERIALS=$(curl -s "http://localhost:$PORT/api/v1/assets/list?type=Material&path=/Game/Materials")

# Then: Custom processing via Python
curl -s -X POST "http://localhost:$PORT/api/v1/python/execute" \
  -H "Content-Type: application/json" \
  -d '{
    "code": "import unreal\nimport json\n# Custom logic here\nprint(json.dumps({\"processed\": True}))"
  }'
```

---

## Example: Custom Asset Filter

No endpoint for filtering by custom metadata:

```python
import unreal
import json

registry = unreal.AssetRegistryHelpers.get_asset_registry()
filter = unreal.ARFilter()
filter.class_names = ["StaticMesh"]
filter.package_paths = ["/Game/Props"]

assets = registry.get_assets(filter)
result = []

for asset in assets:
    # Custom filter: only meshes with "Chair" in name
    if "Chair" in str(asset.asset_name):
        result.append({
            "path": str(asset.package_name),
            "name": str(asset.asset_name)
        })

print(json.dumps({"chairs": result, "count": len(result)}))
```

---

## Example: Sequencer Control

No endpoint for sequencer operations:

```python
import unreal
import json

subsystem = unreal.get_editor_subsystem(unreal.LevelSequenceEditorSubsystem)
if subsystem:
    # Get current sequence
    sequence = subsystem.get_current_level_sequence()
    if sequence:
        print(json.dumps({
            "success": True,
            "sequence_name": sequence.get_name(),
            "duration": sequence.get_playback_end()
        }))
    else:
        print(json.dumps({"success": False, "error": "No sequence open"}))
else:
    print(json.dumps({"success": False, "error": "Subsystem not available"}))
```
