# Agent Guidelines for Khostty

> [!CAUTION]
> ### STRICT DEVELOPER RESTRICTION: NO COMMITS OR PRS
> **Agents are strictly forbidden from running `git commit`, `git push`, or opening PRs (`gh pr create`).**
> All git commits, branch management, and PR submissions are reserved exclusively for the human developer. Confine work strictly to editing working files, building, testing, and reporting status.

---

## 1. Project Overview
- **Khostty**: Native KDE Plasma 6 terminal emulator powered by `libghostty` (C++20, Qt 6, KF 6, Zig 0.17.0).
- **Subsystems**:
  - `src/`: Qt6 / KDE Frameworks 6 GUI (`KXmlGuiWindow`, `resources/khosttyui.rc`).
  - `bridge/`: Ghostty C-ABI bridge extensions exporting DMA-BUF / memory frames (`khostty_bridge.h`, `khostty_bridge.zig`).

---

## 2. Build & Test Commands

The primary build environment is **Nix Flakes**.

```bash
# Enter dev shell (or prefix commands with `nix develop --command`)
nix develop

# Configure & build
cmake -B build -G Ninja
cmake --build build

# Clean rebuild
rm -rf build && cmake -B build -G Ninja && cmake --build build

# Run application & tests
./build/khostty                                 # Local build
nix run .#khostty                               # Via Nix directly
ctest --test-dir build --output-on-failure       # CTest suite
nix flake check                                 # Flake check
clang-format -i src/*.cpp bridge/*.h            # Formatting
```

### Direct Package & Fallback Builds
- **Nix full package**: `nix build .#khostty` (binary at `./result/bin/khostty`).
- **Nix bridge only**: `nix build .#libghostty-bridge`.
- **Standalone CMake**: `cmake -B build -G Ninja -DGHOSTTY_PREFIX=/path/to/ghostty-prefix && cmake --build build`.
- **Arch Linux**: `makepkg -si`.

---

## 3. Core Architectural Invariants
1. **Frame Ownership**: Acquired frames from `khostty_surface_take_frame()` **must** be freed with `khostty_surface_release_frame()` once DMA-BUF FDs or pixel buffers are ingested to prevent kernel FD leaks.
2. **KDE XMLGUI**: Menu items, worklanes, and splits are defined in `resources/khosttyui.rc` and wired in C++ via `actionCollection()`.
3. **Ghostty Runtime Env**: Uninstalled runs outside `nix develop` require `GHOSTTY_RESOURCES_DIR` and `TERMINFO_DIRS`.

---

## 4. StormDrain Memory Protocol
Persistent cross-session architectural invariants are managed via StormDrain MCP / CLI:
- **Inspect Invariants Before Edits**: Call `sd_recall(target_file="...")` or `stormdrain recall -t <path>`.
- **Primary Source Reader**: Call `sd_read(path="...")` or `stormdrain read <path>` for topological AST & invariants.
- **Record Discoveries**: Call `sd_memory(action="add", type="...", title="...", content="...")`.
- **Strict Storage Sandboxing**: Never directly inspect, query, or edit `~/.stormdrain` raw files; interact exclusively via `sd_*` tools or CLI.
