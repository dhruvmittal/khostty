{
  description = "Khostty: A native KDE Plasma 6 terminal emulator powered by libghostty";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    ghostty = {
      url = "github:ghostty-org/ghostty/v1.4.0";
      flake = false;
    };
    zig = {
      url = "github:mitchellh/zig-overlay";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = {
    self,
    nixpkgs,
    ghostty,
    zig,
    ...
  }: let
    systems = ["x86_64-linux" "aarch64-linux"];
    forAllSystems = f:
      nixpkgs.lib.genAttrs systems (system:
        f system (import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        }));
  in {
    packages = forAllSystems (system: pkgs: let
      zig_0_17 = zig.packages.${system}."0.17.0";

      libghostty-bridge = pkgs.callPackage ./nix/libghostty-bridge.nix {
        ghosttySource = ghostty;
        inherit zig_0_17;
      };

      khostty = pkgs.callPackage ./nix/khostty.nix {
        inherit libghostty-bridge;
      };
    in {
      inherit libghostty-bridge khostty;
      default = khostty;
    });

    devShells = forAllSystems (system: pkgs: let
      zig_0_17 = zig.packages.${system}."0.17.0";
      libghostty-bridge = self.packages.${system}.libghostty-bridge;
    in {
      default = pkgs.mkShell {
        nativeBuildInputs = [
          pkgs.cmake
          pkgs.ninja
          pkgs.pkg-config
          zig_0_17
          pkgs.kdePackages.extra-cmake-modules
          pkgs.kdePackages.wrapQtAppsHook
        ];
        buildInputs = [
          pkgs.kdePackages.qtbase
          pkgs.kdePackages.qtwayland
          pkgs.kdePackages.breeze
          pkgs.kdePackages.breeze-icons
          pkgs.kdePackages.plasma-integration
          pkgs.kdePackages.qqc2-desktop-style
          pkgs.kdePackages.kxmlgui
          pkgs.kdePackages.kconfig
          pkgs.kdePackages.kconfigwidgets
          pkgs.kdePackages.kwidgetsaddons
          pkgs.kdePackages.kcoreaddons
          pkgs.kdePackages.ki18n
          pkgs.kdePackages.kglobalaccel
          pkgs.kdePackages.kwindowsystem
          pkgs.kdePackages.knotifications
          pkgs.kdePackages.kio
          pkgs.kdePackages.layer-shell-qt
          libghostty-bridge
        ];
        GHOSTTY_PREFIX = "${libghostty-bridge}";
        GHOSTTY_RESOURCES_DIR = "${libghostty-bridge}/share/ghostty";
        TERMINFO_DIRS = "${libghostty-bridge}/share/terminfo";
      };
    });
  };
}
