# GitHub Actions Workflows

The only active workflow here is `test.yml`. Release and platform package builds are done locally for now; GitHub Actions-based release builds can be added later as new workflow files under this directory.

## Local builds

- `./build.sh` - Main build script for all platforms
- `scripts/build-macos-separate-dmgs.sh` - macOS builds
- `scripts/build-linux-docker.sh` - Linux builds via Docker/Podman
- `scripts/build-windows-cross.sh` - Windows cross-compilation

```bash
./build.sh all
./build.sh macos
./build.sh windows
./build.sh linux
./build.sh [platform] --clean
```
