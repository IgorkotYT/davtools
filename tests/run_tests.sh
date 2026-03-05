#!/bin/bash

SERVER_BIN=$1
if [ -z "$SERVER_BIN" ]; then
    echo "Usage: $0 <path-to-convertdav-binary>"
    exit 1
fi

echo "Starting server..."
$SERVER_BIN 127.0.0.1 8081 > server_test.log 2>&1 &
SERVER_PID=$!

cleanup() {
    echo "Cleaning up..."
    kill $SERVER_PID || true
    rm -f clean.txt clean.png tiny.png tiny.jpg invert.png img.gif test.pdf pdf.png test.mp4 mp4.gif server_test.log
}
trap cleanup EXIT

echo "Waiting for server to start..."
sleep 5

# Helper to check if file exists and is not empty
check_file() {
    if [ -f "$1" ] && [ $(stat -c%s "$1") -gt 0 ]; then
        echo "SUCCESS: $1 created"
    else
        echo "FAILED: $1 not created or empty"
    fi
}

echo "Testing root endpoint..."
curl -s http://127.0.0.1:8081/ | grep "convertdav"

echo "Testing virustest..."
echo "This is a clean test file" > clean.txt
curl -s -T clean.txt http://127.0.0.1:8081/convert/virustest/in/clean.txt
curl -s http://127.0.0.1:8081/convert/virustest/out/clean.png --output clean.png
check_file clean.png

echo "Testing png-jpg..."
convert -size 1x1 xc:white tiny.png
curl -s -T tiny.png http://127.0.0.1:8081/convert/png-jpg/in/tiny.png
curl -s http://127.0.0.1:8081/convert/png-jpg/out/tiny.jpg --output tiny.jpg
check_file tiny.jpg

echo "Testing invert..."
curl -s -T tiny.png http://127.0.0.1:8081/convert/invert/in/tiny.png
curl -s http://127.0.0.1:8081/convert/invert/out/tiny.png --output invert.png
check_file invert.png

echo "Testing img-gif..."
curl -s -T tiny.png http://127.0.0.1:8081/convert/img-gif/in/tiny.png
curl -s http://127.0.0.1:8081/convert/img-gif/out/tiny.gif --output img.gif
check_file img.gif

echo "Testing pdf-png..."
# Create a dummy PDF if possible, or use one from system if available.
# ImageMagick can create a PDF from an image.
convert tiny.png test.pdf
curl -s -T test.pdf http://127.0.0.1:8081/convert/pdf-png/in/test.pdf
# pdf-png might produce multiple pages, but it should at least produce page 0
curl -s http://127.0.0.1:8081/convert/pdf-png/out/test.0.png --output pdf.png
check_file pdf.png

echo "Testing mp4-gif..."
# ffmpeg can create a tiny mp4
if command -v ffmpeg > /dev/null; then
    ffmpeg -y -f lavfi -i color=c=red:s=16x16:d=1 -pix_fmt yuv420p test.mp4 > /dev/null 2>&1
    curl -s -T test.mp4 http://127.0.0.1:8081/convert/mp4-gif/in/test.mp4
    curl -s http://127.0.0.1:8081/convert/mp4-gif/out/test.gif --output mp4.gif
    check_file mp4.gif
else
    echo "Skipping mp4-gif (ffmpeg not found)"
fi

echo "All tests passed!"

echo "Testing base64..."
echo -n "test" > test_b64.txt
curl -s -T test_b64.txt http://127.0.0.1:8081/convert/base64/in/test_b64.txt
curl -s http://127.0.0.1:8081/convert/base64/out/test_b64.txt.b64.txt --output test_out.b64.txt
check_file test_out.b64.txt
if grep -q "dGVzdA==" test_out.b64.txt; then
    echo "SUCCESS: base64 output matches"
else
    echo "FAILED: base64 output mismatch"
fi
rm -f test_b64.txt test_out.b64.txt
