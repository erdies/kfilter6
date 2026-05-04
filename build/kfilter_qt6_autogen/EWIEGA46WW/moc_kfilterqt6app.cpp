/****************************************************************************
** Meta object code from reading C++ file 'kfilterqt6app.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../kfilterqt6app.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kfilterqt6app.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.10.3. It"
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
struct qt_meta_tag_ZN13KFilterQt6AppE_t {};
} // unnamed namespace

template <> constexpr inline auto KFilterQt6App::qt_create_metaobjectdata<qt_meta_tag_ZN13KFilterQt6AppE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KFilterQt6App",
        "newFile",
        "",
        "openFile",
        "saveFile",
        "saveFileAs",
        "refreshOverview",
        "editDriverParameters",
        "editNetworkParameters",
        "refreshCircuitPreview",
        "showAboutDialog",
        "resetWindowLayout",
        "chooseCircuitPreviewBackgroundColor",
        "resetCircuitPreviewBackgroundColor",
        "setFileToolBarVisible",
        "visible",
        "setEditToolBarVisible",
        "setStatusBarVisible",
        "loadSettings",
        "saveSettings"
    };

    QtMocHelpers::UintData qt_methods {
        // Slot 'newFile'
        QtMocHelpers::SlotData<void()>(1, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'openFile'
        QtMocHelpers::SlotData<void()>(3, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'saveFile'
        QtMocHelpers::SlotData<bool()>(4, 2, QMC::AccessPrivate, QMetaType::Bool),
        // Slot 'saveFileAs'
        QtMocHelpers::SlotData<bool()>(5, 2, QMC::AccessPrivate, QMetaType::Bool),
        // Slot 'refreshOverview'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'editDriverParameters'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'editNetworkParameters'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'refreshCircuitPreview'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'showAboutDialog'
        QtMocHelpers::SlotData<void()>(10, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'resetWindowLayout'
        QtMocHelpers::SlotData<void()>(11, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'chooseCircuitPreviewBackgroundColor'
        QtMocHelpers::SlotData<void()>(12, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'resetCircuitPreviewBackgroundColor'
        QtMocHelpers::SlotData<void()>(13, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'setFileToolBarVisible'
        QtMocHelpers::SlotData<void(bool)>(14, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'setEditToolBarVisible'
        QtMocHelpers::SlotData<void(bool)>(16, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'setStatusBarVisible'
        QtMocHelpers::SlotData<void(bool)>(17, 2, QMC::AccessPrivate, QMetaType::Void, {{
            { QMetaType::Bool, 15 },
        }}),
        // Slot 'loadSettings'
        QtMocHelpers::SlotData<void()>(18, 2, QMC::AccessPrivate, QMetaType::Void),
        // Slot 'saveSettings'
        QtMocHelpers::SlotData<void() const>(19, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KFilterQt6App, qt_meta_tag_ZN13KFilterQt6AppE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KFilterQt6App::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KFilterQt6AppE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KFilterQt6AppE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13KFilterQt6AppE_t>.metaTypes,
    nullptr
} };

void KFilterQt6App::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KFilterQt6App *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->newFile(); break;
        case 1: _t->openFile(); break;
        case 2: { bool _r = _t->saveFile();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 3: { bool _r = _t->saveFileAs();
            if (_a[0]) *reinterpret_cast<bool*>(_a[0]) = std::move(_r); }  break;
        case 4: _t->refreshOverview(); break;
        case 5: _t->editDriverParameters(); break;
        case 6: _t->editNetworkParameters(); break;
        case 7: _t->refreshCircuitPreview(); break;
        case 8: _t->showAboutDialog(); break;
        case 9: _t->resetWindowLayout(); break;
        case 10: _t->chooseCircuitPreviewBackgroundColor(); break;
        case 11: _t->resetCircuitPreviewBackgroundColor(); break;
        case 12: _t->setFileToolBarVisible((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 13: _t->setEditToolBarVisible((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 14: _t->setStatusBarVisible((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 15: _t->loadSettings(); break;
        case 16: _t->saveSettings(); break;
        default: ;
        }
    }
}

const QMetaObject *KFilterQt6App::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KFilterQt6App::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13KFilterQt6AppE_t>.strings))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int KFilterQt6App::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 17)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 17;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 17)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 17;
    }
    return _id;
}
QT_WARNING_POP
