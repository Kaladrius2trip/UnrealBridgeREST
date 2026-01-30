# Python Stub File Locations

## What is the Stub File?

The `unreal.py` stub file contains Python type hints and signatures for all exposed Unreal Engine APIs. It's the source of truth for what's available in Python.

## Generating the Stub File

1. Enable **Developer Mode** in Editor Preferences
2. Restart the Unreal Editor
3. Stub file is generated at: `{ProjectDir}/Intermediate/PythonStub/unreal.py`

## Typical Location Pattern

```
{ProjectRoot}/
└── Intermediate/
    └── PythonStub/
        └── unreal.py  (400,000+ lines for UE 5.3+)
```

## Example Paths

```
G:/UE_Projects/MyProject/Intermediate/PythonStub/unreal.py
C:/UnrealProjects/Game/Intermediate/PythonStub/unreal.py
```

## File Size

- **UE 5.3+**: ~400,000-500,000 lines
- **File size**: ~15-20 MB
- **Contains**: All class definitions, methods, properties, enums

## Using the Stub File

### Search for Classes
```bash
grep -n "^class Blueprint(" unreal.py
grep -n "^class StaticMesh(" unreal.py
```

### Search for Methods
```bash
grep "def get_.*parent" unreal.py
grep -i "component" unreal.py | grep "def "
```

### Search for Properties
```bash
grep "parent_class" unreal.py
grep "BlueprintReadWrite" unreal.py
```

## Important Notes

- **Not for execution**: This file is for type hints only, not meant to be executed
- **Editor-only**: Generated in editor builds, not packaged games
- **Regenerated**: Can be regenerated anytime by toggling Developer Mode
- **Version-specific**: Different Unreal versions have different APIs

## If Stub File Doesn't Exist

1. Check Developer Mode is enabled
2. Restart editor completely
3. Check `{Project}/Intermediate/PythonStub/` directory was created
4. Verify PythonScriptPlugin is enabled for the project
