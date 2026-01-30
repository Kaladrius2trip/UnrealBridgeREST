#!/usr/bin/env python3
"""
Check UnrealPythonREST plugin installation status.

Usage:
    python check_plugin_status.py <project_dir>
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Dict, Any

from ue_project_utils import find_uproject_file, read_uproject
from install_plugin import get_source_version, get_installed_version, compare_versions

try:
    import requests
    HAS_REQUESTS = True
except ImportError:
    HAS_REQUESTS = False


def check_status(project_dir: str) -> Dict[str, Any]:
    """
    Check UnrealPythonREST plugin status for a project.

    Returns dict with:
        - installed: bool
        - enabled: bool
        - server_running: bool
        - server_port: Optional[int]
        - details: Dict[str, bool]
    """
    project_path = Path(project_dir).resolve()
    result = {
        "installed": False,
        "enabled": False,
        "server_running": False,
        "server_port": None,
        "installed_version": None,
        "source_version": None,
        "update_available": False,
        "details": {}
    }

    # Check plugin directory
    plugin_dir = project_path / "Plugins" / "UnrealPythonREST"
    result["details"]["plugin_dir_exists"] = plugin_dir.is_dir()

    # Check .uplugin file
    uplugin_file = plugin_dir / "UnrealPythonREST.uplugin"
    result["details"]["uplugin_exists"] = uplugin_file.is_file()

    # Check source files
    build_cs = plugin_dir / "Source" / "UnrealPythonREST" / "UnrealPythonREST.Build.cs"
    result["details"]["source_exists"] = build_cs.is_file()

    result["installed"] = all([
        result["details"]["plugin_dir_exists"],
        result["details"]["uplugin_exists"],
        result["details"]["source_exists"]
    ])

    # Check versions
    result["installed_version"] = get_installed_version(project_path)
    result["source_version"] = get_source_version()
    comparison = compare_versions(result["installed_version"], result["source_version"])
    result["update_available"] = comparison == "update_available"

    # Check .uproject for plugin enablement
    uproject_path = find_uproject_file(project_path)
    if uproject_path:
        try:
            uproject = read_uproject(uproject_path)

            plugins = uproject.get("Plugins", [])
            for plugin in plugins:
                if plugin.get("Name") == "UnrealPythonREST":
                    result["enabled"] = plugin.get("Enabled", False)
                    break

            result["details"]["uproject_found"] = True
        except Exception:
            result["details"]["uproject_found"] = False

    # Check config file for running server
    config_file = project_path / "Saved" / "UnrealPythonREST.json"
    if config_file.is_file():
        try:
            with open(config_file, 'r', encoding='utf-8') as f:
                config = json.load(f)

            port = config.get("port")
            result["server_port"] = port
            result["details"]["config_exists"] = True

            # Try to ping server
            if HAS_REQUESTS and port:
                try:
                    response = requests.get(f"http://localhost:{port}/api/v1/health", timeout=2)
                    result["server_running"] = response.status_code == 200
                except Exception:
                    result["server_running"] = False

        except Exception:
            result["details"]["config_exists"] = False
    else:
        result["details"]["config_exists"] = False

    return result


def print_status(status: Dict[str, Any]):
    """Print status in readable format."""
    print("\n" + "=" * 60)
    print("UnrealPythonREST Plugin Status")
    print("=" * 60)

    # Installation status
    if status["installed"]:
        print("  [!] Plugin installed")
    else:
        print("  [X] Plugin NOT installed")
        for key, value in status["details"].items():
            if not value and key.endswith("_exists"):
                print(f"      Missing: {key.replace('_exists', '').replace('_', ' ')}")

    # Version status
    installed_version = status.get("installed_version")
    source_version = status.get("source_version")
    if installed_version:
        print(f"  [i] Installed version: {installed_version}")
    if source_version:
        print(f"  [i] Source version: {source_version}")
    if status.get("update_available"):
        print(f"  [!] Update available: {installed_version} -> {source_version}")
    elif installed_version and source_version and installed_version == source_version:
        print("  [=] Up to date")

    # Enablement status
    if status["enabled"]:
        print("  [!] Plugin enabled in .uproject")
    else:
        print("  [X] Plugin NOT enabled in .uproject")

    # Server status
    if status["server_running"]:
        print(f"  [!] REST server running on port {status['server_port']}")
    elif status["details"].get("config_exists"):
        print(f"  [o] Config exists but server not responding (port {status['server_port']})")
        print("      Editor may not be running")
    else:
        print("  [o] REST server not running (no config file)")

    print("=" * 60)

    # Summary
    if status["installed"] and status["enabled"] and status["server_running"]:
        print("Status: READY - Full REST API available")
    elif status["installed"] and status["enabled"]:
        print("Status: INSTALLED - Open editor to start REST server")
    elif status["installed"]:
        print("Status: PARTIAL - Plugin installed but not enabled")
    else:
        print("Status: NOT INSTALLED - Run /install-ue-rest to install")
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Check UnrealPythonREST plugin installation status"
    )
    parser.add_argument("project_dir", help="Path to Unreal Engine project directory")
    parser.add_argument("--json", "-j", action="store_true", help="Output as JSON")

    args = parser.parse_args()

    status = check_status(args.project_dir)

    if args.json:
        print(json.dumps(status, indent=2))
    else:
        print_status(status)

    return 0


if __name__ == "__main__":
    sys.exit(main())
