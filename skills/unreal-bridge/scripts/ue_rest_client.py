"""
Unreal Engine Python REST Client

Executes Python code in Unreal Editor via REST API with automatic fallback
to commandlet execution when the REST server is not available.

Usage:
    executor = UnrealExecutor("/path/to/project")
    result = executor.execute("import unreal; unreal.log('Hello!')")

    # Check execution mode
    print(f"Mode: {executor.mode}")  # "rest" or "commandlet"
"""

from __future__ import annotations

import json
import os
import subprocess
import tempfile
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

# requests is optional - graceful fallback if not available
try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


class UnrealExecutor:
    """Executes Python in Unreal - REST if available, commandlet fallback."""

    CONFIG_FILENAME = "UnrealPythonREST.json"
    DEFAULT_TIMEOUT = 30
    HEALTH_CHECK_TIMEOUT = 2

    def __init__(self, project_dir: str, engine_dir: Optional[str] = None):
        """
        Initialize with project directory path.

        Args:
            project_dir: Path to Unreal project directory (contains .uproject file)
            engine_dir: Optional path to Unreal Engine installation
        """
        self.project_dir = Path(project_dir).resolve()
        self.engine_dir = Path(engine_dir) if engine_dir else None
        self.rest_url: Optional[str] = None
        self._server_info: Optional[Dict[str, Any]] = None

        # Auto-discover server on init
        self._discover_server()

    def _get_config_path(self) -> Path:
        """Get path to REST server config file."""
        return self.project_dir / "Saved" / self.CONFIG_FILENAME

    def _discover_server(self) -> Optional[str]:
        """
        Check for running REST server via config file.

        Returns:
            Base URL if server is alive, None otherwise
        """
        if not HAS_REQUESTS:
            return None

        config_path = self._get_config_path()

        if not config_path.exists():
            self.rest_url = None
            self._server_info = None
            return None

        try:
            with open(config_path, 'r', encoding='utf-8') as f:
                config = json.load(f)

            port = config.get("port", 8080)
            base_url = f"http://localhost:{port}/api/v1"

            # Verify server is actually running with health check
            response = requests.get(
                f"{base_url}/health",
                timeout=self.HEALTH_CHECK_TIMEOUT
            )

            if response.status_code == 200:
                self.rest_url = base_url
                self._server_info = config
                return base_url

        except (json.JSONDecodeError, requests.RequestException, OSError):
            pass

        self.rest_url = None
        self._server_info = None
        return None

    def execute(self, code: str, timeout: int = DEFAULT_TIMEOUT) -> Dict[str, Any]:
        """
        Execute Python code, auto-selecting method.

        Args:
            code: Python code to execute in Unreal
            timeout: Maximum execution time in seconds

        Returns:
            Dict with keys: success, output, logs, duration_ms, mode
        """
        start_time = time.time()

        try:
            if self.rest_url and HAS_REQUESTS:
                result = self._execute_rest(code, timeout)
            else:
                result = self._execute_commandlet(code, timeout)
        except Exception as e:
            result = {
                "success": False,
                "output": "",
                "logs": [f"Execution error: {str(e)}"],
                "error": str(e),
            }

        # Ensure consistent response format
        result["duration_ms"] = int((time.time() - start_time) * 1000)
        result["mode"] = self.mode
        result.setdefault("success", False)
        result.setdefault("output", "")
        result.setdefault("logs", [])

        return result

    def _execute_rest(self, code: str, timeout: int) -> Dict[str, Any]:
        """
        Execute via REST API.

        Args:
            code: Python code to execute
            timeout: Request timeout in seconds

        Returns:
            Response from REST API
        """
        if not self.rest_url:
            raise RuntimeError("REST server not available")

        response = requests.post(
            f"{self.rest_url}/python/execute",
            json={"code": code},
            timeout=timeout,
            headers={"Content-Type": "application/json"}
        )

        if response.status_code == 200:
            data = response.json()
            return {
                "success": data.get("success", True),
                "output": data.get("output", ""),
                "logs": data.get("logs", []),
            }
        else:
            return {
                "success": False,
                "output": "",
                "logs": [f"REST API error: HTTP {response.status_code}"],
                "error": response.text,
            }

    def _execute_commandlet(self, code: str, timeout: int) -> Dict[str, Any]:
        """
        Fallback to commandlet execution.

        Args:
            code: Python code to execute
            timeout: Process timeout in seconds

        Returns:
            Execution result dict
        """
        # Find editor executable
        editor_path = find_unreal_editor(str(self.engine_dir) if self.engine_dir else None)
        if not editor_path:
            return {
                "success": False,
                "output": "",
                "logs": ["UnrealEditor-Cmd.exe not found"],
                "error": "Could not locate Unreal Editor",
            }

        # Find .uproject file
        uproject_files = list(self.project_dir.glob("*.uproject"))
        if not uproject_files:
            return {
                "success": False,
                "output": "",
                "logs": ["No .uproject file found in project directory"],
                "error": f"No .uproject in {self.project_dir}",
            }
        uproject = uproject_files[0]

        # Create temp script file
        with tempfile.NamedTemporaryFile(
            mode='w',
            suffix='.py',
            delete=False,
            encoding='utf-8'
        ) as f:
            # Add output marker for parsing
            wrapper_code = f'''
# Auto-generated wrapper for commandlet execution
import unreal

_MARKER_START = "<<<UNREAL_PYTHON_OUTPUT_START>>>"
_MARKER_END = "<<<UNREAL_PYTHON_OUTPUT_END>>>"

unreal.log(_MARKER_START)
try:
{_indent_code(code, 4)}
except Exception as _e:
    unreal.log_error(f"Execution error: {{_e}}")
unreal.log(_MARKER_END)
'''
            f.write(wrapper_code)
            script_path = f.name

        try:
            # Build command
            cmd = [
                editor_path,
                str(uproject),
                "-run=pythonscript",
                f"-script={script_path}",
                "-stdout",
                "-unattended",
                "-nosplash",
                "-nullrhi",
            ]

            # Execute commandlet
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                cwd=str(self.project_dir),
            )

            # Parse output
            logs = parse_log_output_from_text(result.stdout + result.stderr)
            output = _extract_marked_output(result.stdout + result.stderr)

            return {
                "success": result.returncode == 0,
                "output": output,
                "logs": logs,
                "returncode": result.returncode,
            }

        except subprocess.TimeoutExpired:
            return {
                "success": False,
                "output": "",
                "logs": [f"Commandlet timed out after {timeout}s"],
                "error": "Timeout",
            }
        finally:
            # Cleanup temp file
            try:
                os.unlink(script_path)
            except OSError:
                pass

    @property
    def mode(self) -> str:
        """Returns 'rest' or 'commandlet'."""
        return "rest" if self.rest_url else "commandlet"

    @property
    def server_info(self) -> Optional[Dict[str, Any]]:
        """Returns REST server info if connected, None otherwise."""
        return self._server_info.copy() if self._server_info else None

    def refresh_connection(self) -> bool:
        """
        Re-check for server availability.

        Returns:
            True if REST server is now available, False otherwise
        """
        self._discover_server()
        return self.rest_url is not None

    def is_rest_available(self) -> bool:
        """Check if REST mode is currently available."""
        return self.rest_url is not None


