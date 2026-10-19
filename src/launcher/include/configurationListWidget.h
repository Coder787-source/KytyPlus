#ifndef CONFIGURATION_LIST_WIDGET_H
#define CONFIGURATION_LIST_WIDGET_H

#include "common/common.h"
#include "configuration.h"

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <memory>

#include <memory>

class ConfigurationItem;
class CompatibilityDatabase;
class ControllerPreview;
class QTreeWidgetItem;
class QPoint;
class MainDialog;

namespace Ui {
class ConfigurationListWidget;
} // namespace Ui

class ConfigurationListWidget: public QWidget {
	Q_OBJECT
	KYTY_CLASS_NO_COPY(ConfigurationListWidget);

public:
	explicit ConfigurationListWidget(QWidget* parent = nullptr);
	~ConfigurationListWidget() override;

	void               SetRunEnabled(bool flag) { m_run_enabled = flag; }
	void               SetGameRunning(bool flag) { m_game_running = flag; }
	void               SetRuntimeDirectory(const QString& directory) { m_runtime_directory = directory; }
	[[nodiscard]] bool IsRunEnabled() const { return m_run_enabled; }

	[[nodiscard]] const ConfigurationItem* GetSelectedItem() const { return m_selected_item; }
	ConfigurationItem*                     GetSelectedItem() { return m_selected_item; }

	void        SetMainDialog(MainDialog* main_dialog) { m_main_dialog = main_dialog; }
	MainDialog* GetMainDialog() { return m_main_dialog; }

	// The Controls dialog captures gamepad presses through the launcher's single SDL
	// poller, so it needs the owner's preview; without it the pad tab is inert.
	void SetControllerPreview(ControllerPreview* preview) { m_controller_preview = preview; }

	[[nodiscard]] const QString&     GetSettingsFile() const { return m_settings_file; }
	[[nodiscard]] const QStringList& GetGameDirectories() const { return m_game_dirs; }
	void AddGameDirectory(const QString& dir);
	[[nodiscard]] const QStringList& GetHostInputMapping() const {
		return m_global_info.host_input_mapping;
	}
	[[nodiscard]] bool CanViewSelectedTrophies() const;

	[[nodiscard]] std::unique_ptr<Configuration>
	CreateConfiguration(const ConfigurationItem& item) const;

	bool EnsureGameDirectory();
	void ScanGameDirectory();
	bool SelectGameImage(const QString& path);
	void ViewTrophies();
	void edit_global_settings();

signals:

	void Run();
	void Select();
	void PreviewControllerColor(const QString& color);

public slots:
	void WriteSettings();
	void ReadSettings();

public slots:
	void edit_input_mapping();

protected slots:

	void edit_configuration();
	void delete_configuartion();
	void run_configuration();
	void list_currentItemChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
	void list_itemDoubleClicked(QTreeWidgetItem* witem, int column);
	void show_context_menu(const QPoint& pos);
	void open_game_folder();
	void delete_game();
	void remove_save_data();
	void filter_configurations(const QString& text);

private:
	void               ClearCustomSettings(ConfigurationItem* item);
	void               SelectItem(QTreeWidgetItem* witem);
	void               ApplyCompatibility();
	void               ImportGameSettings(QWidget* parent);
	void               ExportGameSettings(QWidget* parent) const;
	[[nodiscard]] bool HasValidGameDirectory() const;
	[[nodiscard]] bool HasRunningGame() const;
	[[nodiscard]] bool CanDeleteGame(const ConfigurationItem* item) const;

	ConfigurationItem*            m_selected_item = nullptr;
	bool                          m_run_enabled   = true;
	bool                          m_game_running  = false;
	Ui::ConfigurationListWidget*  m_ui            = nullptr;
	MainDialog*                   m_main_dialog   = nullptr;
	ControllerPreview*            m_controller_preview = nullptr;
	QString                       m_settings_file;
	QString                       m_runtime_directory;
	QStringList                   m_game_dirs;
	QString                       m_addons_dir;
	Configuration                 m_global_info;
	QMap<QString, Configuration*> m_custom_infos;
	CompatibilityDatabase*        m_compatibility = nullptr;
};

#endif // CONFIGURATION_LIST_WIDGET_H
