/****************************************************************************
** Meta object code from reading C++ file 'CameraManager.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.8.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../../../src/controller/CameraManager.h"
#include <QtGui/qtextcursor.h>
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'CameraManager.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.8.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN13CameraManagerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN13CameraManagerE = QtMocHelpers::stringData(
    "CameraManager",
    "frameReady",
    "",
    "cameraId",
    "frame",
    "frameAvailable",
    "cv::Mat",
    "videoFrameReady",
    "VideoWidget*",
    "widget",
    "cameraStarted",
    "cameraName",
    "cameraStopped",
    "cameraError",
    "error",
    "cameraListChanged",
    "cameraNames",
    "startAllCameras",
    "stopAllCameras",
    "restartAllCameras",
    "onFrameReady",
    "onCameraError",
    "onCameraStatusChanged",
    "isRunning",
    "checkCameraConnection"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN13CameraManagerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    2,   98,    2, 0x06,    1 /* Public */,
       5,    2,  103,    2, 0x06,    4 /* Public */,
       7,    2,  108,    2, 0x06,    7 /* Public */,
      10,    2,  113,    2, 0x06,   10 /* Public */,
      12,    1,  118,    2, 0x06,   13 /* Public */,
      13,    2,  121,    2, 0x06,   15 /* Public */,
      15,    1,  126,    2, 0x06,   18 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      17,    0,  129,    2, 0x0a,   20 /* Public */,
      18,    0,  130,    2, 0x0a,   21 /* Public */,
      19,    0,  131,    2, 0x0a,   22 /* Public */,
      20,    2,  132,    2, 0x08,   23 /* Private */,
      21,    2,  137,    2, 0x08,   26 /* Private */,
      22,    2,  142,    2, 0x08,   29 /* Private */,
      24,    0,  147,    2, 0x08,   32 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int, QMetaType::QPixmap,    3,    4,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 6,    3,    4,
    QMetaType::Void, QMetaType::Int, 0x80000000 | 8,    3,    9,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    3,   11,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    3,   14,
    QMetaType::Void, QMetaType::QStringList,   16,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::QPixmap,    3,    4,
    QMetaType::Void, QMetaType::Int, QMetaType::QString,    3,   14,
    QMetaType::Void, QMetaType::Int, QMetaType::Bool,    3,   23,
    QMetaType::Void,

       0        // eod
};

Q_CONSTINIT const QMetaObject CameraManager::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN13CameraManagerE.offsetsAndSizes,
    qt_meta_data_ZN13CameraManagerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN13CameraManagerE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CameraManager, std::true_type>,
        // method 'frameReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPixmap &, std::false_type>,
        // method 'frameAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const cv::Mat &, std::false_type>,
        // method 'videoFrameReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<VideoWidget *, std::false_type>,
        // method 'cameraStarted'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cameraStopped'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'cameraError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'cameraListChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QStringList &, std::false_type>,
        // method 'startAllCameras'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stopAllCameras'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'restartAllCameras'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'onFrameReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPixmap &, std::false_type>,
        // method 'onCameraError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'onCameraStatusChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'checkCameraConnection'
        QtPrivate::TypeAndForceComplete<void, std::false_type>
    >,
    nullptr
} };

