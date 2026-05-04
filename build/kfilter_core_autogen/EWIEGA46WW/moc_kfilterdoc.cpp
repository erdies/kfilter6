/****************************************************************************
** Meta object code from reading C++ file 'kfilterdoc.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../kfilterdoc.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'kfilterdoc.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN10KFilterDocE_t {};
} // unnamed namespace

template <> constexpr inline auto KFilterDoc::qt_create_metaobjectdata<qt_meta_tag_ZN10KFilterDocE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "KFilterDoc",
        "forceviewrefresh",
        "",
        "refreshDialog",
        "slotUpdateAllViews",
        "KFilterView*",
        "sender",
        "viewrefresh",
        "slotUpdateAllDialogs"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'forceviewrefresh'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'refreshDialog'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'slotUpdateAllViews'
        QtMocHelpers::SlotData<void(KFilterView *)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { 0x80000000 | 5, 6 },
        }}),
        // Slot 'viewrefresh'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'slotUpdateAllDialogs'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessPrivate, QMetaType::Void),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<KFilterDoc, qt_meta_tag_ZN10KFilterDocE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject KFilterDoc::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10KFilterDocE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10KFilterDocE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN10KFilterDocE_t>.metaTypes,
    nullptr
} };

void KFilterDoc::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<KFilterDoc *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->forceviewrefresh(); break;
        case 1: _t->refreshDialog(); break;
        case 2: _t->slotUpdateAllViews((*reinterpret_cast<std::add_pointer_t<KFilterView*>>(_a[1]))); break;
        case 3: _t->viewrefresh(); break;
        case 4: _t->slotUpdateAllDialogs(); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (KFilterDoc::*)()>(_a, &KFilterDoc::forceviewrefresh, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (KFilterDoc::*)()>(_a, &KFilterDoc::refreshDialog, 1))
            return;
    }
}

const QMetaObject *KFilterDoc::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *KFilterDoc::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN10KFilterDocE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int KFilterDoc::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 5)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 5;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 5)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 5;
    }
    return _id;
}

// SIGNAL 0
void KFilterDoc::forceviewrefresh()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void KFilterDoc::refreshDialog()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
