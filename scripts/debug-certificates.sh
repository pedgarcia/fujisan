#!/bin/bash

# Debug script to find the correct certificate identifier for codesign

echo "=== Certificate Debugging ==="
echo ""

echo "1. All signing identities:"
security find-identity -v -p codesigning

echo ""
echo "2. Developer ID certificates only:"
security find-identity -v -p codesigning | grep "Developer ID Application"

echo ""
echo "3. Testing different certificate formats:"

# Get the certificate hash and name
CERT_INFO=$(security find-identity -v -p codesigning | grep "Developer ID Application" | head -1)
CERT_HASH=$(echo "$CERT_INFO" | awk '{print $2}')
CERT_NAME=$(echo "$CERT_INFO" | sed 's/.*"\(.*\)".*/\1/')

echo "   Certificate hash: $CERT_HASH"
echo "   Certificate name: $CERT_NAME"

echo ""
echo "4. Testing codesign with different identifiers:"

# Test with hash
echo "   Testing with hash: $CERT_HASH"
if codesign --verify --sign "$CERT_HASH" /dev/null 2>/dev/null; then
    echo "   ✓ Hash works with codesign"
else
    echo "   ✗ Hash failed with codesign"
fi

# Test with full name
echo "   Testing with full name: $CERT_NAME"
if codesign --verify --sign "$CERT_NAME" /dev/null 2>/dev/null; then
    echo "   ✓ Full name works with codesign"
else
    echo "   ✗ Full name failed with codesign"
fi

# Test with just the team ID part
TEAM_ID=$(echo "$CERT_NAME" | sed 's/.*(\(.*\)).*/\1/')
echo "   Testing with team ID: $TEAM_ID"
if codesign --verify --sign "$TEAM_ID" /dev/null 2>/dev/null; then
    echo "   ✓ Team ID works with codesign"
else
    echo "   ✗ Team ID failed with codesign"
fi

echo ""
echo "5. Recommended certificate identifier for scripts:"
# Use hash as it's most reliable
echo "   Use this in your scripts: $CERT_HASH"

echo ""
echo "6. Alternative: Use common name pattern:"
COMMON_NAME=$(echo "$CERT_NAME" | sed 's/ (.*)//')
echo "   Common name: $COMMON_NAME"
if codesign --verify --sign "$COMMON_NAME" /dev/null 2>/dev/null; then
    echo "   ✓ Common name works: $COMMON_NAME"
else
    echo "   ✗ Common name failed: $COMMON_NAME"
fi
