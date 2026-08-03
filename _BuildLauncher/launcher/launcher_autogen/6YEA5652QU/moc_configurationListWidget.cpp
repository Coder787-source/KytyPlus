/****************************************************************************
** Meta object code from reading C++ file 'configurationListWidget.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/launcher/include/configurationListWidget.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'configurationListWidget.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN23ConfigurationListWidgetE_t {};
} // unnamed namespace

template <> constexpr inline auto ConfigurationListWidget::qt_create_metaobjectdata<qt_meta_tag_ZN23ConfigurationListWidgetE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "ConfigurationListWidget",
        "Run",
        "",
        "Select",
        "WriteSettings",
        "ReadSettings",
        "edit_configuration",
        "delete_configuartion",
        "edit_global_settings",
        "run_configuration",
        "list_currentItemChanged",
        "QTreeWidgetItem*",
        "current",
        "previous",
        "list_itemDoubleClicked",
        "witem",
        "column",
        "show_context_menu",
        "QPoint",
        "pos",
        "open_game_folder",
        "remove_save_data",
        "filter_configurations",
        "text"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'Run'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'Select'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'WriteSettings'
        QtMocHelpers::SlotData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'ReadSettings'
        QtMocHelpers::SlotData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'edit_configuration'
        QtMocHelpers::SlotData<void()>(6, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'delete_configuartion'
        QtMocHelpers::SlotData<void()>(7, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'edit_global_settings'
        QtMocHelpers::SlotData<void()>(8, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'run_configuration'
        QtMocHelpers::SlotData<void()>(9, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'list_currentItemChanged'
        QtMocHelpers::SlotData<void(QTreeWidgetItem *, QTreeWidgetItem *)>(10, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 11, 12 }, { 0x80000000 | 11, 13 },
        }}),
        // Slot 'list_itemDoubleClicked'
        QtMocHelpers::SlotData<void(QTreeWidgetItem *, int)>(14, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 11, 15 }, { QMetaType::Int, 16 },
        }}),
        // Slot 'show_context_menu'
        QtMocHelpers::SlotData<void(const QPoint &)>(17, 2, QMC::AccessProtected, QMetaType::Void, {{
            { 0x80000000 | 18, 19 },
        }}),
        // Slot 'open_game_folder'
        QtMocHelpers::SlotData<void()>(20, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'remove_save_data'
        QtMocHelpers::SlotData<void()>(21, 2, QMC::AccessProtected, QMetaType::Void),
        // Slot 'filter_configurations'
        QtMocHelpers::SlotData<void(const QString &)>(22, 2, QMC::AccessProtected, QMetaType::Void, {{
            { QMetaType::QString, 23 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<ConfigurationListWidget, qt_meta_tag_ZN23ConfigurationListWidgetE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject ConfigurationListWidget::staticMetaObject = { {
    QMetaObject::SuperData::link<QWidget::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConfigurationListWidgetE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConfigurationListWidgetE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN23ConfigurationListWidgetE_t>.metaTypes,
    nullptr
} };

void ConfigurationListWidget::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<ConfigurationListWidget *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->Run(); break;
        case 1: _t->Select(); break;
        case 2: _t->WriteSettings(); break;
        case 3: _t->ReadSettings(); break;
        case 4: _t->edit_configuration(); break;
        case 5: _t->delete_configuartion(); break;
        case 6: _t->edit_global_settings(); break;
        case 7: _t->run_configuration(); break;
        case 8: _t->list_currentItemChanged((*reinterpret_cast<std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QTreeWidgetItem*>>(_a[2]))); break;
        case 9: _t->list_itemDoubleClicked((*reinterpret_cast<std::add_pointer_t<QTreeWidgetItem*>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<int>>(_a[2]))); break;
        case 10: _t->show_context_menu((*reinterpret_cast<std::add_pointer_t<QPoint>>(_a[1]))); break;
        case 11: _t->open_game_folder(); break;
        case 12: _t->remove_save_data(); break;
        case 13: _t->filter_configurations((*reinterpret_cast<std::add_pointer_t<QString>>(_a[1]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (ConfigurationListWidget::*)()>(_a, &ConfigurationListWidget::Run, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (ConfigurationListWidget::*)()>(_a, &ConfigurationListWidget::Select, 1))
            return;
    }
}

const QMetaObject *ConfigurationListWidget::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ConfigurationListWidget::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN23ConfigurationListWidgetE_t>.strings))
        return static_cast<void*>(this);
    return QWidget::qt_metacast(_clname);
}

int ConfigurationListWidget::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QWidget::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void ConfigurationListWidget::Run()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void ConfigurationListWidget::Select()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}
QT_WARNING_POP
