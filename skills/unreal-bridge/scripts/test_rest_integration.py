#!/usr/bin/env python3
"""
Integration tests for UnrealPythonREST plugin.

Tests the REST API integration by verifying all endpoints work correctly.
Can be run against a running Unreal Editor with the UnrealPythonREST plugin enabled.

Usage:
    uv run test_rest_integration.py <project_dir>
    uv run test_rest_integration.py <project_dir> --verbose
    uv run test_rest_integration.py <project_dir> --category discovery

Example:
    uv run test_rest_integration.py "G:/UE_Projects/MyProject"
"""

from __future__ import annotations

import argparse
import io
import json
import sys
import time
from pathlib import Path
from typing import Any, Callable, Dict, List, Optional, Tuple

# Force UTF-8 output on Windows to handle Unicode symbols
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

# Test result symbols (ASCII-safe alternatives for Windows compatibility)
SYM_PASS = "[PASS]"
SYM_FAIL = "[FAIL]"
SYM_SKIP = "[SKIP]"

# Optional: requests for direct HTTP tests
try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False

# Import our client (handle import from same directory)
try:
    from ue_rest_client import UnrealExecutor
    HAS_CLIENT = True
except ImportError:
    # Try relative import when run as script
    import os
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    try:
        from ue_rest_client import UnrealExecutor
        HAS_CLIENT = True
    except ImportError:
        HAS_CLIENT = False


class TestResult:
    """Tracks test results with detailed information."""

    def __init__(self):
        self.passed: int = 0
        self.failed: int = 0
        self.skipped: int = 0
        self.results: List[Tuple[str, Optional[bool], str]] = []

    def record(self, name: str, passed: bool, message: str = "") -> None:
        """Record a test result (pass or fail)."""
        self.results.append((name, passed, message))
        if passed:
            self.passed += 1
        else:
            self.failed += 1

    def skip(self, name: str, reason: str) -> None:
        """Record a skipped test."""
        self.results.append((name, None, reason))
        self.skipped += 1

    def summary(self) -> str:
        """Return summary string."""
        return f"Passed: {self.passed}, Failed: {self.failed}, Skipped: {self.skipped}"

    def get_failures(self) -> List[Tuple[str, str]]:
        """Return list of (name, message) for failed tests."""
        return [(name, msg) for name, passed, msg in self.results if passed is False]

    def get_skipped(self) -> List[Tuple[str, str]]:
        """Return list of (name, reason) for skipped tests."""
        return [(name, msg) for name, passed, msg in self.results if passed is None]


class TestContext:
    """Shared context for test execution."""

    def __init__(self, project_dir: Path, verbose: bool = False):
        self.project_dir = project_dir
        self.verbose = verbose
        self.config_path = project_dir / "Saved" / "UnrealPythonREST.json"
        self.config: Optional[Dict[str, Any]] = None
        self.base_url: Optional[str] = None
        self.executor: Optional[UnrealExecutor] = None

    def load_config(self) -> bool:
        """Load config file if it exists."""
        if not self.config_path.exists():
            return False
        try:
            with open(self.config_path, 'r', encoding='utf-8') as f:
                self.config = json.load(f)
            port = self.config.get("port", 8080)
            self.base_url = f"http://localhost:{port}/api/v1"
            return True
        except (json.JSONDecodeError, OSError):
            return False

    def init_executor(self) -> bool:
        """Initialize UnrealExecutor if client is available."""
        if not HAS_CLIENT:
            return False
        try:
            self.executor = UnrealExecutor(str(self.project_dir))
            return True
        except Exception:
            return False


def run_test(
    name: str,
    test_fn: Callable[[], Tuple[bool, str]],
    results: TestResult,
    verbose: bool = False,
    skip_reason: Optional[str] = None,
) -> None:
    """Run a single test and record result."""
    if skip_reason:
        results.skip(name, skip_reason)
        if verbose:
            print(f"  {SYM_SKIP} {name} (skipped: {skip_reason})")
        else:
            print(f"  {SYM_SKIP} {name}")
        return

    try:
        passed, message = test_fn()
        results.record(name, passed, message)
        if passed:
            print(f"  {SYM_PASS} {name}")
            if verbose and message:
                print(f"      {message}")
        else:
            print(f"  {SYM_FAIL} {name}")
            if message:
                print(f"      {message}")
    except Exception as e:
        results.record(name, False, f"Exception: {e}")
        print(f"  {SYM_FAIL} {name}")
        print(f"      Exception: {e}")


# =============================================================================
# Discovery Tests
# =============================================================================

