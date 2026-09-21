#!/bin/bash
# integration_test.sh

set -e

TEST_DIR="/tmp/scroller_test_$$"
SERVER_PORT=8081
SERVER_PID=""

cleanup() {
    echo "Cleaning up..."
    [ -n "$SERVER_PID" ] && kill $SERVER_PID 2>/dev/null
    rm -rf "$TEST_DIR"
}

trap cleanup EXIT INT TERM

# Setup test data
echo "Setting up test data..."
./build/debug/bin/scr_init "$TEST_DIR"

# Start server
echo "Starting server..."
./build/debug/bin/scroller "$TEST_DIR" &
SERVER_PID=$!
sleep 1

# Check server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start"
    exit 1
fi

# Run tests using expect
echo "Running tests..."

# Test 1: Create user
echo "Test 1: Create user"
expect << EOF
set timeout 10
spawn ./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u scroler
expect "scroller>"
send "CREATE USER new_user;\n"
expect "Ok"
send "exit\r"
expect eof
EOF

# Test 2: Create catalog
echo "Test 2: Create catalog"
expect << EOF
set timeout 10
spawn ./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u new_user
expect "scroller>"
send "CREATE CATALOG cat;\n"
expect "Ok"
send "exit\r"
expect eof
EOF

# Test 3: Create schema
echo "Test 3: Create schema"
expect << EOF
set timeout 10
spawn ./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u new_user -c cat
expect "scroller>"
send "CREATE SCHEMA sch;\n"
expect "Ok"
send "exit\r"
expect eof
EOF

# Test 4: Create table
echo "Test 4: Create table"
expect << EOF
set timeout 10
spawn ./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u new_user -c cat
expect "scroller>"
send "CREATE TABLE sch.users (id int, name char (16));\n"
expect "Ok"
send "exit\r"
expect eof
EOF

# Test 5: Insert data
echo "Test 5: Insert data"
expect << EOF
set timeout 10
spawn ./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u new_user -c cat
expect "scroller>"
send "INSERT INTO sch.users (id, name) VALUES (1, 'Alice');\n"
expect "Ok"
send "exit\r"
expect eof
EOF

# Test 6: Batch queries from file
echo "Test 6: Batch queries"
cat > "$TEST_DIR/queries.sql" << SQL
CREATE TABLE sch.products (id int, name char(16), price int);
INSERT INTO sch.products (id, name, price) VALUES (1, 'Kiwi', 25);
INSERT INTO sch.products (id, name, price) VALUES (2, 'Banana', 50);
SQL

./build/debug/bin/scrc -h localhost -p $SERVER_PORT -u new_user -c cat -f "$TEST_DIR/queries.sql"

echo "✅ All tests passed!"

