#include "configurationEditDialog.h"

#include "configuration.h"
#include "firmware/firmwareManager.h"
#include "inputMappingDialog.h"
#include "mandatoryLineEdit.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QListView>
#include <QListWidget>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QtAlgorithms>

#include "ui_configuration_edit_dialog.h"

constexpr char SETTINGS_CFG_DIALOG[]               = "ConfigurationEditDialog";
constexpr char SETTINGS_CFG_LAST_GEOMETRY[]        = "geometry";
constexpr int  GLOBAL_SETTINGS_GAME_DIRS_MIN_WIDTH = 560;

static QString NormalizeGameDirectory(const QString& dir) {
	const auto trimmed = dir.trimmed();
	if (trimmed.isEmpty()) {
		return {};
	}

	return QDir::cleanPath(QDir(trimmed).absolutePath());
}

static QString GameDirectoryKey(const QString& dir) {
	const auto normalized = NormalizeGameDirectory(dir);
	if (normalized.isEmpty()) {
		return {};
	}

	auto canonical = QFileInfo(normalized).canonicalFilePath();
	if (canonical.isEmpty()) {
		canonical = normalized;
	}
	canonical = QDir::cleanPath(canonical);

#ifdef __linux__
	return canonical;
#else
	return canonical.toCaseFolded();
#endif
}

ConfigurationEditDialog::ConfigurationEditDialog(Configuration& info, QWidget* parent)
    : QDialog(parent, Qt::WindowCloseButtonHint), m_ui(new Ui::ConfigurationEditDialog),
      m_info(info) {
	m_ui->setupUi(this);
	InitGameDirectories();

	connect(m_ui->ok_button, &QPushButton::clicked, this, &ConfigurationEditDialog::save);
	connect(m_ui->clear_button, &QPushButton::clicked, this, &ConfigurationEditDialog::clear);
	connect(m_ui->input_mapping_button, &QPushButton::clicked, this,
	        &ConfigurationEditDialog::open_input_mapping);
	connect(m_ui->comboBox_shader_log_direction, &QComboBox::currentTextChanged, this,
	        [this](const QString& text) {
		        auto log = TextToEnum<Configuration::ShaderLogDirection>(text);
		        m_ui->lineEdit_shader_log_folder->setEnabled(
		            log == Configuration::ShaderLogDirection::File);
	        });
	connect(m_ui->checkBox_cmd_dump, &QCheckBox::toggled, this,
	        [this](bool flag) { m_ui->lineEdit_cmd_dump_folder->setEnabled(flag); });
	connect(m_ui->comboBox_printf_direction, &QComboBox::currentTextChanged, this,
	        [this](const QString& text) {
		        auto log = TextToEnum<Configuration::LogDirection>(text);
		        m_ui->lineEdit_printf_file->setEnabled(log == Configuration::LogDirection::File);
	        });
	connect(m_ui->comboBox_upscaler_method, &QComboBox::currentTextChanged, this,
	        [this](const QString& text) {
		        bool fsr_on = (text == QStringLiteral("Fsr31"));
		        m_ui->comboBox_upscaler_quality->setEnabled(fsr_on);
		        m_ui->doubleSpinBox_upscaler_sharpness->setEnabled(fsr_on);
	        });

	layout()->setSizeConstraint(QLayout::SetFixedSize);

	restoreGeometry(g_last_geometry);

	Init(info);
}

QByteArray ConfigurationEditDialog::g_last_geometry;

ConfigurationEditDialog::~ConfigurationEditDialog() {
	delete m_ui;
}

void ConfigurationEditDialog::WriteSettings(QSettings& s) {
	s.beginGroup(SETTINGS_CFG_DIALOG);

	if (!g_last_geometry.isEmpty()) {
		s.setValue(SETTINGS_CFG_LAST_GEOMETRY, g_last_geometry);
	}

	s.endGroup();
}

void ConfigurationEditDialog::ReadSettings(QSettings& s) {
	s.beginGroup(SETTINGS_CFG_DIALOG);

	g_last_geometry = s.value(SETTINGS_CFG_LAST_GEOMETRY, g_last_geometry).toByteArray();

	s.endGroup();
}

template <class T>
static void ListInit(QComboBox* combo, T value) {
	combo->clear();
	combo->addItems(EnumToList<T>());
	combo->setCurrentText(EnumToText(value));
}

