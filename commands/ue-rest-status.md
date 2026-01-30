---
name: ue-rest-status
description: Check UnrealBridgeREST plugin installation status
arguments:
  - name: project_dir
    description: Path to Unreal Engine project directory
    required: true
---

Check the installation status of UnrealBridgeREST plugin for a project.

## What This Shows

- Whether plugin is installed
- Whether plugin is enabled in .uproject
- Whether REST server is running
- Server port if available

## Requirements

- Directory must be a valid Unreal Engine project
- Use forward slashes or quoted paths with spaces

## Usage

```
/ue-rest-status G:/UE_Projects/MyGame
```

## Execute Status Check

Run the status check script:

```bash
uv run ~/.claude/skills/unreal-bridge/scripts/check_plugin_status.py "$ARGUMENTS"
```

Explain the status to the user and suggest next steps based on the result.
