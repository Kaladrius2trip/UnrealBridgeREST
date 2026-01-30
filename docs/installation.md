# Installation Guide

## For Users

### Prerequisites

- Unreal Engine 5.0+
- PythonScriptPlugin enabled in your project
- Claude Code installed

### Step 1: Install Claude Code Plugin

```bash
claude plugin install unreal-bridge
```

This downloads the plugin and registers the skill and slash commands.

### Step 2: Install UE Plugin to Your Project

```
/install-ue-rest "D:\Projects\UE\YourProject"
```

This creates a symlink from your project's `Plugins/UnrealBridgeREST/` to the plugin source. Updates propagate automatically when the plugin is updated.

**Flags:**

| Flag | Description |
|------|-------------|
| `--update` | Update existing installation to latest version |
| `--force` | Force overwrite existing installation |
| `--check` | Check if installation is possible without installing |
| `--dry-run` | Show what would be done without changes |
| `--quiet` | Only show errors and final status |

### Step 3: Open UE Project

1. Open your project in Unreal Editor
2. Plugin compiles automatically on first launch
3. REST API starts on `http://localhost:8080/api/v1/`

### Step 4: Verify

```
/ue-rest-status "D:\Projects\UE\YourProject"
```

### Uninstall

```
/uninstall-ue-rest "D:\Projects\UE\YourProject"
```

---

## For Developers

### Clone into UE Project

```bash
cd D:\Projects\UE\YourProject\Plugins
git clone https://github.com/Kaladrius2trip/UnrealBridgeREST.git
```

The plugin compiles when you open the UE project.

### Link Skill to Claude Code

Create a junction (Windows, no admin needed):

```cmd
mklink /J "C:\Users\<you>\.claude\skills\unreal-bridge" "D:\Projects\UE\YourProject\Plugins\UnrealBridgeREST\skills\unreal-bridge"
```

Copy command files to `~/.claude/commands/`:

```cmd
copy "D:\Projects\UE\YourProject\Plugins\UnrealBridgeREST\commands\*.md" "C:\Users\<you>\.claude\commands\"
```

> **Note:** Directory symlinks require admin on Windows. Junctions (`mklink /J`) work without admin but only on the same drive. For cross-drive setups, use junctions or copy files.

### Development Workflow

1. Edit C++ source in `Source/`
2. Compile via Live Coding: `POST http://localhost:8080/api/v1/editor/live_coding`
3. Test endpoints via REST on port 8080
4. Commit and push to the plugin's git repo

### Adding Sandbox as .gitignore Entry

If the plugin repo lives inside a UE project that has its own git repo, add to the parent project's `.gitignore`:

```
Plugins/UnrealBridgeREST/
```

This prevents the parent repo from tracking the nested plugin repo.

---

## Troubleshooting

**Plugin doesn't compile:**
- Ensure PythonScriptPlugin is enabled in your `.uproject`
- Check UE version is 5.0+

**REST server not starting:**
- Check `{Project}/Saved/UnrealPythonREST.json` for server config
- Verify port 8080 is not in use

**Skill not loading:**
- Verify junction/symlink: `ls ~/.claude/skills/unreal-bridge/SKILL.md`
- Check commands exist: `ls ~/.claude/commands/install-ue-rest.md`
