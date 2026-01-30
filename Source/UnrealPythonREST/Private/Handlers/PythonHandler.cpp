// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handlers/PythonHandler.h"
#include "RESTRouter.h"
#include "IPythonScriptPlugin.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

void FPythonHandler::RegisterRoutes(FRESTRouter& Router)
{
	// POST /python/execute - Synchronous Python execution
	Router.RegisterRoute(ERESTMethod::POST, TEXT("/python/execute"),
		FRESTRouteHandler::CreateRaw(this, &FPythonHandler::HandleExecute));

	// GET /python/jobs - List all jobs
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/python/jobs"),
		FRESTRouteHandler::CreateRaw(this, &FPythonHandler::HandleListJobs));

	// GET /python/jobs/{id} - Get specific job (handled via query param for now)
	// Note: Path parameters require custom parsing; use query param ?id=xxx
	Router.RegisterRoute(ERESTMethod::GET, TEXT("/python/job"),
		FRESTRouteHandler::CreateLambda([this](const FRESTRequest& Request) -> FRESTResponse
		{
			const FString* JobIdPtr = Request.QueryParams.Find(TEXT("id"));
			if (!JobIdPtr || JobIdPtr->IsEmpty())
			{
				return FRESTResponse::BadRequest(TEXT("Missing required query parameter: id"));
			}
			return HandleGetJob(Request, *JobIdPtr);
		}));

	// DELETE /python/job?id={id} - Cancel a job
	Router.RegisterRoute(ERESTMethod::DELETE, TEXT("/python/job"),
		FRESTRouteHandler::CreateLambda([this](const FRESTRequest& Request) -> FRESTResponse
		{
			const FString* JobIdPtr = Request.QueryParams.Find(TEXT("id"));
			if (!JobIdPtr || JobIdPtr->IsEmpty())
			{
				return FRESTResponse::BadRequest(TEXT("Missing required query parameter: id"));
			}
			return HandleCancelJob(Request, *JobIdPtr);
		}));

	UE_LOG(LogTemp, Log, TEXT("PythonHandler: Registered routes at /python"));
}

FRESTResponse FPythonHandler::HandleExecute(const FRESTRequest& Request)
{
	// Validate JSON body exists
	if (!Request.JsonBody.IsValid())
	{
		return FRESTResponse::BadRequest(TEXT("Request body must be valid JSON"));
	}

	// Get required "code" field
	FString Code;
	if (!Request.JsonBody->TryGetStringField(TEXT("code"), Code) || Code.IsEmpty())
	{
		return FRESTResponse::BadRequest(TEXT("Missing required field: code"));
	}

	// Get optional timeout (default 30 seconds)
	int32 TimeoutSeconds = 30;
	if (Request.JsonBody->HasField(TEXT("timeout")))
	{
		TimeoutSeconds = static_cast<int32>(Request.JsonBody->GetNumberField(TEXT("timeout")));
		if (TimeoutSeconds <= 0)
		{
			TimeoutSeconds = 30;
		}
	}

	// Get optional async flag (for future use, currently always sync)
	bool bAsync = false;
	if (Request.JsonBody->HasField(TEXT("async")))
	{
		bAsync = Request.JsonBody->GetBoolField(TEXT("async"));
	}

	// Record start time
	FDateTime StartTime = FDateTime::UtcNow();

	// Execute the Python code
	FString Output = ExecutePythonCode(Code);

	// Calculate duration
	FDateTime EndTime = FDateTime::UtcNow();
	double DurationMs = (EndTime - StartTime).GetTotalMilliseconds();

	// Create a job record for tracking
	FPythonJob Job;
	Job.JobId = GenerateJobId();
	Job.Code = Code;
	Job.Status = EPythonJobStatus::Completed;
	Job.Output = Output;
	Job.StartTime = StartTime;
	Job.EndTime = EndTime;
	Job.TimeoutSeconds = TimeoutSeconds;

	// Store job for later retrieval
	{
		FScopeLock Lock(&JobsLock);
		Jobs.Add(Job.JobId, Job);
		CleanupOldJobs();
	}

	// Build success response
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetBoolField(TEXT("success"), true);
	ResponseJson->SetStringField(TEXT("job_id"), Job.JobId);
	ResponseJson->SetStringField(TEXT("output"), Output);

	// Empty logs array (Python output goes to UE log, not captured directly)
	TArray<TSharedPtr<FJsonValue>> LogsArray;
	ResponseJson->SetArrayField(TEXT("logs"), LogsArray);

	ResponseJson->SetNumberField(TEXT("duration_ms"), DurationMs);

	return FRESTResponse::Ok(ResponseJson);
}

