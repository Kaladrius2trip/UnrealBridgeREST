# Plugin Setup

## Installing UnrealPythonREST Plugin

Use slash commands to manage the REST plugin:

| Command | Description |
|---------|-------------|
| `/install-ue-rest <project-path>` | Install plugin to UE project |
| `/install-ue-rest <project-path> --update` | Update existing installation |
| `/ue-rest-status <project-path>` | Check installation status |
| `/uninstall-ue-rest <project-path>` | Remove plugin from project |

**Example:**
```bash
/install-ue-rest G:/UE_Projects/MyGame
/install-ue-rest G:/UE_Projects/MyGame --update
```

## What Installation Does

1. Copies plugin to `{Project}/Plugins/UnrealPythonREST/`
2. Enables plugin in `.uproject` file
3. Enables PythonScriptPlugin dependency
4. Tracks installed version for future updates

## After Installation

1. Open project in Unreal Editor
2. Plugin starts REST server automatically on configured port
3. Server config written to `{ProjectDir}/Saved/UnrealPythonREST.json`

## Version Management

- Installer compares versions and skips if already up-to-date
- Automatic backup of previous version before update
- Use `--update` flag to force upgrade to latest version

## Troubleshooting

**Plugin not loading:**
- Check Output Log for "UnrealPythonREST" messages
- Verify PythonScriptPlugin is enabled
- Rebuild project if needed

**Port conflicts:**
- Default port is 8080
- Check `UnrealPythonREST.json` for actual port
- Server auto-selects next available port if default busy
