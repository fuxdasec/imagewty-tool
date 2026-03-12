# IMAGEWTY Tool (`imagewty-tool`)

[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)
![C](https://img.shields.io/badge/language-C-00599C?style=flat&logo=c)
![Build Status](https://github.com/uictorius/imagewty-tool/actions/workflows/ci.yml/badge.svg)
![Version](https://img.shields.io/github/v/tag/uictorius/imagewty-tool)

**IMAGEWTY Tool** is an open-source C utility for **inspecting, extracting, and repacking IMAGEWTY firmware images** used in Allwinner-based devices.

> ⚠️ **Note:** This is an amateur project, created to practice and learn C. The author is not an expert in C, so contributions, improvements, and feedback are welcome. All technical information about IMAGEWTY headers and files was sourced from [nskhuman.ru](http://nskhuman.ru/allwinner/firmware.php).

---

## Features

- Display IMAGEWTY global header and individual file headers.
- Extract internal files and generate `image.cfg` configuration.
- Repack images using extracted files and `image.cfg`.
- Automatically calculate and update V-file checksums.

---

### Compatibility (tested on PhoenixSuit v1.19)

- Header Version `0x00000300` (v3)
- Header Version `0x00000403` (v4)

---

## Project Structure

```
src/          → C source files
include/      → Header files
Makefile      → Build script
LICENSE       → GNU GPL v3.0
README.md     → This file
.clang-format → Code style configuration
```

---

## Build Instructions

**Requirements:** `gcc` (C compiler), `make`

```bash
# Compile the tool
make

# Optional: Install globally
sudo make install

# Optional: Uninstall
sudo make uninstall

# Remove build artifacts
make clean
```

This will generate the executable `imagewty-tool` in the project root or install it under `/usr/local/bin/`.

---

## Usage

```bash
# Display main image header and file information
imagewty-tool info <image.img>

# Extract all files from the firmware image
imagewty-tool extract <image.img>

# Repack extracted files into a new firmware image
imagewty-tool repack <folder.dump> <new_image.img>

# Inspect or display IMAGEWTY configuration files
imagewty-tool config <image.cfg>
```

---

### Makefile Format

- Uses `gcc` with `-Wall -Wextra -std=c11 -O2` flags.
- Source files are in `src/`, headers in `include/`.
- Targets:

  - `all` → builds the tool.
  - `clean` → removes compiled objects and binary.
  - `install` → installs the binary to `/usr/local/bin`.
  - `format` → automatically formats the code with `clang-format` according to `.clang-format` rules.

---

### Code Formatting

- Based on **LLVM style** with Allman braces:

  - Indent width: 4 spaces
  - Tabs: spaces only
  - Column limit: 100
  - Pointer alignment: left
  - Short functions on a single line allowed only if inline

---

### Notes for Users

- Extraction creates a folder named `<image>.dump/` containing all extracted files and a generated `image.cfg`.
- Repacking generates a new firmware image with all checksums recalculated.
- The `config` command allows you to view or verify the contents of an `image.cfg` file.
- File structure and metadata are preserved during extraction and repacking.
- For firmware analysis, `info` provides human-readable descriptions for standard files like `boot.fex`, `super.fex`, etc.

---

## Contributing

Contributions are welcome! You can help by:

- Reporting issues or bugs.
- Sending pull requests with improvements.
- Enhancing documentation or examples.
- Improving code formatting or implementing new features.

Please follow the formatting guidelines with `clang-format` to maintain consistency.

---

## License

This project is licensed under **GNU GPL v3.0**. See the [LICENSE](LICENSE) file for details.
