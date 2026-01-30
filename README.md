# UnrealBridgeREST

C++ REST API plugin for Unreal Engine + Claude Code skill for AI-driven editor automation.

## Quick Start

### 1. Install Claude Code Plugin

```bash
claude plugin install unreal-bridge
```

### 2. Install UE Plugin to Your Project

```
/install-ue-rest "D:\Projects\UE\YourProject"
```

This creates a symlink from your project's `Plugins/UnrealBridgeREST/` to the plugin source.

### 3. Open UE Project

Plugin compiles automatically. REST API starts on port 8080.

### 4. Use It

The `unreal-bridge` skill provides 50+ REST endpoints for automating:

- **Actors** — spawn, transform, destroy, list, properties
- **Materials** — create nodes, connect, parameters, import/export
- **Blueprints** — node CRUD, connect, compile, variables
- **Assets** — list, search, info, references, export
- **Editor** — camera, selection, live coding
- **Level** — info, outliner
- **Python** — execute code sync/async

## API Documentation

See `skills/unreal-bridge/references/endpoints/` for full endpoint documentation.

## Development

Clone into a UE project's Plugins/ directory:

```bash
cd YourProject/Plugins
git clone https://github.com/yevhe/UnrealBridgeREST.git
```

Edit C++ source, compile via Live Coding (`POST /editor/live_coding`), test via REST on `:8080`.

### Repository Structure

```
UnrealBridgeREST/
├── .claude-plugin/plugin.json     # Claude Code marketplace manifest
├── UnrealBridgeREST.uplugin       # UE plugin descriptor
├── VERSION.json
├── Source/                        # UE C++ plugin source
├── skills/unreal-bridge/          # Claude Code skill
│   ├── SKILL.md
│   ├── scripts/                   # Install/status/uninstall scripts
│   └── references/                # API documentation
├── commands/                      # Slash commands
└── docs/                          # Installation guide
```

## Requirements

- Unreal Engine 5.0+
- PythonScriptPlugin enabled
- Claude Code (for skill/commands)

## License

MIT
