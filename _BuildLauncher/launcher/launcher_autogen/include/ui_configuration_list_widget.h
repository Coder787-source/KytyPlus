/********************************************************************************
** Form generated from reading UI file 'configuration_list_widget.ui'
**
** Created by: Qt User Interface Compiler version 6.10.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_CONFIGURATION_LIST_WIDGET_H
#define UI_CONFIGURATION_LIST_WIDGET_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "gameListTreeWidget.h"

QT_BEGIN_NAMESPACE

class Ui_ConfigurationListWidget
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *horizontalLayout_2;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QToolButton *global_settings_button;
    QToolButton *edit_button;
    QToolButton *delete_button;
    QSpacerItem *horizontalSpacer;
    QLineEdit *search_line_edit;
    GameListTreeWidget *cfgs_list;

    void setupUi(QWidget *ConfigurationListWidget)
    {
        if (ConfigurationListWidget->objectName().isEmpty())
            ConfigurationListWidget->setObjectName("ConfigurationListWidget");
        ConfigurationListWidget->resize(292, 268);
        ConfigurationListWidget->setStyleSheet(QString::fromUtf8(""));
        gridLayout = new QGridLayout(ConfigurationListWidget);
        gridLayout->setObjectName("gridLayout");
        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName("horizontalLayout_2");
        horizontalLayout_2->setContentsMargins(6, 0, 6, 6);
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(4);
        verticalLayout->setObjectName("verticalLayout");
        verticalLayout->setContentsMargins(-1, 2, -1, -1);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(4);
        horizontalLayout->setObjectName("horizontalLayout");
        global_settings_button = new QToolButton(ConfigurationListWidget);
        global_settings_button->setObjectName("global_settings_button");
        global_settings_button->setMinimumSize(QSize(36, 36));
        global_settings_button->setIconSize(QSize(32, 32));
        global_settings_button->setAutoRaise(true);

        horizontalLayout->addWidget(global_settings_button);

        edit_button = new QToolButton(ConfigurationListWidget);
        edit_button->setObjectName("edit_button");
        edit_button->setMinimumSize(QSize(36, 36));
        edit_button->setIconSize(QSize(32, 32));
        edit_button->setAutoRaise(true);

        horizontalLayout->addWidget(edit_button);

        delete_button = new QToolButton(ConfigurationListWidget);
        delete_button->setObjectName("delete_button");
        delete_button->setMinimumSize(QSize(36, 36));
        delete_button->setStyleSheet(QString::fromUtf8(""));
        delete_button->setIconSize(QSize(32, 32));
        delete_button->setAutoRaise(true);

        horizontalLayout->addWidget(delete_button);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Policy::Expanding, QSizePolicy::Policy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        search_line_edit = new QLineEdit(ConfigurationListWidget);
        search_line_edit->setObjectName("search_line_edit");
        QSizePolicy sizePolicy(QSizePolicy::Policy::Preferred, QSizePolicy::Policy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(search_line_edit->sizePolicy().hasHeightForWidth());
        search_line_edit->setSizePolicy(sizePolicy);
        search_line_edit->setMinimumSize(QSize(220, 30));
        search_line_edit->setMaximumSize(QSize(320, 30));
        search_line_edit->setClearButtonEnabled(true);

        horizontalLayout->addWidget(search_line_edit);


        verticalLayout->addLayout(horizontalLayout);

        cfgs_list = new GameListTreeWidget(ConfigurationListWidget);
        cfgs_list->setObjectName("cfgs_list");
        cfgs_list->setRootIsDecorated(false);

        verticalLayout->addWidget(cfgs_list);


        horizontalLayout_2->addLayout(verticalLayout);


        gridLayout->addLayout(horizontalLayout_2, 0, 0, 1, 1);


        retranslateUi(ConfigurationListWidget);

        QMetaObject::connectSlotsByName(ConfigurationListWidget);
    } // setupUi

    void retranslateUi(QWidget *ConfigurationListWidget)
    {
        ConfigurationListWidget->setWindowTitle(QCoreApplication::translate("ConfigurationListWidget", "Configurations", nullptr));
#if QT_CONFIG(tooltip)
        global_settings_button->setToolTip(QCoreApplication::translate("ConfigurationListWidget", "<html><head/><body><p>Edit global settings</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        global_settings_button->setText(QString());
#if QT_CONFIG(tooltip)
        edit_button->setToolTip(QCoreApplication::translate("ConfigurationListWidget", "<html><head/><body><p>Edit configuration</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        edit_button->setText(QString());
#if QT_CONFIG(tooltip)
        delete_button->setToolTip(QCoreApplication::translate("ConfigurationListWidget", "<html><head/><body><p>Delete configuration</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        delete_button->setText(QString());
#if QT_CONFIG(tooltip)
        search_line_edit->setToolTip(QCoreApplication::translate("ConfigurationListWidget", "Search game name or serial", nullptr));
#endif // QT_CONFIG(tooltip)
        search_line_edit->setPlaceholderText(QCoreApplication::translate("ConfigurationListWidget", "Search name or serial", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = cfgs_list->headerItem();
        ___qtreewidgetitem->setText(6, QCoreApplication::translate("ConfigurationListWidget", "Comments", nullptr));
        ___qtreewidgetitem->setText(5, QCoreApplication::translate("ConfigurationListWidget", "Status", nullptr));
        ___qtreewidgetitem->setText(4, QCoreApplication::translate("ConfigurationListWidget", "Path", nullptr));
        ___qtreewidgetitem->setText(3, QCoreApplication::translate("ConfigurationListWidget", "Firmware Version", nullptr));
        ___qtreewidgetitem->setText(2, QCoreApplication::translate("ConfigurationListWidget", "Game Version", nullptr));
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("ConfigurationListWidget", "Serial", nullptr));
        ___qtreewidgetitem->setText(0, QCoreApplication::translate("ConfigurationListWidget", "Game", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ConfigurationListWidget: public Ui_ConfigurationListWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_CONFIGURATION_LIST_WIDGET_H