void ConfigurationEditDialog::Init(const Configuration& info) {
	ListInit(m_ui->comboBox_screen_resolution, info.screen_resolution);
	m_ui->spinBox_vblank_frequency->setValue(info.vblank_frequency);
	m_ui->checkBox_shader_validation->setChecked(info.shader_validation_enabled);
	m_ui->checkBox_vulkan_validation->setChecked(info.vulkan_validation_enabled);
	m_ui->checkBox_renderdoc_capture->setChecked(info.renderdoc_enabled);
	m_ui->checkBox_ngg_rectlist_draw->setChecked(info.ngg_rectlist_draw_enabled);
	ListInit(m_ui->comboBox_shader_optimization_type, info.shader_optimization_type);
	ListInit(m_ui->comboBox_shader_log_direction, info.shader_log_direction);
	ListInit(m_ui->comboBox_present_filter, info.present_filter);
	ListInit(m_ui->comboBox_present_mode, info.present_mode);
	ListInit(m_ui->comboBox_aspect_ratio, info.aspect_ratio);
	ListInit(m_ui->comboBox_upscaler_method, info.upscaler_method);
	ListInit(m_ui->comboBox_upscaler_quality, info.upscaler_quality);
	m_ui->doubleSpinBox_upscaler_sharpness->setValue(info.upscaler_sharpness);
	// Enable/disable quality + sharpness based on upscaler method.
	bool fsr_on = (info.upscaler_method == Configuration::UpscalerMethod::Fsr31);
	m_ui->comboBox_upscaler_quality->setEnabled(fsr_on);
	m_ui->doubleSpinBox_upscaler_sharpness->setEnabled(fsr_on);
	m_ui->lineEdit_screenshot_folder->setText(info.screenshot_folder);
	m_ui->lineEdit_screenshot_hotkey->setText(QString::number(info.screenshot_hotkey));
	m_ui->lineEdit_shader_log_folder->setText(info.shader_log_folder);
	m_ui->lineEdit_shader_log_folder->setEnabled(info.shader_log_direction ==
	                                             Configuration::ShaderLogDirection::File);
	m_ui->checkBox_cmd_dump->setChecked(info.command_buffer_dump_enabled);
	m_ui->lineEdit_cmd_dump_folder->setText(info.command_buffer_dump_folder);
	m_ui->lineEdit_cmd_dump_folder->setEnabled(info.command_buffer_dump_enabled);
	m_ui->checkBox_graphics_debug_dump->setChecked(info.graphics_debug_dump_enabled);
	ListInit(m_ui->comboBox_printf_direction, info.printf_direction);
	m_ui->lineEdit_printf_file->setText(info.printf_output_file);
	m_ui->lineEdit_printf_file->setEnabled(info.printf_direction ==
	                                       Configuration::LogDirection::File);
	ListInit(m_ui->comboBox_profiler_direction, info.profiler_direction);
	m_ui->checkBox_spirv_debug_printf->setChecked(info.spirv_debug_printf_enabled);

	// PS4 (shadPS4 delegation) master switch. Built in code (not the .ui) so it
	// can carry an explanatory tooltip and only appear for global settings.
	if (m_ps4_support_check == nullptr) {
		m_ps4_support_check = new QCheckBox(tr("Enable PS4 game support (via shadPS4)"), this);
		m_ps4_support_check->setToolTip(tr("When enabled, PlayStation 4 titles are run through the bundled shadPS4 runtime, and the game list shows a PS4/PS5 badge per game. Requires a shadps4 binary next to the emulator (bundled when built with KYTY_BUNDLE_SHADPS4=ON)."));
		m_ui->gridLayout->addWidget(m_ps4_support_check, 2, 0, 1, 1);
	}
	m_ps4_support_check->setChecked(info.ps4_support_enabled);

	// PS5 Firmware installation (LLE support)
	if (m_firmware_install_btn == nullptr) {
		auto* fw_frame = new QFrame(this);
		auto* fw_layout = new QHBoxLayout(fw_frame);
		fw_layout->setContentsMargins(0, 0, 0, 0);
		fw_layout->setSpacing(8);

		m_firmware_install_btn = new QPushButton(tr("Install PS5 Firmware..."), this);
		m_firmware_install_btn->setToolTip(tr("Install official PS5 firmware (.pup) for Low-Level Emulation. "
		                                        "Download from: https://www.playstation.com/en-us/support/hardware/ps5/system-software/"));
		fw_layout->addWidget(m_firmware_install_btn);

		m_firmware_status_label = new QLabel(tr("No firmware installed (HLE-only)"), this);
		m_firmware_status_label->setStyleSheet("color: gray;");
		fw_layout->addWidget(m_firmware_status_label);

		m_ui->gridLayout->addWidget(fw_frame, 3, 0, 1, 2);

		connect(m_firmware_install_btn, &QPushButton::clicked, this, &ConfigurationEditDialog::install_firmware);
	}

	// Update firmware status display
	const bool firmware_installed = Libs::Firmware::FirmwareManager::Instance().IsInstalled();
	const auto modules = Libs::Firmware::FirmwareManager::Instance().GetModules();

	if (firmware_installed || !info.firmware_path.isEmpty()) {
		m_firmware_status_label->setText(
		    tr("Firmware installed (%1 modules)").arg(modules.size()));
		m_firmware_status_label->setStyleSheet("color: green;");
		m_firmware_install_btn->setText(tr("Update PS5 Firmware..."));
	} else {
		m_firmware_status_label->setText(tr("No firmware installed (HLE-only)"));
		m_firmware_status_label->setStyleSheet("color: gray;");
		m_firmware_install_btn->setText(tr("Install PS5 Firmware..."));
	}
}

