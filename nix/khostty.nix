{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  kdePackages,
  libGL,
  libghostty-bridge,
}:
stdenv.mkDerivation (finalAttrs: {
  pname = "khostty";
  version = "0.1.0";

  src = lib.cleanSourceWith {
    src = ../.;
    filter = path: type: let
      baseName = baseNameOf (toString path);
    in
      !(baseName == ".git" || baseName == "build" || baseName == "result");
  };

  nativeBuildInputs = [
    cmake
    ninja
    pkg-config
    kdePackages.extra-cmake-modules
    kdePackages.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.qtbase
    kdePackages.kxmlgui
    kdePackages.kconfig
    kdePackages.kconfigwidgets
    kdePackages.kcoreaddons
    kdePackages.ki18n
    kdePackages.kglobalaccel
    kdePackages.kwindowsystem
    kdePackages.knotifications
    kdePackages.kio
    kdePackages.layer-shell-qt
    libGL
    libghostty-bridge
  ];

  cmakeFlags = [
    "-DGHOSTTY_PREFIX=${libghostty-bridge}"
  ];

  qtWrapperArgs = [
    "--set"
    "GHOSTTY_RESOURCES_DIR"
    "${libghostty-bridge}/share/ghostty"
    "--prefix"
    "TERMINFO_DIRS"
    ":"
    "${libghostty-bridge}/share/terminfo"
  ];

  meta = {
    description = "A native KDE Plasma 6 terminal emulator powered by libghostty";
    license = lib.licenses.mit;
    platforms = [
      "x86_64-linux"
      "aarch64-linux"
    ];
    mainProgram = "khostty";
  };
})