FString FPythonHandler::ExecutePythonCode(const FString& Code)
{
	// Get the Python script plugin
	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin)
	{
		UE_LOG(LogTemp, Error, TEXT("PythonHandler: IPythonScriptPlugin not available"));
		return TEXT("");
	}

	// Execute the Python command
	// Note: ExecPythonCommand returns bool but output goes to log
	// We cannot capture stdout directly with the sync API
	bool bSuccess = PythonPlugin->ExecPythonCommand(*Code);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PythonHandler: Python execution returned false"));
	}

	// Output is captured via log - return empty string as limitation of sync execution
	// Future async implementation could capture output via custom log handler
	return TEXT("");
}

FRESTResponse FPythonHandler::HandleListJobs(const FRESTRequest& Request)
{
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetBoolField(TEXT("success"), true);

	TArray<TSharedPtr<FJsonValue>> JobsArray;

	{
		FScopeLock Lock(&JobsLock);

		for (const auto& Pair : Jobs)
		{
			const FPythonJob& Job = Pair.Value;

			TSharedPtr<FJsonObject> JobJson = MakeShared<FJsonObject>();
			JobJson->SetStringField(TEXT("id"), Job.JobId);

			// Convert status enum to string
			FString StatusString;
			switch (Job.Status)
			{
			case EPythonJobStatus::Pending:
				StatusString = TEXT("pending");
				break;
			case EPythonJobStatus::Running:
				StatusString = TEXT("running");
				break;
			case EPythonJobStatus::Completed:
				StatusString = TEXT("completed");
				break;
			case EPythonJobStatus::Failed:
				StatusString = TEXT("failed");
				break;
			case EPythonJobStatus::Cancelled:
				StatusString = TEXT("cancelled");
				break;
			}
			JobJson->SetStringField(TEXT("status"), StatusString);

			// Format start time as ISO 8601
			JobJson->SetStringField(TEXT("started_at"), Job.StartTime.ToIso8601());

			JobsArray.Add(MakeShared<FJsonValueObject>(JobJson));
		}
	}

	ResponseJson->SetArrayField(TEXT("jobs"), JobsArray);

	return FRESTResponse::Ok(ResponseJson);
}

FRESTResponse FPythonHandler::HandleGetJob(const FRESTRequest& Request, const FString& JobId)
{
	FScopeLock Lock(&JobsLock);

	const FPythonJob* JobPtr = Jobs.Find(JobId);
	if (!JobPtr)
	{
		return FRESTResponse::Error(404, TEXT("JOB_NOT_FOUND"),
			FString::Printf(TEXT("Job with ID '%s' not found"), *JobId));
	}

	const FPythonJob& Job = *JobPtr;

	// Build detailed job response
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetBoolField(TEXT("success"), true);

	TSharedPtr<FJsonObject> JobJson = MakeShared<FJsonObject>();
	JobJson->SetStringField(TEXT("id"), Job.JobId);

	// Convert status enum to string
	FString StatusString;
	switch (Job.Status)
	{
	case EPythonJobStatus::Pending:
		StatusString = TEXT("pending");
		break;
	case EPythonJobStatus::Running:
		StatusString = TEXT("running");
		break;
	case EPythonJobStatus::Completed:
		StatusString = TEXT("completed");
		break;
	case EPythonJobStatus::Failed:
		StatusString = TEXT("failed");
		break;
	case EPythonJobStatus::Cancelled:
		StatusString = TEXT("cancelled");
		break;
	}
	JobJson->SetStringField(TEXT("status"), StatusString);

	JobJson->SetStringField(TEXT("output"), Job.Output);

	if (!Job.Error.IsEmpty())
	{
		JobJson->SetStringField(TEXT("error"), Job.Error);
	}

	// Logs array
	TArray<TSharedPtr<FJsonValue>> LogsArray;
	for (const FString& LogEntry : Job.Logs)
	{
		LogsArray.Add(MakeShared<FJsonValueString>(LogEntry));
	}
	JobJson->SetArrayField(TEXT("logs"), LogsArray);

	// Duration in milliseconds
	double DurationMs = Job.GetDurationSeconds() * 1000.0;
	JobJson->SetNumberField(TEXT("duration_ms"), DurationMs);

	// Timestamps
	JobJson->SetStringField(TEXT("started_at"), Job.StartTime.ToIso8601());
	if (Job.IsFinished())
	{
		JobJson->SetStringField(TEXT("ended_at"), Job.EndTime.ToIso8601());
	}

	ResponseJson->SetObjectField(TEXT("job"), JobJson);

	return FRESTResponse::Ok(ResponseJson);
}

