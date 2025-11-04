#!/bin/bash

# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2025 Nordic Semiconductor ASA
#
# Generate code coverage report for DECT NR+ stack integration tests
# This script builds the test, runs it, and generates coverage reports
# focusing on the DECT NR+ stack components.

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Configuration
BUILD_DIR="${BUILD_DIR:-build}"
# Use absolute path for coverage directory to avoid issues when changing directories
COVERAGE_DIR="${COVERAGE_DIR:-${SCRIPT_DIR}/${BUILD_DIR}/coverage_report}"
BOARD="${BOARD:-native_sim}"
TIMEOUT="${TIMEOUT:-60}"

# DECT stack directories to include in coverage (only these specific paths)
DECT_STACK_DIRS=(
    "nrf/drivers/dect_nrp/nrf91_mac"
    "nrf/subsys/net/l2_dect_nrp"
    "nrf/subsys/net/lib/dect_nrp/src/utils"
)

# Note: Coverage will be filtered to only include files in DECT_STACK_DIRS above

echo -e "${GREEN}=== DECT NR+ Stack Coverage Measurement ===${NC}"
echo "Build directory: ${BUILD_DIR}"
echo "Coverage output: ${COVERAGE_DIR}"
echo ""

# Check if coverage is enabled in prj.conf
if ! grep -q "^CONFIG_COVERAGE=y" prj.conf; then
    echo -e "${YELLOW}Coverage not enabled in prj.conf, enabling it temporarily...${NC}"
    # Create a temporary prj.conf with coverage enabled
    sed -e 's/^# CONFIG_COVERAGE=y/CONFIG_COVERAGE=y/' \
        -e 's/^# CONFIG_COVERAGE_NATIVE_GCOV=y/CONFIG_COVERAGE_NATIVE_GCOV=y/' \
        prj.conf > prj.conf.coverage

    # Check if coverage was actually enabled
    if ! grep -q "^CONFIG_COVERAGE=y" prj.conf.coverage; then
        echo -e "${YELLOW}Adding coverage config to temporary file...${NC}"
        # Add coverage config if not present
        echo "" >> prj.conf.coverage
        echo "# Code Coverage Support (temporarily enabled for coverage measurement)"
		>> prj.conf.coverage
        echo "CONFIG_COVERAGE=y" >> prj.conf.coverage
        echo "CONFIG_COVERAGE_NATIVE_GCOV=y" >> prj.conf.coverage
    fi

    USE_TEMP_PRJ_CONF=true
    ORIGINAL_PRJ_CONF="prj.conf"
    PRJ_CONF_TO_USE="prj.conf.coverage"
    echo "Using temporary prj.conf with coverage enabled"
else
    USE_TEMP_PRJ_CONF=false
    PRJ_CONF_TO_USE="prj.conf"
    echo "Coverage already enabled in prj.conf"
fi
echo ""

# Step 1: Clean and build with coverage
echo -e "${YELLOW}[1/5] Building test with coverage enabled...${NC}"

# Build with coverage-enabled prj.conf
if [ "${USE_TEMP_PRJ_CONF}" = true ]; then
    echo "Building with temporary prj.conf.coverage..."
    # Use the temporary prj.conf file
    cp "${PRJ_CONF_TO_USE}" prj.conf.bak
    mv "${PRJ_CONF_TO_USE}" prj.conf
    west build -p -b "${BOARD}" . -d "${BUILD_DIR}"
    mv prj.conf "${PRJ_CONF_TO_USE}"
    mv prj.conf.bak prj.conf
else
    west build -p -b "${BOARD}" . -d "${BUILD_DIR}"
fi

# Determine the actual build output directory
# With west build, the structure is: build/<project_name>/zephyr/
PROJECT_NAME=$(basename "${SCRIPT_DIR}")
ZEPHYR_BUILD_DIR="${BUILD_DIR}/${PROJECT_NAME}/zephyr"
ZEPHYR_EXE="${ZEPHYR_BUILD_DIR}/zephyr.exe"

# Fallback: try without project name subdirectory (for some west versions)
if [ ! -f "${ZEPHYR_EXE}" ]; then
    ZEPHYR_BUILD_DIR="${BUILD_DIR}/zephyr"
    ZEPHYR_EXE="${ZEPHYR_BUILD_DIR}/zephyr.exe"
