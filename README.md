# convertdav

A WebDAV-based file conversion server written in **C++20**.

It exposes a **virtual folder tree** (under `/convert/...`) where each converter behaves like a mini file-processing service:

- upload files into `in/`
- get results from `out/`
- configure converter-specific options via virtual `settings/` paths (using WebDAV actions like `DELETE` on setting items)

This makes the project work nicely with:
- command-line tools (`curl`)
- WebDAV clients
- file explorers that support WebDAV (including Windows Explorer / MiniRedir)

---

## What this project is

`convertdav` is a **WebDAV-first conversion framework**.

Instead of a traditional REST API like `/api/convert?op=...`, it presents conversions as a filesystem-like structure:

```text
/convert/<converter-name>/
    in/
    out/
    settings/   (optional, virtual)
````

Each client (currently identified by **IP address**) gets:

* its own uploaded inputs (`in/`)
* its own conversion outputs (`out/`)
* its own converter settings state (e.g. threshold value, JPEG quality)

This makes it feel like a “network drive of tools” rather than a normal web API.

---

## Current features

### ✅ Implemented converters

* **`png-jpg`** — converts image to JPEG
* **`invert`** — inverts image colors
* **`img-gif`** — converts image to GIF
* **`pdf-png`** — renders PDF pages to PNG(s)
* **`mp4-gif`** — converts MP4 video to GIF
* **`threshold`** — converts image to black/white PNG using configurable threshold
* **`jpeg-compress`** — re-encodes image to JPEG with configurable quality

### 🟡 Stub converters (enabled placeholders)

These exist and pass startup self-tests, but are not fully implemented yet:

* **`sha256`**
* **`zip`**
* **`compile-win`**

They currently return informative `.txt` outputs so the WebDAV flow is testable end-to-end.

---

## How it works (high-level)

### Virtual WebDAV layout

```text
/convert/
    png-jpg/
        in/
        out/
    threshold/
        in/
        out/
        settings/
            value/
                1
                2
                ...
                100
    jpeg-compress/
        in/
        out/
        settings/
            quality/
                1
                2
                ...
                100
