cmake_minimum_required(VERSION 3.25)
project(khostty VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(ECM 6.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
include(KDEInstallDirs6)
include(KDECMakeSettings)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    OpenGL
    OpenGLWidgets
    DBus
)

find_package(OpenGL REQUIRED COMPONENTS OpenGL EGL)

find_package(KF6 6.0 REQUIRED COMPONENTS
    XmlGui
    Config
    ConfigWidgets
    ColorScheme
    CoreAddons
    I18n
    Notifications
    WidgetsAddons
    OPTIONAL_COMPONENTS
    WindowSystem
    GlobalAccel
)

find_package(LayerShellQt QUIET)

# Libghostty resolution:
# If GHOSTTY_PREFIX is provided (from Nix or Arch), use it directly.
# Otherwise, fall back to building from vendor/ghostty submodule via cmake/BuildGhosttyBridge.cmake
if(NOT GHOSTTY_PREFIX AND DEFINED ENV{GHOSTTY_PREFIX})
    set(GHOSTTY_PREFIX "$ENV{GHOSTTY_PREFIX}")
endif()

if(GHOSTTY_PREFIX)
    set(GHOSTTY_INCLUDE_DIR "${GHOSTTY_PREFIX}/include")
    set(GHOSTTY_LIB_DIR "${GHOSTTY_PREFIX}/lib")
    find_library(GHOSTTY_LIBRARY
        NAMES ghostty-internal.so ghostty-internal libghostty.so ghostty
        PATHS "${GHOSTTY_LIB_DIR}"
        NO_DEFAULT_PATH
        REQUIRED
    )
    # ghostty-internal.so has SONAME libghostty.so; ensure the symlink exists for the runtime dynamic linker
    if(EXISTS "${GHOSTTY_LIB_DIR}/ghostty-internal.so" AND NOT EXISTS "${GHOSTTY_LIB_DIR}/libghostty.so")
        execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink "ghostty-internal.so" "${GHOSTTY_LIB_DIR}/libghostty.so")
    endif()
    add_library(Ghostty::Core UNKNOWN IMPORTED GLOBAL)
    set_target_properties(Ghostty::Core PROPERTIES
        IMPORTED_LOCATION "${GHOSTTY_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${GHOSTTY_INCLUDE_DIR}"
    )
else()
    include(cmake/BuildGhosttyBridge.cmake)
endif()

add_executable(khostty
    src/main.cpp
    src/core/ghostty_core.cpp
    src/terminal/terminal_surface.cpp
    src/terminal/terminal_pane.cpp
    src/terminal/khostty_splitter.cpp
    src/terminal/worklane_separator.cpp
    src/terminal/worklane_view.cpp
    src/window/worklane_sidebar.cpp
    src/window/main_window.cpp
    resources/resources.qrc
)

target_include_directories(khostty PRIVATE
    src
    bridge
)

target_link_libraries(khostty PRIVATE
    Ghostty::Core
    OpenGL::EGL
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    Qt6::DBus
    KF6::XmlGui
    KF6::ConfigCore
    KF6::ConfigWidgets
    KF6::ColorScheme
    KF6::CoreAddons
    KF6::I18n
    KF6::Notifications
    KF6::WidgetsAddons
)

if(TARGET KF6::WindowSystem)
    target_link_libraries(khostty PRIVATE KF6::WindowSystem)
    target_compile_definitions(khostty PRIVATE KHOSTTY_HAS_KWINDOWSYSTEM=1)
endif()

if(TARGET KF6::GlobalAccel)
    target_link_libraries(khostty PRIVATE KF6::GlobalAccel)
    target_compile_definitions(khostty PRIVATE KHOSTTY_HAS_KGLOBALACCEL=1)
endif()

if(TARGET LayerShellQt::Interface)
    target_link_libraries(khostty PRIVATE LayerShellQt::Interface)
    target_compile_definitions(khostty PRIVATE KHOSTTY_HAS_LAYERSHELL=1)
endif()

if(GHOSTTY_PREFIX)
    target_compile_definitions(khostty PRIVATE GHOSTTY_PREFIX="${GHOSTTY_PREFIX}")
endif()

include(GNUInstallDirs)
install(TARGETS khostty RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dist/linux/com.github.khostty.desktop")
    install(FILES "dist/linux/com.github.khostty.desktop"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications")
endif()
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dist/linux/com.github.khostty.notifyrc")
    install(FILES "dist/linux/com.github.khostty.notifyrc"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/knotifications6")
endif()
