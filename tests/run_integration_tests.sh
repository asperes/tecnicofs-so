#!/bin/bash
#
# run_integration_tests.sh - Integration tests for TecnicoFS client-server
# Runs end-to-end tests using the actual client and server binaries
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SERVER_BIN="$PROJECT_DIR/tecnicofs"
CLIENT_BIN="$PROJECT_DIR/client/tecnicofs-client"
SOCKET_NAME="test_socket_$$"
FIXTURES_DIR="$SCRIPT_DIR/fixtures"
OUTPUT_DIR="$SCRIPT_DIR/output"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counters
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Cleanup function
cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "/tmp/$SOCKET_NAME"
    rm -f /tmp/client-*
    rm -rf "$OUTPUT_DIR"
}

trap cleanup EXIT

# Start server
start_server() {
    "$SERVER_BIN" 4 "$SOCKET_NAME" &
    SERVER_PID=$!
    sleep 0.5  # Give server time to start
    
    if ! kill -0 "$SERVER_PID" 2>/dev/null; then
        echo -e "${RED}ERROR: Server failed to start${NC}"
        exit 1
    fi
}

# Stop server
stop_server() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "/tmp/$SOCKET_NAME"
    SERVER_PID=""
}

# Run a test
run_test() {
    local test_name="$1"
    local input_file="$2"
    local expected_output="$3"
    
    TESTS_RUN=$((TESTS_RUN + 1))
    printf "  Running: %-40s ... " "$test_name"
    
    # Run client and capture output
    local actual_output
    actual_output=$("$CLIENT_BIN" "$input_file" "$SOCKET_NAME" 2>&1) || true
    
    # Compare outputs (ignoring "Mounted!" and "Unmounted!" lines)
    local filtered_actual
    local filtered_expected
    filtered_actual=$(echo "$actual_output" | grep -v "^Mounted\|^Unmounted" || true)
    filtered_expected=$(echo "$expected_output" | grep -v "^Mounted\|^Unmounted" || true)
    
    if [ "$filtered_actual" = "$filtered_expected" ]; then
        echo -e "${GREEN}PASSED${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}FAILED${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        echo "    Expected:"
        echo "$filtered_expected" | sed 's/^/      /'
        echo "    Actual:"
        echo "$filtered_actual" | sed 's/^/      /'
    fi
}

# Create test fixtures if they don't exist
create_fixtures() {
    mkdir -p "$FIXTURES_DIR"
    mkdir -p "$OUTPUT_DIR"
    
    # Test 1: Basic create and lookup
    cat > "$FIXTURES_DIR/test_basic.txt" << 'EOF'
c /file1 f
c /dir1 d
l /file1
l /dir1
l /nonexistent
EOF
    
    # Test 2: Nested operations
    cat > "$FIXTURES_DIR/test_nested.txt" << 'EOF'
c /dir1 d
c /dir1/dir2 d
c /dir1/dir2/file1 f
l /dir1/dir2/file1
EOF
    
    # Test 3: Delete operations
    cat > "$FIXTURES_DIR/test_delete.txt" << 'EOF'
c /dir1 d
c /dir1/file1 f
d /dir1/file1
l /dir1/file1
d /dir1
l /dir1
EOF
    
    # Test 4: Move operations
    cat > "$FIXTURES_DIR/test_move.txt" << 'EOF'
c /dir1 d
c /dir2 d
c /dir1/file1 f
l /dir1/file1
m /dir1/file1 /dir2/file1
l /dir1/file1
l /dir2/file1
EOF
    
    # Test 5: Error cases
    cat > "$FIXTURES_DIR/test_errors.txt" << 'EOF'
c /nonexistent/file1 f
d /nonexistent
l /nonexistent
c /file1 f
c /file1 f
EOF
}

# Main test execution
main() {
    echo ""
    echo "=== TecnicoFS Integration Tests ==="
    echo ""
    
    # Check binaries exist
    if [ ! -x "$SERVER_BIN" ]; then
        echo -e "${RED}ERROR: Server binary not found at $SERVER_BIN${NC}"
        echo "Run 'make' in the project directory first."
        exit 1
    fi
    
    if [ ! -x "$CLIENT_BIN" ]; then
        echo -e "${RED}ERROR: Client binary not found at $CLIENT_BIN${NC}"
        echo "Run 'make' in the client directory first."
        exit 1
    fi
    
    # Create fixtures
    create_fixtures
    
    # Start server
    echo "Starting server..."
    start_server
    echo ""
    
    # Test 1: Basic create and lookup
    run_test "Basic create and lookup" "$FIXTURES_DIR/test_basic.txt" \
"Created file: /file1
Created directory: /dir1
Search: /file1 found
Search: /dir1 found
Search: /nonexistent not found"
    
    # Restart server for clean state
    stop_server
    start_server
    
    # Test 2: Nested operations
    run_test "Nested operations" "$FIXTURES_DIR/test_nested.txt" \
"Created directory: /dir1
Created directory: /dir1/dir2
Created file: /dir1/dir2/file1
Search: /dir1/dir2/file1 found"
    
    # Restart server
    stop_server
    start_server
    
    # Test 3: Delete operations
    run_test "Delete operations" "$FIXTURES_DIR/test_delete.txt" \
"Created directory: /dir1
Created file: /dir1/file1
Deleted: /dir1/file1
Search: /dir1/file1 not found
Deleted: /dir1
Search: /dir1 not found"
    
    # Restart server
    stop_server
    start_server
    
    # Test 4: Move operations
    run_test "Move operations" "$FIXTURES_DIR/test_move.txt" \
"Created directory: /dir1
Created directory: /dir2
Created file: /dir1/file1
Search: /dir1/file1 found
Moved: /dir1/file1 to /dir2/file1
Search: /dir1/file1 not found
Search: /dir2/file1 found"
    
    # Restart server
    stop_server
    start_server
    
    # Test 5: Error cases
    run_test "Error handling" "$FIXTURES_DIR/test_errors.txt" \
"Unable to create file: /nonexistent/file1
Unable to delete: /nonexistent
Search: /nonexistent not found
Created file: /file1
Unable to create file: /file1"
    
    # Stop server
    stop_server
    
    # Print summary
    echo ""
    echo "----------------------------------------"
    echo "Tests run: $TESTS_RUN"
    echo -e "${GREEN}Passed: $TESTS_PASSED${NC}"
    if [ $TESTS_FAILED -gt 0 ]; then
        echo -e "${RED}Failed: $TESTS_FAILED${NC}"
    else
        echo "Failed: 0"
    fi
    echo "----------------------------------------"
    
    # Return exit code
    if [ $TESTS_FAILED -gt 0 ]; then
        exit 1
    fi
    exit 0
}

main "$@"
