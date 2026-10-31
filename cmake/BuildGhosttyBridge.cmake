find_program(ZIG_EXECUTABLE zig REQUIRED)

set(GHOSTTY_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/ghostty" CACHE PATH "Path to unmodified Ghostty source")
set(GHOSTTY_STAGE_DIR "${CMAKE_CURRENT_BINARY_DIR}/ghostty-stage")
set(GHOSTTY_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/ghostty-prefix")
set(GHOSTTY_LIB "${GHOSTTY_PREFIX}/lib/ghostty-internal.so")
set(GHOSTTY_HEADER "${GHOSTTY_PREFIX}/include/ghostty.h")

file(MAKE_DIRECTORY "${GHOSTTY_PREFIX}/include")

add_custom_target(khostty_libghostty
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${GHOSTTY_STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${GHOSTTY_SOURCE_DIR}" "${GHOSTTY_STAGE_DIR}"
    COMMAND patch -p1 -d "${GHOSTTY_STAGE_DIR}" -i "${CMAKE_CURRENT_SOURCE_DIR}/bridge/embedded_linux.patch" -N -r - || true
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/bridge/khostty_bridge.zig" "${GHOSTTY_STAGE_DIR}/src/khostty_bridge.zig"
    COMMAND "${CMAKE_COMMAND}" -E echo "comptime { _ = @import(\"khostty_bridge.zig\"); }" >> "${GHOSTTY_STAGE_DIR}/src/main_c.zig"
    COMMAND "${ZIG_EXECUTABLE}" build
        -Dversion-string=1.4.0
        -Dapp-runtime=none
        -Demit-lib-vt=false
        -Drenderer=opengl
        -Demit-exe=false
        -Demit-docs=false
        -Demit-macos-app=false
        -Demit-xcframework=false
        -fsys=fontconfig
        -fsys=freetype
        -fsys=harfbuzz
        -fsys=oniguruma
        -fsys=zlib
        -fsys=libpng
        -Dcpu=baseline
        -Doptimize=ReleaseFast
        --prefix "${GHOSTTY_PREFIX}"
    COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/bridge/khostty_bridge.h" "${GHOSTTY_PREFIX}/include/khostty_bridge.h"
    WORKING_DIRECTORY "${GHOSTTY_STAGE_DIR}"
    BYPRODUCTS "${GHOSTTY_LIB}" "${GHOSTTY_HEADER}"
    COMMENT "Staging and compiling libghostty.so with Khostty bridge"
    USES_TERMINAL
    VERBATIM
)

add_library(Ghostty::Core SHARED IMPORTED GLOBAL)
set_target_properties(Ghostty::Core PROPERTIES
    IMPORTED_LOCATION "${GHOSTTY_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${GHOSTTY_PREFIX}/include"
)
add_dependencies(Ghostty::Core khostty_libghostty)
