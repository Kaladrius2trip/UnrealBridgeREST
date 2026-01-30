# Batch Operation Patterns

This document provides examples of common batch operations using `POST /batch`.

## Batch Request Format

```json
{
  "requests": [
    {"method": "GET|POST|PUT|DELETE", "path": "/endpoint", "body": {}},
    ...
  ],
  "options": {
    "stop_on_error": true
  }
}
```

## Variable References

Use `$N` to reference results from previous requests:
- `$0` - Full result from first request
- `$0.node.id` - Extract nested field from result
- `$1.data.name` - Extract from second result

---

## Pattern 1: Multiple Actor Spawning

Spawn several actors in sequence:

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/actors/spawn", "body": {"class": "/Script/Engine.StaticMeshActor", "label": "Cube1", "location": {"x": 0, "y": 0, "z": 0}}},
      {"method": "POST", "path": "/actors/spawn", "body": {"class": "/Script/Engine.StaticMeshActor", "label": "Cube2", "location": {"x": 200, "y": 0, "z": 0}}},
      {"method": "POST", "path": "/actors/spawn", "body": {"class": "/Script/Engine.StaticMeshActor", "label": "Cube3", "location": {"x": 400, "y": 0, "z": 0}}}
    ]
  }'
```

---

## Pattern 2: Blueprint Node Chain

Create nodes and connect them using variable references:

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/blueprints/node/create", "body": {"node_type": "CallFunction", "function_name": "PrintString", "x": 0, "y": 0}},
      {"method": "POST", "path": "/blueprints/node/create", "body": {"node_type": "CallFunction", "function_name": "Delay", "x": 300, "y": 0}},
      {"method": "POST", "path": "/blueprints/connect", "body": {"source_node_id": "$0.node.id", "source_pin": "then", "target_node_id": "$1.node.id", "target_pin": "execute"}}
    ]
  }'
```

---

## Pattern 3: Material Expression Setup

Create material nodes and set properties:

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/materials/editor/node/create", "body": {"expression_class": "ScalarParameter", "param_name": "Roughness", "position": {"x": -400, "y": 0}}},
      {"method": "POST", "path": "/materials/editor/node/create", "body": {"expression_class": "Constant3Vector", "position": {"x": -400, "y": 100}}},
      {"method": "POST", "path": "/materials/editor/connect", "body": {"source_expression": "$0.expression_name", "target_property": "Roughness"}},
      {"method": "POST", "path": "/materials/editor/connect", "body": {"source_expression": "$1.expression_name", "target_property": "BaseColor"}}
    ]
  }'
```

---

## Pattern 4: Query Then Act

Get information first, then use it:

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "GET", "path": "/actors/details", "body": {"label": "MainCharacter"}},
      {"method": "POST", "path": "/actors/duplicate", "body": {"source_label": "MainCharacter", "new_label": "MainCharacter_Copy", "offset": {"x": 500, "y": 0, "z": 0}}}
    ]
  }'
```

---

## Pattern 5: Bulk Parameter Updates

Update multiple material instance parameters:

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/materials/param", "body": {"asset_path": "/Game/Materials/MI_Wall", "param_name": "Roughness", "value": 0.8}},
      {"method": "POST", "path": "/materials/param", "body": {"asset_path": "/Game/Materials/MI_Wall", "param_name": "Metallic", "value": 0.0}},
      {"method": "POST", "path": "/materials/param", "body": {"asset_path": "/Game/Materials/MI_Floor", "param_name": "Roughness", "value": 0.5}}
    ]
  }'
```

---

## Error Handling

### Stop on First Error (default)
```json
{"options": {"stop_on_error": true}}
```
Batch stops at first failure, remaining requests not executed.

### Continue on Error
```json
{"options": {"stop_on_error": false}}
```
All requests attempted, failures noted in results.

### Checking Results
```json
{
  "success": false,
  "results": [
    {"index": 0, "success": true, "data": {...}},
    {"index": 1, "success": false, "data": {"error": "ACTOR_NOT_FOUND"}}
  ],
  "completed": 2,
  "failed": 1
}
```

---

## When NOT to Use Batch

Use Python execution instead when:
- Complex conditional logic (if/else based on results)
- Loops with dynamic count
- Operations requiring local state
- Error recovery logic
