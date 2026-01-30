# Python Endpoints

Base path: `/api/v1/python`

## POST /execute

Execute Python code synchronously in the Unreal Editor. The code runs on the game thread and blocks until completion.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| code | string | Yes | Python code to execute |
| timeout | number | No | Timeout in seconds (default: 30) |
| async | boolean | No | Execute asynchronously (currently not implemented, always runs sync) |

**Request:**
```json
{
  "code": "import unreal\nprint('Hello from Python!')",
  "timeout": 30,
  "async": false
}
```

**Response:**
```json
{
  "success": true,
  "job_id": "550e8400-e29b-41d4-a716-446655440000",
  "output": "",
  "logs": [],
  "duration_ms": 123.45
}
```

**Status Codes:**
- `200` - Python code executed successfully
- `400` - Invalid request (missing code field, invalid timeout)
- `500` - Server error during execution

**Errors:**
- `INVALID_REQUEST` - Request body must be valid JSON
- `MISSING_FIELD` - Missing required field: code

**Notes:**
- Python output goes to UE log, not captured in the `output` field (current limitation)
- All execution is synchronous regardless of `async` flag
- A job record is created even for sync execution for tracking purposes
- Jobs are automatically cleaned up after 1 hour

**curl:**
```bash
curl -X POST "http://localhost:$PORT/api/v1/python/execute" \
  -H "Content-Type: application/json" \
  -d '{
    "code": "import unreal\nprint(\"Hello from Python!\")",
    "timeout": 30
  }'
```

---

## GET /jobs

List all Python execution jobs (pending, running, completed, failed, cancelled).

**Parameters:** None

**Response:**
```json
{
  "success": true,
  "jobs": [
    {
      "id": "550e8400-e29b-41d4-a716-446655440000",
      "status": "completed",
      "started_at": "2026-01-25T10:30:00.000Z"
    },
    {
      "id": "660e8400-e29b-41d4-a716-446655440001",
      "status": "running",
      "started_at": "2026-01-25T10:31:00.000Z"
    }
  ]
}
```

**Status Values:**
- `pending` - Job queued but not started
- `running` - Currently executing
- `completed` - Finished successfully
- `failed` - Execution failed with error
- `cancelled` - Job was cancelled

**Status Codes:**
- `200` - Jobs list retrieved successfully
- `500` - Server error retrieving jobs

**Notes:**
- Jobs older than 1 hour are automatically removed
- Maximum of 100 jobs kept in memory

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/python/jobs"
```

---

## GET /job?id={id}

Get detailed information about a specific Python job, including output and logs.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| id | string | Yes | Job ID (UUID) returned from /execute |

**Response:**
```json
{
  "success": true,
  "job": {
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "status": "completed",
    "output": "",
    "logs": [
      "Execution started",
      "Script completed"
    ],
    "duration_ms": 123.45,
    "started_at": "2026-01-25T10:30:00.000Z",
    "ended_at": "2026-01-25T10:30:00.123Z"
  }
}
```

**Failed Job Response:**
```json
{
  "success": true,
  "job": {
    "id": "550e8400-e29b-41d4-a716-446655440000",
    "status": "failed",
    "output": "",
    "error": "SyntaxError: invalid syntax",
    "logs": [],
    "duration_ms": 50.0,
    "started_at": "2026-01-25T10:30:00.000Z",
    "ended_at": "2026-01-25T10:30:00.050Z"
  }
}
```

**Status Codes:**
- `200` - Job details retrieved successfully
- `404` - Job not found
- `500` - Server error retrieving job

**Errors:**
- `MISSING_PARAMETER` - Missing required query parameter: id
- `JOB_NOT_FOUND` - Job with ID 'xxx' not found

**Notes:**
- `ended_at` only present for finished jobs (completed, failed, cancelled)
- `error` field only present when status is `failed`

**curl:**
```bash
curl -s "http://localhost:$PORT/api/v1/python/job?id=550e8400-e29b-41d4-a716-446655440000"
```

---

## DELETE /job?id={id}

Cancel a pending or running Python job.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| id | string | Yes | Job ID (UUID) to cancel |

**Response:**
```json
{
  "success": true,
  "message": "Job '550e8400-e29b-41d4-a716-446655440000' has been cancelled",
  "job_id": "550e8400-e29b-41d4-a716-446655440000"
}
```

**Status Codes:**
- `200` - Job cancelled successfully
- `400` - Job already finished
- `404` - Job not found
- `500` - Server error cancelling job

**Errors:**
- `MISSING_PARAMETER` - Missing required query parameter: id
- `JOB_NOT_FOUND` - Job with ID 'xxx' not found
- `JOB_ALREADY_FINISHED` - Job 'xxx' is already completed/failed/cancelled and cannot be cancelled

**Notes:**
- Cannot cancel jobs that are already completed, failed, or cancelled
- Cancellation is immediate (job status updated to `cancelled`)
- Currently, since all execution is synchronous, cancellation typically only works on pending jobs

**curl:**
```bash
curl -X DELETE "http://localhost:$PORT/api/v1/python/job?id=550e8400-e29b-41d4-a716-446655440000"
```

---

## Threading Considerations

**Current Implementation (Synchronous Only):**
- All Python execution runs synchronously on the game thread
- The `async` parameter is accepted but currently ignored
- Execution blocks the request until Python code completes
- Output capture is limited (stdout goes to UE log, not returned in API)

**Job Management:**
- Every execution creates a job record for tracking
- Jobs track timing, status, and metadata
- Automatic cleanup after 1 hour or when exceeding 100 jobs
- Thread-safe access via critical section locks

**Future Async Support:**
- The infrastructure is designed to support true async execution
- Async execution would return job_id immediately
- Clients would poll `/job?id={id}` to check status
- Output could be captured via custom log handler

**Best Practices:**
- Set appropriate timeout values for long-running scripts
- Poll `/jobs` to track multiple executions
- Use `/job?id={id}` to retrieve detailed results
- Clean up finished jobs if running many executions
