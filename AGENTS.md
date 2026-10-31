# Agent Guidelines for Khostty

> [!CAUTION]
> ### STRICT DEVELOPER-ONLY RESTRICTION: NO COMMITS OR PRS
> **AI agents are strictly forbidden from creating git commits or opening Pull Requests.**
> - **DO NOT** run `git commit`.
> - **DO NOT** run `git push`.
> - **DO NOT** create PRs (`gh pr create`, API calls, etc.).
> - Commit creation, branching, tagging, and PR workflows are exclusively restricted to the human developer.
> - Agents must confine their work to editing files in the working directory, compiling, running tests, and reporting verification results back to the developer.

---

## 1. Project Overview

**Khostty** is a native KDE Plasma 6 terminal emulator powered by the `libghostty` engine.

- **Frontend**: C++20, Qt 6, KDE Frameworks 6 (`KXmlGui`, `KConfig`, `KCoreAddons`, `KI18n`, `KNotifications`, `KWindowSystem`, `KGlobalAccel`, `LayerShellQt`).
- **Backend**: Ghostty terminal core (Zig 0.17.0) adapted for Linux embedding with DMA-BUF / OpenGL frame export.
- **Build System**: CMake (>= 3.25), Ninja, and Nix Flakes.

### Repository Layout

```
khostty/
├── bridge/                     # Ghostty C-ABI bridge extensions & patches
│   ├── embedded_linux.patch    # Upstream Ghostty patch enabling Linux embedded runtime & EGL
│   ├── khostty_bridge.h        # C-ABI header for frame export & presentation health
│   └── khostty_bridge.zig      # Zig implementation exporting DMA-BUF / memory frames
├── cmake/
│   └── BuildGhosttyBridge.cmake# Fallback build script compiling libghostty from source
├── dist/                       # Linux desktop & integration metadata
│   └── linux/
│       ├── org.kde.khostty.desktop   # Desktop application entry
│       └── org.kde.khostty.notifyrc  # KNotification event specifications
├── nix/
│   ├── khostty.nix             # Nix derivation for the Khostty Qt6/KF6 application
│   └── libghostty-bridge.nix   # Nix derivation for libghostty with Khostty bridge
├── resources/                  # Qt resources & KDE XMLGUI menu definitions
│   ├── khosttyui.rc            # KF6 action definitions (Tabs, Windows, Worklanes, Splits)
│   └── resources.qrc           # Qt resource bundle
├── src/                        # C++ application source code
│   └── main.cpp                # Application entry point and window setup
├── CMakeLists.txt              # Root CMake project configuration
├── PKGBUILD                    # Arch Linux package specification
├── flake.nix                   # Nix Flakes definition (packages & devShell)
└── flake.lock                  # Nix dependency lockfile
```

---

## 2. Build Procedures

The primary and recommended build environment uses **Nix Flakes**. Alternative build options (standalone CMake and Arch Linux PKGBUILD) are also maintained.

### A. Primary Workflow: Nix Development Shell (Recommended)

The Nix development shell (`nix develop`) automatically provides CMake, Ninja, Zig 0.17.0, Qt 6, all KDE Frameworks 6 libraries, and pre-built `libghostty-bridge` with the required environment variables (`GHOSTTY_PREFIX`, `GHOSTTY_RESOURCES_DIR`, `TERMINFO_DIRS`).

#### 1. Entering the shell
```bash
nix develop
```

#### 2. Configuring and building Khostty
Inside the development shell:
```bash
# Configure build with Ninja
cmake -B build -G Ninja

# Compile the binary
cmake --build build
```

#### 3. Incremental compilation
After modifying source files in `src/` or `resources/`:
```bash
cmake --build build
```

#### 4. Clean rebuild
```bash
rm -rf build
cmake -B build -G Ninja
cmake --build build
```

#### 5. Non-interactive execution via Nix develop
To invoke a build command directly from outside the shell:
```bash
nix develop --command cmake --build build
```

---

### B. Building via Nix Packages Directly

You can build the complete package or specific components directly using `nix build`:

- **Build Khostty binary and package**:
  ```bash
  nix build .#khostty
  # or simply:
  nix build
  ```
  The built application will be symlinked to `./result/bin/khostty`.

- **Build only the libghostty bridge library**:
  ```bash
  nix build .#libghostty-bridge
  ```
  The resulting headers and shared objects will be symlinked to `./result/`.

- **Evaluate and verify flake definitions**:
  ```bash
  nix flake check
  nix flake show
  ```

---

### C. Standalone CMake Build (Non-Nix)

If developing in an environment with native Qt 6, KF 6, and Zig installed:

- **Using a pre-built libghostty directory (`GHOSTTY_PREFIX`)**:
  ```bash
  cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DGHOSTTY_PREFIX=/path/to/ghostty-prefix
  cmake --build build
  ```

- **Building from a Ghostty source submodule (`GHOSTTY_SOURCE_DIR`)**:
  If `GHOSTTY_PREFIX` is omitted, CMake falls back to `cmake/BuildGhosttyBridge.cmake`, staging and compiling Ghostty with Zig:
  ```bash
  cmake -B build -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DGHOSTTY_SOURCE_DIR=/path/to/ghostty
  cmake --build build
  ```

---

### D. Arch Linux (PKGBUILD)

To build and package on Arch Linux:
```bash
makepkg -si
```

