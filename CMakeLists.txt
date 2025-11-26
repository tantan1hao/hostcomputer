cmake_minimum_required(VERSION 3.16)
project(hostcomputer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ============================================
# Qt6 Modules - 灵活配置
# ============================================
# 首先尝试查找Qt6，如果失败则尝试Qt5
find_package(Qt6 QUIET COMPONENTS
    Core
    Widgets
    Network
    SerialPort
    Multimedia
    MultimediaWidgets
)

if(NOT Qt6_FOUND)
    message(STATUS "Qt6 not found, trying Qt5...")
    find_package(Qt5 REQUIRED COMPONENTS
        Core
        Widgets
        Network
        SerialPort
        Multimedia
        MultimediaWidgets
    )
    set(QT_VERSION 5)
else()
    message(STATUS "Qt6 found successfully")
    # 尝试添加可选的Qt6模块
    find_package(Qt6 QUIET COMPONENTS Quick)
    find_package(Qt6 QUIET COMPONENTS QuickWidgets)
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
add_subdirectory(src/controller)     # 业务逻辑层
add_subdirectory(src/communication)  # 通信层（CAN/TCP/Serial）
add_subdirectory(src/parser)         # 协议解析层
add_subdirectory(src/model)          # ��据模型层
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
)

# ============================================
# 链接各层模块
# ============================================
# 根据Qt版本动态链接
if(QT_VERSION EQUAL 6)
    target_link_libraries(hostcomputer PRIVATE
        controller_layer
        communication_layer
        parser_layer
        model_layer
        Qt6::Core
        Qt6::Widgets
        Qt6::SerialPort
        Qt6::Network
        Qt6::Multimedia
        Qt6::MultimediaWidgets
    )
    # 添加可选的Qt6模块
    if(TARGET Qt6::Quick)
        target_link_libraries(hostcomputer PRIVATE Qt6::Quick)
    endif()
    if(TARGET Qt6::QuickWidgets)
        target_link_libraries(hostcomputer PRIVATE Qt6::QuickWidgets)
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
        parser_layer
        model_layer
        Qt5::Core
        Qt5::Widgets
        Qt5::SerialPort
        Qt5::Network
        Qt5::Multimedia
        Qt5::MultimediaWidgets
    )
endif()

qt_finalize_executable(hostcomputer)
