# Khostty

I like [Ghostty](https://ghostty.org/) and I like KDE Plasma. Unfortunately for me, Ghostty uses GTK for its UI elements on Linux. While this is a reasonable choice, my preference is to have something that feels more consistent on my desktop. Now that libghostty exists, it's possible to have my cake and eat it too. 

My goals are as follows:

- Bring many of the things I like about Ghostty to my KDE desktop in a native-feeling way
- Create a tool that scales across all my use cases, from the basic every-day terminal to the serious developer environment
- Explore some fun concepts for terminal UI/UX, such as sidescrolling panes and my bad ideas around sshing
- Do this in a maintainable way by consuming libghostty as a dependency, rather than forking ghostty directly

<details>
<summary><b>Prior Art</b></summary>

- Ghostty, libghostty
- [Qhostty](https://github.com/angristan/qhostty), a QT fork of Ghostty. I liked this a lot, but its nature as a fork makes it difficult to maintain. 
- [Zentty](https://zentty.org/), which I found from the [awesome-libghostty](https://github.com/Uzaaft/awesome-libghostty) page. This terminal uses [niri-wm](https://github.com/niri-wm/niri) style worklanes, which I've incorporated as well.

</details>

If you want something better, KDE announced a new libghostty based terminal called [Kiwi](https://blogs.kde.org/2026/11/01/introducing-kwi/) the day after I started building this. 

---

## Quickstart & Installation

### Run Directly via Nix

```bash
# Run immediately without installing
nix run github:dhruvmittal/khostty
```

<details>
<summary><b>NixOS Flake Configuration</b></summary>

Add Khostty to your system flake inputs:

```nix
{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    khostty = {
      url = "github:dhruvmittal/khostty";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, khostty, ... }: {
    nixosConfigurations.myhostname = nixpkgs.lib.nixosSystem {
      system = "x86_64-linux";
      modules = [
        ({ pkgs, ... }: {
          environment.systemPackages = [
            khostty.packages.${pkgs.system}.default
          ];
        })
      ];
    };
  };
}
```

</details>

<details>
<summary><b>Local Development Build</b></summary>

Enter the dev shell and compile using CMake and Ninja:

```bash
# Enter the build shell with all Qt6, KF6, and Zig dependencies
nix develop

# Configure and build
cmake -B build -G Ninja
cmake --build build

# Launch local binary
./build/khostty
```

</details>

### Install on Archlinux

<details>
<summary><b>PKGBUILD</b></summary>
```bash
# Build like you got this from the AUR
makepkg -si
```
</details>

---

## Keybindings

Khostty aligns with both Ghostty and native KDE shortcuts:

| Action | Shortcut | Context |
| :--- | :--- | :--- |
| **New Tab** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>T</kbd> | Tabs |
| **Close Tab / Split** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>W</kbd> | Tabs / Splits |
| **Next / Previous Tab** | <kbd>Ctrl</kbd> + <kbd>Tab</kbd> / <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Tab</kbd> | Tabs |
| **Go to Tab 1–9** | <kbd>Alt</kbd> + <kbd>1</kbd> .. <kbd>9</kbd> | Tabs |
| **Split Down** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>E</kbd> | Splits |
| **Split Right** (New Column) | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>O</kbd> | Splits |
| **Toggle Split Zoom** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>Return</kbd> | Splits |
| **Equalize Splits** | <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>=</kbd> | Splits |
| **Navigate Splits** | <kbd>Ctrl</kbd> + <kbd>Alt</kbd> + <kbd>Arrow Keys</kbd> | Splits |
| **Find in Buffer** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>F</kbd> | Search |
| **Copy / Paste** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>C</kbd> / <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>V</kbd> | Clipboard |
| **Preferences** | <kbd>Ctrl</kbd> + <kbd>Shift</kbd> + <kbd>,</kbd> | Settings |

<details>
<summary><b>Worklanes & Navigation Overview</b></summary>

* **Worklanes**: Tabs function as independent worklanes containing one or more terminal splits arranged in flexible columns and rows.
* **Collapsible Sidebar**: View terminal trees, active processes, and background alerts across all workspaces.
* **Zoom**: Maximize a single split temporarily to focus, and toggle back to your multi-column layout without losing your split geometry.

</details>

---

## Configuration

Khostty separates KDE-specific desktop integration options from Ghostty terminal emulation behavior:

<details open>
<summary><b>1. Khostty Settings (<code>khosttyrc</code>)</b></summary>

Stored in `~/.config/khosttyrc` (managed natively via the Preferences dialog):

* **Font**: System fixed font tracking (`UseSystemFixedFont=true`) vs. custom font sizing.
* **General**: Working directory inheritance for new tabs (`NewTabInheritsPwd`) and new splits (`NewSplitInheritsPwd`).
* **Sidebar**: Worklane tree visibility, width, and background status indicators.

</details>

<details open>
<summary><b>2. Ghostty Settings (<code>~/.config/ghostty/config</code>)</b></summary>

See https://ghostty.org/docs/config.  

</details>

## AI Disclosure

I am using AI tools including Google's Antigravity IDE during development. I am using agentic AI to debug and refactor. I am a professional C++ developer but have low knowledge of QtWidgets. 

## Contributing

- Please feel free to fork or PR me
- This is a hobby project, so I'm more likely to respond to issues that also come with solutions
- AI code ok but please write your own issues and PRs, I will probably stop reading if it looks or feels like there's nobody on the other end