---

## 3. Running & Testing

### Running the Application

- **From the local build directory (inside `nix develop`)**:
  ```bash
  ./build/khostty
  ```

- **Directly via Nix**:
  ```bash
  nix run .#khostty
  ```

- **Environment variables for manual execution**:
  If running outside `nix develop` or outside the Nix-wrapped launcher, ensure Ghostty resource paths are exported:
  ```bash
  export GHOSTTY_RESOURCES_DIR="${GHOSTTY_PREFIX}/share/ghostty"
  export TERMINFO_DIRS="${GHOSTTY_PREFIX}/share/terminfo:$TERMINFO_DIRS"
  ./build/khostty
  ```

### Testing & Verification Commands

1. **Smoke Test**:
   Currently, launching `khostty` instantiates `SmokeWindow`, initializes `libghostty` runtime via `ghostty_init`, queries `ghostty_info()`, and prints version diagnostic logs.
   ```bash
   ./build/khostty
   ```

2. **Test Suites (CTest)**:
   When automated tests are registered with CTest:
   ```bash
   ctest --test-dir build --output-on-failure
   # or
   ninja -C build test
   ```

3. **Flake Quality Checks**:
   ```bash
   nix flake check
   ```

4. **Code Formatting**:
   Maintain C++ formatting consistency using `clang-format`:
   ```bash
   clang-format -i src/*.cpp bridge/*.h
   ```

---

## 4. Key Architectural Patterns & Invariants

1. **Ghostty Bridge & Memory Ownership**:
   - `khostty_surface_take_frame()` acquires the latest rendered frame from Ghostty's queue (`khostty_exported_frame_s`).
   - Every acquired frame **must** be released with `khostty_surface_release_frame()` once DMA-BUF file descriptors or memory pixel buffers are ingested by OpenGL/Qt.
   - Failure to release will leak DMA-BUF file descriptors and internal frame boxes.

2. **KDE XMLGUI Architecture**:
   - Menu bars, toolbars, and standard actions are managed via `KXmlGuiWindow` and defined in `resources/khosttyui.rc`.
   - New actions must be declared in `resources/khosttyui.rc` and added to `actionCollection()` in C++.
   - Actions should use `KStandardAction` whenever possible for standard desktop shortcuts.

3. **Plasma Wayland & LayerShell Integration**:
   - Window handling respects Wayland protocols and optionally links `LayerShellQt::Interface` when available (`KHOSTTY_HAS_LAYERSHELL`).
   - Global shortcuts rely on `KF6::GlobalAccel` (`KHOSTTY_HAS_KGLOBALACCEL`).

---

## 5. Agent Instructions Checklist

When assigned a task in this repository, follow these rules:

- [ ] **NEVER run `git commit` or `git push`.**
- [ ] **NEVER open Pull Requests.**
- [ ] Make all edits directly in the repository working tree.
- [ ] Verify changes by building inside `nix develop` (`cmake --build build`) or running `nix build .#khostty`.
- [ ] Verify that the application starts or tests pass before completing the turn.
- [ ] Leave all changes unstaged/staged for the developer to review, test in their environment, and commit.

## StormDrain Persistent Memory Protocol
This project uses StormDrain for persistent cross-session architectural memory via MCP tools (`sd_*`) or CLI (`stormdrain <cmd>`).

### Mandatory Sandboxing & Execution Rules:
1. **Strict Sandboxing (CRITICAL)**: NEVER directly access, inspect, or modify files inside the `~/.stormdrain` directory. The internal storage engine is managed exclusively by StormDrain.
2. **Authorized Execution (MCP or CLI)**: Always interact with StormDrain exclusively through official MCP server tools (`sd_*`) or the official `stormdrain` CLI. Never bypass these interfaces with raw SQLite queries or direct file edits to the storage repository.

### Workflow:
- **Primary Source Reader**: Use `sd_read(path="...")` or CLI `stormdrain read <path>` to inject topological invariants and caller constraints.
- **Pre-Edit Invariant Check**: Call `sd_recall(target_file="...")` or CLI `stormdrain recall -t <path> [--json]` before modifying code.
- **Record Discoveries**: Call `sd_memory(action="add", ...)` / `sd_add(...)` or CLI `stormdrain add <type> <title> [content|--file <path>|-] [--json]` on non-obvious invariants (canonical types: `fact`, `decision`, `guide`, `warning`, `concept`).
- **Query, Browse & Inspect**: Call `sd_memory(action="list"|"search"|"get"|"relate", ...)` or CLI `stormdrain list [--orphans] [--json]`, `stormdrain search <query> [--json]`, `stormdrain get <id> [--json]`.
- **Harvest & Curate**: Run `/sd_harvest` (or CLI `stormdrain harvest`) at session end; run `/sd_curate` (or CLI `stormdrain curate`) and `stormdrain consolidate <target> [ids...]` to consolidate candidate micro-memories.
- **Export & Import**: Use `stormdrain export [<ctx...> | --all] [-o <file.json|.json.gz|->] [--no-codemaps]` and `stormdrain import <file|-> [--as <ctx>] [--bind <dir>] [--strategy merge|overwrite|skip]` to migrate contexts or entire instances across machines.

### Editorial Rule:
Record non-obvious invariants, architectural trade-offs, and critical gotchas; do NOT record routine implementation summaries or transient task progress.
