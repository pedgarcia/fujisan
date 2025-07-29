#!/bin/bash

# Test TCP Server - Run this AFTER enabling TCP Server in Tools menu

echo "=== Testing TCP Connection ==="
echo "Make sure TCP Server is enabled in Tools menu first!"
echo

# Test connection
echo "Testing connection..."
echo '{"command": "status.get_state", "id": "test1"}' | nc localhost 8080
echo

# Enable BASIC  
echo "Enabling BASIC..."
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}, "id": "test2"}' | nc localhost 8080
echo

# Restart
echo "Restarting emulator..."
echo '{"command": "config.apply_restart", "id": "test3"}' | nc localhost 8080
echo

echo "Waiting 3 seconds for restart..."
sleep 3

# Send BASIC program
echo "Sending BASIC program..."
echo '{"command": "input.send_text", "params": {"text": "10 FOR I=1 TO 100\n"}, "id": "line1"}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "20 PRINT \"LOOP \";I\n"}, "id": "line2"}' | nc localhost 8080  
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "30 FOR J=1 TO 50: NEXT J\n"}, "id": "line3"}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "40 NEXT I\n"}, "id": "line4"}' | nc localhost 8080
sleep 0.5

# Run program
echo "Running program..."
echo '{"command": "input.send_text", "params": {"text": "RUN\n"}, "id": "run"}' | nc localhost 8080
echo

echo "Program is running - watch the emulator screen!"
echo "Waiting 10 seconds..."
sleep 10

# Stop program
echo "Stopping program with BREAK..."
echo '{"command": "input.break", "id": "break"}' | nc localhost 8080
echo

sleep 2

# List program
echo "Listing program..."
echo '{"command": "input.send_text", "params": {"text": "LIST\n"}, "id": "list"}' | nc localhost 8080
echo

echo "Test complete! Check the emulator screen to see the results."