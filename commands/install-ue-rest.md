---
name: install-ue-rest
description: Install or update UnrealBridgeREST plugin in Unreal Engine project for live Python execution
arguments:
  - name: project_dir
    description: Path to Unreal Engine project directory
    required: true
---

Install or update the UnrealBridgeREST plugin in the specified Unreal Engine project.

## What This Does

1. Copies the UnrealBridgeREST plugin to `{project}/Plugins/`
2. Enables the plugin in the `.uproject` file
3. Enables the required PythonScriptPlugin dependency
4. Tracks installed version for future updates
5. Verifies all files are in place

## Requirements

Before running, verify:
- Directory contains a `.uproject` file
- Path is a valid Unreal Engine 5.x project
- Use forward slashes or quoted paths with spaces

## Usage

**Install (new installation):**
```
/install-ue-rest G:/UE_Projects/MyGame
```

**Update (existing installation):**
```
/install-ue-rest G:/UE_Projects/MyGame --update
```

## Flags

| Flag | Short | Description |
|------|-------|-------------|
| `--update` | `-u` | Update existing installation to latest version |
| `--force` | `-f` | Force overwrite existing installation (no version check) |
| `--check` | `-c` | Check if installation is possible without installing |
| `--dry-run` | `-n` | Show what would be done without making changes |
| `--quiet` | `-q` | Only show errors and final status |

## Updating Existing Installation

When the plugin is already installed, the installer will:
1. Check current installed version
2. Compare with source version
3. Skip if already up-to-date
4. Backup and update if new version available

Use `--update` to upgrade, or `--force` to replace regardless of version.

## After Installation

1. Open the project in Unreal Editor
2. Plugin auto-starts REST server on port 8080
3. Use the `unreal-bridge` skill with instant execution

## Execute Installation

Run the installer script with the provided project directory:

```bash
uv run ~/.claude/skills/unreal-bridge/scripts/install_plugin.py "$ARGUMENTS"
```

Report the result to the user and explain next steps.
