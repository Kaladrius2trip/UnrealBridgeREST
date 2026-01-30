// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IRESTHandler.h"
#include "HAL/CriticalSection.h"

// Forward declarations
class FRESTRouter;
struct FRESTRequest;
struct FRESTResponse;

/**
 * Status of an async Python job.
 */
UENUM()
enum class EPythonJobStatus : uint8
{
	Pending,
	Running,
	Completed,
	Failed,
	Cancelled
};

/**
 * Represents an async Python execution job.
 * Tracks the state, output, and timing of Python code execution.
 */
struct FPythonJob
{
	/** Unique identifier for this job */
	FString JobId;

	/** Python code to execute */
	FString Code;

	/** Current status of the job */
	EPythonJobStatus Status = EPythonJobStatus::Pending;

	/** Standard output from Python execution */
	FString Output;

	/** Error message if execution failed */
	FString Error;

	/** Log messages collected during execution */
	TArray<FString> Logs;

	/** When the job was submitted */
	FDateTime StartTime;

	/** When the job completed (or failed/cancelled) */
	FDateTime EndTime;

	/** Maximum execution time in seconds (0 = no timeout) */
	int32 TimeoutSeconds = 0;

	/** Check if job is in a terminal state */
	bool IsFinished() const
	{
		return Status == EPythonJobStatus::Completed ||
			   Status == EPythonJobStatus::Failed ||
			   Status == EPythonJobStatus::Cancelled;
	}

	/** Get duration in seconds (or time elapsed if still running) */
	double GetDurationSeconds() const
	{
		FDateTime End = IsFinished() ? EndTime : FDateTime::UtcNow();
		return (End - StartTime).GetTotalSeconds();
	}
};

/**
 * REST handler for Python code execution.
 *
 * Provides endpoints for executing Python code synchronously or asynchronously
 * via Unreal's IPythonScriptPlugin.
 *
 * Endpoints:
 *   POST /python/execute - Execute Python code synchronously
 *   POST /python/jobs - Submit async job (returns job ID)
 *   GET  /python/jobs - List all jobs
 *   GET  /python/jobs/{id} - Get job status and result
 *   DELETE /python/jobs/{id} - Cancel a pending/running job
 */
class FPythonHandler : public IRESTHandler
{
public:
	FPythonHandler() = default;
	virtual ~FPythonHandler() = default;

	//~ Begin IRESTHandler Interface
	virtual FString GetBasePath() const override { return TEXT("/python"); }
	virtual FString GetHandlerName() const override { return TEXT("python"); }
	virtual FString GetDescription() const override { return TEXT("Execute Python code in the editor"); }
	virtual void RegisterRoutes(FRESTRouter& Router) override;
	virtual void Shutdown() override;
	virtual TArray<TSharedPtr<FJsonObject>> GetEndpointSchemas() const override;
	//~ End IRESTHandler Interface

private:
	/**
	 * Handle POST /python/execute - Synchronous execution.
	 * Executes Python code and waits for completion.
	 */
	FRESTResponse HandleExecute(const FRESTRequest& Request);

	/**
	 * Handle GET /python/jobs - List all jobs.
	 * Returns list of job IDs with their current status.
	 */
	FRESTResponse HandleListJobs(const FRESTRequest& Request);

	/**
	 * Handle GET /python/jobs/{id} - Get job details.
	 * Returns full job information including output if completed.
	 */
	FRESTResponse HandleGetJob(const FRESTRequest& Request, const FString& JobId);

	/**
	 * Handle DELETE /python/jobs/{id} - Cancel a job.
	 * Attempts to cancel a pending or running job.
	 */
	FRESTResponse HandleCancelJob(const FRESTRequest& Request, const FString& JobId);

	/**
	 * Execute Python code via IPythonScriptPlugin.
	 * @param Code The Python code to execute
	 * @return Output from Python execution
	 */
	FString ExecutePythonCode(const FString& Code);

	/**
	 * Generate a unique job ID.
	 * @return New UUID-based job identifier
	 */
	FString GenerateJobId();

	/**
	 * Clean up expired jobs to prevent memory leaks.
	 * Removes jobs older than JobExpirationHours.
	 */
	void CleanupOldJobs();

private:
	/** Active and completed jobs */
	TMap<FString, FPythonJob> Jobs;

	/** Lock for thread-safe job access */
	FCriticalSection JobsLock;

	/** Maximum number of jobs to keep in memory */
	static constexpr int32 MaxJobs = 100;

	/** How long to keep completed jobs before cleanup (in hours) */
	static constexpr int32 JobExpirationHours = 1;
};
