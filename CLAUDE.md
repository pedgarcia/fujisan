# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fujisan is a modern Qt5-based frontend for the Atari800 emulator that provides a native desktop experience with full keyboard support, machine configuration, and authentic Atari behavior. It uses libatari800 as its core emulator library and includes comprehensive debugging, TCP server API, and media management capabilities.

## Development Guidelines

### Version Control Best Practices

- don't commit and merge before I confirm the changes are good
- use GH CLI (gh)

## Build System and Common Commands

### Unified Build Script (Recommended)
```bash
# Build for any platform using the unified script
./build.sh [platform] [options]

# Examples:
./build.sh macos          # Build both Intel and ARM64 DMGs
./build.sh windows        # Cross-compile for Windows
./build.sh linux          # Build Linux packages with Docker/Podman
./build.sh all --clean    # Build all platforms with clean

# All outputs go to dist/ directory:
# - Fujisan-{version}-arm64.dmg   (macOS Apple Silicon)
# - Fujisan-{version}-x86_64.dmg  (macOS Intel)  
# - Fujisan-{version}-windows.zip (Windows)
# - fujisan-{version}-linux-x64.tar.gz (Linux)
# - fujisan_{version}_amd64.deb   (Debian/Ubuntu)
```

## Development Environment

### Container and Docker Management

- I have podman installed. Do not try to install it - and I have a symlink docker -> podman, so you can still use docker name
- when creating a binary for test, or even full builds remember the binary has to be signed and using this to avoid Qt5 conflicts (x86 and arm): /opt/homebrew/opt/qt@5/bin/macdeployqt Fujisan.app
