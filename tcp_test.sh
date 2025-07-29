#!/bin/bash

# TCP Server API Test Script
# Tests the complete workflow: Enable BASIC, send program, run it, stop it, list it

SERVER="localhost:8080"

echo "=== Fujisan TCP Server API Test ==="
echo "Make sure to:"
echo "1. Start Fujisan emulator"
echo "2. Enable TCP Server via Tools menu"
echo "3. Run this script"
echo

# Function to send command and show response
send_command() {
    local command="$1"
    local description="$2"
    echo ">>> $description"
    echo "Command: $command"
    echo "Response:"
    echo "$command" | nc $SERVER
    echo
    sleep 1
}

# Test connection
echo "=== Testing Connection ==="
send_command '{"command": "status.get_state"}' "Getting emulator state"

# Enable BASIC
echo "=== Enabling BASIC ==="
send_command '{"command": "config.set_basic_enabled", "params": {"enabled": true}}' "Enable BASIC ROM"

# Apply restart to enable BASIC
echo "=== Applying Configuration ==="
send_command '{"command": "config.apply_restart"}' "Restart emulator with BASIC enabled"

# Wait for restart to complete
echo "Waiting for restart to complete..."
sleep 3

# Send a simple BASIC program that runs for about 10 seconds
echo "=== Sending BASIC Program ==="
BASIC_PROGRAM='10 PRINT "STARTING LOOP TEST"
20 FOR I=1 TO 1000
30 PRINT "LOOP COUNT: ";I
40 FOR J=1 TO 50: NEXT J
50 NEXT I
60 PRINT "LOOP COMPLETE"
'

# Send the program line by line
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"10 PRINT \\\"STARTING LOOP TEST\\\"\\n\"}}" "Sending line 10"
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"20 FOR I=1 TO 1000\\n\"}}" "Sending line 20"
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"30 PRINT \\\"LOOP COUNT: \\\";I\\n\"}}" "Sending line 30"
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"40 FOR J=1 TO 50: NEXT J\\n\"}}" "Sending line 40"
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"50 NEXT I\\n\"}}" "Sending line 50"
send_command "{\"command\": \"input.send_text\", \"params\": {\"text\": \"60 PRINT \\\"LOOP COMPLETE\\\"\\n\"}}" "Sending line 60"

# Run the program
echo "=== Running Program ==="
send_command '{"command": "input.send_text", "params": {"text": "RUN\n"}}' "Starting program execution"

# Let it run for 10 seconds
echo "Letting program run for 10 seconds..."
sleep 10

# Send break to stop the program
echo "=== Stopping Program ==="
send_command '{"command": "input.break"}' "Sending BREAK signal"

# Wait a moment
sleep 2

# List the program
echo "=== Listing Program ==="
send_command '{"command": "input.send_text", "params": {"text": "LIST\n"}}' "Listing the BASIC program"

echo "=== Test Complete ==="
echo "Check the emulator screen to see the results!"