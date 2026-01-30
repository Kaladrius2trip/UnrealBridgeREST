#!/usr/bin/env python3
"""
Silent installer for UnrealPythonREST plugin into Unreal Engine projects.

Usage:
    python install_plugin.py <project_dir> [--dry-run] [--force] [--verbose] [--quiet] [--check]

Example:
    python install_plugin.py "G:/UE_Projects/MyGame"
    python install_plugin.py "G:/UE_Projects/MyGame" --check
    python install_plugin.py "G:/UE_Projects/MyGame" --quiet
"""

from __future__ import annotations

import argparse
import json
import shutil
import sys
from pathlib import Path
from typing import Callable, Dict, Any, List, Tuple, Optional

from ue_project_utils import find_uproject_file_with_reason, read_uproject, write_uproject

# Plugin source location (in skill assets directory)
PLUGIN_SOURCE = Path.home() / ".claude" / "skills" / "unreal-bridge" / "assets" / "UnrealPythonREST"

# Required plugins that must be enabled
REQUIRED_PLUGINS = ["PythonScriptPlugin", "UnrealPythonREST"]

# Version tracking
VERSION_FILE = "VERSION.json"


def read_plugin_version(plugin_dir: Path) -> Optional[Dict[str, Any]]:
    """Read VERSION.json from plugin directory."""
    version_path = plugin_dir / VERSION_FILE
    if not version_path.exists():
        return None
    try:
        with open(version_path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError):
        return None


def get_source_version() -> Optional[str]:
    """Get version string from source plugin."""
    info = read_plugin_version(PLUGIN_SOURCE)
    return info.get("version") if info else None


def get_installed_version(project_dir: Path) -> Optional[str]:
    """Get version string from installed plugin in project."""
    plugin_dir = project_dir / "Plugins" / "UnrealPythonREST"
    info = read_plugin_version(plugin_dir)
    return info.get("version") if info else None


def compare_versions(installed: Optional[str], source: Optional[str]) -> str:
    """
    Compare versions.

    Returns:
        'up_to_date': versions match
        'update_available': source is different (newer assumed)
        'not_installed': no installed version
        'unknown': source version unknown
    """
    if not installed:
        return "not_installed"
    if not source:
        return "unknown"
    if installed == source:
        return "up_to_date"
    return "update_available"


class InstallResult:
    """Tracks installation steps and results."""

    def __init__(self):
        self.steps: List[Tuple[str, bool, str]] = []
        self.warnings: List[str] = []
        self.rollback_actions: List[Callable[[], None]] = []

    def add_step(self, name: str, success: bool, message: str = ""):
        self.steps.append((name, success, message))

    def add_warning(self, message: str):
        self.warnings.append(message)

    def add_rollback(self, action: Callable[[], None]):
        self.rollback_actions.append(action)

    @property
    def success(self) -> bool:
        return all(s[1] for s in self.steps)

    def rollback(self):
        """Execute rollback actions in reverse order."""
        for action in reversed(self.rollback_actions):
            try:
                action()
            except Exception:
                pass  # Best effort


def write_uproject_with_backup(path: Path, data: Dict[str, Any], backup: bool = True) -> Optional[Path]:
    """Write .uproject with optional backup."""
    backup_path = None
    if backup:
        backup_path = path.with_suffix('.uproject.backup')
        shutil.copy2(path, backup_path)
    write_uproject(path, data)
    return backup_path


def ensure_plugin_enabled(uproject_data: Dict[str, Any], plugin_name: str) -> bool:
    """Ensure plugin is enabled in .uproject data. Returns True if modified."""
    if "Plugins" not in uproject_data:
        uproject_data["Plugins"] = []

    plugins = uproject_data["Plugins"]

    # Check if already enabled
    for plugin in plugins:
        if plugin.get("Name") == plugin_name:
            if plugin.get("Enabled", False):
                return False  # Already enabled
            else:
                plugin["Enabled"] = True
                return True

    # Add new plugin entry
    plugins.append({
        "Name": plugin_name,
        "Enabled": True
    })
    return True


