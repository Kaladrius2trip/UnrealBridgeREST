# Research Methodology: Investigating Python Capabilities

**Purpose:** Process for investigating "Can Python do X?" claims beyond basic API lookup.

---

## Investigation Workflow

### 1. Classify the Claim
- **Positive claim** ("Python can do X") → Search stub, test, verify
- **Negative claim** ("Python cannot do X") → Question assumption, search for alternatives

### 2. Search for Functional Equivalents (Not Exact APIs)

**C++ → Python equivalence patterns:**
| C++ Pattern | Python Equivalent | Use Case |
|-------------|------------------|-----------|
| `TFieldIterator<FProperty>` | `dir(obj)` | Iterate class members |
| `Property->Identical()` | `==` operator | Compare values |
| `Property->GetValue()` | `getattr(obj, name)` | Access property |
| `GetCDO()` | `unreal.get_default_object(cls)` | Get class defaults |
| Property flags check | Not available | (Real limitation) |

**Key insight:** Different implementation ≠ Missing capability

### 3. Use Explore Agent for Evidence Gathering

**When to use:**
- Negative claims to verify ("can't do X")
- Large codebase searches (stub file, UE source)
- Finding C++ wrapper implementations (`PyGenUtil`, `PyWrapperObject`)

**What to request:**
- Concrete evidence (file paths, line numbers)
- Both C++ implementation AND Python exposure
- Alternative APIs that achieve same goal

### 4. Test the Hypothesis

**Design test to prove/disprove:**
```python
# Setup known state → Perform operation → Verify result → Report pass/fail
```

**Not just "does API exist" but "does operation work end-to-end"**

---

## Case Study: Property Serialization

**Claim:** "Python cannot compare component properties with CDO defaults"

**Investigation:**
1. **Why claimed?** No `FProperty*` or `TFieldIterator` in Python
2. **Functional equivalent?** Yes: `dir()` + `getattr()` + `get_default_object()`
3. **Explore found:** `PyWrapperObject.cpp` wraps property access via `get_editor_property()`
4. **Test proved:** CDO comparison detects modified properties

**C++ approach:**
```cpp
for (TFieldIterator<FProperty> PropIt(Class); PropIt; ++PropIt) {
    if (!Property->Identical_InContainer(Component, CDO)) { }
}
```

**Python equivalent:**
```python
cdo = unreal.get_default_object(component.get_class())
for prop in dir(component):
    if getattr(component, prop) != getattr(cdo, prop):
        # Modified!
```

**Result:** Different APIs, same capability → Claim disproven

---

## Key Principles

### Think Functionally, Not Literally
- ❌ "Python doesn't have `TFieldIterator`, therefore impossible"
- ✅ "Python has `dir()` which achieves same iteration"

### Question Negative Assumptions
- Comments saying "can't do X" are often untested assumptions
- Especially suspect when based on missing single API (alternatives may exist)

### Understand Implementation vs Capability
- **Implementation limitation:** "Python can't access `FProperty*`" (TRUE)
- **Capability limitation:** "Python can't detect modified properties" (FALSE)
- The former doesn't always imply the latter

### Document for Future Agents
- Disprove assumption → Update reference docs
- Next agent starts with verified facts, not assumptions
- Knowledge compounds iteratively

---

## Research Checklist

Investigating negative claims ("Python cannot do X"):

- [ ] Check reference files for existing knowledge
- [ ] Use Explore agent to find:
  - C++ implementation details
  - Python wrapper code (`PyWrapperObject`, `PyGenUtil`)
  - Alternative Python APIs
- [ ] Identify functional equivalents (not exact API matches)
- [ ] Design test proving/disproving the capability
- [ ] Execute test, analyze results
- [ ] Update reference docs with findings

---

## Anti-Patterns

**❌ Accepting negative claims without evidence**
```python
# Comment says impossible → Must be true
```

**❌ Looking for exact C++ API**
```python
# No TFieldIterator in stub → Impossible
```

**❌ Stopping at "API doesn't exist"**
```python
# FProperty not in Python → Can't access properties
```

**✅ Correct approach:**
```python
# No TFieldIterator → What's Python's way to iterate?
# No FProperty → How does Python access properties?
# Test the functional equivalent
```