fi

if [ ! -f "${ZEPHYR_EXE}" ]; then
    echo -e "${RED}ERROR: Build failed - zephyr.exe not found${NC}"
    echo "Checked: ${BUILD_DIR}/${PROJECT_NAME}/zephyr/zephyr.exe"
    echo "Checked: ${BUILD_DIR}/zephyr/zephyr.exe"
    echo "Build directory structure:"
    find "${BUILD_DIR}" -name "zephyr.exe" -type f 2>/dev/null | head -5
    exit 1
fi

echo -e "${GREEN}Build completed successfully${NC}"
echo "Using executable: ${ZEPHYR_EXE}"
echo ""

# Step 2: Run the test to generate coverage data
echo -e "${YELLOW}[2/5] Running tests to generate coverage data...${NC}"
cd "${ZEPHYR_BUILD_DIR}"

# Verify coverage was enabled in build
if ! strings ./zephyr.exe | grep -q "__gcov"; then
    echo -e "${RED}WARNING: Coverage instrumentation not detected in executable${NC}"
    echo "This may indicate CONFIG_COVERAGE was not properly enabled during build"
    echo ""
fi

# Run the test executable - it will generate .gcda files
# For native_sim, we need to let it exit naturally or it won't write coverage data
# Using timeout but allowing normal exit
echo "Running tests (timeout: ${TIMEOUT}s)..."
timeout "${TIMEOUT}s" ./zephyr.exe 2>&1 | tee /tmp/zephyr_test_output.log || {
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo -e "${YELLOW}Test timed out after ${TIMEOUT}s${NC}"
        echo "Killing process to ensure coverage data is written..."
        pkill -9 -f zephyr.exe 2>/dev/null || true
        sleep 1
    elif [ $EXIT_CODE -eq 130 ]; then
        echo -e "${YELLOW}Test interrupted (this may prevent coverage data generation)${NC}"
    else
        echo -e "${GREEN}Test exited with code $EXIT_CODE${NC}"
    fi
}

# Wait a moment for coverage data to be flushed to disk
sleep 1

cd - > /dev/null
echo -e "${GREEN}Test execution completed${NC}"
echo ""

# Step 3: Check for coverage data files
echo -e "${YELLOW}[3/5] Checking for coverage data files...${NC}"

# Search for .gcda files in build directory
GCDA_FILES=$(find "${BUILD_DIR}" -name "*.gcda" 2>/dev/null | head -100)
# Count non-empty lines
if [ -z "${GCDA_FILES}" ]; then
    GCDA_COUNT=0
else
    GCDA_COUNT=$(echo "${GCDA_FILES}" | grep -v "^$" | wc -l)
fi

if [ "$GCDA_COUNT" -eq 0 ] || [ -z "${GCDA_FILES}" ]; then
    echo -e "${RED}WARNING: No .gcda files found.${NC}"
    echo ""
    echo "Troubleshooting steps:"
    echo "1. Verify coverage is enabled:"
    echo "   grep CONFIG_COVERAGE ${SCRIPT_DIR}/prj.conf"
    echo ""
    echo "2. Check if executable has coverage instrumentation:"
    echo "   strings ${ZEPHYR_EXE} | grep __gcov | head -3"
    echo ""
    echo "3. Coverage data is written on program exit. Ensure the test:"
    echo "   - Exits cleanly (calls exit() or returns from main)"
    echo "   - Is not killed by timeout before it completes"
    echo ""
    echo "4. Try running the executable manually and check for .gcda files:"
    echo "   cd ${ZEPHYR_BUILD_DIR}"
    echo "   ./zephyr.exe"
    echo "   find . -name '*.gcda' | head -5"
    echo ""

    # Continue anyway - lcov might still find data
    echo -e "${YELLOW}Continuing with coverage report generation (may fail)${NC}"
else
    echo -e "${GREEN}Found ${GCDA_COUNT} coverage data files${NC}"
    echo "Sample locations:"
    echo "${GCDA_FILES}" | head -5 | while read -r file; do
        echo "  - ${file}"
    done
