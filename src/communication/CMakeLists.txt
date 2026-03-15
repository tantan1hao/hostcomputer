# ============================================
# Communication Layer (通讯层)
# ============================================
# 职责:
#   - TCP/UDP/Serial 网络IO
#   - 多摄像头视频接收 (每个摄像头独立线程)
#   - ROS1 TCP通讯
#   - 通过 signal 将数据发送到 Controller
#
# 依赖:
#   - 仅依赖 Qt::Core, Qt::Network, Qt::SerialPort
#   - 不依赖 Controller / UI / Parser
#   - 使用 signal/slot 与 Controller 通信
# ============================================

set(COMMUNICATION_SOURCES
    # 原有文件
    serialportmanager.cpp
    serialportmanager.h
    tongxun.cpp
    tongxun.h
    SharedStructs.h

    # 多摄像头视频接收 (每个摄像头独立线程)
    SingleCameraReceiver.cpp
    SingleCameraReceiver.h
    MultiCameraReceiver.cpp
    MultiCameraReceiver.h

    # ROS1 TCP客户端
    ROS1TcpClient.cpp
    ROS1TcpClient.h
)

# 构建静态库
add_library(communication_layer STATIC ${COMMUNICATION_SOURCES})

# 链接Qt库 - 支持Qt5/Qt6
if(QT_VERSION EQUAL 6)
    target_link_libraries(communication_layer PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::SerialPort
        Qt6::Gui
        utils_layer
    )
else()
    target_link_libraries(communication_layer PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::SerialPort
        Qt5::Gui
    )
endif()

# 包含头文件目录
target_include_directories(communication_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