void ConfigurationEditDialog::InitGameDirectories() {
	m_game_dirs_group = new QGroupBox(tr("Game folders"), this);

	auto* group_layout = new QVBoxLayout(m_game_dirs_group);
	group_layout->setContentsMargins(8, 8, 8, 8);
	group_layout->setSpacing(6);

	m_game_dirs_list = new QListWidget(m_game_dirs_group);
	m_game_dirs_list->setMinimumHeight(120);
	m_game_dirs_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
	group_layout->addWidget(m_game_dirs_list);

	auto* button_layout = new QHBoxLayout;
	button_layout->setContentsMargins(0, 0, 0, 0);
	button_layout->setSpacing(4);

	auto* add_button = new QToolButton(m_game_dirs_group);
	add_button->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
	add_button->setToolTip(tr("Add game folder"));
	add_button->setAutoRaise(true);
	button_layout->addWidget(add_button);

	m_remove_game_dir_button = new QToolButton(m_game_dirs_group);
	m_remove_game_dir_button->setIcon(style()->standardIcon(QStyle::SP_DialogDiscardButton));
	m_remove_game_dir_button->setToolTip(tr("Remove selected game folders"));
	m_remove_game_dir_button->setAutoRaise(true);
	button_layout->addWidget(m_remove_game_dir_button);

	button_layout->addStretch(1);
	group_layout->addLayout(button_layout);

	m_ui->gridLayout->addWidget(m_game_dirs_group, 1, 0, 1, 1);
	m_game_dirs_group->setVisible(false);

	connect(add_button, &QToolButton::clicked, this, &ConfigurationEditDialog::add_game_directory);
	connect(m_remove_game_dir_button, &QToolButton::clicked, this,
	        &ConfigurationEditDialog::remove_selected_game_directories);
	connect(m_game_dirs_list, &QListWidget::itemSelectionChanged, this,
	        &ConfigurationEditDialog::update_game_directory_buttons);

	update_game_directory_buttons();
}

void ConfigurationEditDialog::SetTitle(const QString& str) {
	setWindowTitle(str);
}

void ConfigurationEditDialog::SetGameDirectories(const QStringList& dirs) {
	m_show_game_dirs = true;
	m_game_dirs_list->clear();
	m_game_dirs_group->setMinimumWidth(GLOBAL_SETTINGS_GAME_DIRS_MIN_WIDTH);

	for (const auto& dir: dirs) {
		AddGameDirectoryItem(dir);
	}

	m_game_dirs_group->setVisible(true);
	update_game_directory_buttons();
	adjustSize();
}

