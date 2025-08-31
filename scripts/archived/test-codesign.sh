#!/bin/bash

# Simple codesign test
echo "Testing codesign with your certificate..."

# Create a test file
echo "test" > /tmp/test_sign_file

echo ""
echo "Available certificates:"
security find-identity -v -p codesigning

echo ""
echo "Testing different certificate identifiers:"

# Try the most common working patterns
echo ""
echo "1. Testing with Team ID only:"
codesign --sign "ZC4PGBX6D6" /tmp/test_sign_file 2>&1 && echo "✓ Team ID works!" || echo "✗ Team ID failed"

echo ""
echo "2. Testing with certificate subject:"
codesign --sign "Developer ID Application: Paulo Garcia (ZC4PGBX6D6)" /tmp/test_sign_file 2>&1 && echo "✓ Full name works!" || echo "✗ Full name failed"

echo ""
echo "3. Testing with hash:"
codesign --sign "8151013729CE380EA7DE535314D46C0284335941" /tmp/test_sign_file 2>&1 && echo "✓ Hash works!" || echo "✗ Hash failed"

echo ""
echo "4. Testing with common name:"
codesign --sign "Paulo Garcia" /tmp/test_sign_file 2>&1 && echo "✓ Common name works!" || echo "✗ Common name failed"

echo ""
echo "5. Testing with partial name:"
codesign --sign "Developer ID Application: Paulo Garcia" /tmp/test_sign_file 2>&1 && echo "✓ Partial name works!" || echo "✗ Partial name failed"

# Clean up
rm -f /tmp/test_sign_file

echo ""
echo "=== SOLUTION ==="
echo "Whichever method above shows '✓ works!' should be used in the build scripts."
echo ""
echo "Common fixes:"
echo "1. If Team ID works: Use 'ZC4PGBX6D6'"
echo "2. If keychain is locked: security unlock-keychain"
echo "3. If certificate access denied: Check Keychain Access permissions"
echo "4. If nothing works: Restart and try again, or reinstall certificate"
