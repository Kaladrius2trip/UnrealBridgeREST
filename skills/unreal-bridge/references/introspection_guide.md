# Python Introspection in Unreal Engine - Compact Guide

## Blueprint Introspection (READ + WRITE)

**Status:** ✅ All aspects verified (Parent, Variables, Functions, Components, Interfaces, Property Serialization)

### Core Technique: CDO Comparison

Blueprint data lives on generated class CDO, not the asset itself.

```python
bp = unreal.load_asset(path)
gen_class = bp.generated_class()
cdo = unreal.get_default_object(gen_class)
parent_class = type(cdo).__bases__[0]
parent_cdo = unreal.get_default_object(parent_class)

# BP-only attributes = All CDO attrs - Parent CDO attrs
bp_only = set(dir(cdo)) - set(dir(parent_cdo))
```

### 1. Parent Class
```python
parent_class = type(cdo).__bases__[0]  # e.g., "Character"
```

### 2. Variables
```python
bp_vars = {a: type(getattr(cdo, a)).__name__
           for a in dir(cdo)
           if not a.startswith('_') and not callable(getattr(cdo, a, None))}
parent_vars = {a for a in dir(parent_cdo) if not a.startswith('_') and not callable(getattr(parent_cdo, a, None))}
bp_only_vars = {k: v for k, v in bp_vars.items() if k not in parent_vars}
```

### 3. Functions
```python
bp_methods = {a for a in dir(cdo) if callable(getattr(cdo, a)) and not a.startswith('_')}
parent_methods = {a for a in dir(parent_cdo) if callable(getattr(parent_cdo, a)) and not a.startswith('_')}
bp_only_funcs = bp_methods - parent_methods
```

### 4. Components (SubobjectData - PREFERRED)
```python
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)  # NOT editor_subsystem!
handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
BFL = unreal.SubobjectDataBlueprintFunctionLibrary

for handle in handles:
    data = subsystem.k2_find_subobject_data_from_handle(handle)
    if BFL.is_valid(data) and BFL.is_component(data):
        name = BFL.get_variable_name(data)
        obj = BFL.get_associated_object(data)

        # Detect source: Inherited/Native/Blueprint
        is_native = BFL.is_native_component(data)
        is_inherited = BFL.is_inherited_component(data)
        source = "Inherited" if (is_native and is_inherited) else "Native" if is_native else "Blueprint"
```

**Alternative (CDO approach - simpler but less info):**
```python
if isinstance(cdo, unreal.Actor):
    components = cdo.get_components_by_class(unreal.ActorComponent)
    for comp in components:
        name = comp.get_name()
        is_bp_added = comp.is_created_by_construction_script()  # False = native/inherited
```

### 5. Interfaces
```python
interfaces = [b.__name__ for b in type(cdo).__mro__ if 'Interface' in b.__name__ and b.__name__ != 'Interface']
```

### 6. Property Serialization (Detecting Modified Properties) ✅ VERIFIED

**Status:** Proven via test - Python CAN detect modified component properties via CDO comparison.

```python
def get_modified_properties(component):
    """Get all properties that differ from CDO defaults"""
    cdo = unreal.get_default_object(component.get_class())
    modified = {}

    for prop_name in dir(component):
        if prop_name.startswith('_'):
            continue

        try:
            comp_value = getattr(component, prop_name)
            cdo_value = getattr(cdo, prop_name)

            if callable(comp_value):
                continue

            if comp_value != cdo_value:
                modified[prop_name] = comp_value
        except:
            pass

    return modified
```

**Proven capabilities:**
- Detects boolean property changes (e.g., `cast_shadow`, `hidden_in_game`)
- Detects struct property changes (e.g., `Vector`, `Rotator`)
- Detects enum property changes (e.g., `ComponentMobility`)
- Works on any UObject/Component instance
- Test results: Detected 5 modified properties including complex types

**Note:** This disproves the assumption that "Python cannot access property serialization system". Python CAN serialize properties, just differently than C++ PropertyParser (via `dir()` + `getattr()` + CDO comparison instead of `FProperty` iteration).

### 7. Adding Variables/Functions (WRITE)

**Key:** Use `BlueprintEditorLibrary` helpers - EdGraphPinType properties not directly settable.