QStringList ConfigurationEditDialog::GetGameDirectories() const {
	QStringList dirs;
	if (!m_show_game_dirs) {
		return dirs;
	}

	for (int index = 0; index < m_game_dirs_list->count(); index++) {
		auto* item = m_game_dirs_list->item(index);
		dirs.append(item->text());
	}

	return dirs;
}

void ConfigurationEditDialog::AddGameDirectoryItem(const QString& dir) {
	const auto normalized = NormalizeGameDirectory(dir);
	const auto key        = GameDirectoryKey(normalized);
	if (normalized.isEmpty() || key.isEmpty()) {
		return;
	}

	for (int index = 0; index < m_game_dirs_list->count(); index++) {
		auto* item = m_game_dirs_list->item(index);
		if (GameDirectoryKey(item->text()) == key) {
			return;
		}
	}

	auto* item = new QListWidgetItem(normalized, m_game_dirs_list);
	item->setToolTip(normalized);
}

void ConfigurationEditDialog::moveEvent(QMoveEvent* event) {
	QDialog::moveEvent(event);
	g_last_geometry = saveGeometry();
}

static void UpdateInfo(Configuration& info, Ui::ConfigurationEditDialog& ui) {
	info.screen_resolution =
	    TextToEnum<Configuration::Resolution>(ui.comboBox_screen_resolution->currentText());
	info.vblank_frequency          = ui.spinBox_vblank_frequency->value();
	info.vulkan_validation_enabled = ui.checkBox_vulkan_validation->isChecked();
	info.shader_validation_enabled = ui.checkBox_shader_validation->isChecked();
	info.renderdoc_enabled         = ui.checkBox_renderdoc_capture->isChecked();
	info.ngg_rectlist_draw_enabled = ui.checkBox_ngg_rectlist_draw->isChecked();
	info.shader_optimization_type  = TextToEnum<Configuration::ShaderOptimizationType>(
	    ui.comboBox_shader_optimization_type->currentText());
	info.shader_log_direction = TextToEnum<Configuration::ShaderLogDirection>(
	    ui.comboBox_shader_log_direction->currentText());
	info.present_filter =
	    TextToEnum<Configuration::PresentFilter>(ui.comboBox_present_filter->currentText());
	info.present_mode =
	    TextToEnum<Configuration::PresentMode>(ui.comboBox_present_mode->currentText());
	info.aspect_ratio =
	    TextToEnum<Configuration::AspectRatio>(ui.comboBox_aspect_ratio->currentText());
	info.upscaler_method =
	    TextToEnum<Configuration::UpscalerMethod>(ui.comboBox_upscaler_method->currentText());
	info.upscaler_quality =
	    TextToEnum<Configuration::UpscalerQuality>(ui.comboBox_upscaler_quality->currentText());
	info.upscaler_sharpness = ui.doubleSpinBox_upscaler_sharpness->value();
	info.screenshot_folder = ui.lineEdit_screenshot_folder->text();
	{
		bool ok = false;
		const auto sc = ui.lineEdit_screenshot_hotkey->text().toUInt(&ok, 0);
		info.screenshot_hotkey = ok ? sc : 0u;
	}
	info.shader_log_folder           = ui.lineEdit_shader_log_folder->text();
	info.command_buffer_dump_enabled = ui.checkBox_cmd_dump->isChecked();
	info.command_buffer_dump_folder  = ui.lineEdit_cmd_dump_folder->text();
	info.graphics_debug_dump_enabled = ui.checkBox_graphics_debug_dump->isChecked();
	info.printf_direction =
	    TextToEnum<Configuration::LogDirection>(ui.comboBox_printf_direction->currentText());
	info.printf_output_file = ui.lineEdit_printf_file->text();
	info.profiler_direction =
	    TextToEnum<Configuration::ProfilerDirection>(ui.comboBox_profiler_direction->currentText());
	info.spirv_debug_printf_enabled = ui.checkBox_spirv_debug_printf->isChecked();
}

void ConfigurationEditDialog::update_info() {
	UpdateInfo(m_info, *m_ui);
	// The PS4-support checkbox is created in code (m_ps4_support_check), not in
	// the generated Ui, so it is read here in the member, not in UpdateInfo().
	m_info.ps4_support_enabled = (m_ps4_support_check != nullptr && m_ps4_support_check->isChecked());
}

void ConfigurationEditDialog::adjust_size() {
	this->adjustSize();
}

