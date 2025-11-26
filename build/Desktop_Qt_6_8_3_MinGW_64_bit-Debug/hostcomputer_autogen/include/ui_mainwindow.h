/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.8.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <videowidget.h>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QAction *action_connect;
    QAction *action_disconnect;
    QAction *action_exit;
    QAction *action_fullscreen;
    QAction *action_reset_layout;
    QAction *action_about;
    QWidget *centralwidget;
    QVBoxLayout *verticalLayout_main;
    QFrame *frame_status;
    QHBoxLayout *horizontalLayout_status;
    QLabel *label_connection_status;
    QLabel *label_connection_value;
    QFrame *line_separator1;
    QLabel *label_heartbeat;
    QLabel *label_heartbeat_value;
    QFrame *line_separator2;
    QLabel *label_fps;
    QLabel *label_fps_value;
    QFrame *line_separator3;
    QLabel *label_cpu;
    QLabel *label_cpu_value;
    QFrame *line_separator4;
    QLabel *label_mode;
    QLabel *label_mode_value;
    QSpacerItem *horizontalSpacer_status;
    QHBoxLayout *horizontalLayout_main;
    QGroupBox *group_cameras;
    QGridLayout *gridLayout_cameras;
    VideoWidget *camera_1;
    VideoWidget *camera_2;
    VideoWidget *camera_3;
    VideoWidget *camera_4;
    VideoWidget *camera_5;
    VideoWidget *camera_6;
    QVBoxLayout *verticalLayout_right;
    QGroupBox *group_car_model;
    QVBoxLayout *verticalLayout_car_model;
    QLabel *car_model_display;
    QHBoxLayout *horizontalLayout_car_info;
    QLabel *label_roll;
    QLabel *label_pitch;
    QLabel *label_yaw;
    QGroupBox *group_commands;
    QVBoxLayout *verticalLayout_commands;
    QTextEdit *text_commands;
    QHBoxLayout *horizontalLayout_commands_control;
    QPushButton *btn_clear_commands;
    QCheckBox *checkbox_auto_scroll;
    QSpacerItem *horizontalSpacer_commands;
    QGroupBox *group_errors;
    QVBoxLayout *verticalLayout_errors;
    QTextEdit *text_errors;
    QHBoxLayout *horizontalLayout_errors_control;
    QPushButton *btn_clear_errors;
    QLabel *label_error_count;
    QSpacerItem *horizontalSpacer_errors;
    QMenuBar *menubar;
    QMenu *menu_file;
    QMenu *menu_view;
    QMenu *menu_help;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1920, 1080);
        MainWindow->setMinimumSize(QSize(1600, 900));
        action_connect = new QAction(MainWindow);
        action_connect->setObjectName("action_connect");
        action_disconnect = new QAction(MainWindow);
        action_disconnect->setObjectName("action_disconnect");
        action_exit = new QAction(MainWindow);
        action_exit->setObjectName("action_exit");
        action_fullscreen = new QAction(MainWindow);
        action_fullscreen->setObjectName("action_fullscreen");
        action_reset_layout = new QAction(MainWindow);
        action_reset_layout->setObjectName("action_reset_layout");
        action_about = new QAction(MainWindow);
        action_about->setObjectName("action_about");
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        verticalLayout_main = new QVBoxLayout(centralwidget);
        verticalLayout_main->setSpacing(5);
        verticalLayout_main->setObjectName("verticalLayout_main");
        verticalLayout_main->setContentsMargins(5, 5, 5, 5);
        frame_status = new QFrame(centralwidget);
        frame_status->setObjectName("frame_status");
        frame_status->setMaximumSize(QSize(16777215, 80));
        frame_status->setFrameShape(QFrame::Shape::StyledPanel);
        horizontalLayout_status = new QHBoxLayout(frame_status);
        horizontalLayout_status->setSpacing(15);
        horizontalLayout_status->setObjectName("horizontalLayout_status");
        label_connection_status = new QLabel(frame_status);
        label_connection_status->setObjectName("label_connection_status");

        horizontalLayout_status->addWidget(label_connection_status);

        label_connection_value = new QLabel(frame_status);
        label_connection_value->setObjectName("label_connection_value");

        horizontalLayout_status->addWidget(label_connection_value);

        line_separator1 = new QFrame(frame_status);
        line_separator1->setObjectName("line_separator1");
        line_separator1->setFrameShape(QFrame::Shape::VLine);
        line_separator1->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_status->addWidget(line_separator1);

        label_heartbeat = new QLabel(frame_status);
        label_heartbeat->setObjectName("label_heartbeat");

        horizontalLayout_status->addWidget(label_heartbeat);

        label_heartbeat_value = new QLabel(frame_status);
        label_heartbeat_value->setObjectName("label_heartbeat_value");

        horizontalLayout_status->addWidget(label_heartbeat_value);

        line_separator2 = new QFrame(frame_status);
        line_separator2->setObjectName("line_separator2");
        line_separator2->setFrameShape(QFrame::Shape::VLine);
        line_separator2->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_status->addWidget(line_separator2);

        label_fps = new QLabel(frame_status);
        label_fps->setObjectName("label_fps");

        horizontalLayout_status->addWidget(label_fps);

        label_fps_value = new QLabel(frame_status);
        label_fps_value->setObjectName("label_fps_value");

        horizontalLayout_status->addWidget(label_fps_value);

        line_separator3 = new QFrame(frame_status);
        line_separator3->setObjectName("line_separator3");
        line_separator3->setFrameShape(QFrame::Shape::VLine);
        line_separator3->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_status->addWidget(line_separator3);

        label_cpu = new QLabel(frame_status);
        label_cpu->setObjectName("label_cpu");

        horizontalLayout_status->addWidget(label_cpu);

        label_cpu_value = new QLabel(frame_status);
        label_cpu_value->setObjectName("label_cpu_value");

        horizontalLayout_status->addWidget(label_cpu_value);

        line_separator4 = new QFrame(frame_status);
        line_separator4->setObjectName("line_separator4");
        line_separator4->setFrameShape(QFrame::Shape::VLine);
        line_separator4->setFrameShadow(QFrame::Shadow::Sunken);

        horizontalLayout_status->addWidget(line_separator4);

        label_mode = new QLabel(frame_status);
        label_mode->setObjectName("label_mode");

        horizontalLayout_status->addWidget(label_mode);

        label_mode_value = new QLabel(frame_status);
        label_mode_value->setObjectName("label_mode_value");

        horizontalLayout_status->addWidget(label_mode_value);

        horizontalSpacer_status = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_status->addItem(horizontalSpacer_status);


        verticalLayout_main->addWidget(frame_status);

        horizontalLayout_main = new QHBoxLayout();
        horizontalLayout_main->setSpacing(5);
        horizontalLayout_main->setObjectName("horizontalLayout_main");
        group_cameras = new QGroupBox(centralwidget);
        group_cameras->setObjectName("group_cameras");
        group_cameras->setMinimumSize(QSize(1200, 600));
        gridLayout_cameras = new QGridLayout(group_cameras);
        gridLayout_cameras->setSpacing(3);
        gridLayout_cameras->setObjectName("gridLayout_cameras");
        camera_1 = new VideoWidget(group_cameras);
        camera_1->setObjectName("camera_1");
        camera_1->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_1, 0, 0, 1, 1);

        camera_2 = new VideoWidget(group_cameras);
        camera_2->setObjectName("camera_2");
        camera_2->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_2, 0, 1, 1, 1);

        camera_3 = new VideoWidget(group_cameras);
        camera_3->setObjectName("camera_3");
        camera_3->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_3, 0, 2, 1, 1);

        camera_4 = new VideoWidget(group_cameras);
        camera_4->setObjectName("camera_4");
        camera_4->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_4, 1, 0, 1, 1);

        camera_5 = new VideoWidget(group_cameras);
        camera_5->setObjectName("camera_5");
        camera_5->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_5, 1, 1, 1, 1);

        camera_6 = new VideoWidget(group_cameras);
        camera_6->setObjectName("camera_6");
        camera_6->setMinimumSize(QSize(390, 290));

        gridLayout_cameras->addWidget(camera_6, 1, 2, 1, 1);


        horizontalLayout_main->addWidget(group_cameras);

        verticalLayout_right = new QVBoxLayout();
        verticalLayout_right->setSpacing(5);
        verticalLayout_right->setObjectName("verticalLayout_right");
        group_car_model = new QGroupBox(centralwidget);
        group_car_model->setObjectName("group_car_model");
        group_car_model->setMinimumSize(QSize(400, 300));
        verticalLayout_car_model = new QVBoxLayout(group_car_model);
        verticalLayout_car_model->setObjectName("verticalLayout_car_model");
        car_model_display = new QLabel(group_car_model);
        car_model_display->setObjectName("car_model_display");
        car_model_display->setMinimumSize(QSize(380, 260));
        car_model_display->setAlignment(Qt::AlignmentFlag::AlignCenter);

        verticalLayout_car_model->addWidget(car_model_display);

        horizontalLayout_car_info = new QHBoxLayout();
        horizontalLayout_car_info->setObjectName("horizontalLayout_car_info");
        label_roll = new QLabel(group_car_model);
        label_roll->setObjectName("label_roll");

        horizontalLayout_car_info->addWidget(label_roll);

        label_pitch = new QLabel(group_car_model);
        label_pitch->setObjectName("label_pitch");

        horizontalLayout_car_info->addWidget(label_pitch);

        label_yaw = new QLabel(group_car_model);
        label_yaw->setObjectName("label_yaw");

        horizontalLayout_car_info->addWidget(label_yaw);


        verticalLayout_car_model->addLayout(horizontalLayout_car_info);


        verticalLayout_right->addWidget(group_car_model);

        group_commands = new QGroupBox(centralwidget);
        group_commands->setObjectName("group_commands");
        group_commands->setMinimumSize(QSize(400, 200));
        verticalLayout_commands = new QVBoxLayout(group_commands);
        verticalLayout_commands->setObjectName("verticalLayout_commands");
        text_commands = new QTextEdit(group_commands);
        text_commands->setObjectName("text_commands");
        text_commands->setMinimumSize(QSize(380, 160));
        text_commands->setMaximumSize(QSize(16777215, 160));
        text_commands->setReadOnly(true);

        verticalLayout_commands->addWidget(text_commands);

        horizontalLayout_commands_control = new QHBoxLayout();
        horizontalLayout_commands_control->setObjectName("horizontalLayout_commands_control");
        btn_clear_commands = new QPushButton(group_commands);
        btn_clear_commands->setObjectName("btn_clear_commands");

        horizontalLayout_commands_control->addWidget(btn_clear_commands);

        checkbox_auto_scroll = new QCheckBox(group_commands);
        checkbox_auto_scroll->setObjectName("checkbox_auto_scroll");
        checkbox_auto_scroll->setChecked(true);

        horizontalLayout_commands_control->addWidget(checkbox_auto_scroll);

        horizontalSpacer_commands = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_commands_control->addItem(horizontalSpacer_commands);


        verticalLayout_commands->addLayout(horizontalLayout_commands_control);


        verticalLayout_right->addWidget(group_commands);

        group_errors = new QGroupBox(centralwidget);
        group_errors->setObjectName("group_errors");
        group_errors->setMinimumSize(QSize(400, 150));
        verticalLayout_errors = new QVBoxLayout(group_errors);
        verticalLayout_errors->setObjectName("verticalLayout_errors");
        text_errors = new QTextEdit(group_errors);
        text_errors->setObjectName("text_errors");
        text_errors->setMinimumSize(QSize(380, 110));
        text_errors->setMaximumSize(QSize(16777215, 110));
        text_errors->setReadOnly(true);

        verticalLayout_errors->addWidget(text_errors);

        horizontalLayout_errors_control = new QHBoxLayout();
        horizontalLayout_errors_control->setObjectName("horizontalLayout_errors_control");
        btn_clear_errors = new QPushButton(group_errors);
        btn_clear_errors->setObjectName("btn_clear_errors");

        horizontalLayout_errors_control->addWidget(btn_clear_errors);

        label_error_count = new QLabel(group_errors);
        label_error_count->setObjectName("label_error_count");

        horizontalLayout_errors_control->addWidget(label_error_count);

        horizontalSpacer_errors = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_errors_control->addItem(horizontalSpacer_errors);


        verticalLayout_errors->addLayout(horizontalLayout_errors_control);


        verticalLayout_right->addWidget(group_errors);


        horizontalLayout_main->addLayout(verticalLayout_right);


        verticalLayout_main->addLayout(horizontalLayout_main);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1920, 22));
        menu_file = new QMenu(menubar);
        menu_file->setObjectName("menu_file");
        menu_view = new QMenu(menubar);
        menu_view->setObjectName("menu_view");
        menu_help = new QMenu(menubar);
        menu_help->setObjectName("menu_help");
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        menubar->addAction(menu_file->menuAction());
        menubar->addAction(menu_view->menuAction());
        menubar->addAction(menu_help->menuAction());
        menu_file->addAction(action_connect);
        menu_file->addAction(action_disconnect);
        menu_file->addSeparator();
        menu_file->addAction(action_exit);
        menu_view->addAction(action_fullscreen);
        menu_view->addAction(action_reset_layout);
        menu_help->addAction(action_about);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\347\224\265\346\234\272\346\216\247\345\210\266\347\263\273\347\273\237\344\270\212\344\275\215\346\234\272", nullptr));
        action_connect->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\350\256\276\345\244\207", nullptr));