def test_discovery(ctx: TestContext, results: TestResult) -> None:
    """Test server discovery functionality."""
    print("\n[Discovery Tests]")

    # Test: Config file exists
    def test_config_file_exists() -> Tuple[bool, str]:
        exists = ctx.config_path.exists()
        return exists, f"Path: {ctx.config_path}"

    run_test("test_config_file_exists", test_config_file_exists, results, ctx.verbose)

    # Test: Config file is valid JSON
    def test_config_file_valid() -> Tuple[bool, str]:
        if not ctx.config_path.exists():
            return False, "Config file does not exist"
        try:
            with open(ctx.config_path, 'r', encoding='utf-8') as f:
                data = json.load(f)
            keys = list(data.keys())
            return True, f"Keys: {keys[:5]}"
        except json.JSONDecodeError as e:
            return False, f"Invalid JSON: {e}"
        except OSError as e:
            return False, f"Read error: {e}"

    run_test("test_config_file_valid", test_config_file_valid, results, ctx.verbose)

    # Load config for subsequent tests
    ctx.load_config()

    # Test: Health endpoint responds
    skip_health = None
    if not HAS_REQUESTS:
        skip_health = "requires requests library"
    elif not ctx.base_url:
        skip_health = "no valid config file"

    def test_health_endpoint() -> Tuple[bool, str]:
        try:
            response = requests.get(f"{ctx.base_url}/health", timeout=5)
            if response.status_code == 200:
                return True, f"Status: {response.status_code}"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Connection failed: {e}"

    run_test("test_health_endpoint", test_health_endpoint, results, ctx.verbose, skip_health)

    # Test: Handlers include python
    def test_handlers_include_python() -> Tuple[bool, str]:
        try:
            response = requests.get(f"{ctx.base_url}/health", timeout=5)
            if response.status_code != 200:
                return False, f"HTTP {response.status_code}"
            data = response.json()
            handlers = data.get("handlers", [])
            if "python" in handlers:
                return True, f"Handlers: {handlers}"
            return False, f"'python' not in handlers: {handlers}"
        except requests.RequestException as e:
            return False, f"Connection failed: {e}"
        except json.JSONDecodeError:
            return False, "Invalid JSON response"

    run_test("test_handlers_include_python", test_handlers_include_python, results, ctx.verbose, skip_health)


# =============================================================================
# Direct API Tests
# =============================================================================

def test_direct_api(ctx: TestContext, results: TestResult) -> None:
    """Test REST API directly with requests library."""
    print("\n[Direct API Tests]")

    skip_reason = None
    if not HAS_REQUESTS:
        skip_reason = "requires requests library"
    elif not ctx.base_url:
        skip_reason = "no valid config file"

    # Check if server is actually running
    if not skip_reason:
        try:
            response = requests.get(f"{ctx.base_url}/health", timeout=2)
            if response.status_code != 200:
                skip_reason = "server not responding"
        except requests.RequestException:
            skip_reason = "server not responding"

    # Test: Execute endpoint exists
    def test_execute_endpoint_exists() -> Tuple[bool, str]:
        try:
            # OPTIONS request to check endpoint exists
            response = requests.options(f"{ctx.base_url}/python/execute", timeout=5)
            # Some servers return 200, some 204, some 405 (method not allowed but endpoint exists)
            if response.status_code in (200, 204, 405):
                return True, f"Status: {response.status_code}"
            # Try POST with empty body to verify endpoint
            response = requests.post(
                f"{ctx.base_url}/python/execute",
                json={},
                timeout=5
            )
            # Even an error response means endpoint exists
            return True, f"Status: {response.status_code}"
        except requests.RequestException as e:
            return False, f"Connection failed: {e}"

    run_test("test_execute_endpoint_exists", test_execute_endpoint_exists, results, ctx.verbose, skip_reason)

    # Test: Execute simple code
    def test_execute_simple() -> Tuple[bool, str]:
        try:
            response = requests.post(
                f"{ctx.base_url}/python/execute",
                json={"code": "result = 1 + 1"},
                timeout=10
            )
            if response.status_code == 200:
                data = response.json()
                return data.get("success", False), f"Response: {data}"
            return False, f"HTTP {response.status_code}: {response.text[:100]}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_execute_simple", test_execute_simple, results, ctx.verbose, skip_reason)

    # Test: Execute with output
    def test_execute_with_output() -> Tuple[bool, str]:
        try:
            response = requests.post(
                f"{ctx.base_url}/python/execute",
                json={"code": "import unreal\nunreal.log('Test output from integration test')"},
                timeout=10
            )
            if response.status_code == 200:
                data = response.json()
                success = data.get("success", False)
                logs = data.get("logs", [])
                return success, f"Logs: {logs[:3]}"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_execute_with_output", test_execute_with_output, results, ctx.verbose, skip_reason)

    # Test: Execute returns job_id
    def test_execute_returns_job_id() -> Tuple[bool, str]:
        try:
            response = requests.post(
                f"{ctx.base_url}/python/execute",
                json={"code": "pass"},
                timeout=10
            )
            if response.status_code == 200:
                data = response.json()
                job_id = data.get("job_id") or data.get("jobId") or data.get("id")
                if job_id:
                    return True, f"job_id: {job_id}"
                return False, f"No job_id in response: {list(data.keys())}"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_execute_returns_job_id", test_execute_returns_job_id, results, ctx.verbose, skip_reason)