void ConfigurationEditDialog::save() {
	if (MandatoryLineEdit::FindEmpty(this)) {
		QMessageBox::critical(this, tr("Save failed"), tr("Please fill all mandatory fields"));
		return;
	}

	update_info();

	emit accept();
}

void ConfigurationEditDialog::clear() {
	Configuration default_info;
	Init(default_info);

	if (m_show_game_dirs) {
		m_game_dirs_list->clear();
		update_game_directory_buttons();
	}
}

void ConfigurationEditDialog::open_input_mapping() {
	// Edits m_info directly (rather than a temp copy) so Cancel on this dialog also discards
	// any input mapping changes, matching every other field's Save/Cancel semantics here.
	InputMappingDialog dlg(m_info, this);
	dlg.exec();
}

void ConfigurationEditDialog::add_game_directory() {
	QString start_dir = QDir::homePath();
	if (m_game_dirs_list->count() > 0) {
		start_dir = m_game_dirs_list->item(m_game_dirs_list->count() - 1)->text();
	}

	QFileDialog dialog(this, tr("Select game folders"), start_dir);
	dialog.setFileMode(QFileDialog::Directory);
	dialog.setOption(QFileDialog::ShowDirsOnly, true);
	dialog.setOption(QFileDialog::DontUseNativeDialog, true);
	dialog.setLabelText(QFileDialog::Accept, tr("Add"));

	auto* list_view = dialog.findChild<QListView*>(QStringLiteral("listView"));
	if (list_view != nullptr) {
		list_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	}
	auto* tree_view = dialog.findChild<QTreeView*>(QStringLiteral("treeView"));
	if (tree_view != nullptr) {
		tree_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
	}

	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	for (const auto& dir: dialog.selectedFiles()) {
		AddGameDirectoryItem(dir);
	}

	update_game_directory_buttons();
}

void ConfigurationEditDialog::remove_selected_game_directories() {
	qDeleteAll(m_game_dirs_list->selectedItems());
	update_game_directory_buttons();
}

void ConfigurationEditDialog::update_game_directory_buttons() {
	m_remove_game_dir_button->setEnabled(!m_game_dirs_list->selectedItems().isEmpty());
}

void ConfigurationEditDialog::install_firmware() {
	// Open file dialog to select .pup file
	const QString pup_path = QFileDialog::getOpenFileName(
	    this,
	    tr("Select PS5 Firmware File (.pup)"),
	    QDir::homePath(),
	    tr("PS5 Firmware Files (*.pup *.PUP);;All Files (*)"));

	if (pup_path.isEmpty()) {
		return; // User cancelled
	}

	// Show progress dialog
	QProgressDialog progress(tr("Parsing PS5 firmware..."), tr("Cancel"), 0, 100, this);
	progress.setWindowTitle(tr("Firmware Installation"));
	progress.setWindowModality(Qt::WindowModal);
	progress.setValue(10);
	progress.show();

	// Call actual firmware installation
	const auto result = Libs::Firmware::FirmwareManager::Instance().InstallFromPup(
	    pup_path.toStdString());

	progress.setValue(100);

	if (!result.ok) {
		// Installation failed
		QMessageBox::critical(
		    this,
		    tr("Firmware Installation Failed"),
		    tr("Failed to install firmware:\n%1\n\n"
		       "Please ensure you downloaded the official PS5 system software from:\n"
		       "https://www.playstation.com/en-us/support/hardware/ps5/system-software/")
		        .arg(QString::fromStdString(result.error)));
		return;
	}

	// Installation succeeded
	m_info.firmware_path = pup_path;

	QMessageBox::information(
	    this,
	    tr("Firmware Installation Successful"),
	    tr("Firmware installed successfully!\n\n"
	       "Version: %1\n"
	       "Modules extracted: %2\n\n"
	       "The emulator will now use Low-Level Emulation (LLE) for available system modules.\n"
	       "Restart the emulator to apply changes.")
	        .arg(QString::fromStdString(result.version))
	        .arg(result.modules_installed));

	// Update UI
	m_firmware_status_label->setText(tr("Firmware installed (%1 modules)").arg(result.modules_installed));
	m_firmware_status_label->setStyleSheet("color: green;");
	m_firmware_install_btn->setText(tr("Update PS5 Firmware..."));
}
