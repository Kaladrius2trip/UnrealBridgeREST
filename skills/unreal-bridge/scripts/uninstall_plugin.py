#!/usr/bin/env python3
"""
Uninstaller for UnrealPythonREST plugin from Unreal Engine projects.

Usage:
    python uninstall_plugin.py <project_dir> [--keep-uproject]
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Dict, Any

from ue_project_utils import find_uproject_file, write_uproject


def remove_plugin_from_uproject(uproject_path: Path) -> bool:
    """Remove UnrealPythonREST from .uproject plugins list."""
    try:
        with open(uproject_path, 'r', encoding='utf-8') as f:
            data = json.load(f)

        if "Plugins" not in data:
            return True

        # Filter out UnrealPythonREST
        original_count = len(data["Plugins"])
        data["Plugins"] = [
            p for p in data["Plugins"]
            if p.get("Name") != "UnrealPythonREST"
        ]

        if len(data["Plugins"]) < original_count:
            write_uproject(uproject_path, data)
            return True

        return True  # Nothing to remove

    except Exception as e:
        print(f"Error updating .uproject: {e}")
        return False


def uninstall_plugin(project_dir: str, keep_uproject: bool = False) -> bool:
    """
    Uninstall UnrealPythonREST plugin from Unreal Engine project.

    Args:
        project_dir: Path to UE project directory
        keep_uproject: If True, don't modify .uproject file

    Returns:
        True if uninstallation successful
    """
    project_path = Path(project_dir).resolve()

    if not project_path.is_dir():
        print(f"Error: Directory not found: {project_path}")
        return False

    plugin_dir = project_path / "Plugins" / "UnrealPythonREST"

    print("\n" + "=" * 60)
    print("UnrealPythonREST Plugin Uninstallation")
    print("=" * 60)

    # Remove plugin directory
    if plugin_dir.exists():
        try:
            shutil.rmtree(plugin_dir)
            print(f"  [OK]   Removed plugin directory")
        except Exception as e:
            print(f"  [FAIL] Failed to remove plugin: {e}")
            return False
    else:
        print(f"  [OK]   Plugin directory not found (already removed)")

    # Remove from .uproject
    if not keep_uproject:
        uproject_path = find_uproject_file(project_path)
        if uproject_path:
            if remove_plugin_from_uproject(uproject_path):
                print(f"  [OK]   Removed from .uproject")
            else:
                print(f"  [FAIL] Failed to update .uproject")
                return False

    # Remove config file if exists
    config_file = project_path / "Saved" / "UnrealPythonREST.json"
    if config_file.exists():
        try:
            config_file.unlink()
            print(f"  [OK]   Removed config file")
        except Exception:
            print(f"  [WARN] Could not remove config file")

    print("=" * 60)
    print("! Uninstallation complete!")
    print()

    return True


def main():
    parser = argparse.ArgumentParser(
        description="Uninstall UnrealPythonREST plugin from Unreal Engine project"
    )
    parser.add_argument("project_dir", help="Path to Unreal Engine project directory")
    parser.add_argument("--keep-uproject", action="store_true",
                       help="Don't remove plugin entry from .uproject file")

    args = parser.parse_args()

    success = uninstall_plugin(args.project_dir, args.keep_uproject)
    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
