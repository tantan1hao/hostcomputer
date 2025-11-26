# ============================================
# Communication B (�B)
# ============================================
# L#
#   - � CAN / Serial / TCP I��
#   - ���6��pn'
#   - � signal �60�pn�� Controller
#
# �V�
#   - ���V Qt::Core, Qt::Network, Qt::SerialPort
#   - ��V Controller / UI / Parser
#   - � signal/slot  Controller �
# ============================================

set(COMMUNICATION_SOURCES
    canmanager.cpp
    canmanager.h
    serialportmanager.cpp
    serialportmanager.h
    SharedStructs.h
)

# �Y�
add_library(communication_layer STATIC ${COMMUNICATION_SOURCES})

# 链接Qt库 - 支持Qt5/Qt6
if(QT_VERSION EQUAL 6)
    target_link_libraries(communication_layer PUBLIC
        Qt6::Core
        Qt6::Network
        Qt6::SerialPort
    )
else()
    target_link_libraries(communication_layer PUBLIC
        Qt5::Core
        Qt5::Network
        Qt5::SerialPort
    )
endif()

# �n4��+�
target_include_directories(communication_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