def copy_plugin(source: Path, dest: Path, result: InstallResult) -> Tuple[bool, Optional[Path]]:
    """
    Copy plugin directory to project Plugins folder.

    Returns:
        Tuple of (success, backup_path) where backup_path is set if existing plugin was backed up
    """
    backup_path: Optional[Path] = None
    try:
        if dest.exists():
            result.add_warning(f"Plugin already exists at {dest}, will be overwritten")
            # Backup existing to Saved folder (NOT in Plugins to avoid compilation)
            saved_dir = dest.parent.parent / "Saved" / "PluginBackups"
            saved_dir.mkdir(parents=True, exist_ok=True)
            backup_path = saved_dir / (dest.name + ".backup")
            if backup_path.exists():
                shutil.rmtree(backup_path)
            shutil.move(str(dest), str(backup_path))
            result.add_rollback(lambda bp=backup_path, d=dest: shutil.move(str(bp), str(d)))

        # Copy plugin (exclude .git)
        shutil.copytree(
            source,
            dest,
            ignore=shutil.ignore_patterns('.git', '.git*', '*.pyc', '__pycache__')
        )

        result.add_step("Copy plugin", True, f"Copied to {dest}")
        return True, backup_path

    except Exception as e:
        result.add_step("Copy plugin", False, str(e))
        return False, backup_path


def update_uproject_file(uproject_path: Path, result: InstallResult) -> bool:
    """Update .uproject to enable required plugins."""
    try:
        # Read current content
        data = read_uproject(uproject_path)

        # Backup and enable plugins
        backup_path = write_uproject_with_backup(uproject_path, data, backup=True)
        if backup_path:
            result.add_rollback(lambda bp=backup_path, up=uproject_path: shutil.copy2(bp, up))

        modified = False
        for plugin_name in REQUIRED_PLUGINS:
            if ensure_plugin_enabled(data, plugin_name):
                modified = True
                result.add_step(f"Enable {plugin_name}", True, "Added to .uproject")
            else:
                result.add_step(f"Enable {plugin_name}", True, "Already enabled")

        if modified:
            write_uproject(uproject_path, data)

        return True

    except Exception as e:
        result.add_step("Update .uproject", False, str(e))
        return False


def verify_installation(project_dir: Path, result: InstallResult) -> bool:
    """Verify plugin installation is complete."""
    plugin_dir = project_dir / "Plugins" / "UnrealPythonREST"

    # Check plugin files exist
    required_files = [
        "UnrealPythonREST.uplugin",
        "Source/UnrealPythonREST/UnrealPythonREST.Build.cs",
        "Source/UnrealPythonREST/Public/UnrealPythonREST.h",
    ]

    all_exist = True
    for rel_path in required_files:
        full_path = plugin_dir / rel_path
        if full_path.exists():
            result.add_step(f"Verify {rel_path}", True)
        else:
            result.add_step(f"Verify {rel_path}", False, "File missing")
            all_exist = False

    return all_exist


def install_plugin(project_dir: str, dry_run: bool = False, force: bool = False,
                   check_only: bool = False, update: bool = False) -> InstallResult:
    """
    Install UnrealPythonREST plugin into Unreal Engine project.

    Args:
        project_dir: Path to UE project directory
        dry_run: If True, only show what would be done
        force: If True, overwrite existing installation
        check_only: If True, only check if installation is possible
        update: If True, update existing installation to latest version

    Returns:
        InstallResult with step-by-step results
    """
    result = InstallResult()
    project_path = Path(project_dir).resolve()

    # Validate project directory
    if not project_path.is_dir():
        result.add_step("Validate project", False, f"Directory not found: {project_path}")
        return result

    result.add_step("Validate project", True, str(project_path))

    # Find .uproject file
    uproject_path, uproject_msg = find_uproject_file_with_reason(project_path)
    if not uproject_path:
        result.add_step("Find .uproject", False, uproject_msg)
        return result

    result.add_step("Find .uproject", True, f"{uproject_path.name} - {uproject_msg}")

    # Check plugin source exists
    if not PLUGIN_SOURCE.is_dir():
        result.add_step("Check plugin source", False, f"Plugin not found at {PLUGIN_SOURCE}")
        return result

    result.add_step("Check plugin source", True, str(PLUGIN_SOURCE))

    # Destination
    plugins_dir = project_path / "Plugins"
    plugin_dest = plugins_dir / "UnrealPythonREST"

    # Check if already installed
    if plugin_dest.exists():
        installed_version = get_installed_version(project_path)
        source_version = get_source_version()

        if update:
            # Update mode: check versions and proceed
            comparison = compare_versions(installed_version, source_version)
            if comparison == "up_to_date":
                result.add_step("Version check", True,
                    f"Already at latest version ({installed_version}). No update needed.")
                return result
            elif comparison == "update_available":
                result.add_step("Version check", True,
                    f"Updating {installed_version} → {source_version}")
            else:
                result.add_step("Version check", True,
                    "Updating (versions unknown, proceeding)")
        elif force:
            # Force mode: proceed with overwrite
            result.add_step("Force mode", True, "Will overwrite existing installation")
        else:
            # Normal mode: suggest update instead of force
            version_info = f" (installed: {installed_version})" if installed_version else ""
            result.add_step("Check existing", False,
                f"Plugin already installed{version_info}. Use --update to upgrade or --force to overwrite.")
            return result

    # Check-only mode: all validations passed
    if check_only:
        result.add_step("Check complete", True, "Installation is possible")
        return result

    if dry_run:
        result.add_step("Dry run", True, "Would copy plugin and update .uproject")
        return result

    # Create Plugins directory if needed
    plugins_dir.mkdir(exist_ok=True)

    # Copy plugin
    copy_success, backup_path = copy_plugin(PLUGIN_SOURCE, plugin_dest, result)
    if not copy_success:
        result.rollback()
        return result

    # Update .uproject
    if not update_uproject_file(uproject_path, result):
        result.rollback()
        return result

    # Verify
    verify_installation(project_path, result)

    # Clean up backup after successful installation
    if result.success and backup_path and backup_path.exists():
        try:
            shutil.rmtree(backup_path)
            result.add_step("Cleanup backup", True, "Removed old plugin backup")
        except Exception as e:
            result.add_warning(f"Could not remove backup at {backup_path}: {e}")

    return result


