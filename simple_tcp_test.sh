#!/bin/bash

# Simple TCP Server Test
# Prerequisites: 
# 1. Fujisan is running
# 2. TCP Server is enabled via Tools menu

SERVER="localhost:8080"

echo "Testing TCP Server connection..."

# Test basic connection
echo "=== Connection Test ==="
echo '{"command": "status.get_state", "id": "test1"}' | nc $SERVER

echo -e "\n=== Enable BASIC ==="
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}, "id": "test2"}' | nc $SERVER

echo -e "\n=== Apply Configuration ==="
echo '{"command": "config.apply_restart", "id": "test3"}' | nc $SERVER

echo -e "\nWait 3 seconds for restart..."
sleep 3

echo -e "\n=== Send Simple Program ==="
echo '{"command": "input.send_text", "params": {"text": "10 FOR I=1 TO 100\n"}, "id": "test4"}' | nc $SERVER
sleep 1
echo '{"command": "input.send_text", "params": {"text": "20 PRINT \"COUNT: \";I\n"}, "id": "test5"}' | nc $SERVER  
sleep 1
echo '{"command": "input.send_text", "params": {"text": "30 FOR J=1 TO 100: NEXT J\n"}, "id": "test6"}' | nc $SERVER
sleep 1
echo '{"command": "input.send_text", "params": {"text": "40 NEXT I\n"}, "id": "test7"}' | nc $SERVER

echo -e "\n=== Run Program ==="
echo '{"command": "input.send_text", "params": {"text": "RUN\n"}, "id": "test8"}' | nc $SERVER

echo -e "\nLetting program run for 10 seconds..."
sleep 10

echo -e "\n=== Stop Program ==="
echo '{"command": "input.break", "id": "test9"}' | nc $SERVER

sleep 2

echo -e "\n=== List Program ==="
echo '{"command": "input.send_text", "params": {"text": "LIST\n"}, "id": "test10"}' | nc $SERVER

echo -e "\nTest complete! Check the emulator screen."