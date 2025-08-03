# 📋 GitHub Actions Release Instructions

This guide explains how to use the GitHub Actions workflows to build multi-platform Fujisan releases.

## 🎯 Overview

The release process consists of three automated workflows:

1. **`build-windows.yml`** - Builds Windows MSI installer
2. **`build-linux.yml`** - Builds Linux .deb and .tar.gz packages  
3. **`create-release.yml`** - Creates draft GitHub release with all platforms

## 🚀 How to Create a Release

### Option A: Full Automated Release (Recommended)

1. **Go to GitHub Actions**:
   - Visit: `https://github.com/YOUR_USERNAME/fujisan/actions`
   - Click on "Create Release" workflow

2. **Run the Workflow**:
   - Click "Run workflow" button
   - Fill in the form:
     - **Version**: `v1.0.0` (must start with 'v')
     - **Release title**: `Fujisan v1.0.0` (optional, auto-generated if empty)
     - **Release notes**: Custom notes (optional, auto-generated if empty)
     - **Pre-release**: Check if this is a beta/RC version

3. **Wait for Completion**:
   - This will trigger Windows + Linux builds and build macOS
   - Takes ~15-20 minutes total
   - Creates a **draft release** with all platform binaries

4. **Test the Release**:
   - Go to GitHub Releases page
   - Find your draft release
   - Download each platform's binary
   - Test on Windows, Linux, and macOS
   - Edit release notes if needed

5. **Publish When Ready**:
   - Click "Publish release" to make it public

### Option B: Individual Platform Builds

If you want more control, build each platform separately:

#### Windows Build
1. Go to Actions → "Build Windows"
2. Click "Run workflow"
3. Enter version (e.g., `v1.0.0`)
4. Downloads will be in the run's artifacts

#### Linux Build  
1. Go to Actions → "Build Linux"
2. Click "Run workflow" 
3. Enter version (e.g., `v1.0.0`)
4. Downloads will be in the run's artifacts

#### macOS Build (Manual)
Run locally on your Mac:
```bash
export FUJISAN_VERSION="v1.0.0"
./scripts/build-macos-release.sh --skip-notarize
```

## 📦 What Gets Built

### Windows (windows-latest runner)
- **`Fujisan-v1.0.0-Windows-x64.msi`** - MSI installer for Windows 10/11
- **`Fujisan-v1.0.0-Windows-x64.msi.sha256`** - Checksum file
- **Includes**: Qt5 DLLs, libatari800, Windows installer with shortcuts

### Linux (ubuntu-20.04 runner)  
- **`fujisan_1.0.0_amd64.deb`** - Debian/Ubuntu package
- **`fujisan-v1.0.0-linux-x64.tar.gz`** - Portable archive
- **Checksum files** for both packages
- **Includes**: Qt5 libraries, libatari800, desktop integration

### macOS (your local build)
- **`Fujisan-v1.0.0-macOS.dmg`** - DMG installer for macOS 11.0+
- **`Fujisan-v1.0.0-macOS.dmg.sha256`** - Checksum file  
- **Includes**: Qt5 frameworks, libatari800, app bundle with signing

## 🔍 Testing Checklist

When you download the draft release artifacts:

### Windows Testing
- [ ] Download and run the .msi installer
- [ ] Verify Fujisan starts without errors
- [ ] Test basic emulation functionality
- [ ] Check that no Qt5 installation is required
- [ ] Verify uninstaller works

### Linux Testing  
- [ ] Test .deb package: `sudo apt install ./fujisan_1.0.0_amd64.deb`
- [ ] Test portable: Extract .tar.gz and run `./fujisan.sh`
- [ ] Verify desktop integration (if using .deb)
- [ ] Check that no external Qt5 is required
- [ ] Test on different distros if possible

### macOS Testing
- [ ] Mount the .dmg and drag to Applications
- [ ] Verify code signature: `codesign --verify --deep Fujisan.app`
- [ ] Test emulation functionality
- [ ] Check that no external dependencies are needed

## 🛠️ Troubleshooting

### Build Failures

**Windows Build Fails**:
- Check Qt5 installation in the workflow log
- Verify NSIS is properly installed
- Look for MSI creation errors

**Linux Build Fails**:
- Check system dependencies installation
- Verify Qt5 packages are available
- Look for .deb packaging errors

**macOS Build Fails**:
- Ensure Homebrew Qt5 is installed
- Check libatari800 integration
- Verify DMG creation steps

### Workflow Dispatch Issues

**"Run workflow" button not visible**:
- Make sure you're on the main branch
- Ensure you have write access to the repository
- Refresh the GitHub Actions page

**Workflows don't trigger**:
- Check that all .yml files are in `.github/workflows/`
- Verify the workflow syntax with GitHub's validator
- Ensure the repository has Actions enabled

### Release Creation Problems

**Assets not uploading**:
- Check that all artifact names match the upload patterns
- Verify artifacts were created by the build jobs
- Look for file path mismatches

**Draft release not created**:
- Ensure GITHUB_TOKEN has sufficient permissions
- Check that the version doesn't already exist as a tag
- Verify the release workflow ran after builds completed

## 📋 Version Naming Convention

Use semantic versioning with 'v' prefix:
- **Release**: `v1.0.0`, `v1.1.0`, `v2.0.0`
- **Pre-release**: `v1.0.0-beta1`, `v1.0.0-rc1`
- **Development**: `v1.0.0-dev` (auto-generated for non-tagged builds)

## 🔐 Security Notes

- All builds are performed on GitHub's hosted runners
- No secrets are required for building (only for publishing releases)
- Source code is public, builds are transparent
- Checksums are provided for integrity verification
- macOS builds can be fully signed when you add Developer ID certificates

## 📞 Support

If you encounter issues:
1. Check the workflow run logs for detailed error messages
2. Compare successful runs to identify what changed
3. Test the build process locally first (especially for macOS)
4. Ensure all required files are committed to the repository

---

**🎉 Happy Building!** Once set up, you can create professional, multi-platform releases with just a few clicks.