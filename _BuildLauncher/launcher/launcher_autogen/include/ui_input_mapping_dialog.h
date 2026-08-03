/********************************************************************************
** Form generated from reading UI file 'input_mapping_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INPUT_MAPPING_DIALOG_H
#define UI_INPUT_MAPPING_DIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_InputMappingDialog
{
public:
    QVBoxLayout *verticalLayout_main;
    QTabWidget *tabWidget;
    QWidget *tab_keyboard;
    QVBoxLayout *verticalLayout_keyboard;
    QLabel *label_keyboard_hint;
    QTableWidget *table_keyboard;
    QWidget *tab_controller;
    QVBoxLayout *verticalLayout_controller;
    QLabel *label_controller_hint;
    QTableWidget *table_controller;
    QHBoxLayout *horizontalLayout_buttons;
    QPushButton *reset_defaults_button;
    QSpacerItem *horizontalSpacer;
    QPushButton *ok_button;
    QPushButton *cancel_button;

    void setupUi(QDialog *InputMappingDialog)
    {
        if (InputMappingDialog->objectName().isEmpty())
            InputMappingDialog->setObjectName("InputMappingDialog");
        InputMappingDialog->resize(480, 480);
        verticalLayout_main = new QVBoxLayout(InputMappingDialog);
        verticalLayout_main->setObjectName("verticalLayout_main");
        tabWidget = new QTabWidget(InputMappingDialog);
        tabWidget->setObjectName("tabWidget");
        tab_keyboard = new QWidget();
        tab_keyboard->setObjectName("tab_keyboard");
        verticalLayout_keyboard = new QVBoxLayout(tab_keyboard);
        verticalLayout_keyboard->setObjectName("verticalLayout_keyboard");
        label_keyboard_hint = new QLabel(tab_keyboard);
        label_keyboard_hint->setObjectName("label_keyboard_hint");
        label_keyboard_hint->setWordWrap(true);

        verticalLayout_keyboard->addWidget(label_keyboard_hint);

        table_keyboard = new QTableWidget(tab_keyboard);
        if (table_keyboard->columnCount() < 2)
            table_keyboard->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        table_keyboard->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        table_keyboard->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        table_keyboard->setObjectName("table_keyboard");
        table_keyboard->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_keyboard->setSelectionMode(QAbstractItemView::NoSelection);

        verticalLayout_keyboard->addWidget(table_keyboard);

        tabWidget->addTab(tab_keyboard, QString());
        tab_controller = new QWidget();
        tab_controller->setObjectName("tab_controller");
        verticalLayout_controller = new QVBoxLayout(tab_controller);
        verticalLayout_controller->setObjectName("verticalLayout_controller");
        label_controller_hint = new QLabel(tab_controller);
        label_controller_hint->setObjectName("label_controller_hint");
        label_controller_hint->setWordWrap(true);

        verticalLayout_controller->addWidget(label_controller_hint);

        table_controller = new QTableWidget(tab_controller);
        if (table_controller->columnCount() < 2)
            table_controller->setColumnCount(2);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        table_controller->setHorizontalHeaderItem(0, __qtablewidgetitem2);
        QTableWidgetItem *__qtablewidgetitem3 = new QTableWidgetItem();
        table_controller->setHorizontalHeaderItem(1, __qtablewidgetitem3);
        table_controller->setObjectName("table_controller");
        table_controller->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table_controller->setSelectionMode(QAbstractItemView::NoSelection);

        verticalLayout_controller->addWidget(table_controller);

        tabWidget->addTab(tab_controller, QString());

        verticalLayout_main->addWidget(tabWidget);

        horizontalLayout_buttons = new QHBoxLayout();
        horizontalLayout_buttons->setObjectName("horizontalLayout_buttons");
        reset_defaults_button = new QPushButton(InputMappingDialog);
        reset_defaults_button->setObjectName("reset_defaults_button");

        horizontalLayout_buttons->addWidget(reset_defaults_button);

        horizontalSpacer = new QSpacerItem(0, 0, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout_buttons->addItem(horizontalSpacer);

        ok_button = new QPushButton(InputMappingDialog);
        ok_button->setObjectName("ok_button");

        horizontalLayout_buttons->addWidget(ok_button);

        cancel_button = new QPushButton(InputMappingDialog);
        cancel_button->setObjectName("cancel_button");

        horizontalLayout_buttons->addWidget(cancel_button);


        verticalLayout_main->addLayout(horizontalLayout_buttons);


        retranslateUi(InputMappingDialog);
        QObject::connect(cancel_button, &QPushButton::clicked, InputMappingDialog, qOverload<>(&QDialog::reject));

        ok_button->setDefault(true);


        QMetaObject::connectSlotsByName(InputMappingDialog);
    } // setupUi

    void retranslateUi(QDialog *InputMappingDialog)
    {
        InputMappingDialog->setWindowTitle(QCoreApplication::translate("InputMappingDialog", "Input Mapping", nullptr));
        label_keyboard_hint->setText(QCoreApplication::translate("InputMappingDialog", "Click a field, then press the key you want to bind. Press Escape to unbind.", nullptr));
        QTableWidgetItem *___qtablewidgetitem = table_keyboard->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("InputMappingDialog", "PS5 Button", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = table_keyboard->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("InputMappingDialog", "Key", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_keyboard), QCoreApplication::translate("InputMappingDialog", "Keyboard", nullptr));
        label_controller_hint->setText(QCoreApplication::translate("InputMappingDialog", "Select which physical controller button triggers each PS5 button.", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = table_controller->horizontalHeaderItem(0);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("InputMappingDialog", "PS5 Button", nullptr));
        QTableWidgetItem *___qtablewidgetitem3 = table_controller->horizontalHeaderItem(1);
        ___qtablewidgetitem3->setText(QCoreApplication::translate("InputMappingDialog", "Controller Button", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_controller), QCoreApplication::translate("InputMappingDialog", "Controller", nullptr));
        reset_defaults_button->setText(QCoreApplication::translate("InputMappingDialog", "Reset to Defaults", nullptr));
        ok_button->setText(QCoreApplication::translate("InputMappingDialog", "Save", nullptr));
        cancel_button->setText(QCoreApplication::translate("InputMappingDialog", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class InputMappingDialog: public Ui_InputMappingDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INPUT_MAPPING_DIALOG_H