def find_unreal_editor(engine_dir: Optional[str] = None) -> Optional[str]:
    """
    Find UnrealEditor-Cmd.exe path.

    Args:
        engine_dir: Optional specific engine directory to check

    Returns:
        Path to UnrealEditor-Cmd.exe or None if not found
    """
    # Platform-specific executable name
    if os.name == 'nt':
        exe_name = "UnrealEditor-Cmd.exe"
        relative_path = Path("Engine/Binaries/Win64") / exe_name
    else:
        exe_name = "UnrealEditor-Cmd"
        relative_path = Path("Engine/Binaries/Linux") / exe_name

    candidates: List[Path] = []

    # Check specified engine directory first
    if engine_dir:
        engine_path = Path(engine_dir)
        candidates.append(engine_path / relative_path)
        # Also check if engine_dir is the Binaries folder directly
        candidates.append(engine_path / exe_name)

    # Check environment variable
    ue_root = os.environ.get("UE_ROOT") or os.environ.get("UNREAL_ENGINE_ROOT")
    if ue_root:
        candidates.append(Path(ue_root) / relative_path)

    # Check common installation locations on Windows
    if os.name == 'nt':
        program_files = os.environ.get("ProgramFiles", "C:\\Program Files")
        epic_games = Path(program_files) / "Epic Games"

        if epic_games.exists():
            # Check for UE5.x installations
            for ue_dir in epic_games.glob("UE_5.*"):
                candidates.append(ue_dir / relative_path)
            # Also check for source builds
            for ue_dir in epic_games.glob("UnrealEngine*"):
                candidates.append(ue_dir / relative_path)

    # Check PATH
    path_dirs = os.environ.get("PATH", "").split(os.pathsep)
    for path_dir in path_dirs:
        candidates.append(Path(path_dir) / exe_name)

    # Return first existing candidate
    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return str(candidate)

    return None