def print_result(result: InstallResult, quiet: bool = False):
    """Print installation result in a readable format."""
    if quiet:
        # Only print errors and final status
        failed_steps = [s for s in result.steps if not s[1]]
        for name, _, message in failed_steps:
            print(f"ERROR: {name} - {message}")

        if result.success:
            print("Installation complete!")
        else:
            print("Installation failed.")
        return

    # Verbose output (default behavior)
    print("\n" + "=" * 60)
    print("UnrealPythonREST Plugin Installation")
    print("=" * 60)

    for name, success, message in result.steps:
        status = "[OK]  " if success else "[FAIL]"
        msg = f" - {message}" if message else ""
        print(f"  {status} {name}{msg}")

    if result.warnings:
        print("\nWarnings:")
        for warning in result.warnings:
            print(f"  ! {warning}")

    print("=" * 60)
    if result.success:
        print("Installation complete!")

        # Show version info if available
        source_version = get_source_version()
        if source_version:
            print(f"\nInstalled version: {source_version}")

        print("\nNext steps:")
        print("  1. Open the project in Unreal Editor")
        print("  2. Plugin will auto-enable on first launch")
        print("  3. REST API available at http://localhost:8080/api/v1/")
    else:
        print("Installation failed. See errors above.")
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Install UnrealPythonREST plugin into Unreal Engine project",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python install_plugin.py "G:/UE_Projects/MyGame"
  python install_plugin.py "G:/UE_Projects/MyGame" --dry-run
  python install_plugin.py "G:/UE_Projects/MyGame" --force
  python install_plugin.py "G:/UE_Projects/MyGame" --update
  python install_plugin.py "G:/UE_Projects/MyGame" --check
  python install_plugin.py "G:/UE_Projects/MyGame" --quiet
        """
    )
    parser.add_argument("project_dir", help="Path to Unreal Engine project directory")
    parser.add_argument("--dry-run", "-n", action="store_true",
                       help="Show what would be done without making changes")
    parser.add_argument("--force", "-f", action="store_true",
                       help="Overwrite existing plugin installation")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Show detailed step-by-step output")
    parser.add_argument("--quiet", "-q", action="store_true",
                       help="Only show errors and final status")
    parser.add_argument("--check", "-c", action="store_true",
                       help="Check if installation is possible without installing")
    parser.add_argument("--update", "-u", action="store_true",
                       help="Update existing plugin installation to latest version")

    args = parser.parse_args()

    # Validate mutually exclusive options
    if args.quiet and args.verbose:
        print("Error: --quiet and --verbose are mutually exclusive")
        return 1

    if args.update and args.force:
        print("Error: --update and --force are mutually exclusive")
        return 1

    result = install_plugin(args.project_dir, args.dry_run, args.force, args.check, args.update)
    print_result(result, quiet=args.quiet)

    return 0 if result.success else 1


if __name__ == "__main__":
    sys.exit(main())
