/****************************************************************************
** Meta object code from reading C++ file 'configuration.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.10.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../../src/launcher/include/configuration.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'configuration.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN13ConfigurationE_t {};
} // unnamed namespace

template <> constexpr inline auto Configuration::qt_create_metaobjectdata<qt_meta_tag_ZN13ConfigurationE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Configuration",
        "Resolution",
        "R1280X720",
        "R1920X1080",
        "ShaderOptimizationType",
        "None",
        "Size",
        "Performance",
        "ShaderLogDirection",
        "Silent",
        "Console",
        "File",
        "ProfilerDirection",
        "Network",
        "LogDirection",
        "GameStatus",
        "Unknown",
        "InGame",
        "Logo",
        "DoesntBoot",
        "MainMenu"
    };

    QtMocHelpers::UintData qt_methods {
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'Resolution'
        QtMocHelpers::EnumData<enum Resolution>(1, 1, QMC::EnumIsScoped).add({
            {    2, Resolution::R1280X720 },
            {    3, Resolution::R1920X1080 },
        }),
        // enum 'ShaderOptimizationType'
        QtMocHelpers::EnumData<enum ShaderOptimizationType>(4, 4, QMC::EnumIsScoped).add({
            {    5, ShaderOptimizationType::None },
            {    6, ShaderOptimizationType::Size },
            {    7, ShaderOptimizationType::Performance },
        }),
        // enum 'ShaderLogDirection'
        QtMocHelpers::EnumData<enum ShaderLogDirection>(8, 8, QMC::EnumIsScoped).add({
            {    9, ShaderLogDirection::Silent },
            {   10, ShaderLogDirection::Console },
            {   11, ShaderLogDirection::File },
        }),
        // enum 'ProfilerDirection'
        QtMocHelpers::EnumData<enum ProfilerDirection>(12, 12, QMC::EnumIsScoped).add({
            {    5, ProfilerDirection::None },
            {   13, ProfilerDirection::Network },
        }),
        // enum 'LogDirection'
        QtMocHelpers::EnumData<enum LogDirection>(14, 14, QMC::EnumIsScoped).add({
            {    9, LogDirection::Silent },
            {   10, LogDirection::Console },
            {   11, LogDirection::File },
        }),
        // enum 'GameStatus'
        QtMocHelpers::EnumData<enum GameStatus>(15, 15, QMC::EnumIsScoped).add({
            {   16, GameStatus::Unknown },
            {   17, GameStatus::InGame },
            {   18, GameStatus::Logo },
            {   19, GameStatus::DoesntBoot },
            {   20, GameStatus::MainMenu },
        }),
    };
    return QtMocHelpers::metaObjectData<Configuration, qt_meta_tag_ZN13ConfigurationE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Configuration::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ConfigurationE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ConfigurationE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN13ConfigurationE_t>.metaTypes,
    nullptr
} };

void Configuration::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Configuration *>(_o);
    (void)_t;
    (void)_c;
    (void)_id;
    (void)_a;
}

const QMetaObject *Configuration::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Configuration::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN13ConfigurationE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Configuration::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    return _id;
}
QT_WARNING_POP
