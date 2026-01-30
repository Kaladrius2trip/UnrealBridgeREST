---
name: uninstall-ue-rest
description: Remove UnrealPythonREST plugin from Unreal Engine project
arguments:
  - name: project_dir
    description: Path to Unreal Engine project directory
    required: true
---

Remove the UnrealPythonREST plugin from the specified Unreal Engine project.

## What This Does

1. Removes the plugin directory from `{project}/Plugins/`
2. Removes the plugin entry from `.uproject` file
3. Removes the discovery config file if present

## Requirements

Before running, verify:
- Directory contains a `.uproject` file
- Plugin was previously installed via `/install-ue-rest`

## Usage

```
/uninstall-ue-rest G:/UE_Projects/MyGame
```

## Execute Uninstallation

Run the uninstaller script:

```bash
uv run ~/.claude/skills/unreal-bridge/scripts/uninstall_plugin.py "$ARGUMENTS"
```

Report the result to the user.