FRESTResponse FPythonHandler::HandleCancelJob(const FRESTRequest& Request, const FString& JobId)
{
	FScopeLock Lock(&JobsLock);

	FPythonJob* JobPtr = Jobs.Find(JobId);
	if (!JobPtr)
	{
		return FRESTResponse::Error(404, TEXT("JOB_NOT_FOUND"),
			FString::Printf(TEXT("Job with ID '%s' not found"), *JobId));
	}

	FPythonJob& Job = *JobPtr;

	// Check if job is already finished
	if (Job.IsFinished())
	{
		FString StatusString;
		switch (Job.Status)
		{
		case EPythonJobStatus::Completed:
			StatusString = TEXT("completed");
			break;
		case EPythonJobStatus::Failed:
			StatusString = TEXT("failed");
			break;
		case EPythonJobStatus::Cancelled:
			StatusString = TEXT("cancelled");
			break;
		default:
			StatusString = TEXT("unknown");
			break;
		}

		return FRESTResponse::Error(400, TEXT("JOB_ALREADY_FINISHED"),
			FString::Printf(TEXT("Job '%s' is already %s and cannot be cancelled"), *JobId, *StatusString));
	}

	// Cancel the job
	Job.Status = EPythonJobStatus::Cancelled;
	Job.EndTime = FDateTime::UtcNow();

	// Build success response
	TSharedPtr<FJsonObject> ResponseJson = MakeShared<FJsonObject>();
	ResponseJson->SetBoolField(TEXT("success"), true);
	ResponseJson->SetStringField(TEXT("message"), FString::Printf(TEXT("Job '%s' has been cancelled"), *JobId));
	ResponseJson->SetStringField(TEXT("job_id"), JobId);

	return FRESTResponse::Ok(ResponseJson);
}

FString FPythonHandler::GenerateJobId()
{
	return FGuid::NewGuid().ToString();
}

void FPythonHandler::CleanupOldJobs()
{
	// Called with JobsLock already held

	FDateTime Now = FDateTime::UtcNow();
	FTimespan ExpirationTime = FTimespan::FromHours(JobExpirationHours);

	// Collect keys to remove
	TArray<FString> KeysToRemove;

	for (const auto& Pair : Jobs)
	{
		const FPythonJob& Job = Pair.Value;

		// Only clean up finished jobs
		if (Job.IsFinished())
		{
			FTimespan Age = Now - Job.EndTime;
			if (Age > ExpirationTime)
			{
				KeysToRemove.Add(Pair.Key);
			}
		}
	}

	// Remove expired jobs
	for (const FString& Key : KeysToRemove)
	{
		Jobs.Remove(Key);
	}

	// If still over MaxJobs, remove oldest finished jobs
	while (Jobs.Num() > MaxJobs)
	{
		// Find oldest finished job
		FString OldestKey;
		FDateTime OldestTime = FDateTime::MaxValue();

		for (const auto& Pair : Jobs)
		{
			const FPythonJob& Job = Pair.Value;
			if (Job.IsFinished() && Job.EndTime < OldestTime)
			{
				OldestTime = Job.EndTime;
				OldestKey = Pair.Key;
			}
		}

		if (OldestKey.IsEmpty())
		{
			// No more finished jobs to remove
			break;
		}

		Jobs.Remove(OldestKey);
	}
}

void FPythonHandler::Shutdown()
{
	FScopeLock Lock(&JobsLock);
	Jobs.Empty();
	UE_LOG(LogTemp, Log, TEXT("PythonHandler: Shutdown complete"));
}

TArray<TSharedPtr<FJsonObject>> FPythonHandler::GetEndpointSchemas() const
{
	TArray<TSharedPtr<FJsonObject>> Schemas;

	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("POST")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/python/execute")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Execute Python code synchronously (body: code, timeout, async)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/python/jobs")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("List all Python execution jobs"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("GET")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/python/job")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Get job status and result (query: id)"));
	
	Schemas.Add(MakeShared<FJsonObject>()); Schemas.Last()->SetStringField(TEXT("method"), TEXT("DELETE")); Schemas.Last()->SetStringField(TEXT("path"), TEXT("/python/job")); Schemas.Last()->SetStringField(TEXT("description"), TEXT("Cancel a Python job (query: id)"));

	return Schemas;
}
