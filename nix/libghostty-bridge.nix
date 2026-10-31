{
  lib,
  stdenv,
  callPackage,
  pkg-config,
  zig_0_17,
  autoPatchelfHook,
  patchelf,
  ncurses,
  git,
  fontconfig,
  freetype,
  harfbuzz,
  oniguruma,
  zlib,
  libpng,
  libGL,
  expat,
  libxml2,
  bzip2,
  simdutf,
  glslang,
  spirv-cross,
  ghosttySource,
  optimize ? "ReleaseFast",
}: let
  strip = optimize != "Debug";
in
  stdenv.mkDerivation (finalAttrs: {
    pname = "libghostty-bridge";
    version = "1.4.0";

    src = ghosttySource;

    deps = callPackage "${ghosttySource}/build.zig.zon.nix" {
      name = "ghostty-cache-${finalAttrs.version}";
    };

    nativeBuildInputs = [
      pkg-config
      zig_0_17
      autoPatchelfHook
      patchelf
      ncurses # provides `tic` for terminfo compilation
      git
    ];

    buildInputs = [
      fontconfig
      freetype
      harfbuzz
      oniguruma
      zlib
      libpng
      libGL
      expat
      libxml2
      bzip2
      simdutf
      glslang
      spirv-cross
    ];

    dontStrip = !strip;
    dontUseCmakeConfigure = true;
    dontUseNinjaBuild = true;
    dontUseNinjaInstall = true;
    dontUseMakeBuild = true;
    dontUseMakeInstall = true;

    postPatch = ''
      patch -p1 < ${../bridge/embedded_linux.patch}
      cp ${../bridge/khostty_bridge.zig} src/khostty_bridge.zig
      echo 'comptime { _ = @import("khostty_bridge.zig"); }' >> src/main_c.zig
    '';

    target = "${stdenv.hostPlatform.parsed.cpu.name}-linux-gnu";

    zigBuildFlags = [
      "--system"
      "${finalAttrs.deps}"
      "-Dversion-string=${finalAttrs.version}"
      "-Dtarget=${finalAttrs.target}"
      "-Dcpu=baseline"
      "-Dapp-runtime=none"
      "-Demit-lib-vt=false"
      "-Drenderer=opengl"
      "-Demit-exe=false"
      "-Demit-docs=false"
      "-Demit-macos-app=false"
      "-Demit-xcframework=false"
      "-fsys=glslang"
      "--search-prefix"
      "${lib.getLib glslang}"
      "-Doptimize=${optimize}"
      "-Dstrip=${lib.boolToString strip}"
      "--prefix"
      "${placeholder "out"}"
    ];

    buildPhase = ''
      runHook preBuild
      export ZIG_GLOBAL_CACHE_DIR="$TMPDIR/zig-cache"
      zig build $zigBuildFlags
      runHook postBuild
    '';

    installPhase = ''
      runHook preInstall
      # zig build installs directly to $out via --prefix
      runHook postInstall
    '';

    postInstall = ''
      # Ensure lib/ exists and create libghostty.so symlink
      mkdir -p "$out/lib"
      if [ -f "$out/lib/ghostty-internal.so" ]; then
        patchelf --set-soname libghostty.so "$out/lib/ghostty-internal.so"
        ln -sf ghostty-internal.so "$out/lib/libghostty.so"
      fi

      # Install Khostty bridge C header alongside ghostty.h
      mkdir -p "$out/include"
      cp ${../bridge/khostty_bridge.h} "$out/include/khostty_bridge.h"
    '';

    meta = {
      description = "libghostty shared library with Khostty Linux bridge extensions";
      homepage = "https://ghostty.org";
      license = lib.licenses.mit;
      platforms = [
        "x86_64-linux"
        "aarch64-linux"
      ];
    };
  })
