#!/bin/bash
# Helper script to run Unreal Python tests and display filtered logs

# Default paths - override with environment variables
UE_ENGINE="${UE_ENGINE_DIR:-G:/UE_5_3_Source/UnrealEngine}"
UE_PROJECT="${UE_PROJECT_FILE:-G:/UE_Projects/TempForMcp/TempForMcp.uproject}"
UE_LOG="${UE_LOG_FILE:-G:/UE_Projects/TempForMcp/Saved/Logs/TempForMcp.log}"

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <python_script> [num_lines]"
    echo "Example: $0 test.py 30"
    exit 1
fi

SCRIPT="$1"
NUM_LINES="${2:-20}"

if [ ! -f "$SCRIPT" ]; then
    echo "Error: Script not found: $SCRIPT"
    exit 1
fi

echo "Running: $SCRIPT"
echo "Engine: $UE_ENGINE"
echo "Project: $UE_PROJECT"
echo ""

# Run the script
"$UE_ENGINE/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$UE_PROJECT" \
    -run=pythonscript -script="$SCRIPT" \
    -unattended -nopause -nosplash -nullrhi > /dev/null 2>&1

# Check exit code
if [ $? -eq 0 ]; then
    echo "✓ Script executed"
else
    echo "✗ Script failed"
fi

echo ""
echo "=== Python Log Output (last $NUM_LINES lines) ==="
grep "LogPython:" "$UE_LOG" | tail -n "$NUM_LINES"

echo ""
echo "=== Errors/Warnings ==="
tail -100 "$UE_LOG" | grep -E "Error:|Warning:|Fatal" | tail -10
