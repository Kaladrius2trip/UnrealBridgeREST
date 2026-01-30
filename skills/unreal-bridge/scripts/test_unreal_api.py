"""
Template for testing Unreal Python APIs

Usage:
    Modify the test_function() to test your specific API
    Run via commandlet to verify it exists and works
"""

import unreal


def test_function():
    """Replace this with your actual test"""

    # Example: Test if API exists
    unreal.log("=== API Test Start ===")

    try:
        # Test your API here
        result = unreal.EditorLevelLibrary.get_selected_level_actors()
        unreal.log(f"✓ API EXISTS: Got {len(result)} actors")

    except AttributeError as e:
        unreal.log(f"✗ API NOT FOUND: {e}")

    except Exception as e:
        unreal.log(f"✗ ERROR: {e}")

    unreal.log("=== API Test End ===")


if __name__ == '__main__':
    test_function()
