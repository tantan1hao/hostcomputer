# ============================================
# Controller层 - 业务逻辑层
# ============================================
# 职责：
#   - 协调Model、Parser、Communication三层
#   - 处理用户交互逻辑
#   - 管理电机控制状态
#
# 依赖：
#   - 依赖其他所有层级 (Model, Parser, Communication)
#   - 依赖 Qt::Core
# ============================================

set(CONTROLLER_SOURCES
    controller.cpp
    controller.h
    CameraGridWidget.cpp
    CameraGridWidget.h
    CO2DisplayWidget.cpp
    CO2DisplayWidget.h
    GamepadDisplayWidget.cpp
    GamepadDisplayWidget.h
    DisplayLayoutManager.cpp
    DisplayLayoutManager.h
    KeyboardController.cpp
    KeyboardController.h
    handlekey.cpp
    handlekey.h
    RtspPlayerWidget.cpp
    RtspPlayerWidget.h
    RobotAttitudeWidget.cpp
    RobotAttitudeWidget.h
    RobotViewModel.cpp
    RobotViewModel.h
)

# 创建静态库
add_library(controller_layer STATIC ${CONTROLLER_SOURCES})

# 绕过 GCC 13.1 汇编器内部错误 (Segmentation fault)
set_source_files_properties(handlekey.cpp PROPERTIES COMPILE_FLAGS "-O0")

# 链接依赖
target_link_libraries(controller_layer PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Network
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickWidgets

    # 层级依赖
    communication_layer
    utils_layer

    # OpenCV 依赖（如果需要）
    # ${OpenCV_LIBS}  # 暂时注释，避免依赖问题
)

# Windows平台链接XInput库（手柄驱动）
if(WIN32)
    target_link_libraries(controller_layer PUBLIC xinput)
endif()

# 设置包含目录
target_include_directories(controller_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/communication
    ${CMAKE_SOURCE_DIR}/src/utils
)