void CameraManager::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CameraManager *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameReady((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[2]))); break;
        case 1: _t->frameAvailable((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<cv::Mat>>(_a[2]))); break;
        case 2: _t->videoFrameReady((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<VideoWidget*>>(_a[2]))); break;
        case 3: _t->cameraStarted((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 4: _t->cameraStopped((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 5: _t->cameraError((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 6: _t->cameraListChanged((*reinterpret_cast< std::add_pointer_t<QStringList>>(_a[1]))); break;
        case 7: _t->startAllCameras(); break;
        case 8: _t->stopAllCameras(); break;
        case 9: _t->restartAllCameras(); break;
        case 10: _t->onFrameReady((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[2]))); break;
        case 11: _t->onCameraError((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<QString>>(_a[2]))); break;
        case 12: _t->onCameraStatusChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[2]))); break;
        case 13: _t->checkCameraConnection(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
        case 2:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType(); break;
            case 1:
                *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType::fromType< VideoWidget* >(); break;
            }
            break;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (CameraManager::*)(int , const QPixmap & );
            if (_q_method_type _q_method = &CameraManager::frameReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(int , const cv::Mat & );
            if (_q_method_type _q_method = &CameraManager::frameAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(int , VideoWidget * );
            if (_q_method_type _q_method = &CameraManager::videoFrameReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(int , const QString & );
            if (_q_method_type _q_method = &CameraManager::cameraStarted; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(int );
            if (_q_method_type _q_method = &CameraManager::cameraStopped; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(int , const QString & );
            if (_q_method_type _q_method = &CameraManager::cameraError; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 5;
                return;
            }
        }
        {
            using _q_method_type = void (CameraManager::*)(const QStringList & );
            if (_q_method_type _q_method = &CameraManager::cameraListChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 6;
                return;
            }
        }
    }
}

const QMetaObject *CameraManager::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CameraManager::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN13CameraManagerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CameraManager::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void CameraManager::frameReady(int _t1, const QPixmap & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CameraManager::frameAvailable(int _t1, const cv::Mat & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void CameraManager::videoFrameReady(int _t1, VideoWidget * _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CameraManager::cameraStarted(int _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void CameraManager::cameraStopped(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void CameraManager::cameraError(int _t1, const QString & _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void CameraManager::cameraListChanged(const QStringList & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 6, _a);
}
namespace {
struct qt_meta_tag_ZN12CameraWorkerE_t {};
} // unnamed namespace


#ifdef QT_MOC_HAS_STRINGDATA
static constexpr auto qt_meta_stringdata_ZN12CameraWorkerE = QtMocHelpers::stringData(
    "CameraWorker",
    "frameReady",
    "",
    "frame",
    "frameAvailable",
    "cv::Mat",
    "errorOccurred",
    "error",
    "statusChanged",
    "isRunning",
    "finished",
    "start",
    "stop",
    "captureFrame",
    "setResolution",
    "width",
    "height",
    "setFps",
    "fps",
    "onTimerTimeout",
    "handleCameraError",
    "QCamera::Error"
);
#else  // !QT_MOC_HAS_STRINGDATA
#error "qtmochelpers.h not found or too old."
#endif // !QT_MOC_HAS_STRINGDATA

Q_CONSTINIT static const uint qt_meta_data_ZN12CameraWorkerE[] = {

 // content:
      12,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       5,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   86,    2, 0x06,    1 /* Public */,
       4,    1,   89,    2, 0x06,    3 /* Public */,
       6,    1,   92,    2, 0x06,    5 /* Public */,
       8,    1,   95,    2, 0x06,    7 /* Public */,
      10,    0,   98,    2, 0x06,    9 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      11,    0,   99,    2, 0x0a,   10 /* Public */,
      12,    0,  100,    2, 0x0a,   11 /* Public */,
      13,    0,  101,    2, 0x0a,   12 /* Public */,
      14,    2,  102,    2, 0x0a,   13 /* Public */,
      17,    1,  107,    2, 0x0a,   16 /* Public */,
      19,    0,  110,    2, 0x08,   18 /* Private */,
      20,    1,  111,    2, 0x08,   19 /* Private */,

 // signals: parameters
    QMetaType::Void, QMetaType::QPixmap,    3,
    QMetaType::Void, 0x80000000 | 5,    3,
    QMetaType::Void, QMetaType::QString,    7,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,   15,   16,
    QMetaType::Void, QMetaType::Int,   18,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 21,    7,

       0        // eod
};

Q_CONSTINIT const QMetaObject CameraWorker::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_ZN12CameraWorkerE.offsetsAndSizes,
    qt_meta_data_ZN12CameraWorkerE,
    qt_static_metacall,
    nullptr,
    qt_incomplete_metaTypeArray<qt_meta_tag_ZN12CameraWorkerE_t,
        // Q_OBJECT / Q_GADGET
        QtPrivate::TypeAndForceComplete<CameraWorker, std::true_type>,
        // method 'frameReady'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QPixmap &, std::false_type>,
        // method 'frameAvailable'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const cv::Mat &, std::false_type>,
        // method 'errorOccurred'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<const QString &, std::false_type>,
        // method 'statusChanged'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<bool, std::false_type>,
        // method 'finished'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'start'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'stop'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'captureFrame'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'setResolution'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'setFps'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<int, std::false_type>,
        // method 'onTimerTimeout'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        // method 'handleCameraError'
        QtPrivate::TypeAndForceComplete<void, std::false_type>,
        QtPrivate::TypeAndForceComplete<QCamera::Error, std::false_type>
    >,
    nullptr
} };

void CameraWorker::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<CameraWorker *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->frameReady((*reinterpret_cast< std::add_pointer_t<QPixmap>>(_a[1]))); break;
        case 1: _t->frameAvailable((*reinterpret_cast< std::add_pointer_t<cv::Mat>>(_a[1]))); break;
        case 2: _t->errorOccurred((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 3: _t->statusChanged((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->finished(); break;
        case 5: _t->start(); break;
        case 6: _t->stop(); break;
        case 7: _t->captureFrame(); break;
        case 8: _t->setResolution((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 9: _t->setFps((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 10: _t->onTimerTimeout(); break;
        case 11: _t->handleCameraError((*reinterpret_cast< std::add_pointer_t<QCamera::Error>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _q_method_type = void (CameraWorker::*)(const QPixmap & );
            if (_q_method_type _q_method = &CameraWorker::frameReady; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 0;
                return;
            }
        }
        {
            using _q_method_type = void (CameraWorker::*)(const cv::Mat & );
            if (_q_method_type _q_method = &CameraWorker::frameAvailable; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 1;
                return;
            }
        }
        {
            using _q_method_type = void (CameraWorker::*)(const QString & );
            if (_q_method_type _q_method = &CameraWorker::errorOccurred; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 2;
                return;
            }
        }
        {
            using _q_method_type = void (CameraWorker::*)(bool );
            if (_q_method_type _q_method = &CameraWorker::statusChanged; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 3;
                return;
            }
        }
        {
            using _q_method_type = void (CameraWorker::*)();
            if (_q_method_type _q_method = &CameraWorker::finished; *reinterpret_cast<_q_method_type *>(_a[1]) == _q_method) {
                *result = 4;
                return;
            }
        }
    }
}

const QMetaObject *CameraWorker::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *CameraWorker::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ZN12CameraWorkerE.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int CameraWorker::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 12;
    }
    return _id;
}

// SIGNAL 0
void CameraWorker::frameReady(const QPixmap & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void CameraWorker::frameAvailable(const cv::Mat & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void CameraWorker::errorOccurred(const QString & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void CameraWorker::statusChanged(bool _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void CameraWorker::finished()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}
QT_WARNING_POP
