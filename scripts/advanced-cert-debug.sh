#!/bin/bash

# Advanced certificate debugging script

echo "=== Advanced Certificate Debugging ==="
echo ""

echo "1. All available signing identities (detailed):"
security find-identity -v -p codesigning

echo ""
echo "2. Check certificate in login keychain:"
security find-certificate -c "Paulo Garcia" -p

echo ""
echo "3. Check certificate details:"
security find-certificate -c "Developer ID Application" -Z

echo ""
echo "4. Test codesign with different methods:"

# Create a temporary file to test signing
TEMP_FILE=$(mktemp)
echo "test" > "$TEMP_FILE"

# Method 1: Try with hash
CERT_HASH="8151013729CE380EA7DE535314D46C0284335941"
echo "Testing hash: $CERT_HASH"
if codesign --sign "$CERT_HASH" "$TEMP_FILE" 2>&1; then
    echo "✓ Hash method works"
else
    echo "✗ Hash method failed"
fi

# Method 2: Try with full name including quotes
CERT_NAME="Developer ID Application: Paulo Garcia (ZC4PGBX6D6)"
echo "Testing quoted name: \"$CERT_NAME\""
if codesign --sign "$CERT_NAME" "$TEMP_FILE" 2>&1; then
    echo "✓ Quoted name works"
else
    echo "✗ Quoted name failed"
fi

# Method 3: Try with team ID only
echo "Testing team ID: ZC4PGBX6D6"
if codesign --sign "ZC4PGBX6D6" "$TEMP_FILE" 2>&1; then
    echo "✓ Team ID works"
else
    echo "✗ Team ID failed"
fi

# Method 4: Try with common name only
echo "Testing common name: Paulo Garcia"
if codesign --sign "Paulo Garcia" "$TEMP_FILE" 2>&1; then
    echo "✓ Common name works"
else
    echo "✗ Common name failed"
fi

# Method 5: Try with partial certificate name
echo "Testing partial name: Developer ID Application: Paulo Garcia"
if codesign --sign "Developer ID Application: Paulo Garcia" "$TEMP_FILE" 2>&1; then
    echo "✓ Partial name works"
else
    echo "✗ Partial name failed"
fi

# Clean up
rm -f "$TEMP_FILE"

echo ""
echo "5. Check keychain access permissions:"
security list-keychains

echo ""
echo "6. Check if keychain is unlocked:"
security show-keychain-info

echo ""
echo "7. Manual certificate verification:"
echo "Try manually running:"
echo "security find-identity -p codesigning -v"
echo "Then copy the EXACT certificate identifier that works"

echo ""
echo "8. Alternative: Export and reimport certificate"
echo "If nothing works, try:"
echo "1. Open Keychain Access"
echo "2. Find the Developer ID Application certificate"
echo "3. Right-click → Export"
echo "4. Save as .p12 file"
echo "5. Delete the certificate from keychain"
echo "6. Double-click the .p12 file to reimport"
