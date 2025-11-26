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
    CameraManager_simple.cpp
    CameraManager_simple.h
    VideoWidget.cpp
    VideoWidget.h
)

# 创建静态库
add_library(controller_layer STATIC ${CONTROLLER_SOURCES})

# 链接依赖
target_link_libraries(controller_layer PUBLIC
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets

    # OpenCV 依赖（如果需要）
    # ${OpenCV_LIBS}  # 暂时注释，避免依赖问题

    # TODO: 添加其他层级依赖（实现后启用）
    # model_layer
    # parser_layer
    # communication_layer
)

# 设置包含目录
target_include_directories(controller_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    # TODO: 添加其他层级包含目录（实现后启用）
    # ${CMAKE_SOURCE_DIR}/src/model
    # ${CMAKE_SOURCE_DIR}/src/parser
    # ${CMAKE_SOURCE_DIR}/src/communication
)
