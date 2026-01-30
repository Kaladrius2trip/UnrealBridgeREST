# Execution Methods

## Overview

Two methods for executing Python in Unreal Engine, auto-detected based on context.

## Method 1: REST API (Preferred)

**When to use:** Unreal Editor is open with UnrealPythonREST plugin

**Detection:** Check for `{ProjectDir}/Saved/UnrealPythonREST.json`

```bash
# Get port from config
PORT=$(cat /path/to/project/Saved/UnrealPythonREST.json | jq -r '.port')

# Execute Python code
curl -s -X POST "http://localhost:$PORT/api/v1/python/execute" \
  -H "Content-Type: application/json" \
  -d '{"code": "import unreal; print(unreal.EditorAssetLibrary.list_assets(\"/Game\"))"}'
```

**Advantages:**
- Instant execution, no editor restart
- Structured JSON responses
- Full access to editor state
- Hot reload support

## Method 2: Commandlet (Fallback)

**When to use:** Editor is closed, CI/CD pipelines, batch processing

**Syntax:**
```bash
{UEPath}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe {ProjectPath} \
  -run=pythonscript -script="{ScriptPath}"
```

**Example:**
```bash
"C:/Program Files/Epic Games/UE_5.4/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" \
  "D:/Projects/UE/MyGame/MyGame.uproject" \
  -run=pythonscript -script="D:/Scripts/my_script.py"
```

**Important:**
- NEVER use `-ExecutePythonScript` flag (disabled in commandlets)
- Output goes to log file: `{ProjectDir}/Saved/Logs/{ProjectName}.log`
- `unreal.log()` output only appears in log file, not stdout

## Auto-Detection Flow

```
1. Check for UnrealPythonREST.json
   ├── Found → Use REST API (port from JSON)
   └── Not found → Use Commandlet
```

## Log File Location

All Python output (both methods) can be found in:
- `{ProjectDir}/Saved/Logs/{ProjectName}.log`

Filter for Python messages:
```bash
grep -i "python\|LogPython" {ProjectDir}/Saved/Logs/{ProjectName}.log
```