fi
echo ""

# Step 4: Generate coverage report using lcov
echo -e "${YELLOW}[4/5] Generating coverage report with lcov...${NC}"

# Check for lcov
if ! command -v lcov &> /dev/null; then
    echo -e "${RED}ERROR: lcov not found. Please install lcov:${NC}"
    echo "  sudo apt-get install lcov  # Debian/Ubuntu"
    echo "  sudo yum install lcov      # RHEL/CentOS"
    exit 1
fi

# Create coverage directory (use absolute path)
COVERAGE_DIR=$(cd "${SCRIPT_DIR}" && readlink -f "${COVERAGE_DIR}" || echo "${COVERAGE_DIR}")
mkdir -p "${COVERAGE_DIR}"
echo "Coverage output directory: ${COVERAGE_DIR}"

# Get workspace root (assuming we're in NCS)
# Script is at: nrf/tests/drivers/dect_nrp/nrf91_mac/integration/
# Workspace root should be: ../../../../../../ (7 levels up: integration ->
# nrf91_mac -> dect_nrp -> drivers -> tests -> nrf -> workspace root)
if [ -z "${WORKSPACE_ROOT}" ]; then
    # Try NCS structure: go up 7 levels from integration/ directory
    TEST_ROOT="${SCRIPT_DIR}/../../../../../../"
    if [ -d "${TEST_ROOT}/nrf" ] && [ -d "${TEST_ROOT}/zephyr" ]; then
        WORKSPACE_ROOT=$(cd "${TEST_ROOT}" && pwd)
    else
        # Try 6 levels up as fallback
        TEST_ROOT="${SCRIPT_DIR}/../../../../../"
        if [ -d "${TEST_ROOT}/nrf" ] && [ -d "${TEST_ROOT}/zephyr" ]; then
            WORKSPACE_ROOT=$(cd "${TEST_ROOT}" && pwd)
        else
            # Last resort: use ZEPHYR_BASE if set
            if [ -n "${ZEPHYR_BASE}" ]; then
                WORKSPACE_ROOT=$(dirname "${ZEPHYR_BASE}")
            else
                # Default: assume 7 levels up
                WORKSPACE_ROOT=$(cd "${SCRIPT_DIR}/../../../../../../" 2>/dev/null && pwd ||
			echo "${SCRIPT_DIR}/../../../../../../")
                echo -e "${YELLOW}WARNING: Using default workspace root, may be incorrect${NC}"
            fi
        fi
    fi
fi

echo "Workspace root: ${WORKSPACE_ROOT}"

# Verify workspace root looks correct
if [ ! -d "${WORKSPACE_ROOT}/nrf" ] || [ ! -d "${WORKSPACE_ROOT}/zephyr" ]; then
    echo -e "${RED}ERROR: Workspace root is incorrect${NC}"
    echo "Expected directories: ${WORKSPACE_ROOT}/nrf and ${WORKSPACE_ROOT}/zephyr"
    echo "Got: ${WORKSPACE_ROOT}"
    exit 1
fi

# Capture coverage data from build directory
# Try multiple possible locations for coverage data
COVERAGE_CAPTURED=false

# Convert paths to absolute for lcov
ABS_BUILD_DIR=$(cd "${SCRIPT_DIR}/${BUILD_DIR}" && pwd)
ABS_COVERAGE_INFO="${COVERAGE_DIR}/coverage.info"

# Try capturing from project-specific build directory
if [ -d "${ABS_BUILD_DIR}/${PROJECT_NAME}" ]; then
    cd "${ABS_BUILD_DIR}/${PROJECT_NAME}"
    if lcov --capture \
            --directory . \
            --output-file "${ABS_COVERAGE_INFO}" \
            --rc lcov_branch_coverage=1 \
            --quiet 2>/dev/null; then
        COVERAGE_CAPTURED=true
        echo "Captured coverage from: ${ABS_BUILD_DIR}/${PROJECT_NAME}"
    fi
    cd - > /dev/null
fi