```

### Core behavior

* `PUT /convert/<op>/in/<file>` → upload and trigger conversion
* `GET /convert/<op>/out/<file>` → download conversion result
* `DELETE /convert/<op>/out/<file>` → delete output file
* `DELETE /convert/<op>/settings/...` → mutate settings (virtual control action)
* `PROPFIND` → browse folders/files as a WebDAV client

### Per-client state

Client identity is currently based on **remote IP address**.

For each IP, the server stores:

* `in_files`
* `out_files`
* settings (e.g. threshold value, JPEG quality)

### Cleanup (GC)

A background thread removes cached `in/` and `out/` files after a TTL (currently **10 minutes**).

> Note: outputs are **not** deleted immediately after download yet — they expire by TTL.

---

## Converter settings (implemented)

## `threshold` (Image → PNG)

Converts an image to black/white with threshold applied to RGB channels (alpha preserved).

* **Default:** `50`
* **Range:** `1..100`
* **Virtual setting path:** `/convert/threshold/settings/value/<N>`

Set threshold (per client/IP):

```bash
curl -X DELETE http://127.0.0.1:8080/convert/threshold/settings/value/72
```

## `jpeg-compress` (Image → JPEG)

Re-encodes image to JPEG with configurable quality.

* **Default:** `85`
* **Range:** `1..100`
* **Virtual setting path:** `/convert/jpeg-compress/settings/quality/<N>`

Set JPEG quality (per client/IP):

```bash
curl -X DELETE http://127.0.0.1:8080/convert/jpeg-compress/settings/quality/60
```

---

## Quick usage examples

## 1) Threshold conversion

Upload:

```bash
curl -T input.png http://127.0.0.1:8080/convert/threshold/in/input.png
```

Download result:

```bash
curl http://127.0.0.1:8080/convert/threshold/out/input.png --output out.png
```

## 2) JPEG compression

Upload:

```bash
curl -T input.png http://127.0.0.1:8080/convert/jpeg-compress/in/input.png
```

Download result:

```bash
curl http://127.0.0.1:8080/convert/jpeg-compress/out/input.jpg --output out.jpg
```

## 3) Stub converters (currently placeholders)

### SHA256 stub

```bash
curl -T file.bin http://127.0.0.1:8080/convert/sha256/in/file.bin
curl http://127.0.0.1:8080/convert/sha256/out/file.sha256.txt
```

### ZIP stub

```bash
curl -T file.txt http://127.0.0.1:8080/convert/zip/in/file.txt
curl http://127.0.0.1:8080/convert/zip/out/archive.zip.stub.txt
```

### Compile-Windows stub

```bash
curl -T main.cpp http://127.0.0.1:8080/convert/compile-win/in/main.cpp
curl http://127.0.0.1:8080/convert/compile-win/out/main.build.log.txt
```

---

## Build requirements

### Required

* **C++20 compiler**
* **CMake** (3.20+)
* **OpenSSL**
* **Threads** (pthreads on Linux)

### Runtime tools (for some converters)

* **ImageMagick CLI** (`magick` or `convert`)
  Used by:

  * `png-jpg`
  * `invert`
  * `img-gif`
  * `pdf-png`
  * `threshold`
  * `jpeg-compress`

* **ffmpeg**
  Used by:

  * `mp4-gif`

> If a dependency is missing, startup self-tests may disable the affected converter automatically.

---

## Build

```bash
cmake -S . -B build
cmake --build build -j
```

Run:

```bash
./build/convertdav
```

Custom bind IP / port:

```bash
./build/convertdav 0.0.0.0 8080
```

---

## Startup self-tests and auto-disable behavior

At startup, the server runs lightweight self-tests for registered converters.

This does two things:

1. checks whether required external tools are available
2. verifies each converter can produce a minimally valid output

Example behavior:

* `mp4-gif` may be **disabled** if `ffmpeg` is missing
* image converters may be **disabled** if ImageMagick CLI is missing
* stubs remain enabled and return placeholder outputs

This makes the server robust on machines with partial dependencies installed.

---

## WebDAV methods supported

The server supports (fully or partially):

* `OPTIONS`
* `GET`
* `HEAD`
* `PUT`
* `DELETE`
* `PROPFIND`
* `PROPPATCH` (minimal response)
* `LOCK` / `UNLOCK` (minimal compatibility support)
* `MOVE` (within same converter `out/` collection)

Not implemented / partial:

* `COPY` (returns not implemented)
* `MKCOL` (virtual folders only)

---

## Project structure (important files)

```text
src/
  main.cpp                  # startup + self-tests + server launch
  server.cpp                # HTTP/WebDAV handling, virtual paths, per-IP cache
  app.hpp                   # shared app state, blobs, converter settings snapshot

  converters/
    registry.cpp/.hpp       # converter registration + self-test logic
    common.cpp/.hpp         # temp dirs, process exec, file helpers, CLI helpers

    png_jpg.cpp
    invert.cpp
    img_gif.cpp
    pdf_png.cpp
    mp4_gif.cpp
    threshold.cpp
    jpeg_compress.cpp

    sha256_stub.cpp
    zip_stub.cpp
    compile_win_stub.cpp
```

---

## Design notes

### Why WebDAV?

WebDAV gives a natural “network filesystem” interface:

* users can browse converters as folders
* file upload/download is already standardized
* settings can be represented as virtual files/folders

### Why per-IP state?

It keeps the protocol simple and stateless for clients while still allowing:

* configurable converters
* separate outputs for different users

(You can later replace this with sessions/tokens/auth if needed.)

### Why external tools (ImageMagick / ffmpeg)?

They are:

* reliable
* battle-tested
* fast enough for a practical conversion server

The code is structured so pure C++ replacements can be added later if desired.

---

## Current limitations

* Per-client isolation is based on **IP address** (not authentication/session IDs)
* Outputs are retained until TTL expiry (not deleted immediately after download)
* Some converters are still **stubs**
* Multi-file workflows (e.g. ZIP bundling) need additional session/batch semantics
* `compile-win` is planned but not implemented (security/resource controls will be needed)

---

## Roadmap (planned)

* [ ] Real `sha256` converter (file → text digest)
* [ ] Real `zip` converter (multi-file bundling)
* [ ] Real `compile-win` converter (sandboxed/limited)
* [ ] More virtual `settings/` trees for stubs
* [ ] Optional auth/session IDs (instead of per-IP only)
* [ ] Better output lifecycle controls (delete-after-read / configurable TTL)
* [ ] Dependency status endpoint / health endpoint
* [ ] Safer process timeouts and file size limits per converter

---

## Contributing

PRs/issues welcome — especially for:

* additional converters
* pure-C++ backends (optional replacements for CLI tools)
* WebDAV compatibility improvements
* converter settings UX improvements
