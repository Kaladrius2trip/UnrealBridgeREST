# Unreal Python Best Practices

**Last Updated:** 2025-11-01 for UE 5.3
**Status:** Verified through testing (see [common_apis.md](common_apis.md))

This guide compiles official Unreal Engine best practices with verified patterns.

## Table of Contents

1. [Core Principles](#core-principles)
   - [Test-First Approach](#1-test-first-approach-95-confidence-rule)
   - [Working with Assets](#2-working-with-assets)
   - [Changing Editor Properties](#3-changing-editor-properties)
   - [Use Unreal Types](#4-use-unreal-types)
   - [Logging and Output](#5-logging-and-output)
   - [Supporting Undo/Redo](#6-supporting-undoredo)
   - [Blueprint Operations](#7-blueprint-operations)
   - [Asset Discovery](#8-asset-discovery-and-filtering)
   - [Level Operations](#9-level-operations)
   - [Class Introspection](#10-class-and-type-introspection)
2. [Common Pitfalls](#common-pitfalls)
3. [Testing Workflow](#testing-workflow)
4. [References](#references)

## Core Principles

### 1. Test-First Approach (95% Confidence Rule)

**NEVER suggest an API without verification:**

1. Search Python stub file for the API
2. Write a test script using the API
3. Run via commandlet: `-run=pythonscript -script=test_script.py`
4. Verify in log file
5. Only then use in production code

**Example verification workflow:**

```python
# test_new_api.py
import unreal

def test_api():
    try:
        result = unreal.SomeNewAPI.some_method()
        unreal.log(f"✓ API works: {result}")
    except AttributeError as e:
        unreal.log(f"✗ API not found: {e}")
    except Exception as e:
        unreal.log(f"✗ Error: {e}")

test_api()
```

### 2. Working with Assets

**CRITICAL: Asset File Operations**

```python
# ✗ NEVER do this - corrupts Unreal asset database!
import os
import shutil
os.rename("/path/to/asset.uasset", "/new/path.uasset")  # WRONG
shutil.move("asset.uasset", "new_location/")             # WRONG

# ✓ ALWAYS use Unreal's asset library
import unreal

# Rename asset
unreal.EditorAssetLibrary.rename_asset(
    "/Game/OldName",
    "/Game/NewName"
)

# Move asset (rename with new path)
unreal.EditorAssetLibrary.rename_asset(
    "/Game/Meshes/MyMesh",
    "/Game/NewFolder/MyMesh"
)

# Delete asset
unreal.EditorAssetLibrary.delete_asset("/Game/AssetToDelete")

# Duplicate asset
unreal.EditorAssetLibrary.duplicate_asset(
    "/Game/SourceAsset",
    "/Game/CopiedAsset"
)
```

**Why this matters:**
- `.uasset` files contain internal references and dependencies
- Asset registry must be updated when assets move/change
- Direct file operations bypass Unreal's systems
- Can cause project corruption and reference breaks

### 3. Changing Editor Properties

**Use `set_editor_property()` instead of direct assignment:**

```python
import unreal

obj = unreal.MediaPlayer()

# ✗ Direct assignment - skips validation and editor callbacks
obj.play_on_open = True  # May not trigger update logic

# ✓ Use set_editor_property - triggers pre/post-edit notifications
obj.set_editor_property("play_on_open", True)

# Set multiple properties
obj.set_editor_properties({
    "play_on_open": True,
    "vertical_field_of_view": 80
})

# Read property
value = obj.get_editor_property("play_on_open")
```

**When this matters:**
- Properties with `EditAnywhere` or `ViewAnywhere` specifiers
- Properties that trigger validation or dependent updates
- Blueprint CDO (Class Default Object) modifications
- Any property visible in Details panel

**When direct assignment is OK:**
- Transient runtime properties
- Properties without editor callbacks
- Pure data structures

### 4. Use Unreal Types

**Always use Unreal's math types for consistency:**

```python
import unreal

# ✓ Use Unreal types
location = unreal.Vector(100, 200, 300)
rotation = unreal.Rotator(0, 90, 0)
scale = unreal.Vector(1, 1, 1)

transform = unreal.Transform()
transform.translation = location
transform.rotation = rotation.quaternion()
transform.scale3d = scale

# Math operations work naturally
v1 = unreal.Vector(10, 20, 30)
v2 = unreal.Vector(1, 2, 3)
result = v1 + v2
scaled = v1 * 2.0
distance = v1.distance(v2)

# ✗ Don't use Python tuples or custom types
location = (100, 200, 300)  # Won't work with Unreal APIs
```

### 5. Logging and Output

**Understand where output goes:**

```python
import unreal

# All these go to LOG FILE, not stdout!
unreal.log("Information message")
unreal.log_warning("Warning message")
unreal.log_error("Error message")

# print() also goes to log file (passes through to unreal.log)
print("This appears in log, not console")
```

**Log file location:**
```
{ProjectDir}/Saved/Logs/{ProjectName}.log
```

**Commandlet execution:**
```bash
# Run script
UnrealEditor-Cmd.exe {project} -run=pythonscript -script=script.py

# Then check log
cat G:/UE_Projects/TempForMcp/Saved/Logs/TempForMcp.log
```

**Progress feedback for long operations:**

```python
import unreal

total = 100
with unreal.ScopedSlowTask(total, "Processing items...") as task:
    task.make_dialog(True)  # Show modal progress dialog

    for i in range(total):
        if task.should_cancel():  # User clicked Cancel
            unreal.log("User cancelled operation")
            break

        task.enter_progress_frame(1, f"Processing item {i+1}/{total}")
        # Do work here
```

### 6. Supporting Undo/Redo

**Bundle operations into transactions:**

```python
import unreal

obj = unreal.MediaPlayer()

# ✓ Single operation = single undo entry
with unreal.ScopedEditorTransaction("Update Media Settings"):
    obj.set_editor_property("play_on_open", False)
    obj.set_editor_property("vertical_field_of_view", 80)
    obj.set_editor_property("horizontal_field_of_view", 120)
    # User can undo all three changes with one Ctrl+Z

# ✗ Without transaction - may not be undoable
obj.set_editor_property("play_on_open", False)  # Not undoable
```

**Important notes:**
- Not all operations support undo (e.g., asset import, file operations)
- Transaction name appears in Edit → Undo History
- Nested transactions not recommended
- Keep transactions focused and short-lived

### 7. Blueprint Operations

**Load and compile blueprints correctly:**

```python
import unreal

# Load blueprint asset
bp_path = "/Game/Blueprints/BP_MyActor"
bp = unreal.load_asset(bp_path)

if bp:
    # Get generated class (runtime class)
    gen_class = bp.generated_class()

    # Get Class Default Object (CDO)
    cdo = gen_class.get_default_object()

    # Modify CDO properties
    with unreal.ScopedEditorTransaction("Update Blueprint Defaults"):
        cdo.set_editor_property("speed", 500.0)

    # Compile to apply changes
    # (Use EditorAssetSubsystem or compile functions)
```

**Working with Blueprint hierarchies:**

```python
import unreal

# Get parent class using Python introspection
bp_class = unreal.load_class(None, '/Game/BP_Child.BP_Child_C')
parent_class = bp_class.__bases__[0]

# Full hierarchy
mro = bp_class.__mro__
for cls in mro:
    unreal.log(f"Class: {cls}")
```

### 8. Asset Discovery and Filtering

**Use Asset Registry for efficient queries:**

```python
import unreal

# Get asset registry
registry = unreal.AssetRegistryHelpers.get_asset_registry()

# Create filter
filter = unreal.ARFilter(
    class_names=["Blueprint"],     # Filter by type
    package_paths=["/Game"],       # Filter by path
    recursive_paths=True           # Include subfolders
)

# Find assets
asset_data_list = registry.get_assets(filter)

for asset_data in asset_data_list:
    unreal.log(f"Found: {asset_data.package_name}")
    unreal.log(f"  Class: {asset_data.asset_class}")
    unreal.log(f"  Path: {asset_data.object_path}")
```

**Why use Asset Registry:**
- Fast queries without loading assets
- Filter by class, path, tags
- Get metadata without full asset load
- Essential for large projects

### 9. Level Operations

**Work with levels safely:**

```python
import unreal

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# Levels not auto-loaded in commandlet mode!
level_subsystem.load_level("/Game/Maps/MyMap")

# Get actors in level (after loading)
actors = unreal.EditorLevelLibrary.get_all_level_actors()

# Save current level
level_subsystem.save_current_level()

# Save all dirty levels
level_subsystem.save_all_dirty_levels()
```

### 10. Class and Type Introspection

**Use Python's introspection on Unreal types:**

```python
import unreal
import inspect

# Check if something is a class
is_class = inspect.isclass(unreal.Character)  # True

# Get parent class
parent = unreal.Character.__bases__[0]  # <class 'Pawn'>

# Full hierarchy (Method Resolution Order)
mro = unreal.Character.__mro__
# (Character, Pawn, Actor, Object, _ObjectBase, _WrapperBase, object)

# Check for attributes/methods
has_method = hasattr(unreal.Character, 'some_method')

# List all members
members = dir(unreal.Character)
```

**Important distinction:**

```python
# Python class type - has __bases__, __mro__
class_type = unreal.Character  # type
parent = class_type.__bases__[0]  # Works!

# UClass instance - no __bases__
bp = unreal.load_asset("/Game/BP_MyActor")
gen_class = bp.generated_class()  # UClass instance
# gen_class.__bases__  # AttributeError!

# Use Python introspection only on Python types
```

## Common Pitfalls

### 1. Assuming APIs Exist

```python
# ✗ Don't assume based on C++ API
# unreal.SystemLibrary.get_engine_directory()  # Does NOT exist!

# ✓ Always verify in stub file first
# Search: G:/UE_Projects/TempForMcp/Intermediate/PythonStub/unreal.py
```

### 2. Forgetting Commandlet vs Editor Mode

```python
# Commandlet mode (headless):
# - No UI interactions
# - Levels not auto-loaded
# - Some subsystems unavailable
# - Faster execution

# Editor mode (GUI):
# - Full UI available
# - Can open asset editors
# - Slower but interactive
```

### 3. Not Checking Log Files

```python
# Script seems to run but no output?
# Check the log file!
# {ProjectDir}/Saved/Logs/{ProjectName}.log
```

### 4. Using Deprecated APIs

```python
# ⚠️ Still works but deprecated
actors = unreal.EditorLevelLibrary.get_selected_level_actors()

# ✓ Use subsystems instead (when available)
# (Check for Editor Actor Utilities Subsystem replacement)
```

## Testing Workflow

### 1. Quick API Test

```python
# test_quick.py
import unreal

# Test specific API
unreal.log("=== Quick Test Start ===")
try:
    result = unreal.EditorAssetLibrary.list_assets("/Game")
    unreal.log(f"✓ Success: Found {len(result)} assets")
except Exception as e:
    unreal.log(f"✗ Failed: {e}")
unreal.log("=== Quick Test End ===")
```

### 2. Comprehensive Test Template

```python
# test_comprehensive.py
import unreal

def test_section(name):
    unreal.log("=" * 60)
    unreal.log(f"TESTING: {name}")
    unreal.log("=" * 60)

def test_api(description, test_func):
    try:
        result = test_func()
        unreal.log(f"✓ {description}")
        return True
    except AttributeError as e:
        unreal.log(f"✗ API NOT FOUND - {description}: {e}")
        return False
    except Exception as e:
        unreal.log(f"✗ ERROR - {description}: {e}")
        return False

# Track results
passed = 0
failed = 0

# Run tests
test_section("My Feature Tests")

if test_api("Test description", lambda: your_test_code()):
    passed += 1
else:
    failed += 1

# Summary
unreal.log(f"✓ Passed: {passed}")
unreal.log(f"✗ Failed: {failed}")
unreal.log(f"Success Rate: {100 * passed / (passed + failed):.1f}%")
```

## References

- [common_apis.md](common_apis.md) - Verified API reference with examples
- [introspection_guide.md](introspection_guide.md) - Python introspection techniques
- [stub_locations.md](stub_locations.md) - Finding and searching the Python stub

## Tags

#unreal-engine/python #unreal-engine/best-practices #unreal-engine/editor-scripting
