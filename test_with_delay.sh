#!/bin/bash

# Enable BASIC and restart
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}}' | nc localhost 8080
echo '{"command": "config.apply_restart"}' | nc localhost 8080

# Wait for emulator to stabilize after restart
sleep 2

# Send BASIC program with small delays between lines
echo '{"command": "input.send_text", "params": {"text": "10 FOR I=1 TO 10\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "20 PRINT I\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "30 NEXT I\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "RUN\n"}}' | nc localhost 8080