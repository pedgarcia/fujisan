#!/bin/bash

# Enable BASIC and restart
echo '{"command": "config.set_basic_enabled", "params": {"enabled": true}}' | nc localhost 8080
echo '{"command": "config.apply_restart"}' | nc localhost 8080

# Wait for emulator to stabilize after restart
sleep 2

# Send a longer BASIC program with longer lines
echo '{"command": "input.send_text", "params": {"text": "10 PRINT \"HELLO WORLD FROM TCP SERVER TEST\"\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "20 FOR COUNTER=1 TO 100 STEP 5\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "30 PRINT \"COUNTER VALUE IS: \";COUNTER\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "40 IF COUNTER>50 THEN PRINT \"HALFWAY THERE!\"\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "50 NEXT COUNTER\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "60 PRINT \"PROGRAM FINISHED SUCCESSFULLY\"\n"}}' | nc localhost 8080
sleep 0.5
echo '{"command": "input.send_text", "params": {"text": "70 END\n"}}' | nc localhost 8080
sleep 1
echo '{"command": "input.send_text", "params": {"text": "LIST\n"}}' | nc localhost 8080