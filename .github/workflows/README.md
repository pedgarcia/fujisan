# GitHub Actions Workflows (Disabled)

These workflows have been disabled in favor of local build scripts.

## Why Disabled?

All builds are now handled locally using the unified build system:
- `./build.sh` - Main build script for all platforms
- `scripts/build-macos-separate-dmgs.sh` - macOS builds
- `scripts/build-linux-docker.sh` - Linux builds via Docker/Podman
- `scripts/build-windows-cross.sh` - Windows cross-compilation

## Local Build Benefits

1. **Full control**: No dependency on GitHub Actions runners
2. **Consistency**: Same build process locally and in CI
3. **Cost savings**: No GitHub Actions minutes consumed
4. **Faster iteration**: No need to push to test builds

## To Build Locally

```bash
# Build all platforms
./build.sh all

# Build specific platform
./build.sh macos
./build.sh windows
./build.sh linux

# Clean build
./build.sh [platform] --clean
```

## Re-enabling Workflows

To re-enable any workflow, simply remove the `.disabled` extension:
```bash
mv workflow-name.yml.disabled workflow-name.yml
```