# Fallback: try from main build directory
if [ "${COVERAGE_CAPTURED}" = false ]; then
    cd "${ABS_BUILD_DIR}"
    if lcov --capture \
            --directory . \
            --output-file "${ABS_COVERAGE_INFO}" \
            --rc lcov_branch_coverage=1 \
            --quiet 2>/dev/null; then
        COVERAGE_CAPTURED=true
        echo "Captured coverage from: ${ABS_BUILD_DIR}"
    fi
    cd - > /dev/null
fi

# Fallback: try from zephyr subdirectory
if [ "${COVERAGE_CAPTURED}" = false ] && [ -d "${ABS_BUILD_DIR}/${PROJECT_NAME}/zephyr" ]; then
    cd "${ABS_BUILD_DIR}/${PROJECT_NAME}/zephyr"
    if lcov --capture \
            --directory . \
            --output-file "${ABS_COVERAGE_INFO}" \
            --rc lcov_branch_coverage=1 \
            --quiet 2>/dev/null; then
        COVERAGE_CAPTURED=true
        echo "Captured coverage from: ${ABS_BUILD_DIR}/${PROJECT_NAME}/zephyr"
    fi
    cd - > /dev/null
fi

if [ "${COVERAGE_CAPTURED}" = false ]; then
    echo -e "${YELLOW}WARNING: lcov capture failed. Attempting with error output...${NC}"
    # Last attempt with verbose output
    cd "${ABS_BUILD_DIR}"
    if lcov --capture \
         --directory . \
         --output-file "${ABS_COVERAGE_INFO}" \
         --rc lcov_branch_coverage=1 2>&1 | head -20; then
        COVERAGE_CAPTURED=true
    else
        echo -e "${RED}ERROR: lcov capture failed${NC}"
        echo "Tried directories:"
        echo "  ${ABS_BUILD_DIR}/${PROJECT_NAME}"
        echo "  ${ABS_BUILD_DIR}"
        echo "  ${ABS_BUILD_DIR}/${PROJECT_NAME}/zephyr"
        echo ""
        echo "Coverage output file: ${ABS_COVERAGE_INFO}"
        echo ""
        echo "Looking for .gcda files..."
        find "${ABS_BUILD_DIR}" -name "*.gcda" 2>/dev/null | head -10
        exit 1
    fi
fi

cd - > /dev/null

# Filter coverage to DECT stack files only using a more reliable method
echo "Filtering coverage to DECT NR+ stack files..."

# Strategy: Use lcov to extract by matching actual file paths in the coverage data
# Build a combined pattern from all DECT directories
TEMP_COVERAGE="${COVERAGE_DIR}/temp_dect_coverage.info"
FINAL_COVERAGE="${COVERAGE_DIR}/dect_coverage.info"
rm -f "${TEMP_COVERAGE}" "${FINAL_COVERAGE}"

# Extract each directory separately and combine
# lcov --extract matches against file paths in the tracefile using shell glob patterns
COVERAGE_EXTRACTED=false
for dir in "${DECT_STACK_DIRS[@]}"; do
    abs_dir="${WORKSPACE_ROOT}/${dir}"
    if [ ! -d "${abs_dir}" ]; then
        echo -e "${YELLOW}WARNING: Directory not found: ${abs_dir}${NC}"
        continue
    fi

    echo "  Extracting: ${dir}"

    # Try multiple pattern approaches
    EXTRACT_SUCCESS=false

    # Method 1: Try exact directory path (matches files starting with this path)
    if [ "${COVERAGE_EXTRACTED}" = false ]; then
        if lcov --extract "${COVERAGE_DIR}/coverage.info" \
             "${abs_dir}/*" \
             --output-file "${TEMP_COVERAGE}" \
             --rc lcov_branch_coverage=1 \
             --quiet 2>&1; then
            EXTRACT_SUCCESS=true
            COVERAGE_EXTRACTED=true
        fi
    else
        # Add to existing
        TEMP_FILE="${TEMP_COVERAGE}.add"
        if lcov --extract "${COVERAGE_DIR}/coverage.info" \
             "${abs_dir}/*" \
             --output-file "${TEMP_FILE}" \
             --rc lcov_branch_coverage=1 \
             --quiet 2>&1; then
            # Merge with existing
            if [ -f "${TEMP_COVERAGE}" ]; then
                lcov --add-tracefile "${TEMP_FILE}" \
                     --add-tracefile "${TEMP_COVERAGE}" \
                     --output-file "${TEMP_COVERAGE}.merged" \
                     --rc lcov_branch_coverage=1 \
                     --quiet 2>&1 && \
                mv "${TEMP_COVERAGE}.merged" "${TEMP_COVERAGE}" && \
                rm -f "${TEMP_FILE}" && \
                EXTRACT_SUCCESS=true
            else
                mv "${TEMP_FILE}" "${TEMP_COVERAGE}"
                EXTRACT_SUCCESS=true
            fi
        fi
    fi

    # Method 2: If method 1 failed, try with directory path without wildcard
    if [ "${EXTRACT_SUCCESS}" = false ] && [ "${COVERAGE_EXTRACTED}" = false ]; then
        if lcov --extract "${COVERAGE_DIR}/coverage.info" \
             "${abs_dir}" \
             --output-file "${TEMP_COVERAGE}" \
             --rc lcov_branch_coverage=1 \
             --quiet 2>&1; then
            EXTRACT_SUCCESS=true
            COVERAGE_EXTRACTED=true
        fi
    fi

    if [ "${EXTRACT_SUCCESS}" = false ]; then
        echo -e "${YELLOW}WARNING: Could not extract ${dir}, check if any files from this"
		"directory have coverage data${NC}"
    fi