# =============================================================================
# Python Execution Tests
# =============================================================================

def test_python_execution(ctx: TestContext, results: TestResult) -> None:
    """Test Python code execution via client or REST."""
    print("\n[Python Execution Tests]")

    skip_reason = None
    if not HAS_CLIENT:
        skip_reason = "ue_rest_client not available"
    elif not ctx.init_executor():
        skip_reason = "failed to create executor"
    elif ctx.executor and ctx.executor.mode != "rest":
        # Check if server is running
        try:
            if HAS_REQUESTS and ctx.base_url:
                response = requests.get(f"{ctx.base_url}/health", timeout=2)
                if response.status_code != 200:
                    skip_reason = "REST server not running"
            else:
                skip_reason = "REST server not available"
        except Exception:
            skip_reason = "REST server not responding"

    # Test: Simple print statement
    def test_simple_print() -> Tuple[bool, str]:
        result = ctx.executor.execute("import unreal\nunreal.log('Hello from test')")
        return result["success"], f"Mode: {result.get('mode')}, Duration: {result.get('duration_ms')}ms"

    run_test("test_simple_print", test_simple_print, results, ctx.verbose, skip_reason)

    # Test: Import unreal module
    def test_import_unreal() -> Tuple[bool, str]:
        result = ctx.executor.execute("import unreal\nunreal.log('unreal module imported')")
        return result["success"], f"Logs: {result.get('logs', [])[:2]}"

    run_test("test_import_unreal", test_import_unreal, results, ctx.verbose, skip_reason)

    # Test: Get engine version
    def test_get_engine_version() -> Tuple[bool, str]:
        code = """
import unreal
version = unreal.SystemLibrary.get_engine_version()
unreal.log(f'Engine version: {version}')
"""
        result = ctx.executor.execute(code)
        logs = result.get("logs", [])
        version_log = [l for l in logs if "Engine version" in l]
        if result["success"] and version_log:
            return True, version_log[0]
        return result["success"], f"Logs: {logs[:2]}"

    run_test("test_get_engine_version", test_get_engine_version, results, ctx.verbose, skip_reason)

    # Test: Syntax error handling
    def test_syntax_error() -> Tuple[bool, str]:
        result = ctx.executor.execute("def broken(")
        # This should fail (success=False) because of syntax error
        if not result["success"]:
            return True, "Correctly detected syntax error"
        return False, "Syntax error was not detected"

    run_test("test_syntax_error", test_syntax_error, results, ctx.verbose, skip_reason)

    # Test: Runtime error handling
    def test_runtime_error() -> Tuple[bool, str]:
        result = ctx.executor.execute("raise ValueError('Test error from integration test')")
        # This should fail (success=False) because of runtime error
        if not result["success"]:
            return True, "Correctly detected runtime error"
        return False, "Runtime error was not detected"

    run_test("test_runtime_error", test_runtime_error, results, ctx.verbose, skip_reason)

    # Test: Multiple statements
    def test_multiple_statements() -> Tuple[bool, str]:
        code = """
import unreal
x = 10
y = 20
z = x + y
unreal.log(f'Sum: {z}')
"""
        result = ctx.executor.execute(code)
        return result["success"], f"Duration: {result.get('duration_ms')}ms"

    run_test("test_multiple_statements", test_multiple_statements, results, ctx.verbose, skip_reason)


# =============================================================================
# Job Management Tests
# =============================================================================

