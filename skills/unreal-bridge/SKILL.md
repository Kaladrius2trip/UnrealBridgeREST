---
name: unreal-bridge
description: Use when automating Unreal Engine editor tasks via REST API, manipulating actors/materials/blueprints, working with the UnrealPythonREST plugin, or executing Python in UE.
---

<objective>
Tiered workflows for Unreal Engine editor automation. Prevents errors by requiring 95% confidence before suggesting APIs. Covers actors, materials, blueprints, assets, and editor automation.
</objective>

<essential_principles>
**Tier Priority:** REST endpoints > Python execution > Suggest new endpoint

**95% Confidence Rule:** Never suggest an API without verifying it exists in the Python stub file or REST schema.

**Asset Safety:** Never use `os.rename()`, `shutil.move()` on .uasset files. Use `unreal.EditorAssetLibrary` instead.
</essential_principles>

<intake>
Use **AskUserQuestion** to determine what the user needs:

```
AskUserQuestion({
  questions: [{
    question: "What do you want to do in Unreal Engine?",
    header: "Task",
    multiSelect: false,
    options: [
      { label: "Actors", description: "Spawn, transform, modify properties, destroy" },
      { label: "Materials", description: "Create, edit graph, set parameters" },
      { label: "Blueprints/Assets", description: "Nodes, connections, list, search, export" },
      { label: "Setup/Python", description: "Install plugin, execute custom Python" }
    ]
  }]
})
```

**Wait for user selection before proceeding.**
</intake>

<routing>
| Intent | Reference |
|--------|-----------|
| Actors (spawn, transform, properties) | references/endpoints/actors.md |
| Materials (create, edit graph, parameters) | references/endpoints/materials.md |
| Blueprints (nodes, connections, compile) | references/endpoints/blueprints.md |
| Assets (list, search, info, refs) | references/endpoints/assets.md |
| Python execution | references/python_fallback.md |
| Plugin setup | references/setup.md |
| Batch operations | references/batch_patterns.md |
| Choosing REST vs Python | references/decision_trees.md |
| Editor state/camera | references/endpoints/editor.md |
| Level info/outliner | references/endpoints/level.md |

**After reading reference, follow it exactly.**
</routing>

<quick_start>
**Check if REST API available:**
```bash
cat {ProjectDir}/Saved/UnrealPythonREST.json  # Get port
curl -s "http://localhost:$PORT/api/v1/health"
```

**Example - Spawn actor:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/actors/spawn" \
  -H "Content-Type: application/json" \
  -d '{"class": "/Script/Engine.StaticMeshActor", "label": "Cube1"}'
```

**Example - Create material:**
```bash
curl -s -X POST "http://localhost:$PORT/api/v1/materials/create" \
  -H "Content-Type: application/json" \
  -d '{"name": "M_MyMaterial", "path": "/Game/Materials"}'
```

**Discover endpoints:**
```bash
curl -s "http://localhost:$PORT/api/v1/schema"
curl -s "http://localhost:$PORT/api/v1/schema?handler=materials"
```
</quick_start>

<api_discovery>
**REST Schema:** `GET /schema`, `GET /schema?handler=<name>`, `GET /schema?endpoint=<path>`

**Python Stub:** `{ProjectDir}/Intermediate/PythonStub/unreal.py` - Search here FIRST before suggesting Python APIs. If not in stub, it's NOT available.
</api_discovery>

<critical_constraints>
**NEVER:**
- Use `-ExecutePythonScript` flag (disabled in commandlets)
- Use Python file operations on .uasset/.umap files
- Assume APIs exist without verification
- Expect `unreal.log()` in stdout (goes to log file)

**ALWAYS:**
- Prefer REST endpoints over Python
- Verify API in stub before suggesting
- Use `unreal.EditorAssetLibrary` for asset operations
</critical_constraints>

<reference_index>
**Endpoints (8 handlers, 50+ endpoints):**

| Handler | File | Key Operations |
|---------|------|----------------|
| actors | endpoints/actors.md | spawn, transform, properties, destroy |
| materials | endpoints/materials.md | create, graph edit, connect, export/import XML |
| blueprints | endpoints/blueprints.md | nodes, connections, compile, variables |
| assets | endpoints/assets.md | list, search, info, refs, export |
| editor | endpoints/editor.md | camera, selection, live coding |
| level | endpoints/level.md | info, outliner |
| python | endpoints/python.md | execute, async, status |
| infrastructure | endpoints/infrastructure.md | health, schema, batch |

**Guides:**

| Guide | Purpose |
|-------|---------|
| api_overview.md | Handler index, discovery flow |
| decision_trees.md | REST vs Python decision |
| batch_patterns.md | Multi-operation examples |
| python_fallback.md | Python patterns, threading |
| setup.md | Plugin installation |
| execution_methods.md | REST vs Commandlet |
| best_practices.md | Verified UE Python patterns |
| common_apis.md | Tested API reference |
</reference_index>

<success_criteria>
- REST endpoint used when available
- Python fallback only when no endpoint exists
- All API suggestions verified (95% confidence)
- Asset operations use Unreal APIs (never direct file ops)
- User directed to correct reference for their task
</success_criteria>
