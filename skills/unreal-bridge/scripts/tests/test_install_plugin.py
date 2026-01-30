#!/usr/bin/env python3
"""Tests for install_plugin.py"""

import json
import tempfile
from pathlib import Path
import sys
import pytest

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent))

from install_plugin import (
    ensure_plugin_enabled,
    InstallResult,
    read_plugin_version,
    compare_versions,
)
from ue_project_utils import find_uproject_file


class TestFindUprojectFile:
    """Tests for find_uproject_file()"""

    def test_finds_single_uproject(self, tmp_path):
        """Should find the only .uproject file"""
        uproject = tmp_path / "MyGame.uproject"
        uproject.write_text('{}')

        result = find_uproject_file(tmp_path)
        assert result == uproject

    def test_returns_none_when_no_uproject(self, tmp_path):
        """Should return None when no .uproject exists"""
        result = find_uproject_file(tmp_path)
        assert result is None

    def test_prefers_matching_name_when_multiple(self, tmp_path):
        """Should prefer .uproject matching directory name"""
        project_dir = tmp_path / "MyGame"
        project_dir.mkdir()

        # Create multiple uprojects
        (project_dir / "MyGame.uproject").write_text('{}')
        (project_dir / "Other.uproject").write_text('{}')

        result = find_uproject_file(project_dir)
        assert result.name == "MyGame.uproject"

    def test_returns_none_when_multiple_no_match(self, tmp_path):
        """Should return None when multiple uprojects and none match dir name"""
        (tmp_path / "First.uproject").write_text('{}')
        (tmp_path / "Second.uproject").write_text('{}')

        result = find_uproject_file(tmp_path)
        assert result is None


class TestEnsurePluginEnabled:
    """Tests for ensure_plugin_enabled()"""

    def test_adds_plugin_when_plugins_list_empty(self):
        """Should add plugin when Plugins list is empty"""
        data = {"Plugins": []}
        modified = ensure_plugin_enabled(data, "TestPlugin")

        assert modified is True
        assert len(data["Plugins"]) == 1
        assert data["Plugins"][0] == {"Name": "TestPlugin", "Enabled": True}

    def test_adds_plugin_when_no_plugins_key(self):
        """Should create Plugins list when key doesn't exist"""
        data = {}
        modified = ensure_plugin_enabled(data, "TestPlugin")

        assert modified is True
        assert "Plugins" in data
        assert data["Plugins"] == [{"Name": "TestPlugin", "Enabled": True}]

    def test_enables_disabled_plugin(self):
        """Should enable existing disabled plugin"""
        data = {"Plugins": [{"Name": "TestPlugin", "Enabled": False}]}
        modified = ensure_plugin_enabled(data, "TestPlugin")

        assert modified is True
        assert data["Plugins"][0]["Enabled"] is True

    def test_no_change_when_already_enabled(self):
        """Should not modify when plugin already enabled"""
        data = {"Plugins": [{"Name": "TestPlugin", "Enabled": True}]}
        modified = ensure_plugin_enabled(data, "TestPlugin")

        assert modified is False
        assert len(data["Plugins"]) == 1

    def test_preserves_other_plugins(self):
        """Should not affect other plugins in the list"""
        data = {"Plugins": [
            {"Name": "OtherPlugin", "Enabled": True},
            {"Name": "AnotherPlugin", "Enabled": False}
        ]}
        ensure_plugin_enabled(data, "TestPlugin")

        assert len(data["Plugins"]) == 3
        assert data["Plugins"][0] == {"Name": "OtherPlugin", "Enabled": True}
        assert data["Plugins"][1] == {"Name": "AnotherPlugin", "Enabled": False}


class TestInstallResult:
    """Tests for InstallResult class"""

    def test_success_when_all_steps_pass(self):
        """Should report success when all steps pass"""
        result = InstallResult()
        result.add_step("Step 1", True)
        result.add_step("Step 2", True)
        result.add_step("Step 3", True)

        assert result.success is True

    def test_failure_when_any_step_fails(self):
        """Should report failure when any step fails"""
        result = InstallResult()
        result.add_step("Step 1", True)
        result.add_step("Step 2", False, "Something went wrong")
        result.add_step("Step 3", True)

        assert result.success is False

    def test_failure_when_first_step_fails(self):
        """Should report failure when first step fails"""
        result = InstallResult()
        result.add_step("Step 1", False, "Error")

        assert result.success is False

    def test_success_when_no_steps(self):
        """Should report success when no steps (edge case)"""
        result = InstallResult()
        assert result.success is True

    def test_rollback_executes_in_reverse_order(self):
        """Should execute rollback actions in reverse order"""
        result = InstallResult()
        order = []

        result.add_rollback(lambda: order.append(1))
        result.add_rollback(lambda: order.append(2))
        result.add_rollback(lambda: order.append(3))

        result.rollback()

        assert order == [3, 2, 1]

    def test_rollback_continues_on_error(self):
        """Should continue rollback even if one action fails"""
        result = InstallResult()
        executed = []

        def raise_error():
            raise Exception("fail")

        result.add_rollback(lambda: executed.append(1))
        result.add_rollback(raise_error)
        result.add_rollback(lambda: executed.append(3))

        result.rollback()

        # Should still execute 3 and 1 even though middle one failed
        assert 3 in executed
        assert 1 in executed

    def test_warnings_are_tracked(self):
        """Should track warnings separately from steps"""
        result = InstallResult()
        result.add_warning("This is a warning")
        result.add_warning("Another warning")

        assert len(result.warnings) == 2
        assert "This is a warning" in result.warnings


class TestVersionFunctions:
    """Tests for version comparison functions"""

    def test_read_plugin_version_valid(self, tmp_path):
        """Should read valid VERSION.json"""
        version_file = tmp_path / "VERSION.json"
        version_file.write_text('{"version": "2.0.0", "release_date": "2026-01-24"}')

        result = read_plugin_version(tmp_path)
        assert result["version"] == "2.0.0"

    def test_read_plugin_version_missing(self, tmp_path):
        """Should return None when VERSION.json missing"""
        result = read_plugin_version(tmp_path)
        assert result is None

    def test_read_plugin_version_invalid_json(self, tmp_path):
        """Should return None for invalid JSON"""
        version_file = tmp_path / "VERSION.json"
        version_file.write_text('not valid json')

        result = read_plugin_version(tmp_path)
        assert result is None

    def test_compare_versions_same(self):
        """Should return up_to_date when versions match"""
        assert compare_versions("2.0.0", "2.0.0") == "up_to_date"

    def test_compare_versions_different(self):
        """Should return update_available when versions differ"""
        assert compare_versions("1.0.0", "2.0.0") == "update_available"

    def test_compare_versions_not_installed(self):
        """Should return not_installed when installed is None"""
        assert compare_versions(None, "2.0.0") == "not_installed"

    def test_compare_versions_unknown_source(self):
        """Should return unknown when source version is None"""
        assert compare_versions("1.0.0", None) == "unknown"


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
