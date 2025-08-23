## 08/22/2025

**Refactor IO Library build system, dependencies, and tests**

---

## Summary

This PR modernizes **The IO Library** by removing legacy changelog/config tooling, updating CMake to support multi-variant builds, introducing a unified build script, restructuring examples, and adding a comprehensive test suite with coverage support.

---

## Key Changes

### 🔧 Build & Tooling

* Removed obsolete files:

    * `.changes/*`, `.changie.yaml`, `CHANGELOG.md`, `NEWS.md`, `build_install.sh`.
* Added **`build.sh`** script with:

    * Commands: `build`, `install`, `coverage`, `clean`.
    * Coverage integration with `llvm-cov`.
* Updated `.gitignore` with new build dirs.
* Rewrote **`BUILDING.md`**:

    * Clear local build instructions.
    * Explicit dependency setup (`a-memory-library`, `the-lz4-library`, `the-macro-library`, `zlib`).
    * Optional Dockerfile build path.
* Dockerfile modernized:

    * Ubuntu base image, configurable CMake version.
    * Non-root `dev` user, optional dev tooling.
    * Explicit dependency clones (`a-memory-library`, `the-lz4-library`, `the-macro-library`).
    * Installs this project cleanly with CMake.

### 📦 CMake

* Raised minimum CMake to **3.20**.
* Project renamed to `the_io_library` (underscore convention).
* Introduced **multi-variant builds**:

    * `debug`, `memory`, `static`, `shared`.
    * Umbrella alias: `the_io_library::the_io_library`.
* Coverage toggle (`A_ENABLE_COVERAGE`) and memory profiling define (`_AML_DEBUG_`).
* Dependencies declared via `find_package`:

    * `a_memory_library`, `the_macro_library`, `the_lz4_library`, `ZLIB`.
* Proper **install/export**:

    * Generates `the_io_libraryConfig.cmake` + version file.
    * Namespace `the_io_library::`.

### 📚 Examples

* `examples/map-reduce`:

    * Modernized CMake with explicit per-binary targets (`dump_files_1..7`, `list_files`).
    * Adds `build.sh` helper script.
    * All examples link against umbrella target.
* Removed old `examples/recs` (convert\_interactions demo).

### 📝 License & Metadata

* AUTHORS: updated with GitHub profile.
* NOTICE: cleaned up (Apache-2.0 note inline).
* SPDX headers updated:

    * Year ranges use en-dashes (`2019–2025`).
    * Knode.ai attribution includes “technical questions: contact Andy”.

### 🛠️ Library Source

* Function signatures fixed:

    * `io_prefix(void)`, `io_in_empty(void)` now ANSI-C compliant.
    * Comparator/split functions mark unused `tag` with `__attribute__((unused))`.
* Experimental files updated with SPDX headers.

### ✅ Tests

* Added **`tests/`** with CMake integration and coverage aggregation.
* `tests/build.sh` helper supports variants and coverage mode.
* Added test executables:

    * `test_io.c` — file helpers, formats, record sorting, partitions.
    * `test_io_in.c` — input formats, options, records-init, merge, unique.
    * `test_io_out.c` — output formats, write/record APIs, ext options.
* Coverage target generates HTML + console report.

---

## Impact

* 🚀 Modernized build system with multi-variant, dependency-aware CMake.
* 🛡️ Safer, clearer API with corrected signatures and unused parameter handling.
* 📖 Documentation and examples updated for real-world builds.
* ✅ High confidence with new test suite and coverage workflow.