#if QT_CONFIG(shortcut)
        action_connect->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+C", nullptr));
#endif // QT_CONFIG(shortcut)
        action_disconnect->setText(QCoreApplication::translate("MainWindow", "\346\226\255\345\274\200\350\277\236\346\216\245", nullptr));
#if QT_CONFIG(shortcut)
        action_disconnect->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+D", nullptr));
#endif // QT_CONFIG(shortcut)
        action_exit->setText(QCoreApplication::translate("MainWindow", "\351\200\200\345\207\272", nullptr));
#if QT_CONFIG(shortcut)
        action_exit->setShortcut(QCoreApplication::translate("MainWindow", "Ctrl+Q", nullptr));
#endif // QT_CONFIG(shortcut)
        action_fullscreen->setText(QCoreApplication::translate("MainWindow", "\345\205\250\345\261\217", nullptr));
#if QT_CONFIG(shortcut)
        action_fullscreen->setShortcut(QCoreApplication::translate("MainWindow", "F11", nullptr));
#endif // QT_CONFIG(shortcut)
        action_reset_layout->setText(QCoreApplication::translate("MainWindow", "\351\207\215\347\275\256\345\270\203\345\261\200", nullptr));
        action_about->setText(QCoreApplication::translate("MainWindow", "\345\205\263\344\272\216", nullptr));
        label_connection_status->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\347\212\266\346\200\201:", nullptr));
        label_connection_value->setStyleSheet(QCoreApplication::translate("MainWindow", "color: red; font-weight: bold;", nullptr));
        label_connection_value->setText(QCoreApplication::translate("MainWindow", "\346\226\255\345\274\200", nullptr));
        label_heartbeat->setText(QCoreApplication::translate("MainWindow", "\344\270\213\344\275\215\346\234\272\345\277\203\350\267\263:", nullptr));
        label_heartbeat_value->setStyleSheet(QCoreApplication::translate("MainWindow", "color: red; font-weight: bold;", nullptr));
        label_heartbeat_value->setText(QCoreApplication::translate("MainWindow", "\344\270\242\345\244\261", nullptr));
        label_fps->setText(QCoreApplication::translate("MainWindow", "\345\275\223\345\211\215\345\270\247\347\216\207:", nullptr));
        label_fps_value->setText(QCoreApplication::translate("MainWindow", "0 FPS", nullptr));
        label_cpu->setText(QCoreApplication::translate("MainWindow", "CPU/GPU:", nullptr));
        label_cpu_value->setText(QCoreApplication::translate("MainWindow", "0% / 0%", nullptr));
        label_mode->setText(QCoreApplication::translate("MainWindow", "\347\224\265\346\234\272\346\250\241\345\274\217:", nullptr));
        label_mode_value->setText(QCoreApplication::translate("MainWindow", "\345\276\205\346\234\272", nullptr));
        group_cameras->setTitle(QCoreApplication::translate("MainWindow", "\347\233\270\346\234\272\347\224\273\351\235\242", nullptr));
        camera_1->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        camera_2->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        camera_3->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        camera_4->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        camera_5->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        camera_6->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #000000;", nullptr));
        group_car_model->setTitle(QCoreApplication::translate("MainWindow", "\345\260\217\350\275\246\345\247\277\346\200\201\346\250\241\345\236\213", nullptr));
        car_model_display->setStyleSheet(QCoreApplication::translate("MainWindow", "border: 2px solid #cccccc; background-color: #f8f8f8;", nullptr));
        car_model_display->setText(QCoreApplication::translate("MainWindow", "3D\345\247\277\346\200\201\346\250\241\345\236\213\346\230\276\347\244\272\345\214\272\345\237\237", nullptr));
        label_roll->setText(QCoreApplication::translate("MainWindow", "Roll: 0\302\260", nullptr));
        label_pitch->setText(QCoreApplication::translate("MainWindow", "Pitch: 0\302\260", nullptr));
        label_yaw->setText(QCoreApplication::translate("MainWindow", "Yaw: 0\302\260", nullptr));
        group_commands->setTitle(QCoreApplication::translate("MainWindow", "\344\270\213\344\275\215\346\234\272\346\214\207\344\273\244", nullptr));
        text_commands->setStyleSheet(QCoreApplication::translate("MainWindow", "font-family: 'Consolas', 'Courier New', monospace; font-size: 12px;", nullptr));
        btn_clear_commands->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        checkbox_auto_scroll->setText(QCoreApplication::translate("MainWindow", "\350\207\252\345\212\250\346\273\232\345\212\250", nullptr));
        group_errors->setTitle(QCoreApplication::translate("MainWindow", "\347\263\273\347\273\237\346\266\210\346\201\257", nullptr));
        text_errors->setStyleSheet(QCoreApplication::translate("MainWindow", "font-family: 'Consolas', 'Courier New', monospace; font-size: 11px; background-color: #fff8f8;", nullptr));
        btn_clear_errors->setText(QCoreApplication::translate("MainWindow", "\346\270\205\347\251\272", nullptr));
        label_error_count->setText(QCoreApplication::translate("MainWindow", "\351\224\231\350\257\257: 0", nullptr));
        menu_file->setTitle(QCoreApplication::translate("MainWindow", "\346\226\207\344\273\266", nullptr));
        menu_view->setTitle(QCoreApplication::translate("MainWindow", "\350\247\206\345\233\276", nullptr));
        menu_help->setTitle(QCoreApplication::translate("MainWindow", "\345\270\256\345\212\251", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
