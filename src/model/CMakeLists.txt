# ============================================
# Model B (pn!�B)
# ============================================
# L#
#   - : UI Л�њ�pn!� (QObject + Q_PROPERTY)
#   - � signal/slot �� UI
#
# �V�
#   - ���V Qt::Core
#   - ��V Parser / Communication / Controller
#   - 1 Controller #�� Model pn
# ============================================

set(MODEL_SOURCES
    motormodel.cpp
    motormodel.h
)

# �Y�
add_library(model_layer STATIC ${MODEL_SOURCES})

# 链接Qt::Core - 支持Qt5/Qt6
if(QT_VERSION EQUAL 6)
    target_link_libraries(model_layer PUBLIC Qt6::Core)
else()
    target_link_libraries(model_layer PUBLIC Qt5::Core)
endif()

# �n4��+�
target_include_directories(model_layer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