def test_job_management(ctx: TestContext, results: TestResult) -> None:
    """Test job listing and management endpoints."""
    print("\n[Job Management Tests]")

    skip_reason = None
    if not HAS_REQUESTS:
        skip_reason = "requires requests library"
    elif not ctx.base_url:
        skip_reason = "no valid config file"

    # Check if server is running
    if not skip_reason:
        try:
            response = requests.get(f"{ctx.base_url}/health", timeout=2)
            if response.status_code != 200:
                skip_reason = "server not responding"
        except requests.RequestException:
            skip_reason = "server not responding"

    # Store job_id for subsequent tests
    job_id_holder = {"id": None}

    # Test: Execute and get job_id
    def test_execute_get_job_id() -> Tuple[bool, str]:
        try:
            response = requests.post(
                f"{ctx.base_url}/python/execute",
                json={"code": "import unreal\nunreal.log('Job management test')"},
                timeout=10
            )
            if response.status_code == 200:
                data = response.json()
                job_id = data.get("job_id") or data.get("jobId") or data.get("id")
                if job_id:
                    job_id_holder["id"] = job_id
                    return True, f"job_id: {job_id}"
                return False, f"No job_id: {list(data.keys())}"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_execute_get_job_id", test_execute_get_job_id, results, ctx.verbose, skip_reason)

    # Test: List jobs endpoint
    def test_list_jobs() -> Tuple[bool, str]:
        try:
            response = requests.get(f"{ctx.base_url}/python/jobs", timeout=5)
            if response.status_code == 200:
                data = response.json()
                jobs = data.get("jobs", data) if isinstance(data, dict) else data
                if isinstance(jobs, list):
                    return True, f"Found {len(jobs)} jobs"
                return True, f"Response: {type(data)}"
            elif response.status_code == 404:
                return False, "Jobs endpoint not implemented"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_list_jobs", test_list_jobs, results, ctx.verbose, skip_reason)

    # Test: Get job by ID
    def test_get_job_by_id() -> Tuple[bool, str]:
        if not job_id_holder["id"]:
            return False, "No job_id from previous test"
        try:
            job_id = job_id_holder["id"]
            response = requests.get(f"{ctx.base_url}/python/job?id={job_id}", timeout=5)
            if response.status_code == 200:
                data = response.json()
                return True, f"Job data: {list(data.keys())[:5]}"
            elif response.status_code == 404:
                return False, "Job endpoint not implemented or job not found"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_get_job_by_id", test_get_job_by_id, results, ctx.verbose, skip_reason)

    # Test: Job status is completed
    def test_job_status_completed() -> Tuple[bool, str]:
        if not job_id_holder["id"]:
            return False, "No job_id from previous test"
        try:
            job_id = job_id_holder["id"]
            # Wait a bit for job to complete
            time.sleep(0.5)
            response = requests.get(f"{ctx.base_url}/python/job?id={job_id}", timeout=5)
            if response.status_code == 200:
                data = response.json()
                # Status is nested inside job object
                job_data = data.get("job", data)
                status = job_data.get("status", "unknown")
                if status in ("completed", "success", "done"):
                    return True, f"Status: {status}"
                return False, f"Status: {status}"
            elif response.status_code == 404:
                return False, "Job endpoint not implemented"
            return False, f"HTTP {response.status_code}"
        except requests.RequestException as e:
            return False, f"Request failed: {e}"

    run_test("test_job_status_completed", test_job_status_completed, results, ctx.verbose, skip_reason)


# =============================================================================
# Client Tests
# =============================================================================

