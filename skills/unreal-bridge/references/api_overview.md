# UnrealPythonREST API Overview

**Base URL:** `http://localhost:{port}/api/v1`
**Port Discovery:** `{ProjectDir}/Saved/UnrealPythonREST.json`

## Quick Start with curl

```bash
# Get port from config
PORT=$(cat /path/to/project/Saved/UnrealPythonREST.json | grep -o '"port":[0-9]*' | cut -d: -f2)

# Health check
curl -s "http://localhost:$PORT/api/v1/health"

# Get full schema
curl -s "http://localhost:$PORT/api/v1/schema"

# Get handler schema
curl -s "http://localhost:$PORT/api/v1/schema?handler=assets"

# Get endpoint schema
curl -s "http://localhost:$PORT/api/v1/schema?endpoint=/assets/list"
```

## Available Handlers

| Handler | File | Endpoints | Description |
|---------|------|-----------|-------------|
| infrastructure | [endpoints/infrastructure.md](endpoints/infrastructure.md) | 3 | Health, schema, batch |
| assets | [endpoints/assets.md](endpoints/assets.md) | 7 | Asset discovery and metadata |
| actors | [endpoints/actors.md](endpoints/actors.md) | 8 | Actor spawning and manipulation |
| blueprints | [endpoints/blueprints.md](endpoints/blueprints.md) | 9 | Blueprint graph editing |
| materials | [endpoints/materials.md](endpoints/materials.md) | 13 | Material and expression editing |
| level | [endpoints/level.md](endpoints/level.md) | 2 | Level info and outliner |
| editor | [endpoints/editor.md](endpoints/editor.md) | 5 | Editor state and camera |
| python | [endpoints/python.md](endpoints/python.md) | 4 | Python code execution |

## Discovery Flow

1. Read this overview for handler index
2. Check specific `endpoints/*.md` for detailed parameters
3. If endpoint not documented, query `/schema?endpoint=...`
4. If no endpoint exists, fall back to `/python/execute`

## Common Patterns

### Batch Operations

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/actors/spawn", "body": {"class": "/Script/Engine.StaticMeshActor", "label": "Cube1"}},
      {"method": "POST", "path": "/actors/spawn", "body": {"class": "/Script/Engine.StaticMeshActor", "label": "Cube2"}}
    ]
  }'
```

### Variable References in Batch

```bash
curl -s -X POST "http://localhost:$PORT/api/v1/batch" \
  -H "Content-Type: application/json" \
  -d '{
    "requests": [
      {"method": "POST", "path": "/blueprints/node/create", "body": {"node_type": "CallFunction", "function_name": "PrintString"}},
      {"method": "POST", "path": "/blueprints/node/create", "body": {"node_type": "CallFunction", "function_name": "Delay"}},
      {"method": "POST", "path": "/blueprints/connect", "body": {"source_node_id": "$0.node.id", "target_node_id": "$1.node.id", "source_pin": "then", "target_pin": "execute"}}
    ]
  }'
```
