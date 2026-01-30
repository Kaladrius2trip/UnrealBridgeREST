#!/usr/bin/env python3
"""Shared utilities for UE project operations."""

from __future__ import annotations

import json
from pathlib import Path
from typing import Dict, Any, Optional, Tuple


def find_uproject_file_with_reason(project_dir: Path) -> Tuple[Optional[Path], str]:
    """Find .uproject file in project directory with detailed status.

    Args:
        project_dir: Path to the project directory

    Returns:
        (path, message) tuple:
        - If found: (path, "Found") or (path, "Found (matched directory name)")
        - If not found: (None, "reason why not found")
    """
    uproject_files = list(project_dir.glob("*.uproject"))

    if len(uproject_files) == 0:
        return None, "No .uproject file found in directory"
    elif len(uproject_files) == 1:
        return uproject_files[0], "Found"
    else:
        # Multiple files - try to find matching name
        expected = project_dir.name + ".uproject"
        for f in uproject_files:
            if f.name == expected:
                return f, f"Found (matched directory name from {len(uproject_files)} files)"

        file_names = [f.name for f in uproject_files]
        return None, f"Multiple .uproject files found, none match directory name: {file_names}"


def find_uproject_file(project_dir: Path) -> Optional[Path]:
    """Find .uproject file in project directory.

    For detailed status messages, use find_uproject_file_with_reason() instead.

    Args:
        project_dir: Path to the project directory

    Returns:
        Path to the .uproject file, or None if not found/ambiguous
    """
    path, _ = find_uproject_file_with_reason(project_dir)
    return path


def read_uproject(path: Path) -> Dict[str, Any]:
    """Read and parse .uproject JSON file.

    Args:
        path: Path to the .uproject file

    Returns:
        Parsed JSON data as dict
    """
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def write_uproject(path: Path, data: Dict[str, Any]) -> None:
    """Write .uproject JSON file with tab indentation.

    Args:
        path: Path to the .uproject file
        data: Data to write
    """
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f, indent='\t')
