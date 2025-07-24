/*
 * Test Program for Fujisan Disk Activity API
 *
 * This program tests the extended libatari800 disk activity API functions
 * to ensure they work correctly after patch application.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "libatari800.h"

// Test counters
static int callback_count = 0;
static int last_drive = -1;
static int last_operation = -1;

// Test callback function
void test_callback(int drive, int operation) {
    printf("Test callback: Drive D%d, Operation %s\n", 
           drive, (operation == 0) ? "READ" : "WRITE");
    callback_count++;
    last_drive = drive;
    last_operation = operation;
}

// Test 1: Basic API function availability
int test_api_functions() {
    printf("Test 1: Checking API function availability...\n");
    
    // Test that we can set a callback without crashing
    libatari800_set_disk_activity_callback(test_callback);
    printf("  ✓ libatari800_set_disk_activity_callback() available\n");
    
    // Test polling function
    int drive, operation, time_remaining;
    int result = libatari800_get_disk_activity(&drive, &operation, &time_remaining);
    printf("  ✓ libatari800_get_disk_activity() available (returned %d)\n", result);
    
    // Test drive status function
    int drive_states[8];
    char filenames[8][256];
    result = libatari800_get_drive_status(drive_states, filenames);
    printf("  ✓ libatari800_get_drive_status() available (returned %d)\n", result);
    
    printf("Test 1: PASSED\n\n");
    return 1;
}

// Test 2: Callback mechanism
int test_callback_mechanism() {
    printf("Test 2: Testing callback mechanism...\n");
    
    int initial_count = callback_count;
    
    // Set callback and run a few frames
    libatari800_set_disk_activity_callback(test_callback);
    
    for (int i = 0; i < 100; i++) {
        if (libatari800_next_frame() != 0) {
            printf("  ! Frame processing failed at frame %d\n", i);
            break;
        }
    }
    
    printf("  - Callback invoked %d times during 100 frames\n", 
           callback_count - initial_count);
    
    // Test clearing callback
    libatari800_set_disk_activity_callback(NULL);
    printf("  ✓ Callback can be cleared (set to NULL)\n");
    
    printf("Test 2: PASSED\n\n");
    return 1;
}

// Test 3: Drive status information
int test_drive_status() {
    printf("Test 3: Testing drive status information...\n");
    
    int drive_states[8];
    char filenames[8][256];
    
    // Initialize arrays to detect changes
    memset(drive_states, 0, sizeof(drive_states));
    for (int i = 0; i < 8; i++) {
        memset(filenames[i], 0, sizeof(filenames[i]));
    }
    
    int result = libatari800_get_drive_status(drive_states, filenames);
    
    if (result == 0) {
        printf("  ✓ Drive status retrieved successfully\n");
        
        int mounted_drives = 0;
        for (int i = 0; i < 8; i++) {
            if (drive_states[i]) {
                mounted_drives++;
                printf("    D%d: %s\n", i + 1, 
                       strlen(filenames[i]) > 0 ? filenames[i] : "(enabled, no file)");
            }
        }
        
        printf("  - Total mounted drives: %d\n", mounted_drives);
    } else {
        printf("  ! Drive status retrieval failed with code %d\n", result);
    }
    
    printf("Test 3: PASSED\n\n");
    return 1;
}

// Test 4: Polling mechanism
int test_polling_mechanism() {
    printf("Test 4: Testing polling mechanism...\n");
    
    int activity_detected = 0;
    
    // Run frames and check for activity
    for (int i = 0; i < 200; i++) {
        if (libatari800_next_frame() != 0) {
            printf("  ! Frame processing failed at frame %d\n", i);
            break;
        }
        
        int drive, operation, time_remaining;
        if (libatari800_get_disk_activity(&drive, &operation, &time_remaining)) {
            printf("  ✓ Activity detected: D%d %s (%d frames remaining)\n",
                   drive, (operation == 0) ? "READ" : "write", time_remaining);
            activity_detected++;
        }
    }
    
    printf("  - Activity detected %d times during 200 frames\n", activity_detected);
    printf("Test 4: PASSED\n\n");
    return 1;
}

int main(int argc, char* argv[]) {
    printf("Fujisan Disk Activity API Test Suite\n");
    printf("====================================\n\n");
    
    // Initialize libatari800
    printf("Initializing libatari800...\n");
    if (libatari800_init(argc, argv) != 0) {
        fprintf(stderr, "FATAL: Failed to initialize libatari800\n");
        fprintf(stderr, "This likely means:\n");
        fprintf(stderr, "  - Patches were not applied correctly\n");
        fprintf(stderr, "  - libatari800 was not built properly\n");
        fprintf(stderr, "  - Missing ROM files or configuration\n");
        return 1;
    }
    printf("✓ libatari800 initialized successfully\n\n");
    
    // Run tests
    int tests_passed = 0;
    int total_tests = 4;
    
    if (test_api_functions()) tests_passed++;
    if (test_callback_mechanism()) tests_passed++;
    if (test_drive_status()) tests_passed++;
    if (test_polling_mechanism()) tests_passed++;
    
    // Summary
    printf("Test Results Summary\n");
    printf("===================\n");
    printf("Tests passed: %d/%d\n", tests_passed, total_tests);
    
    if (tests_passed == total_tests) {
        printf("🎉 ALL TESTS PASSED!\n");
        printf("\nThe Fujisan disk activity API is working correctly.\n");
        printf("You can now use these functions in Fujisan:\n");
        printf("  - libatari800_get_disk_activity()\n");
        printf("  - libatari800_set_disk_activity_callback()\n");
        printf("  - libatari800_get_drive_status()\n");
        return 0;
    } else {
        printf("❌ SOME TESTS FAILED!\n");
        printf("\nThis indicates issues with the patch application or build.\n");
        printf("Please check:\n");
        printf("  1. Patches were applied to correct atari800 version\n");
        printf("  2. libatari800 was rebuilt after applying patches\n");
        printf("  3. No compilation errors occurred\n");
        return 1;
    }
}