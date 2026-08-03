/********************************************************************************
** Form generated from reading UI file 'main_dialog.ui'
**
** Created by: Qt User Interface Compiler version 6.10.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAIN_DIALOG_H
#define UI_MAIN_DIALOG_H

#include <QtCore/QLocale>
#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QVBoxLayout>
#include "configurationListWidget.h"

QT_BEGIN_NAMESPACE

class Ui_MainDialog
{
public:
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout_5;
    QHBoxLayout *horizontalLayout_3;
    QVBoxLayout *verticalLayout_3;
    ConfigurationListWidget *widget;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_settings_file;
    QLabel *label_Interpreter;
    QLabel *label_Version;
    QButtonGroup *buttonGroup_Console;

    void setupUi(QDialog *MainDialog)
    {
        if (MainDialog->objectName().isEmpty())
            MainDialog->setObjectName("MainDialog");
        MainDialog->resize(428, 454);
        MainDialog->setMinimumSize(QSize(0, 0));
        MainDialog->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
        gridLayout = new QGridLayout(MainDialog);
        gridLayout->setSpacing(6);
        gridLayout->setContentsMargins(11, 11, 11, 11);
        gridLayout->setObjectName("gridLayout");
        verticalLayout_5 = new QVBoxLayout();
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setObjectName("verticalLayout_5");
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(6);
        horizontalLayout_3->setObjectName("horizontalLayout_3");
        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setObjectName("verticalLayout_3");
        widget = new ConfigurationListWidget(MainDialog);
        widget->setObjectName("widget");
        QSizePolicy sizePolicy(QSizePolicy::Policy::MinimumExpanding, QSizePolicy::Policy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widget->sizePolicy().hasHeightForWidth());
        widget->setSizePolicy(sizePolicy);
        widget->setMinimumSize(QSize(400, 300));

        verticalLayout_3->addWidget(widget);


        horizontalLayout_3->addLayout(verticalLayout_3);


        verticalLayout_5->addLayout(horizontalLayout_3);

        verticalLayout_4 = new QVBoxLayout();
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setObjectName("verticalLayout_4");
        label_settings_file = new QLabel(MainDialog);
        label_settings_file->setObjectName("label_settings_file");

        verticalLayout_4->addWidget(label_settings_file);

        label_Interpreter = new QLabel(MainDialog);
        label_Interpreter->setObjectName("label_Interpreter");
        label_Interpreter->setFrameShape(QFrame::NoFrame);
        label_Interpreter->setFrameShadow(QFrame::Sunken);

        verticalLayout_4->addWidget(label_Interpreter);

        label_Version = new QLabel(MainDialog);
        label_Version->setObjectName("label_Version");

        verticalLayout_4->addWidget(label_Version);


        verticalLayout_5->addLayout(verticalLayout_4);


        gridLayout->addLayout(verticalLayout_5, 0, 0, 1, 1);


        retranslateUi(MainDialog);

        QMetaObject::connectSlotsByName(MainDialog);
    } // setupUi

    void retranslateUi(QDialog *MainDialog)
    {
        MainDialog->setWindowTitle(QCoreApplication::translate("MainDialog", "Kyty Launcher", nullptr));
        label_settings_file->setText(QCoreApplication::translate("MainDialog", "TextLabel", nullptr));
        label_Interpreter->setText(QCoreApplication::translate("MainDialog", "TextLabel", nullptr));
        label_Version->setText(QCoreApplication::translate("MainDialog", "TextLabel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainDialog: public Ui_MainDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAIN_DIALOG_H
