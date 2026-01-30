
# Unreal Engine Python Skill

## Purpose

Provide verified, test-first workflows for Unreal Engine Python API usage. Prevent errors from assuming APIs exist by testing before suggesting.

## Table of Contents

1. [Critical Command Rules](#-critical-command-rules)
2. [Core Principle: Test-First](#core-principle-test-first-95-confidence-rule)
3. [Environment Setup](#environment-setup)
4. [Running Python Scripts](#running-python-scripts)
5. [API Discovery Workflow](#api-discovery-workflow)
6. [Best Practices](#best-practices)
7. [Common Patterns](#common-patterns)
8. [Common Mistakes to Avoid](#common-mistakes-to-avoid)
9. [Helper Scripts](#helper-scripts)
10. [References](#references)
11. [Workflow Example](#workflow-example)

## ⚠️ CRITICAL COMMAND RULES

  **1. NEVER use `-ExecutePythonScript`**
  ```bash
  # ✓ CORRECT
  -run=pythonscript -script="script.py"

  # ✗ WRONG (disabled in commandlets)
  -ExecutePythonScript="script.py"

  2. ALWAYS use full Windows paths with forward slashes
  # ✓ CORRECT
  "G:/UE_5_3_Source/UnrealEngine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "G:/UE_Projects/Project/Project.uproject" -run=pythonscript
  -script="G:/path/script.py"

  # ✗ WRONG - Unix paths
  /g/UE_Projects/...

  # ✗ WRONG - Relative paths or cd
  cd /path && UnrealEditor-Cmd.exe "Project.uproject" ...

  3. Output goes to LOG FILE only
  - Location: {ProjectDir}/Saved/Logs/{ProjectName}.log
  - unreal.log() does NOT print to stdout
  - Always check log file after execution

## Core Principle: Test-First (95% Confidence Rule)

**NEVER suggest API usage without verification:**

1. Search Python stub file (`{ProjectDir}/Intermediate/PythonStub/unreal.py`)
2. Write minimal test script
3. Execute test via commandlet
4. Read log output to verify
5. Only then suggest verified API

## Environment Setup

### Project Structure
- **Stub file**: `{ProjectDir}/Intermediate/PythonStub/unreal.py` (400k+ lines)
  - Generated when Developer Mode enabled + editor relaunch
- **Log file**: `{ProjectDir}/Saved/Logs/{ProjectName}.log`
- **Commandlet**: `{EngineDir}/Engine/Binaries/Win64/UnrealEditor-Cmd.exe`

### Detect Environment
```python
import os
project_dir = os.environ.get('UE_PROJECT_DIR', 'G:/UE_Projects/YourProject')
stub_file = f"{project_dir}/Intermediate/PythonStub/unreal.py"
log_file = f"{project_dir}/Saved/Logs/{os.path.basename(project_dir)}.log"
```

## Running Python Scripts

### Commandlet (Fast, Headless)

**PowerShell:**
```powershell
& "UnrealEditor-Cmd.exe" "Project.uproject" -run=pythonscript -script="test.py" `
  -unattended -nopause -nosplash -nullrhi > $null; `
  Select-String -Path "Logs/Project.log" -Pattern "LogPython:" | Select-Object -Last 20
```

**Bash:**
```bash
"UnrealEditor-Cmd.exe" "Project.uproject" -run=pythonscript -script="test.py" \
  -unattended -nopause -nosplash -nullrhi > /dev/null 2>&1 && \
  grep "LogPython:" "Logs/Project.log" | tail -20
```

**Critical Points:**
- `unreal.log()` goes to LOG FILE, NOT stdout
- Check for: `LogPythonScriptCommandlet: Display: Python script executed successfully`
- Levels NOT auto-loaded (load manually if needed)
- Use `-nullrhi` for headless mode

### Load Level Before Work
```python
import unreal
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Maps/YourMap")
```

## API Discovery Workflow

### 1. Search Stub File
```bash
# Find class definition
grep -n "^class Blueprint(" unreal.py

# Find methods
grep -n "def get_.*parent" unreal.py
grep -i "component.*add" unreal.py | grep "def "
```

### 2. Runtime Introspection
```python
import unreal, inspect

# List attributes
attrs = [a for a in dir(unreal.Character) if not a.startswith('_')]
unreal.log(f"Found {len(attrs)} attributes")

# Check existence
if hasattr(unreal.Character, '__bases__'):
    unreal.log(f"Parent: {unreal.Character.__bases__[0]}")

# Full inheritance
for i, cls in enumerate(inspect.getmro(unreal.Character)):
    unreal.log(f"[{i}] {cls}")
```

### 3. Test Script Template
```python
import unreal

unreal.log("=== Test: API_NAME ===")
try:
    result = unreal.SomeClass.some_method()
    unreal.log(f"✓ SUCCESS: {result}")
except AttributeError as e:
    unreal.log(f"✗ NOT FOUND: {e}")
except Exception as e:
    unreal.log(f"✗ ERROR: {e}")
unreal.log("=== End Test ===")
```

## Best Practices

### Working with Assets

**✓ DO:**
```python
import unreal

# Use Unreal APIs for asset operations
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
asset_tools.rename_assets([...])

# Or use EditorAssetLibrary
unreal.EditorAssetLibrary.rename_asset(old_path, new_path)
```

**✗ DON'T:**
```python
import os, shutil

# NEVER use Python file operations on assets!
os.rename("Asset.uasset", "NewAsset.uasset")  # Breaks references!
shutil.move("Asset.uasset", "NewFolder/")     # Breaks content!
```

### Editor Properties

Two types of property access:

**1. BlueprintReadOnly/BlueprintReadWrite** - Direct access:
```python
obj.property_name = value
```

**2. ViewAnywhere/EditAnywhere** - Use editor property methods:
```python
# ✓ PREFER: Triggers pre/post-edit code (like UI would)
obj.set_editor_property("play_on_open", True)

# ✗ AVOID: Bypasses editor code
obj.play_on_open = True

# Reading: Both equivalent
value = obj.get_editor_property("play_on_open")
value = obj.play_on_open
```

**Check API Reference** for "Editor Properties" section to see available properties.

### Use Unreal Types
```python
import unreal

# ✓ Use Unreal types (optimized for Engine)
v1 = unreal.Vector(10, 20, 30)
v2 = unreal.Vector()
v3 = (v1 + v2) * 2

# ✗ Don't use custom implementations
# They're slower in Engine context
```

### Logging
```python
import unreal

# Info (print() also works, passes through unreal.log())
unreal.log("Processing asset...")

# Warning
unreal.log_warning("Asset might be missing!")

# Error
unreal.log_error("Failed to load asset!")
```

All messages appear in Output Log panel.

### Undo/Redo Support
```python
import unreal

obj = unreal.MediaPlayer()

# Bundle operations into single undo entry
with unreal.ScopedEditorTransaction("Update Media Settings"):
    obj.set_editor_property("play_on_open", False)
    obj.set_editor_property("vertical_field_of_view", 80)

# Note: Not all operations are undoable (e.g., importing assets)
```

### Progress Dialogs
```python
import unreal

total = 100
with unreal.ScopedSlowTask(total, "Processing...") as task:
    task.make_dialog(True)  # Show dialog

    for i in range(total):
        if task.should_cancel():  # User clicked Cancel
            break

        task.enter_progress_frame(1, f"Processing {i}/{total}")
        # Do work here...
```

## Common Patterns

### Get Parent Class
```python
import unreal

# ✓ Python class types support __bases__
parent = unreal.Character.__bases__[0]  # <class 'Pawn'>

# Full hierarchy
mro = unreal.Character.__mro__
# (Character, Pawn, Actor, Object, _ObjectBase, _WrapperBase, object)

# ✗ UClass instances (from Blueprints) DON'T have __bases__
bp = unreal.load_asset("/Game/BP_Character")
gen_class = bp.generated_class()  # This is UClass instance, not Python type!
# gen_class.__bases__ will fail!
```

### Type Checking
```python
import unreal

# Python class type
type(unreal.Character)              # <class 'type'>
isinstance(unreal.Character, type)  # True

# UClass instance
bp = unreal.load_asset("/Game/BP_Actor")
gen_class = bp.generated_class()
type(gen_class)                     # <class 'Class'>
isinstance(gen_class, type)         # False (instance!)
```

### Safe Attribute Access
```python
# Check before access
if hasattr(obj, 'method_name'):
    result = obj.method_name()

# With default
result = getattr(obj, 'method_name', lambda: None)()
```

## Common Mistakes to Avoid

### ❌ Assuming C++ APIs Are Exposed
```python
# C++ has GetSuperClass(), but Python doesn't expose it
gen_class.get_super_class()  # AttributeError!

# Use Python introspection instead
parent = unreal.Character.__bases__[0]
```

### ❌ Using -ExecCmd for Python
```bash
# WRONG - Runs before editor ready!
UnrealEditor.exe Project.uproject -ExecCmd="py script.py"

# ✓ Use -run=pythonscript instead
```

### ❌ Expecting stdout for unreal.log()
```python
# unreal.log() only goes to LOG FILE
unreal.log("Won't appear in console!")

# Must check log file for output
```

### ❌ Direct File Operations on Assets
```python
# NEVER use os/shutil on .uasset files
os.rename("Asset.uasset", "New.uasset")  # Breaks everything!

# ✓ Use Unreal APIs
unreal.EditorAssetLibrary.rename_asset(old, new)
```

### ❌ Skipping Verification
```python
# DON'T suggest APIs without testing first!

# ✓ Always: search stub → test → verify → suggest
```

## Helper Scripts

See `scripts/` directory:
- `test_unreal_api.py` - API testing template
- `run_python_test.sh` - Execute tests with log filtering

## References

See `references/` directory:
- `stub_locations.md` - Finding and using the Python stub
- `introspection_guide.md` - Python introspection in Unreal
- `common_apis.md` - Verified API examples
- `best_practices.md` - Official Unreal Python best practices

## Workflow Example

**User:** "How do I get Blueprint parent class?"

**Your Process:**
1. Search stub: `grep -i "parent.*class" unreal.py` → No obvious method
2. Test Python introspection:
   ```python
   import unreal
   unreal.log(f"Parent: {unreal.Character.__bases__}")
   ```
3. Run test, verify in logs
4. Share verified solution

**Never skip verification!**