```python
# Create pin type
pin_type = unreal.BlueprintEditorLibrary.get_basic_type_by_name("int")  # "float", "bool", "string", "name", "vector"

# Add variable/function
unreal.BlueprintEditorLibrary.add_member_variable(bp, "MyVar", pin_type)
unreal.BlueprintEditorLibrary.add_function_graph(bp, "MyFunc")
```

**Helpers:**
- `get_basic_type_by_name(type)` - Basic types
- `get_array_type(elem_type)` - Arrays
- `get_struct_type(struct)` - Structs
- `get_object_reference_type(class)` - Object refs

### Important: Blueprint Asset vs CDO

**Blueprint asset properties are PROTECTED** - Direct access fails:
```python
bp.new_variables                        # ❌ Protected
bp.get_editor_property("NewVariables")  # ❌ Protected
bp.function_graphs                      # ❌ Protected
bp.implemented_interfaces               # ❌ Protected
```

**CDO comparison WORKS** - Detects existing members:
```python
cdo = unreal.get_default_object(bp.generated_class())
vars = set(dir(cdo)) - set(dir(parent_cdo))  # ✅ Works!
```

**Limitations:**
- CDO comparison finds *existing* members but can't access Blueprint graph structure, metadata, or details
- For graph node introspection (connections, pins, etc.), use C++ MCP implementation
- Property serialization (detecting modified values) DOES work via CDO comparison (see section 6)

### Common Mistakes

```python
# WRONG - Testing asset instead of CDO
bp.new_variables  # Empty/Protected!

# CORRECT
cdo = unreal.get_default_object(bp.generated_class())
vars = set(dir(cdo)) - set(dir(parent_cdo))
```

```python
# WRONG - SubobjectData is EngineSubsystem, not EditorSubsystem
subsystem = unreal.get_editor_subsystem(unreal.SubobjectDataSubsystem)  # TypeError!

# CORRECT
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
```

```python
# WRONG - EdGraphPinType properties not exposed to Python
pin_type = unreal.EdGraphPinType()
pin_type.pin_category = "int"  # AttributeError!

# CORRECT
pin_type = unreal.BlueprintEditorLibrary.get_basic_type_by_name("int")
```

## General Python Introspection

### Core Tools

**`dir(obj)`** - List all attributes/methods
**`type(obj)`** - Get type (note: UClass instances ≠ Python types)
**`hasattr(obj, attr)`** / **`getattr(obj, attr, default)`** - Safe checking
**`callable(obj)`** - Check if method vs property

### Class Hierarchy

```python
# Python types (unreal.Character, unreal.Actor)
parent = cls.__bases__[0]           # Direct parent
hierarchy = cls.__mro__              # Full chain (or cls.mro())

# UClass instances (bp.generated_class()) - DON'T have __bases__!
# Use CDO comparison instead (see above)
```

### Type Checking

```python
isinstance(unreal.Character, type)  # True - Python class type
isinstance(bp.generated_class(), type)  # False - UClass instance
```

### Filtering Attributes

```python
# Methods only
methods = [a for a in dir(obj) if callable(getattr(obj, a)) and not a.startswith('_')]

# Properties only
props = [a for a in dir(obj) if not callable(getattr(obj, a)) and not a.startswith('_')]

# By pattern
get_methods = [m for m in dir(obj) if m.startswith('get_')]
```

## Key Insights

1. **CDO is the key** - Blueprint data on generated class CDO, not asset
2. **Two access patterns** - Blueprint asset properties (protected) vs CDO comparison (works)
3. **Set difference reveals additions** - Compare CDO vs parent CDO
4. **Python mirrors C++ reflection** - `dir()` exposes UPROPERTY/UFUNCTION
5. **SubobjectData preferred for components** - Better source detection than CDO approach
6. **Subsystem type matters** - SubobjectData = EngineSubsystem (NOT EditorSubsystem)
7. **EdGraphPinType needs helpers** - Can't set properties directly
8. **Python types ≠ UClass instances** - Only Python types have `__bases__`
9. **Property serialization works** - Python CAN detect modified properties via CDO comparison (section 6)
10. **Limitation** - CDO finds existing members, but not graph structure/metadata
