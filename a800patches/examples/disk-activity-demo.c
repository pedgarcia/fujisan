/*
 * Fujisan Disk Activity API Demo
 * 
 * This example demonstrates how to use the extended libatari800 API
 * to monitor disk drive activity in real-time.
 * 
 * Compile with:
 *   gcc -o disk-activity-demo disk-activity-demo.c -latari800 -lSDL2
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include "libatari800.h"

static int running = 1;

// Signal handler for clean shutdown
void signal_handler(int sig) {
    printf("\nShutting down...\n");
    running = 0;
}

// Callback function for real-time disk activity notifications
void disk_activity_callback(int drive, int operation) {
    const char* op_name = (operation == 0) ? "READ" : "WRITE";
    printf("🔴 CALLBACK: Drive D%d: %s\n", drive, op_name);
    fflush(stdout);
}

// Display current drive status
void display_drive_status() {
    int drive_states[8];
    char filenames[8][256];
    
    if (libatari800_get_drive_status(drive_states, filenames) == 0) {
        printf("\n📀 Drive Status:\n");
        for (int i = 0; i < 8; i++) {
            if (drive_states[i]) {
                printf("  D%d: %s\n", i + 1, strlen(filenames[i]) > 0 ? filenames[i] : "(empty)");
            }
        }
        printf("\n");
    }
}

int main(int argc, char* argv[]) {
    printf("Fujisan Disk Activity API Demo\n");
    printf("==============================\n\n");
    
    // Set up signal handler for clean shutdown
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize libatari800
    printf("Initializing libatari800...\n");
    if (libatari800_init(argc, argv) != 0) {
        fprintf(stderr, "Failed to initialize libatari800\n");
        return 1;
    }
    
    // Set up disk activity callback
    printf("Setting up disk activity callback...\n");
    libatari800_set_disk_activity_callback(disk_activity_callback);
    
    // Display initial drive status
    display_drive_status();
    
    printf("Starting emulation...\n");
    printf("Instructions:\n");
    printf("  - Load disk images and perform I/O operations to see activity\n");
    printf("  - Try: DIR, LOAD\"D:FILENAME\", SAVE\"D:FILENAME\"\n");
    printf("  - Press Ctrl+C to exit\n\n");
    
    int frame_count = 0;
    const int status_update_interval = 300; // Update status every ~5 seconds at 60fps
    
    // Main emulation loop
    while (running) {
        // Process one frame
        if (libatari800_next_frame() != 0) {
            fprintf(stderr, "Emulation error occurred\n");
            break;
        }
        
        // Polling method demonstration - check for disk activity
        int drive, operation, time_remaining;
        if (libatari800_get_disk_activity(&drive, &operation, &time_remaining)) {
            const char* op_name = (operation == 0) ? "READ" : "write";
            printf("📊 POLLING: Drive D%d %s activity (%d frames remaining)\n", 
                   drive, op_name, time_remaining);
        }
        
        // Periodically display drive status
        frame_count++;
        if (frame_count % status_update_interval == 0) {
            display_drive_status();
        }
        
        // Small delay to prevent excessive CPU usage in this demo
        usleep(16667); // ~60 FPS
    }
    
    printf("Cleaning up...\n");
    return 0;
}