done

# If lcov extraction failed or produced empty result, use Python script to filter
USE_PYTHON_FILTER=false
if [ ! -f "${TEMP_COVERAGE}" ] || [ ! -s "${TEMP_COVERAGE}" ]; then
    USE_PYTHON_FILTER=true
else
    # Check if the extracted file has any DECT files (count SF: lines)
    FILE_COUNT=$(lcov --list "${TEMP_COVERAGE}" 2>&1 | grep -c "^SF:" || echo "0")
    if [ "${FILE_COUNT}" -eq 0 ]; then
        USE_PYTHON_FILTER=true
        echo "lcov extraction produced 0 files, using Python filter"
    fi
fi

if [ "${USE_PYTHON_FILTER}" = true ]; then
    echo "Using Python-based filtering (more reliable)..."

    # Build list of absolute target directories (properly quoted for spaces)
    TARGET_DIR_ARGS=()
    for dir in "${DECT_STACK_DIRS[@]}"; do
        abs_dir="${WORKSPACE_ROOT}/${dir}"
        abs_dir=$(cd "${WORKSPACE_ROOT}" && realpath "${dir}" 2>/dev/null || echo "${abs_dir}")
        if [ -d "${abs_dir}" ]; then
            TARGET_DIR_ARGS+=("${abs_dir}")
            echo "    Including: ${abs_dir}"
        else
            echo -e "${YELLOW}    WARNING: Directory not found: ${abs_dir}${NC}"
        fi
    done
    # Use Python script to filter
    if [ ${#TARGET_DIR_ARGS[@]} -gt 0 ]; then
        if python3 "${SCRIPT_DIR}/filter_coverage.py" \
             "${COVERAGE_DIR}/coverage.info" \
             "${FINAL_COVERAGE}" \
             "${TARGET_DIR_ARGS[@]}" 2>&1; then
            echo -e "${GREEN}Python filtering succeeded${NC}"
        else
            echo -e "${RED}ERROR: Python filtering failed${NC}"
            echo "Falling back to full coverage (this will include all files)"
            cp "${COVERAGE_DIR}/coverage.info" "${FINAL_COVERAGE}"
        fi
    else
        echo -e "${RED}ERROR: No target directories found${NC}"
        cp "${COVERAGE_DIR}/coverage.info" "${FINAL_COVERAGE}"
    fi
else
    mv "${TEMP_COVERAGE}" "${FINAL_COVERAGE}"
    echo -e "${GREEN}Filtered coverage using lcov${NC}"
fi

# Verify filtering worked
if [ -f "${FINAL_COVERAGE}" ] && [ -s "${FINAL_COVERAGE}" ]; then
    FILE_COUNT=$(lcov --list "${FINAL_COVERAGE}" 2>&1 | grep -c "^SF:" || echo "0")
    echo -e "${GREEN}Filtered coverage to: ${DECT_STACK_DIRS[*]}${NC}"
    echo "  Files in filtered report: ${FILE_COUNT}"

    # Show sample of included files
    echo "  Sample included files:"
    lcov --list "${FINAL_COVERAGE}" 2>&1 |
    	grep "^SF:" | sed 's|^SF:||' | head -3 | while read -r f; do
        echo "    - $(basename "${f}")"
    done
else
    echo -e "${RED}ERROR: Failed to create filtered coverage file${NC}"
    echo "Falling back to full coverage (this will include all files)"
    cp "${COVERAGE_DIR}/coverage.info" "${FINAL_COVERAGE}"
fi

echo -e "${GREEN}Coverage data processed${NC}"
echo ""

# Step 5: Generate HTML report
echo -e "${YELLOW}[5/5] Generating HTML coverage report...${NC}"

# Check for genhtml
if ! command -v genhtml &> /dev/null; then
    echo -e "${RED}ERROR: genhtml not found (part of lcov package)${NC}"
    exit 1
fi

# Generate HTML report
genhtml "${COVERAGE_DIR}/dect_coverage.info" \
        --output-directory "${COVERAGE_DIR}/html" \
        --quiet \
        --ignore-errors source \
        --branch-coverage \
        --highlight \
        --legend \
        --title "DECT NR+ Stack Coverage Report" || {
    echo -e "${RED}ERROR: HTML report generation failed${NC}"
    exit 1
}

# Generate summary
echo ""
echo -e "${GREEN}=== Coverage Summary ===${NC}"
lcov --summary "${COVERAGE_DIR}/dect_coverage.info" 2>/dev/null || true

echo ""
echo -e "${GREEN}=== Coverage Report Generated ===${NC}"

# Convert to relative path from script directory for cleaner output
HTML_REPORT_ABS="${COVERAGE_DIR}/html/index.html"
HTML_REPORT_REL=$(realpath --relative-to="${SCRIPT_DIR}" "${HTML_REPORT_ABS}" \
    2>/dev/null || echo "${HTML_REPORT_ABS}")

echo "HTML Report: ${HTML_REPORT_REL}"
echo ""
echo "View the report with:"
echo "  firefox ${HTML_REPORT_REL}"
echo "  or"
echo "  xdg-open ${HTML_REPORT_REL}"
echo ""

# Generate JSON summary using gcovr if available
if command -v gcovr &> /dev/null; then
    echo -e "${YELLOW}Generating gcovr summary...${NC}"
    # Build filter patterns for gcovr
    GCOVR_FILTERS=""
    for dir in "${DECT_STACK_DIRS[@]}"; do
        abs_dir="${WORKSPACE_ROOT}/${dir}"
        if [ -d "${abs_dir}" ]; then
            GCOVR_FILTERS="${GCOVR_FILTERS} --filter ${abs_dir}/.*"
        fi
    done

    # Generate JSON summary
    gcovr -r "${WORKSPACE_ROOT}" \
          ${GCOVR_FILTERS} \
          --json \
          --output "${COVERAGE_DIR}/coverage.json" \
          --root "${WORKSPACE_ROOT}" \
          "${BUILD_DIR}" 2>/dev/null || true

    # Generate text summary
    gcovr -r "${WORKSPACE_ROOT}" \
          ${GCOVR_FILTERS} \
          --root "${WORKSPACE_ROOT}" \
          "${BUILD_DIR}" > "${COVERAGE_DIR}/coverage_summary.txt" 2>/dev/null || true

    if [ -f "${COVERAGE_DIR}/coverage_summary.txt" ]; then
        echo ""
        echo -e "${GREEN}=== Gcovr Summary ===${NC}"
        cat "${COVERAGE_DIR}/coverage_summary.txt"
    fi
fi

echo ""
echo -e "${GREEN}Coverage measurement complete!${NC}"

# Clean up temporary prj.conf if created
if [ "${USE_TEMP_PRJ_CONF}" = true ] && [ -f "${PRJ_CONF_TO_USE}" ]; then
    rm -f "${PRJ_CONF_TO_USE}"
    echo "Cleaned up temporary prj.conf file"
fi

