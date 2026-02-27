# Fujisan Troubleshooting

## macOS: FujiNet-PC Won't Start (NetSIO / FujiNet grayed out or crashes)

### Symptom

FujiNet doesn't start when you enable NetSIO in Fujisan. You may see one of:

- FujiNet status shows "Not connected" and never starts
- Console/log shows: `code signature in 'libcrypto.3.dylib' not valid for use in process: mapping process and mapped file (non-platform) have different Team IDs`
- Error: `tried: '/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib' (code signature ... not valid ... different Team IDs)`

### Cause

The FujiNet-PC binary was built from source and links to your system's Homebrew OpenSSL. When Fujisan (signed with a Developer ID) launches FujiNet, macOS blocks it from loading Homebrew's libcrypto because the two have different code signing Team IDs.

**Note:** This issue is automatically resolved in Fujisan v1.1.5+ when developers build with `--build-fujinet-pc`. The build script now bundles and signs OpenSSL libraries inside the app. If you're seeing this error, you may have an older build or a custom build.

### Workaround for Users

**If you installed Fujisan from a DMG and FujiNet won't start:**

1. **Point Fujisan to a different FujiNet binary:**
   - Open Fujisan → Settings → FujiNet tab
   - Click "Browse..." next to FujiNet-PC Binary
   - Download a pre-built FujiNet-PC from [FujiNet releases](https://github.com/FujiNetWIFI/fujinet-firmware/releases) or the nightly builds
   - Select the downloaded `fujinet` binary

2. **Or run FujiNet-PC separately** (if you have it installed):
   - Start FujiNet-PC manually before launching Fujisan
   - In Settings → FujiNet, set "Launch behavior" to "Detect existing FujiNet-PC process"

**If you're a developer building Fujisan:**

Starting with v1.1.5+, OpenSSL bundling is automatic when using `--build-fujinet-pc`:

```bash
# This now automatically bundles and signs OpenSSL (v1.1.5+)
./build.sh macos --build-fujinet-pc --sign --notarize
```

For older versions or to use pre-built binaries:

```bash
./scripts/download-fujinet-pc.sh
./build.sh macos --sign --notarize   # without --build-fujinet-pc
```

See [BUILD_FUJINET_PC.md](../docs_local/BUILD_FUJINET_PC.md) for details.
