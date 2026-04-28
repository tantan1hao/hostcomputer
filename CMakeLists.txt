cmake_minimum_required(VERSION 3.16)
project(hostcomputer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局禁用优化，绕过 GCC 13.1 汇编器 Segmentation fault
add_compile_options(-O0)

# ============================================
# Qt6 Modules - 灵活配置
# ============================================
# 首先尝试查找Qt6，如果失败则尝试Qt5
find_package(Qt6 QUIET COMPONENTS
    Core
    Widgets
    Network
    Qml
    Quick           # <-- 确保添加
    QuickWidgets    # <-- 用于在 Widget 中嵌入 QML
)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 REQUIRED COMPONENTS
        Core
        Widgets
        Network
    )
    set(QT_VERSION 5)
else()
    message(STATUS "Qt6 found successfully")
    # 尝试添加可选的Qt6模块
    find_package(Qt6 QUIET COMPONENTS Qml)
    find_package(Qt6 QUIET COMPONENTS Quick)
    find_package(Qt6 QUIET COMPONENTS QuickWidgets)
    find_package(Qt6 QUIET COMPONENTS Quick3D)
    find_package(Qt6 QUIET COMPONENTS OpenGL)
    find_package(Qt6 QUIET COMPONENTS OpenGLWidgets)
    find_package(Qt6 QUIET COMPONENTS Charts)
    set(QT_VERSION 6)
endif()

# ============================================
# OpenCV 依赖（可选）
# ============================================
find_package(OpenCV QUIET)
if(OpenCV_FOUND)
    message(STATUS "OpenCV found successfully: ${OpenCV_VERSION}")
    # 设置OpenCV包含目录
    include_directories(${OpenCV_INCLUDE_DIRS})
    # 设置OpenCV链接库
    set(OpenCV_LIBS
        ${OpenCV_LIBS}
        opencv_core
        opencv_imgproc
        opencv_imgcodecs
        opencv_videoio
        opencv_highgui
    )
else()
    message(STATUS "OpenCV not found, using Qt Camera only")
    set(OpenCV_LIBS "")
endif()

# ============================================
# 子目录（分层）
# 每一层一个文件夹，也对应一个 CMakeLists.txt
# ============================================
if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/SDL/CMakeLists.txt")
    set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "Do not build SDL3 static library" FORCE)
    set(SDL_AUDIO OFF CACHE BOOL "Disable SDL3 audio subsystem" FORCE)
    set(SDL_VIDEO OFF CACHE BOOL "Disable SDL3 video subsystem" FORCE)
    set(SDL_GPU OFF CACHE BOOL "Disable SDL3 GPU subsystem" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "Disable SDL3 render subsystem" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "Disable SDL3 camera subsystem" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "Disable SDL3 dialog subsystem" FORCE)
    set(SDL_TRAY OFF CACHE BOOL "Disable SDL3 tray subsystem" FORCE)
    set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "Allow SDL3 without X11/Wayland window support" FORCE)
    set(SDL_JOYSTICK ON CACHE BOOL "Enable SDL3 joystick subsystem" FORCE)
    set(SDL_HAPTIC ON CACHE BOOL "Enable SDL3 haptic subsystem" FORCE)
    set(SDL_HIDAPI ON CACHE BOOL "Enable SDL3 HIDAPI support" FORCE)
    set(SDL_HIDAPI_JOYSTICK ON CACHE BOOL "Enable SDL3 HIDAPI joystick support" FORCE)
    set(SDL_SENSOR ON CACHE BOOL "Enable SDL3 sensor subsystem" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "Do not build SDL3 test library" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "Do not build SDL3 tests" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL3 examples" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "Do not install vendored SDL3" FORCE)

    set(HOSTCOMPUTER_AUTOMOC_SAVED ${CMAKE_AUTOMOC})
    set(HOSTCOMPUTER_AUTOUIC_SAVED ${CMAKE_AUTOUIC})
    set(HOSTCOMPUTER_AUTORCC_SAVED ${CMAKE_AUTORCC})
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTOUIC OFF)
    set(CMAKE_AUTORCC OFF)
    add_subdirectory(third_party/SDL EXCLUDE_FROM_ALL)
    set(CMAKE_AUTOMOC ${HOSTCOMPUTER_AUTOMOC_SAVED})
    set(CMAKE_AUTOUIC ${HOSTCOMPUTER_AUTOUIC_SAVED})
    set(CMAKE_AUTORCC ${HOSTCOMPUTER_AUTORCC_SAVED})

    message(STATUS "Vendored SDL3 enabled")
else()
    message(STATUS "Vendored SDL3 not found; platform gamepad fallback only")
endif()

add_subdirectory(src/utils)          # 工具层（日志、错误处理）
add_subdirectory(src/controller)     # 业务逻辑层
add_subdirectory(src/communication)  # 通信层（CAN/TCP/Serial）
# add_subdirectory(resources)        # 资源(qrc) - 暂时注释掉，等有资源文件时再启用

# ============================================
# 主程序 UI 层
# ============================================
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
)

qt_add_executable(hostcomputer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
    resources/resources.qrc
)

# 解决 GCC 13.1 汇编器内部错误 (Segmentation fault)
# 对 mainwindow.cpp 禁用优化以绕过 as.exe 的 bug
set_source_files_properties(mainwindow.cpp PROPERTIES COMPILE_FLAGS "-O0")

# ============================================
# 链接各层模块
# ============================================
# 根据Qt版本动态链接
if(QT_VERSION EQUAL 6)
    target_link_libraries(hostcomputer PRIVATE
        controller_layer
        communication_layer
        utils_layer
        Qt6::Core
        Qt6::Widgets
        Qt6::Network
    )
    # 添加可选的Qt6模块
    if(TARGET Qt6::Quick)
        target_link_libraries(hostcomputer PRIVATE Qt6::Quick)
    endif()
    if(TARGET Qt6::QuickWidgets)
        target_link_libraries(hostcomputer PRIVATE Qt6::QuickWidgets)
    endif()
    if(TARGET Qt6::Quick3D)
        target_link_libraries(hostcomputer PRIVATE Qt6::Quick3D)
    endif()
    if(TARGET Qt6::OpenGL)
        target_link_libraries(hostcomputer PRIVATE Qt6::OpenGL)
    endif()
    if(TARGET Qt6::OpenGLWidgets)
        target_link_libraries(hostcomputer PRIVATE Qt6::OpenGLWidgets)
    endif()
    if(TARGET Qt6::Charts)
        target_link_libraries(hostcomputer PRIVATE Qt6::Charts)
    endif()
else()
    target_link_libraries(hostcomputer PRIVATE
        controller_layer
        communication_layer
        Qt5::Core
        Qt5::Widgets
        Qt5::Network
    )
endif()

# 注册QML模块（嵌入RobotView.qml到资源系统）
if(TARGET Qt6::Quick)
    qt_add_qml_module(hostcomputer
        URI RobotUI
        VERSION 1.0
        QML_FILES
            resources/qml/RobotView.qml
        NO_RESOURCE_TARGET_PATH
    )
endif()

qt_finalize_executable(hostcomputer)
