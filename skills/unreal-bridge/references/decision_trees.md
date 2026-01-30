# Decision Trees for UE Action Routing

This document describes how to choose between REST endpoints, Python execution, and when to suggest API improvements.

## Primary Decision Flow

When performing any UE Editor action:

```
┌──────────────────────────────────────┐
│ User requests action in UE Editor    │
└──────────────────┬───────────────────┘
                   ▼
┌──────────────────────────────────────┐
│ Check references/endpoints/*.md      │
└──────────────────┬───────────────────┘
                   ▼
          ┌────────────────┐
          │ Endpoint found? │
          └───────┬────────┘
         YES      │      NO
          ▼       │       ▼
┌─────────────┐   │   ┌─────────────────────────┐
│ Use REST API│   │   │ Query /schema?endpoint= │
└─────────────┘   │   └───────────┬─────────────┘
                  │               ▼
                  │      ┌────────────────┐
                  │      │ Endpoint exists?│
                  │      └───────┬────────┘
                  │     YES      │      NO
                  │      ▼       │       ▼
                  │ ┌─────────┐  │  ┌────────────────┐
                  │ │Use REST │  │  │ Write Python   │
                  │ └─────────┘  │  │ for /python/   │
                  │              │  │ execute        │
                  │              │  └───────┬────────┘
                  │              │          ▼
                  │              │  ┌───────────────────────┐
                  │              │  │ Evaluate for API add: │
                  │              │  │ - Frequent?           │
                  │              │  │ - Complex?            │
                  │              │  │ - Perf-sensitive?     │
                  │              │  └───────────────────────┘
                  │              │   ALL YES -> Suggest API
                  │              │   ANY NO  -> Python is fine
```

## Batch Decision

When to use `POST /batch`:
- Multiple similar operations → Use batch
- Mixed operations with dependencies → Use batch with variable refs ($0.node.id)
- Complex conditional logic → Use Python execution instead

## Tier Summary

| Tier | When to Use | Examples |
|------|-------------|----------|
| 1: REST Endpoint | Endpoint exists in docs or schema | Spawn actor, list assets, create BP node |
| 2: Python Fallback | No endpoint, operation is custom/rare | Custom asset filters, widget manipulation |
| 3: API Suggestion | Frequent + Complex + Perf-sensitive | Suggest new endpoint if all 3 criteria met |

## API Suggestion Criteria

All three criteria must be met:

### 1. Frequency
- Same operation requested 3+ times in conversation
- OR common pattern seen across sessions
- OR user says "I do this often"

### 2. Complexity
- Python solution requires 10+ lines
- OR requires multiple API calls
- OR involves complex error handling
- OR needs thread context switching

### 3. Performance
- Operation is in loop/batch context
- OR latency-sensitive (interactive workflow)
- OR processes large data sets

### Suggestion Format
```markdown
**API Enhancement Suggestion**
- Endpoint: `POST /animations/retarget`
- Purpose: Retarget animation from source to target skeleton
- Why: Requested 4x, requires 15-line Python with thread switch, batch usage
- Priority: Medium
```
