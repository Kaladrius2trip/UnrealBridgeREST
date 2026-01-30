# UnrealPythonREST API Reference

**Version:** 1.0
**Base URL:** `http://localhost:{port}/api/v1`

This document provides complete reference for the UnrealPythonREST plugin HTTP API.

## Table of Contents

- [Discovery](#discovery)
- [Core Endpoints](#core-endpoints)
  - [GET /health](#get-health)
  - [GET /handlers](#get-handlers)
- [Python Handler](#python-handler)
  - [POST /python/execute](#post-pythonexecute)
  - [GET /python/jobs](#get-pythonjobs)
  - [GET /python/job](#get-pythonjob)
  - [DELETE /python/job](#delete-pythonjob)
- [Error Handling](#error-handling)
  - [Error Response Format](#error-response-format)
  - [Error Codes](#error-codes)
  - [HTTP Status Codes](#http-status-codes)

---

## Discovery

Before connecting to the REST API, clients must discover the server port and verify it's running.

### Config File Location

```
{ProjectDir}/Saved/UnrealPythonREST.json
```

Example paths:
- Windows: `C:\Users\yevhe\Documents\Unreal Projects\MyGame\Saved\UnrealPythonREST.json`
- macOS: `/Users/yevhe/Documents/Unreal Projects/MyGame/Saved/UnrealPythonREST.json`

### Config File Format

```json
{
  "version": 1,
  "port": 8080,
  "pid": 12345,
  "project": "MyGame",
  "started_at": "2026-01-24T12:00:00Z",
  "handlers": ["python"]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `version` | integer | Config file format version (currently 1) |
| `port` | integer | HTTP server port (1024-65535) |
| `pid` | integer | Unreal Editor process ID |
| `project` | string | Project name |
| `started_at` | string | ISO 8601 timestamp when server started |
| `handlers` | string[] | List of registered handler names |

### Discovery Workflow

1. **Locate config file** in `{ProjectDir}/Saved/UnrealPythonREST.json`
2. **Parse JSON** to get port and PID
3. **Verify process** is still running (optional but recommended)
4. **Call /health** endpoint to confirm server is responsive
5. **Begin API calls** using discovered port

### Discovery Example (Python)

```python
import json
import os
import requests

def discover_server(project_dir: str) -> dict | None:
    """Discover UnrealPythonREST server for a project."""
    config_path = os.path.join(project_dir, "Saved", "UnrealPythonREST.json")

    if not os.path.exists(config_path):
        return None

    with open(config_path, "r") as f:
        config = json.load(f)

    # Verify server is healthy
    try:
        response = requests.get(
            f"http://localhost:{config['port']}/api/v1/health",
            timeout=5
        )
        if response.status_code == 200:
            return config
    except requests.RequestException:
        pass

    return None
```

---

## Core Endpoints

### GET /health

Check server health and get basic status information.

**Request:**

```bash
curl http://localhost:8080/api/v1/health
```

**Response:**

```json
{
  "success": true,
  "status": "healthy",
  "uptime_seconds": 123,
  "handlers": ["python"]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | Always `true` for healthy server |
| `status` | string | Server status: `"healthy"` |
| `uptime_seconds` | integer | Seconds since server started |
| `handlers` | string[] | List of registered handler names |

**Use Cases:**
- Verify server is running after discovery
- Monitor server uptime
- Check available handlers

---

### GET /handlers

List all registered handlers with their metadata.

**Request:**

```bash
curl http://localhost:8080/api/v1/handlers
```

**Response:**

```json
{
  "success": true,
  "handlers": [
    {
      "name": "python",
      "base_path": "/python",
      "description": "Execute Python code in Unreal Editor context",
      "endpoints": [
        {"method": "POST", "path": "/execute", "description": "Execute Python code"},
        {"method": "GET", "path": "/jobs", "description": "List all jobs"},
        {"method": "GET", "path": "/job", "description": "Get job details"},
        {"method": "DELETE", "path": "/job", "description": "Cancel job"}
      ]
    }
  ]
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | Request success status |
| `handlers` | object[] | Array of handler definitions |
| `handlers[].name` | string | Handler identifier |
| `handlers[].base_path` | string | URL path prefix for handler |
| `handlers[].description` | string | Human-readable handler description |
| `handlers[].endpoints` | object[] | Available endpoints in this handler |

---

## Python Handler

The Python handler executes Python code within the Unreal Editor context with full access to the `unreal` module.

### POST /python/execute

Execute Python code and return results.

**Request:**

```bash
curl -X POST http://localhost:8080/api/v1/python/execute \
  -H "Content-Type: application/json" \
  -d '{
    "code": "import unreal\nprint(unreal.SystemLibrary.get_project_directory())",
    "timeout": 30
  }'
```

**Request Body:**

```json
{
  "code": "import unreal\nprint('Hello from Unreal!')",
  "timeout": 30
}
```

| Field | Type | Required | Default | Description |
|-------|------|----------|---------|-------------|
| `code` | string | **Yes** | - | Python code to execute |
| `timeout` | integer | No | 30 | Maximum execution time in seconds (1-300) |

**Response (Success):**

```json
{
  "success": true,
  "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "output": "/Game/MyProject/",
  "logs": [
    {"level": "info", "message": "Hello from Unreal!", "timestamp": "2026-01-24T12:00:01Z"}
  ],
  "duration_ms": 45
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | `true` if code executed without errors |
| `job_id` | string | Unique job identifier (UUID) |
| `output` | string | Captured stdout/print output |
| `logs` | object[] | Log entries from `unreal.log()`, `unreal.log_warning()`, etc. |
| `logs[].level` | string | Log level: `"info"`, `"warning"`, `"error"` |
| `logs[].message` | string | Log message content |
| `logs[].timestamp` | string | ISO 8601 timestamp |
| `duration_ms` | integer | Execution time in milliseconds |

**Response (Error):**

```json
{
  "success": false,
  "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "error": {
    "code": "PYTHON_RUNTIME_ERROR",
    "message": "name 'undefined_var' is not defined",
    "traceback": "Traceback (most recent call last):\n  File \"<string>\", line 1, in <module>\nNameError: name 'undefined_var' is not defined"
  },
  "duration_ms": 12
}
```

**Example - Execute with unreal module:**

```bash
curl -X POST http://localhost:8080/api/v1/python/execute \
  -H "Content-Type: application/json" \
  -d '{
    "code": "import unreal\nassets = unreal.EditorAssetLibrary.list_assets(\"/Game\")\nfor a in assets[:5]:\n    print(a)"
  }'
```

**Example - Multi-line code:**

```bash
curl -X POST http://localhost:8080/api/v1/python/execute \
  -H "Content-Type: application/json" \
  -d '{
    "code": "import unreal\n\ndef get_actor_count():\n    actors = unreal.EditorLevelLibrary.get_all_level_actors()\n    return len(actors)\n\ncount = get_actor_count()\nprint(f\"Total actors: {count}\")"
  }'
```

---

### GET /python/jobs

List all jobs (running, completed, and failed).

**Request:**

```bash
curl http://localhost:8080/api/v1/python/jobs
```

**Query Parameters:**

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `status` | string | all | Filter by status: `"pending"`, `"running"`, `"completed"`, `"failed"`, `"cancelled"` |
| `limit` | integer | 100 | Maximum jobs to return (1-1000) |
| `offset` | integer | 0 | Skip first N jobs for pagination |

**Example with filters:**

```bash
curl "http://localhost:8080/api/v1/python/jobs?status=completed&limit=10"
```

**Response:**

```json
{
  "success": true,
  "jobs": [
    {
      "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "status": "completed",
      "created_at": "2026-01-24T12:00:00Z",
      "completed_at": "2026-01-24T12:00:01Z",
      "duration_ms": 45
    },
    {
      "job_id": "b2c3d4e5-f6a7-8901-bcde-f23456789012",
      "status": "running",
      "created_at": "2026-01-24T12:00:05Z",
      "completed_at": null,
      "duration_ms": null
    }
  ],
  "total": 42,
  "limit": 100,
  "offset": 0
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | Request success status |
| `jobs` | object[] | Array of job summaries |
| `jobs[].job_id` | string | Unique job identifier |
| `jobs[].status` | string | Job status (see below) |
| `jobs[].created_at` | string | Job creation timestamp |
| `jobs[].completed_at` | string \| null | Completion timestamp (null if not finished) |
| `jobs[].duration_ms` | integer \| null | Execution time (null if not finished) |
| `total` | integer | Total jobs matching filter |
| `limit` | integer | Applied limit |
| `offset` | integer | Applied offset |

**Job Statuses:**

| Status | Description |
|--------|-------------|
| `pending` | Job queued, waiting to execute |
| `running` | Job currently executing |
| `completed` | Job finished successfully |
| `failed` | Job finished with error |
| `cancelled` | Job was cancelled |

---

### GET /python/job

Get detailed information about a specific job.

**Request:**

```bash
curl "http://localhost:8080/api/v1/python/job?id=a1b2c3d4-e5f6-7890-abcd-ef1234567890"
```

**Query Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | **Yes** | Job ID (UUID) |

**Response (Completed Job):**

```json
{
  "success": true,
  "job": {
    "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
    "status": "completed",
    "code": "import unreal\nprint('Hello!')",
    "created_at": "2026-01-24T12:00:00Z",
    "started_at": "2026-01-24T12:00:00Z",
    "completed_at": "2026-01-24T12:00:01Z",
    "duration_ms": 45,
    "output": "Hello!",
    "logs": [
      {"level": "info", "message": "Hello!", "timestamp": "2026-01-24T12:00:01Z"}
    ],
    "error": null
  }
}
```

**Response (Failed Job):**

```json
{
  "success": true,
  "job": {
    "job_id": "b2c3d4e5-f6a7-8901-bcde-f23456789012",
    "status": "failed",
    "code": "import unreal\nundefined_var",
    "created_at": "2026-01-24T12:00:00Z",
    "started_at": "2026-01-24T12:00:00Z",
    "completed_at": "2026-01-24T12:00:00Z",
    "duration_ms": 12,
    "output": "",
    "logs": [],
    "error": {
      "code": "PYTHON_RUNTIME_ERROR",
      "message": "name 'undefined_var' is not defined",
      "traceback": "Traceback (most recent call last):\n  File \"<string>\", line 2, in <module>\nNameError: name 'undefined_var' is not defined"
    }
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `job.job_id` | string | Unique job identifier |
| `job.status` | string | Current job status |
| `job.code` | string | Submitted Python code |
| `job.created_at` | string | When job was created |
| `job.started_at` | string \| null | When execution started |
| `job.completed_at` | string \| null | When execution finished |
| `job.duration_ms` | integer \| null | Total execution time |
| `job.output` | string | Captured stdout |
| `job.logs` | object[] | Captured log entries |
| `job.error` | object \| null | Error details (if failed) |

**Response (Job Not Found):**

```json
{
  "success": false,
  "error": {
    "code": "JOB_NOT_FOUND",
    "message": "Job with ID 'invalid-id' not found"
  }
}
```

---

### DELETE /python/job

Cancel a running job or remove a completed job from history.

**Request:**

```bash
curl -X DELETE "http://localhost:8080/api/v1/python/job?id=a1b2c3d4-e5f6-7890-abcd-ef1234567890"
```

**Query Parameters:**

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `id` | string | **Yes** | Job ID (UUID) |

**Response (Success - Running Job Cancelled):**

```json
{
  "success": true,
  "message": "Job cancelled",
  "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "previous_status": "running"
}
```

**Response (Success - Completed Job Removed):**

```json
{
  "success": true,
  "message": "Job removed from history",
  "job_id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
  "previous_status": "completed"
}
```

**Response (Error - Job Not Found):**

```json
{
  "success": false,
  "error": {
    "code": "JOB_NOT_FOUND",
    "message": "Job with ID 'invalid-id' not found"
  }
}
```

---

## Error Handling

### Error Response Format

All error responses follow this consistent format:

```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human-readable error description",
    "details": {}
  }
}
```

| Field | Type | Description |
|-------|------|-------------|
| `success` | boolean | Always `false` for errors |
| `error.code` | string | Machine-readable error code |
| `error.message` | string | Human-readable error description |
| `error.details` | object | Additional error context (optional) |

For Python execution errors, additional fields are included:

```json
{
  "success": false,
  "error": {
    "code": "PYTHON_RUNTIME_ERROR",
    "message": "name 'undefined_var' is not defined",
    "traceback": "Traceback (most recent call last):\n  File \"<string>\", line 1, in <module>\nNameError: name 'undefined_var' is not defined",
    "line": 1
  }
}
```

### Error Codes

| Code | HTTP Status | Description |
|------|-------------|-------------|
| `MISSING_CODE` | 400 | Request body missing required `code` field |
| `INVALID_CODE` | 400 | Code field is empty or not a string |
| `INVALID_TIMEOUT` | 400 | Timeout value out of range (1-300) |
| `INVALID_JSON` | 400 | Request body is not valid JSON |
| `PYTHON_SYNTAX_ERROR` | 400 | Python code has syntax errors |
| `PYTHON_RUNTIME_ERROR` | 500 | Python code raised an exception |
| `TIMEOUT` | 504 | Code execution exceeded timeout |
| `JOB_NOT_FOUND` | 404 | Requested job ID does not exist |
| `JOB_ALREADY_FINISHED` | 400 | Cannot cancel job that already finished |
| `INVALID_JOB_ID` | 400 | Job ID format is invalid |
| `SERVER_ERROR` | 500 | Internal server error |
| `HANDLER_NOT_FOUND` | 404 | Requested handler does not exist |
| `METHOD_NOT_ALLOWED` | 405 | HTTP method not supported for endpoint |

### HTTP Status Codes

| Status | Meaning | When Used |
|--------|---------|-----------|
| 200 | OK | Successful request |
| 400 | Bad Request | Invalid request parameters or body |
| 404 | Not Found | Job or handler not found |
| 405 | Method Not Allowed | Wrong HTTP method for endpoint |
| 500 | Internal Server Error | Server error or Python runtime error |
| 504 | Gateway Timeout | Execution timeout exceeded |

### Error Handling Example (Python)

```python
import requests

def execute_python(port: int, code: str) -> dict:
    """Execute Python code with error handling."""
    try:
        response = requests.post(
            f"http://localhost:{port}/api/v1/python/execute",
            json={"code": code, "timeout": 30},
            timeout=35  # Slightly longer than execution timeout
        )

        data = response.json()

        if not data["success"]:
            error = data["error"]
            if error["code"] == "PYTHON_SYNTAX_ERROR":
                print(f"Syntax error: {error['message']}")
            elif error["code"] == "PYTHON_RUNTIME_ERROR":
                print(f"Runtime error: {error['message']}")
                print(f"Traceback:\n{error.get('traceback', '')}")
            elif error["code"] == "TIMEOUT":
                print("Execution timed out")
            else:
                print(f"Error [{error['code']}]: {error['message']}")
            return None

        return data

    except requests.Timeout:
        print("Request timed out")
        return None
    except requests.ConnectionError:
        print("Cannot connect to server")
        return None
    except requests.JSONDecodeError:
        print("Invalid response from server")
        return None
```

---

## Request Headers

All requests should include:

| Header | Value | Required |
|--------|-------|----------|
| `Content-Type` | `application/json` | Yes (for POST) |
| `Accept` | `application/json` | Recommended |

---

## Rate Limiting

The server does not implement rate limiting. However, Python execution is single-threaded within Unreal Engine, so concurrent execution requests will queue.

For best performance:
- Avoid submitting many rapid requests
- Use reasonable timeouts
- Poll job status instead of blocking on long operations

---

## Security Considerations

**Important:** The REST API provides full Python execution within Unreal Editor context. This includes:

- Full file system access
- Ability to modify assets
- Access to all editor functionality
- No authentication by default

**Recommendations:**
- Only bind to localhost (default behavior)
- Do not expose port to network
- Validate code before execution in automated pipelines
- Use in trusted development environments only