def test_client(ctx: TestContext, results: TestResult) -> None:
    """Test UnrealExecutor client functionality."""
    print("\n[Client Tests]")

    skip_reason = None
    if not HAS_CLIENT:
        skip_reason = "ue_rest_client not available"

    # Test: Executor creation
    def test_executor_creation() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            return True, f"Mode: {executor.mode}"
        except Exception as e:
            return False, f"Creation failed: {e}"

    run_test("test_executor_creation", test_executor_creation, results, ctx.verbose, skip_reason)

    # Test: Executor mode property
    def test_executor_mode() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            mode = executor.mode
            if mode in ("rest", "commandlet"):
                return True, f"Mode: {mode}"
            return False, f"Invalid mode: {mode}"
        except Exception as e:
            return False, f"Error: {e}"

    run_test("test_executor_mode", test_executor_mode, results, ctx.verbose, skip_reason)

    # Test: Server info property
    def test_executor_server_info() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            info = executor.server_info
            if executor.mode == "rest":
                if info:
                    return True, f"Info keys: {list(info.keys())[:5]}"
                return False, "REST mode but no server_info"
            else:
                if info is None:
                    return True, "Correctly None for commandlet mode"
                return False, f"Expected None, got: {info}"
        except Exception as e:
            return False, f"Error: {e}"

    run_test("test_executor_server_info", test_executor_server_info, results, ctx.verbose, skip_reason)

    # Test: is_rest_available method
    def test_is_rest_available() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            available = executor.is_rest_available()
            expected = (executor.mode == "rest")
            if available == expected:
                return True, f"Available: {available}"
            return False, f"Mismatch: available={available}, mode={executor.mode}"
        except Exception as e:
            return False, f"Error: {e}"

    run_test("test_is_rest_available", test_is_rest_available, results, ctx.verbose, skip_reason)

    # Test: refresh_connection method
    def test_refresh_connection() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            initial_mode = executor.mode
            result = executor.refresh_connection()
            final_mode = executor.mode
            return True, f"Refresh returned {result}, mode: {initial_mode} -> {final_mode}"
        except Exception as e:
            return False, f"Error: {e}"

    run_test("test_refresh_connection", test_refresh_connection, results, ctx.verbose, skip_reason)

    # Test: Execute method returns correct structure
    def test_execute_result_structure() -> Tuple[bool, str]:
        try:
            executor = UnrealExecutor(str(ctx.project_dir))
            # This will work in either mode
            result = executor.execute("pass", timeout=5)
            required_keys = {"success", "output", "logs", "duration_ms", "mode"}
            actual_keys = set(result.keys())
            missing = required_keys - actual_keys
            if missing:
                return False, f"Missing keys: {missing}"
            return True, f"Keys: {list(result.keys())}"
        except Exception as e:
            return False, f"Error: {e}"

    run_test("test_execute_result_structure", test_execute_result_structure, results, ctx.verbose, skip_reason)


# =============================================================================
# Main Entry Point
# =============================================================================

def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Integration tests for UnrealPythonREST plugin",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
    uv run test_rest_integration.py "G:/UE_Projects/MyProject"
    uv run test_rest_integration.py "G:/UE_Projects/MyProject" --verbose
    uv run test_rest_integration.py "G:/UE_Projects/MyProject" --category discovery
    uv run test_rest_integration.py "G:/UE_Projects/MyProject" --category client

Categories:
    discovery   - Config file and health endpoint tests
    api         - Direct REST API tests
    execution   - Python execution tests
    jobs        - Job management tests
    client      - UnrealExecutor client tests
    all         - Run all test categories (default)
"""
    )
    parser.add_argument("project_dir", help="Path to Unreal project directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument(
        "--category", "-c",
        choices=["discovery", "api", "execution", "jobs", "client", "all"],
        default="all",
        help="Test category to run (default: all)"
    )
    args = parser.parse_args()

    project_dir = Path(args.project_dir).resolve()
    if not project_dir.exists():
        print(f"Error: Project directory does not exist: {project_dir}")
        return 1

    results = TestResult()
    ctx = TestContext(project_dir, args.verbose)

    print(f"Testing UnrealPythonREST for project: {project_dir}")
    print("=" * 60)

    # Show environment info
    print(f"  requests library: {'available' if HAS_REQUESTS else 'NOT AVAILABLE'}")
    print(f"  ue_rest_client:   {'available' if HAS_CLIENT else 'NOT AVAILABLE'}")
    print(f"  config file:      {'exists' if ctx.config_path.exists() else 'not found'}")

    if ctx.load_config() and ctx.base_url:
        print(f"  server URL:       {ctx.base_url}")
        # Try to check if server is running
        if HAS_REQUESTS:
            try:
                response = requests.get(f"{ctx.base_url}/health", timeout=2)
                print(f"  server status:    {'running' if response.status_code == 200 else 'not responding'}")
            except requests.RequestException:
                print("  server status:    not responding")

    # Run selected test categories
    category = args.category

    if category in ("discovery", "all"):
        test_discovery(ctx, results)

    if category in ("api", "all"):
        test_direct_api(ctx, results)

    if category in ("execution", "all"):
        test_python_execution(ctx, results)

    if category in ("jobs", "all"):
        test_job_management(ctx, results)

    if category in ("client", "all"):
        test_client(ctx, results)

    # Print summary
    print()
    print("=" * 60)
    print(f"Results: {results.summary()}")

    # Print failures
    failures = results.get_failures()
    if failures:
        print(f"\nFailed tests ({len(failures)}):")
        for name, msg in failures:
            print(f"  {SYM_FAIL} {name}")
            if msg:
                print(f"      {msg}")

    # Print skipped (only in verbose mode)
    if args.verbose:
        skipped = results.get_skipped()
        if skipped:
            print(f"\nSkipped tests ({len(skipped)}):")
            for name, reason in skipped:
                print(f"  {SYM_SKIP} {name}: {reason}")

    return 0 if results.failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