def parse_log_output(log_path: str, marker: str = "LogPython:") -> List[str]:
    """
    Extract Python log lines from Unreal log file.

    Args:
        log_path: Path to Unreal log file
        marker: Log category marker to filter for

    Returns:
        List of log lines matching the marker
    """
    try:
        with open(log_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
        return parse_log_output_from_text(content, marker)
    except OSError:
        return []


def parse_log_output_from_text(text: str, marker: str = "LogPython:") -> List[str]:
    """
    Extract Python log lines from log text.

    Args:
        text: Log file content as string
        marker: Log category marker to filter for

    Returns:
        List of log lines matching the marker
    """
    lines = []
    for line in text.splitlines():
        if marker in line:
            # Extract just the message part after the marker
            idx = line.find(marker)
            if idx >= 0:
                msg = line[idx + len(marker):].strip()
                # Remove common prefixes like "Display:", "Warning:", "Error:"
                for prefix in ("Display: ", "Warning: ", "Error: "):
                    if msg.startswith(prefix):
                        msg = msg[len(prefix):]
                        break
                lines.append(msg)
    return lines


def _indent_code(code: str, spaces: int) -> str:
    """Indent all lines of code by specified spaces."""
    indent = " " * spaces
    lines = code.splitlines()
    return "\n".join(indent + line if line.strip() else line for line in lines)


def _extract_marked_output(text: str) -> str:
    """Extract output between start/end markers."""
    start_marker = "<<<UNREAL_PYTHON_OUTPUT_START>>>"
    end_marker = "<<<UNREAL_PYTHON_OUTPUT_END>>>"

    start_idx = text.find(start_marker)
    end_idx = text.find(end_marker)

    if start_idx >= 0 and end_idx > start_idx:
        # Extract content between markers
        content = text[start_idx + len(start_marker):end_idx]
        # Parse out just the Python log messages
        lines = parse_log_output_from_text(content)
        return "\n".join(lines)

    return ""


if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python ue_rest_client.py <project_dir> [code]")
        print()
        print("Arguments:")
        print("  project_dir  Path to Unreal project directory")
        print("  code         Python code to execute (optional)")
        print()
        print("Examples:")
        print('  python ue_rest_client.py "C:\\Projects\\MyGame"')
        print('  python ue_rest_client.py "C:\\Projects\\MyGame" "import unreal; print(unreal.get_editor_subsystem())"')
        sys.exit(1)

    project_dir = sys.argv[1]
    code = sys.argv[2] if len(sys.argv) > 2 else 'import unreal; unreal.log("Hello from REST client!")'

    print(f"Project: {project_dir}")
    print("-" * 50)

    executor = UnrealExecutor(project_dir)
    print(f"Execution mode: {executor.mode}")

    if executor.server_info:
        print(f"Server port: {executor.server_info.get('port')}")
        print(f"Server PID: {executor.server_info.get('pid')}")

    print("-" * 50)
    print(f"Executing: {code[:60]}..." if len(code) > 60 else f"Executing: {code}")
    print("-" * 50)

    result = executor.execute(code)

    print(f"Success: {result['success']}")
    print(f"Mode: {result['mode']}")
    print(f"Duration: {result.get('duration_ms', 0)}ms")

    if result.get('output'):
        print(f"\nOutput:\n{result['output']}")

    if result.get('logs'):
        print(f"\nLogs:")
        for log in result['logs']:
            print(f"  {log}")

    if result.get('error'):
        print(f"\nError: {result['error']}")

    sys.exit(0 if result['success'] else 1)
