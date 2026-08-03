/********************************************************************************
** Form generated from reading UI file 'configuration_edit_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONFIGURATION_EDIT_DIALOG_H
#define UI_CONFIGURATION_EDIT_DIALOG_H

#include <QtCore/QLocale>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>
#include "mandatoryLineEdit.h"

QT_BEGIN_NAMESPACE

class Ui_ConfigurationEditDialog
{
public:
    QGridLayout *gridLayout;
    QFrame *configuaration_groupbox;
    QFormLayout *formLayout_6;
    QFrame *frame_4;
    QGridLayout *gridLayout_5;
    QCheckBox *checkBox_vulkan_validation;
    QCheckBox *checkBox_shader_validation;
    QCheckBox *checkBox_cmd_dump;
    QCheckBox *checkBox_renderdoc_capture;
    QCheckBox *checkBox_ngg_rectlist_draw;
    QCheckBox *checkBox_graphics_debug_dump;
    QCheckBox *checkBox_spirv_debug_printf;
    QLabel *label_16;
    QComboBox *comboBox_screen_resolution;
    QLabel *label_vblank_frequency;
    QSpinBox *spinBox_vblank_frequency;
    QLabel *label_20;
    QComboBox *comboBox_shader_optimization_type;
    QLabel *label_21;
    QComboBox *comboBox_shader_log_direction;
    QLabel *label_22;
    MandatoryLineEdit *lineEdit_shader_log_folder;
    QLabel *label_24;
    MandatoryLineEdit *lineEdit_cmd_dump_folder;
    QLabel *label_25;
    QComboBox *comboBox_printf_direction;
    QLabel *label_26;
    MandatoryLineEdit *lineEdit_printf_file;
    QLabel *label_27;
    QComboBox *comboBox_profiler_direction;
    QVBoxLayout *verticalLayout;
    QPushButton *ok_button;
    QPushButton *cancel_button;
    QFrame *line_input_mapping;
    QPushButton *input_mapping_button;
    QFrame *line;
    QPushButton *clear_button;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *ConfigurationEditDialog)
    {
        if (ConfigurationEditDialog->objectName().isEmpty())
            ConfigurationEditDialog->setObjectName("ConfigurationEditDialog");
        ConfigurationEditDialog->resize(560, 420);
        ConfigurationEditDialog->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        gridLayout = new QGridLayout(ConfigurationEditDialog);
        gridLayout->setObjectName("gridLayout");
        configuaration_groupbox = new QFrame(ConfigurationEditDialog);
        configuaration_groupbox->setObjectName("configuaration_groupbox");
        configuaration_groupbox->setFrameShape(QFrame::StyledPanel);
        formLayout_6 = new QFormLayout(configuaration_groupbox);
        formLayout_6->setObjectName("formLayout_6");
        formLayout_6->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        frame_4 = new QFrame(configuaration_groupbox);
        frame_4->setObjectName("frame_4");
        frame_4->setFrameShape(QFrame::StyledPanel);
        gridLayout_5 = new QGridLayout(frame_4);
        gridLayout_5->setObjectName("gridLayout_5");
        checkBox_vulkan_validation = new QCheckBox(frame_4);
        checkBox_vulkan_validation->setObjectName("checkBox_vulkan_validation");
        checkBox_vulkan_validation->setChecked(true);

        gridLayout_5->addWidget(checkBox_vulkan_validation, 0, 0, 1, 1);

        checkBox_shader_validation = new QCheckBox(frame_4);
        checkBox_shader_validation->setObjectName("checkBox_shader_validation");
        checkBox_shader_validation->setChecked(true);

        gridLayout_5->addWidget(checkBox_shader_validation, 0, 1, 1, 1);

        checkBox_cmd_dump = new QCheckBox(frame_4);
        checkBox_cmd_dump->setObjectName("checkBox_cmd_dump");

        gridLayout_5->addWidget(checkBox_cmd_dump, 1, 1, 1, 1);

        checkBox_renderdoc_capture = new QCheckBox(frame_4);
        checkBox_renderdoc_capture->setObjectName("checkBox_renderdoc_capture");

        gridLayout_5->addWidget(checkBox_renderdoc_capture, 1, 0, 1, 1);

        checkBox_ngg_rectlist_draw = new QCheckBox(frame_4);
        checkBox_ngg_rectlist_draw->setObjectName("checkBox_ngg_rectlist_draw");
        checkBox_ngg_rectlist_draw->setChecked(true);

        gridLayout_5->addWidget(checkBox_ngg_rectlist_draw, 2, 0, 1, 2);

        checkBox_graphics_debug_dump = new QCheckBox(frame_4);
        checkBox_graphics_debug_dump->setObjectName("checkBox_graphics_debug_dump");

        gridLayout_5->addWidget(checkBox_graphics_debug_dump, 3, 0, 1, 1);

        checkBox_spirv_debug_printf = new QCheckBox(frame_4);
        checkBox_spirv_debug_printf->setObjectName("checkBox_spirv_debug_printf");

        gridLayout_5->addWidget(checkBox_spirv_debug_printf, 3, 1, 1, 1);


        formLayout_6->setWidget(0, QFormLayout::ItemRole::SpanningRole, frame_4);

        label_16 = new QLabel(configuaration_groupbox);
        label_16->setObjectName("label_16");

        formLayout_6->setWidget(1, QFormLayout::ItemRole::LabelRole, label_16);

        comboBox_screen_resolution = new QComboBox(configuaration_groupbox);
        comboBox_screen_resolution->setObjectName("comboBox_screen_resolution");

        formLayout_6->setWidget(1, QFormLayout::ItemRole::FieldRole, comboBox_screen_resolution);

        label_vblank_frequency = new QLabel(configuaration_groupbox);
        label_vblank_frequency->setObjectName("label_vblank_frequency");

        formLayout_6->setWidget(2, QFormLayout::ItemRole::LabelRole, label_vblank_frequency);

        spinBox_vblank_frequency = new QSpinBox(configuaration_groupbox);
        spinBox_vblank_frequency->setObjectName("spinBox_vblank_frequency");
        spinBox_vblank_frequency->setMinimum(30);
        spinBox_vblank_frequency->setMaximum(360);
        spinBox_vblank_frequency->setValue(60);

        formLayout_6->setWidget(2, QFormLayout::ItemRole::FieldRole, spinBox_vblank_frequency);

        label_20 = new QLabel(configuaration_groupbox);
        label_20->setObjectName("label_20");

        formLayout_6->setWidget(3, QFormLayout::ItemRole::LabelRole, label_20);

        comboBox_shader_optimization_type = new QComboBox(configuaration_groupbox);
        comboBox_shader_optimization_type->setObjectName("comboBox_shader_optimization_type");

        formLayout_6->setWidget(3, QFormLayout::ItemRole::FieldRole, comboBox_shader_optimization_type);

        label_21 = new QLabel(configuaration_groupbox);
        label_21->setObjectName("label_21");

        formLayout_6->setWidget(4, QFormLayout::ItemRole::LabelRole, label_21);

        comboBox_shader_log_direction = new QComboBox(configuaration_groupbox);
        comboBox_shader_log_direction->setObjectName("comboBox_shader_log_direction");

        formLayout_6->setWidget(4, QFormLayout::ItemRole::FieldRole, comboBox_shader_log_direction);

        label_22 = new QLabel(configuaration_groupbox);
        label_22->setObjectName("label_22");

        formLayout_6->setWidget(5, QFormLayout::ItemRole::LabelRole, label_22);

        lineEdit_shader_log_folder = new MandatoryLineEdit(configuaration_groupbox);
        lineEdit_shader_log_folder->setObjectName("lineEdit_shader_log_folder");
        lineEdit_shader_log_folder->setClearButtonEnabled(true);

        formLayout_6->setWidget(5, QFormLayout::ItemRole::FieldRole, lineEdit_shader_log_folder);

        label_24 = new QLabel(configuaration_groupbox);
        label_24->setObjectName("label_24");

        formLayout_6->setWidget(6, QFormLayout::ItemRole::LabelRole, label_24);

        lineEdit_cmd_dump_folder = new MandatoryLineEdit(configuaration_groupbox);
        lineEdit_cmd_dump_folder->setObjectName("lineEdit_cmd_dump_folder");
        lineEdit_cmd_dump_folder->setClearButtonEnabled(true);

        formLayout_6->setWidget(6, QFormLayout::ItemRole::FieldRole, lineEdit_cmd_dump_folder);

        label_25 = new QLabel(configuaration_groupbox);
        label_25->setObjectName("label_25");

        formLayout_6->setWidget(7, QFormLayout::ItemRole::LabelRole, label_25);

        comboBox_printf_direction = new QComboBox(configuaration_groupbox);
        comboBox_printf_direction->setObjectName("comboBox_printf_direction");

        formLayout_6->setWidget(7, QFormLayout::ItemRole::FieldRole, comboBox_printf_direction);

        label_26 = new QLabel(configuaration_groupbox);
        label_26->setObjectName("label_26");

        formLayout_6->setWidget(8, QFormLayout::ItemRole::LabelRole, label_26);

        lineEdit_printf_file = new MandatoryLineEdit(configuaration_groupbox);
        lineEdit_printf_file->setObjectName("lineEdit_printf_file");
        lineEdit_printf_file->setClearButtonEnabled(true);

        formLayout_6->setWidget(8, QFormLayout::ItemRole::FieldRole, lineEdit_printf_file);

        label_27 = new QLabel(configuaration_groupbox);
        label_27->setObjectName("label_27");

        formLayout_6->setWidget(9, QFormLayout::ItemRole::LabelRole, label_27);

        comboBox_profiler_direction = new QComboBox(configuaration_groupbox);
        comboBox_profiler_direction->setObjectName("comboBox_profiler_direction");

        formLayout_6->setWidget(9, QFormLayout::ItemRole::FieldRole, comboBox_profiler_direction);


        gridLayout->addWidget(configuaration_groupbox, 0, 0, 1, 1);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName("verticalLayout");
        ok_button = new QPushButton(ConfigurationEditDialog);
        ok_button->setObjectName("ok_button");
        ok_button->setMinimumSize(QSize(80, 25));

        verticalLayout->addWidget(ok_button);

        cancel_button = new QPushButton(ConfigurationEditDialog);
        cancel_button->setObjectName("cancel_button");
        cancel_button->setMinimumSize(QSize(80, 25));

        verticalLayout->addWidget(cancel_button);

        line_input_mapping = new QFrame(ConfigurationEditDialog);
        line_input_mapping->setObjectName("line_input_mapping");
        line_input_mapping->setFrameShape(QFrame::Shape::HLine);
        line_input_mapping->setFrameShadow(QFrame::Shadow::Sunken);

        verticalLayout->addWidget(line_input_mapping);

        input_mapping_button = new QPushButton(ConfigurationEditDialog);
        input_mapping_button->setObjectName("input_mapping_button");
        input_mapping_button->setMinimumSize(QSize(80, 25));

        verticalLayout->addWidget(input_mapping_button);

        line = new QFrame(ConfigurationEditDialog);
        line->setObjectName("line");
        line->setFrameShape(QFrame::Shape::HLine);
        line->setFrameShadow(QFrame::Shadow::Sunken);

        verticalLayout->addWidget(line);

        clear_button = new QPushButton(ConfigurationEditDialog);
        clear_button->setObjectName("clear_button");
        clear_button->setMinimumSize(QSize(80, 25));

        verticalLayout->addWidget(clear_button);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        gridLayout->addLayout(verticalLayout, 0, 1, 1, 1);


        retranslateUi(ConfigurationEditDialog);
        QObject::connect(cancel_button, &QPushButton::clicked, ConfigurationEditDialog, qOverload<>(&QDialog::reject));

        ok_button->setDefault(true);


        QMetaObject::connectSlotsByName(ConfigurationEditDialog);
    } // setupUi

    void retranslateUi(QDialog *ConfigurationEditDialog)
    {
        ConfigurationEditDialog->setWindowTitle(QCoreApplication::translate("ConfigurationEditDialog", "Settings", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_vulkan_validation->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Enable Vulkan validation layers", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_vulkan_validation->setText(QCoreApplication::translate("ConfigurationEditDialog", "Vulkan validation", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_shader_validation->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Validate SPIR-V binary", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_shader_validation->setText(QCoreApplication::translate("ConfigurationEditDialog", "Shader validation", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_cmd_dump->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Dump command buffers", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_cmd_dump->setText(QCoreApplication::translate("ConfigurationEditDialog", "Command buffer dump", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_renderdoc_capture->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Enable RenderDoc capture", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_renderdoc_capture->setText(QCoreApplication::translate("ConfigurationEditDialog", "RenderDoc capture", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_ngg_rectlist_draw->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Use the NGG 4-vertex path for rect-list DrawIndexAuto primitive 7", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_ngg_rectlist_draw->setText(QCoreApplication::translate("ConfigurationEditDialog", "Use NGG rect-list draw", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_graphics_debug_dump->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Dump intermediate GPU/graphics debug state (developer diagnostic)", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_graphics_debug_dump->setText(QCoreApplication::translate("ConfigurationEditDialog", "Graphics debug dump", nullptr));
#if QT_CONFIG(tooltip)
        checkBox_spirv_debug_printf->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Enable SPIR-V debugPrintfEXT output from recompiled shaders (developer diagnostic)", nullptr));
#endif // QT_CONFIG(tooltip)
        checkBox_spirv_debug_printf->setText(QCoreApplication::translate("ConfigurationEditDialog", "SPIR-V debug printf", nullptr));
        label_16->setText(QCoreApplication::translate("ConfigurationEditDialog", "Screen resolution:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_screen_resolution->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Window resolution", nullptr));
#endif // QT_CONFIG(tooltip)
        label_vblank_frequency->setText(QCoreApplication::translate("ConfigurationEditDialog", "Vblank frequency:", nullptr));
#if QT_CONFIG(tooltip)
        spinBox_vblank_frequency->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Virtual display refresh rate used for frame pacing", nullptr));
#endif // QT_CONFIG(tooltip)
        label_20->setText(QCoreApplication::translate("ConfigurationEditDialog", "Shader optimization type:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_shader_optimization_type->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Optimize shaders for code size or performance", nullptr));
#endif // QT_CONFIG(tooltip)
        label_21->setText(QCoreApplication::translate("ConfigurationEditDialog", "Shader log direction:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_shader_log_direction->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Dump shaders to file or console window. If enabled may decrease emulator performance", nullptr));
#endif // QT_CONFIG(tooltip)
        label_22->setText(QCoreApplication::translate("ConfigurationEditDialog", "Shader log folder:", nullptr));
#if QT_CONFIG(tooltip)
        lineEdit_shader_log_folder->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Specify directory to dump shaders", nullptr));
#endif // QT_CONFIG(tooltip)
        label_24->setText(QCoreApplication::translate("ConfigurationEditDialog", "Command buffer dump folder:", nullptr));
#if QT_CONFIG(tooltip)
        lineEdit_cmd_dump_folder->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Specify directory to dump command buffers", nullptr));
#endif // QT_CONFIG(tooltip)
        label_25->setText(QCoreApplication::translate("ConfigurationEditDialog", "Printf direction:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_printf_direction->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Print logs to file or console window. If enabled may decrease emulator performance", nullptr));
#endif // QT_CONFIG(tooltip)
        label_26->setText(QCoreApplication::translate("ConfigurationEditDialog", "Printf output file:", nullptr));
#if QT_CONFIG(tooltip)
        lineEdit_printf_file->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Specify file to dump logs", nullptr));
#endif // QT_CONFIG(tooltip)
        label_27->setText(QCoreApplication::translate("ConfigurationEditDialog", "Profiler direction:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_profiler_direction->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Enable or disable profiler. If enabled may decrease emulator performance", nullptr));
#endif // QT_CONFIG(tooltip)
        ok_button->setText(QCoreApplication::translate("ConfigurationEditDialog", "Save", nullptr));
        cancel_button->setText(QCoreApplication::translate("ConfigurationEditDialog", "Cancel", nullptr));
#if QT_CONFIG(tooltip)
        input_mapping_button->setToolTip(QCoreApplication::translate("ConfigurationEditDialog", "Configure keyboard and controller button mapping", nullptr));
#endif // QT_CONFIG(tooltip)
        input_mapping_button->setText(QCoreApplication::translate("ConfigurationEditDialog", "Input Mapping...", nullptr));
        clear_button->setText(QCoreApplication::translate("ConfigurationEditDialog", "Clear", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConfigurationEditDialog: public Ui_ConfigurationEditDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGURATION_EDIT_DIALOG_H
