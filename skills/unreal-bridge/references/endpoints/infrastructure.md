# Infrastructure Endpoints

Base path: `/api/v1`

## GET /health

Health check and server info.

**Parameters:** None

**Response:**
```json
{
  "healthy": true,
  "status": "running",
  "server": {"name": "UnrealPythonREST", "version": "2.0.0"},
  "engine": {"version": "5.7.2", "project": "MyProject"},
  "units": {"distance": "centimeters", "rotation": "degrees", "scale": "multiplier"},
  "coordinate_system": {"handedness": "left-handed", "x": "forward", "y": "right", "z": "up"}
}
```

**Status Codes:**
- `200` - Server is healthy and operational
- `500` - Server error (engine shutdown, critical failure)

**Error Cases:**
- Server is not initialized
- Engine is not responsive
- Critical plugin error

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/health"
```

---

## GET /schema

Get API schema at various granularity levels.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| handler | string | No | Filter to specific handler |
| endpoint | string | No | Filter to specific endpoint path |

**Response Example:**
```json
{
  "handlers": {
    "materials": {
      "endpoints": [
        {
          "path": "/materials/editor/node/create",
          "method": "POST",
          "description": "Create a new node"
        }
      ]
    }
  }
}
```

**Status Codes:**
- `200` - Schema retrieved successfully
- `400` - Invalid handler or endpoint parameter
- `500` - Server error while generating schema

**Examples:**

```bash
# Full schema
curl -s "http://localhost:$PORT/api/v1/schema"

# Handler schema
curl -s "http://localhost:$PORT/api/v1/schema?handler=materials"

# Endpoint schema
curl -s "http://localhost:$PORT/api/v1/schema?endpoint=/materials/editor/node/create"
```

---

## POST /batch

Execute multiple requests in a single call.

**Request Body:**
```json
{
  "requests": [
    {"method": "GET|POST|PUT|DELETE", "path": "/endpoint", "body": {}},
    ...
  ],
  "options": {
    "stop_on_error": true
  }
}
```

**Variable References:**
- `$0`, `$1`, etc. - Reference result from request at index
- `$0.field.nested` - Extract nested field from result

**Response:**
```json
{
  "success": true,
  "results": [
    {"index": 0, "success": true, "data": {...}},
    {"index": 1, "success": true, "data": {...}}
  ],
  "completed": 2,
  "failed": 0
}
```

**Status Codes:**
- `200` - Batch executed (check individual result success flags)
- `400` - Invalid batch request format
- `500` - Server error executing batch

**Errors:**
- `MISSING_REQUESTS` - requests array not provided
- `INVALID_REQUEST` - malformed request in array

**Error Response Format:**
```json
{
  "success": false,
  "error": "ERROR_CODE",
  "message": "Human readable description of the error"
}
```
