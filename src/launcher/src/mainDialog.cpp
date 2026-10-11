#include "mainDialog.h"

#include <functional>

#include "common.h"
#include "configuration.h"
#include "configurationItem.h"
#include "configurationListWidget.h"
#include "patchesDialog.h"

#include <QApplication>
#include <QByteArray>
#include <QFileDialog>
#include <QDialog>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <memory>
#include <QRadioButton>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStringList>
#include <QTextStream>
#include <QVariant>
#include <QtCore>

#include "ui_main_dialog.h"

#if defined(_WIN32)
#include <windows.h> // IWYU pragma: keep
#endif

// IWYU pragma: no_include <minwindef.h>
// IWYU pragma: no_include <processthreadsapi.h>
// IWYU pragma: no_include <winbase.h>

class QWidget;

#if defined(_WIN32)
constexpr char EMULATOR_EXE[] = "kyty_emulator.exe";
#else
constexpr char EMULATOR_EXE[] = "kyty_emulator";
#endif

#if defined(__linux__)
constexpr char KYTY_BASH_FILE[] = "kyty_run.sh";
#endif
constexpr char SETTINGS_MAIN_DIALOG[]        = "MainDialog";
constexpr char SETTINGS_MAIN_LAST_GEOMETRY[] = "geometry";

class DetachableProcess: public QProcess {
	Q_OBJECT;

public:
	explicit DetachableProcess(QObject* parent = nullptr): QProcess(parent) {}
	void Detach() {
		this->waitForStarted();
		setProcessState(QProcess::NotRunning);
	}
};

class MainDialogPrivate: public QObject {
	Q_OBJECT
	KYTY_QT_CLASS_NO_COPY(MainDialogPrivate);

public:
	explicit MainDialogPrivate(QObject* parent = nullptr): QObject(parent) {}
	~MainDialogPrivate() override;

	void Setup(MainDialog* main_dialog);

	/*slots:*/

	void Update();
	void FindInterpreter();
	void Run();
	void InstallPkg();
	void LoadGame();
	void RunInstall(const QString& file, const QString& flag);
	void OpenGlobalSettings();

	[[nodiscard]] const QString& GetInterpreter() const { return m_interpreter; }

	static void WriteSettings(QSettings& s);
	static void ReadSettings(QSettings& s);

private:
	static QByteArray g_last_geometry;

	Ui::MainDialog* m_ui          = {nullptr};
	MainDialog*     m_main_dialog = nullptr;
	QString         m_interpreter;

	/*DetachableProcess*/ QProcess m_process;

	ConfigurationItem* m_running_item = nullptr;
};

QByteArray MainDialogPrivate::g_last_geometry;

MainDialog::MainDialog(QWidget* parent): QDialog(parent), m_p(new MainDialogPrivate(this)) {
	m_p->Setup(this);
}

MainDialogPrivate::~MainDialogPrivate() {
	delete m_ui;
}

void MainDialogPrivate::Setup(MainDialog* main_dialog) {
	m_ui = new Ui::MainDialog;
	m_ui->setupUi(main_dialog);

	m_main_dialog = main_dialog;
	m_ui->widget->SetMainDialog(main_dialog);

	main_dialog->setWindowFlags(Qt::Dialog /*| Qt::MSWindowsFixedSizeDialogHint*/);

	connect(main_dialog, &MainDialog::Start, this, &MainDialogPrivate::FindInterpreter,
	        Qt::QueuedConnection);
	connect(m_ui->widget, &ConfigurationListWidget::Select, this, &MainDialogPrivate::Update);
	connect(m_ui->widget, &ConfigurationListWidget::Run, this, &MainDialogPrivate::Run);
	connect(m_ui->pushButton_InstallPkg, &QPushButton::clicked, this, &MainDialogPrivate::InstallPkg);
	connect(m_ui->pushButton_LoadGame, &QPushButton::clicked, this, &MainDialogPrivate::LoadGame);
	connect(m_ui->pushButton_GoToSettings, &QPushButton::clicked, this, &MainDialogPrivate::OpenGlobalSettings);
	connect(main_dialog, &MainDialog::Resize, [this]() {
		g_last_geometry = m_main_dialog->saveGeometry();
		m_ui->widget->WriteSettings();
	});

	connect(&m_process,
	        static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
	        [this](int /*exitCode*/, QProcess::ExitStatus /*exitStatus*/) {
		        if (m_running_item != nullptr) {
			        m_running_item->SetRunning(false);
		        }
		        m_running_item = nullptr;
		        Update();
	        });

	// connect(main_dialog, &MainDialog::Quit, [=]() { m_process.Detach(); });

	m_ui->label_settings_file->setText(tr("Settings file: ") + m_ui->widget->GetSettingsFile());

	// KytyPlus: a geometry saved before the minimum size was raised can be too short for
	// the layout, which makes the tree squeeze over the text labels. A geometry saved while
	// the launcher was on a larger display also made the window overflow a smaller panel.
	// Clamp the restored size up to the dialog minimum, then back down to the available
	// screen work area, so the window always fits and never overlaps its own widgets.
	const QSize dlg_min = m_main_dialog->minimumSize();
	if (!g_last_geometry.isEmpty() && m_main_dialog->restoreGeometry(g_last_geometry)) {
		QSize sz = m_main_dialog->size().expandedTo(dlg_min);
		QScreen* scr = m_main_dialog->screen() != nullptr ? m_main_dialog->screen()
		                                                  : QGuiApplication::primaryScreen();
		if (scr != nullptr) {
			const QRect avail = scr->availableGeometry();
			sz.setWidth(qMin(sz.width(), avail.width()));
			sz.setHeight(qMin(sz.height(), avail.height()));
		}
		if (sz != m_main_dialog->size()) {
			m_main_dialog->resize(sz);
		}
	}
	Update();
}

void MainDialogPrivate::FindInterpreter() {
	QDir search_dir(QApplication::applicationDirPath());
	m_interpreter = search_dir.absoluteFilePath(EMULATOR_EXE);

	if (!QFile::exists(m_interpreter)) {
		search_dir.cdUp();
		m_interpreter = search_dir.absoluteFilePath(EMULATOR_EXE);
	}

	bool found = QFile::exists(m_interpreter);

	if (found) {
		m_ui->label_Interpreter->setText(tr("Emulator: ") + m_interpreter);

		QProcess test;
		test.setProgram(m_interpreter);
		test.start();
		test.waitForFinished();

		auto output = QString(test.readAllStandardOutput());
		auto lines  = output.split(QRegularExpression("[\r\n]"), Qt::SkipEmptyParts);

		if (lines.count() >= 2) {
			m_ui->label_Version->setText(
			    tr("Version: ") + (lines.at(0).startsWith("exe_name") ? lines.at(1) : lines.at(0)));
		} else {
			found = false;
		}
	}

	if (!found) {
		QMessageBox::critical(m_main_dialog, tr("Error"), tr("Can't find emulator"));
		QApplication::quit();
		return;
	}

	// Support Explorer's positional path and --open-image <path>.
	QString startup_image;
	const auto startup_args = QCoreApplication::arguments();
	for (int i = 1; i < startup_args.size(); ++i) {
		if (startup_args.at(i) == QStringLiteral("--open-image")) {
			if (i + 1 < startup_args.size()) {
				startup_image = startup_args.at(++i);
			} else {
				QMessageBox::warning(m_main_dialog, tr("Open game image"),
				                     tr("--open-image requires a .ffpfsc file path."));
			}
			break;
		}
		if (startup_args.at(i).endsWith(QStringLiteral(".ffpfsc"), Qt::CaseInsensitive)) {
			startup_image = startup_args.at(i);
			break;
		}
	}
	const bool startup_selected = !startup_image.isEmpty() &&
	                              m_ui->widget->SelectGameImage(startup_image);

	if (!m_ui->widget->EnsureGameDirectory()) {
		QApplication::quit();
		return;
	}

	m_ui->label_settings_file->setText(tr("Settings file: ") + m_ui->widget->GetSettingsFile());

	Update();
	if (startup_selected && m_ui->widget->IsRunEnabled()) {
		Run();
	}
}

static QString BoolArg(bool value) {
	return value ? QStringLiteral("true") : QStringLiteral("false");
}

static QStringList CreateEmulatorArgs(const Configuration& info) {
	QStringList args;
	auto        r = EnumToText(info.screen_resolution).split('x');

	if (r.size() != 2) {
		return {};
	}

	args << "--screen-width" << r.at(0);
	args << "--screen-height" << r.at(1);
	if (info.fullscreen_enabled) {
		args << "--fullscreen";
	}
	args << "--vblank-frequency" << QString::number(info.vblank_frequency);
	args << "--console-language" << QString::number(info.console_language);
	args << "--vulkan-validation" << BoolArg(info.vulkan_validation_enabled);
	args << "--shader-validation" << BoolArg(info.shader_validation_enabled);
	args << "--shader-optimization-type" << EnumToText(info.shader_optimization_type);
	args << "--shader-log-direction" << EnumToText(info.shader_log_direction);
	args << "--shader-log-folder" << info.shader_log_folder;
	args << "--command-buffer-dump" << BoolArg(info.command_buffer_dump_enabled);
	args << "--command-buffer-dump-folder" << info.command_buffer_dump_folder;
	args << "--printf-direction" << EnumToText(info.printf_direction);
	args << "--printf-output-file" << info.printf_output_file;
	args << "--profiler-direction" << EnumToText(info.profiler_direction);
	args << "--upscaler-method" << EnumToText(info.upscaler_method);
	args << "--upscaler-quality" << EnumToText(info.upscaler_quality);
	args << "--upscaler-sharpness" << QString::number(info.upscaler_sharpness, 'f', 2);
	if (info.guest_render_width > 0 && info.guest_render_height > 0) {
		args << "--guest-render-width" << QString::number(info.guest_render_width);
		args << "--guest-render-height" << QString::number(info.guest_render_height);
	}
	if (info.upscaler_method == Configuration::UpscalerMethod::Fsr1 && info.fsr_output_width > 0 &&
		    info.fsr_output_height > 0) {
		args << "--fsr-output-width" << QString::number(info.fsr_output_width);
		args << "--fsr-output-height" << QString::number(info.fsr_output_height);
	}
	args << "--igpu-optimization" << EnumToText(info.igpu_optimization);
	args << "--texture-lod-bias" << QString::number(info.texture_lod_bias);
	args << "--present-mode" << EnumToText(info.present_mode);
	args << "--present-filter" << EnumToText(info.present_filter);
	args << "--aspect-ratio" << EnumToText(info.aspect_ratio);
	args << "--spirv-debug-printf" << "false";
#if defined(_WIN32)
	if (info.red_zone_protection_enabled) {
		args << "--redzone";
	}
#endif
	for (const auto& binding: info.host_input_mapping) {
		args << "--keymap" << binding;
	}
	if (info.renderdoc_enabled) {
		args << "--rd";
	}

	QString game = info.basedir;
	if (!info.elf.isEmpty()) {
		game = QDir(info.basedir).filePath(info.elf);
	}
	args << "--game" << game;

	const auto patch_plan = PatchesDialog::PatchPlanPath(info.title_id);
	if (QFileInfo::exists(patch_plan)) {
		args << "--game-patch" << patch_plan;
	}

	return args;
}

#ifdef __linux__
static QString BashQuote(QString value) {
	value.replace('\'', "'\\''");
	return QStringLiteral("'") + value + QStringLiteral("'");
}

static bool CreateBashScript(const QString& interpreter, const QStringList& args,
                             const QString& file_name) {
	QFile file(file_name);
	if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		QTextStream s(&file);

		s << "#!/bin/bash\n";
		s << BashQuote(interpreter);
		for (const auto& arg: args) {
			s << " " << BashQuote(arg);
		}
		s << "\n";
		s << "echo Press any key...\n";
		s << "read -n1\n";

		file.close();

		return file.setPermissions(file.permissions() | QFile::ExeUser | QFile::ExeOwner |
		                           QFile::ExeGroup);
	}
	return false;
}

// Find a terminal and its command separator.
static bool FindTerminal(QString* program, QStringList* prefix) {
	struct TerminalSpec {
		const char* executable;
		const char* separator; // nullptr when the command follows immediately
	};

	static const TerminalSpec candidates[] = {
	    {"x-terminal-emulator", "-e"},
	    {"gnome-terminal", "--"},
	    {"konsole", "-e"},
	    {"xfce4-terminal", "-x"},
	    {"mate-terminal", "--"},
	    {"tilix", "-e"},
	    {"alacritty", "-e"},
	    {"kitty", nullptr},
	    {"foot", nullptr},
	    {"wezterm", "-e"},
	    {"urxvt", "-e"},
	    {"xterm", "-e"},
	};

	const auto try_candidate = [program, prefix](const QString& executable, const char* separator) {
		const auto resolved = QStandardPaths::findExecutable(executable);
		if (resolved.isEmpty()) {
			return false;
		}
		*program = resolved;
		prefix->clear();
		if (separator != nullptr) {
			*prefix << QString::fromLatin1(separator);
		}
		return true;
	};

	if (const auto from_env = qEnvironmentVariable("TERMINAL"); !from_env.isEmpty()) {
		// Reuse the known separator for an explicit terminal.
		const auto  env_name  = QFileInfo(from_env).fileName();
		const char* separator = "-e";
		for (const auto& candidate: candidates) {
			if (env_name == QLatin1String(candidate.executable)) {
				separator = candidate.separator;
				break;
			}
		}
		if (try_candidate(from_env, separator)) {
			return true;
		}
	}

	for (const auto& candidate: candidates) {
		if (try_candidate(QString::fromLatin1(candidate.executable), candidate.separator)) {
			return true;
		}
	}

	return false;
}
#endif

void MainDialog::RunInterpreter(QProcess* process, const Configuration& info) {
	const auto& interpreter = m_p->GetInterpreter();

	QFileInfo f(interpreter);
	auto      dir = f.absoluteDir();

	auto args = CreateEmulatorArgs(info);
	if (args.isEmpty()) {
		QMessageBox::critical(this, tr("Error"), tr("Invalid emulator configuration"));
		QApplication::quit();
		return;
	}

#ifdef __linux__
	auto bash_file_name = dir.filePath(KYTY_BASH_FILE);
	if (!CreateBashScript(interpreter, args, bash_file_name)) {
		QMessageBox::critical(this, tr("Error"), tr("Can't create file:\n") + bash_file_name);
		QApplication::quit();
		return;
	}

	{
		QString     terminal;
		QStringList terminal_prefix;
		if (FindTerminal(&terminal, &terminal_prefix)) {
			process->setProgram(terminal);
			process->setArguments(terminal_prefix + QStringList {"bash", "-c", bash_file_name});
		} else {
			// Run without a terminal as a fallback.
			process->setProgram(QStringLiteral("bash"));
			process->setArguments({QStringLiteral("-c"), bash_file_name});
		}
	}
#elif defined(_WIN32)
	// KytyPlus: spawn the emulator directly instead of through `cmd /K`. The console
	// host stole the foreground at boot (the game window then lost input focus) and
	// its console buffer swallowed the emulator's stdout, so the launcher could not
	// report early failures. QProcess still gives us errorString()/exit codes, and
	// the game's own log files remain the verbose record.
	process->setProgram(interpreter);
	process->setArguments(args);
#else
	process->setProgram(interpreter);
	process->setArguments(args);
#endif
	process->setWorkingDirectory(dir.path());

	if (info.basedir.endsWith(QStringLiteral(".ffpfsc"), Qt::CaseInsensitive)) {
		auto* progress = new QProgressDialog(tr("Reading compressed image index (no extraction)..."), QString(), 0, 0, this);
		progress->setWindowTitle(tr("Opening game image"));
		progress->setWindowModality(Qt::WindowModal);
		progress->setMinimumDuration(0);
		progress->setAutoClose(false);
		progress->setAutoReset(false);
		progress->setCancelButton(nullptr);
		progress->show();
		auto pending = std::make_shared<QByteArray>();
		auto consume = [this, process, progress, pending]() {
			pending->append(process->readAllStandardOutput());
			int newline = 0;
			while ((newline = pending->indexOf('\n')) >= 0) {
				const QString line = QString::fromUtf8(pending->left(newline)).trimmed();
				pending->remove(0, newline + 1);
				if (line == QStringLiteral("IMAGE_MOUNT_READY")) {
					progress->hide();
				} else if (line.startsWith(QStringLiteral("IMAGE_MOUNT_ERROR="))) {
					progress->hide();
					QMessageBox::warning(this, tr("Image load failed"), line.mid(18));
				} else if (line.startsWith(QStringLiteral("KYTY_PROGRESS="))) {
					const auto fields = line.mid(14).split('/');
					if (fields.size() == 2) {
						bool ok1 = false, ok2 = false;
						const double done = fields[0].toDouble(&ok1), total = fields[1].toDouble(&ok2);
						if (ok1 && ok2 && total > 0 && done >= 0 && done <= total) {
							progress->setRange(0, 100);
							progress->setValue(int(done * 100 / total));
						}
					}
				}
			}
			if (pending->size() > 65536) pending->clear();
		};
		connect(process, &QProcess::readyReadStandardOutput, progress, consume);
		connect(process, &QProcess::readyReadStandardError, progress, [process]() { process->readAllStandardError(); });
		connect(process, &QProcess::finished, progress, [progress, consume](int, QProcess::ExitStatus) { consume(); progress->hide(); progress->deleteLater(); });
		connect(process, &QProcess::errorOccurred, progress, [progress](QProcess::ProcessError) { progress->hide(); progress->deleteLater(); });
	}
	process->start();
	// Windows: also report+log early launch failures (was Linux-only, so silent).
	const bool started_ok = process->waitForStarted(5000);
	{
		QFile dbg(dir.filePath(QStringLiteral("_launcher_debug.txt")));
		if (dbg.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
			QTextStream ts(&dbg);
			ts << "interpreter=" << interpreter << "\n"
			   << "workdir=" << dir.path() << "\n"
			   << "args=" << args.join(QStringLiteral(" | ")) << "\n"
			   << "started=" << (started_ok ? "true" : "false") << "\n"
			   << "error=" << process->errorString() << "\n";
		}
	}
	if (!started_ok) {
		QMessageBox::critical(
		    this, tr("Error"),
		    tr("Failed to start:\n%1\n\n%2").arg(process->program(), process->errorString()));
		return;
	}
	process->waitForFinished(100);
}

void MainDialog::WriteSettings(QSettings& s) {
	MainDialogPrivate::WriteSettings(s);
}

void MainDialog::ReadSettings(QSettings& s) {
	MainDialogPrivate::ReadSettings(s);
}

void MainDialog::resizeEvent(QResizeEvent* event) {
	emit Resize();
	QDialog::resizeEvent(event);
}

void MainDialogPrivate::WriteSettings(QSettings& s) {
	s.beginGroup(SETTINGS_MAIN_DIALOG);

	if (!g_last_geometry.isEmpty()) {
		s.setValue(SETTINGS_MAIN_LAST_GEOMETRY, g_last_geometry);
	}

	s.endGroup();
}

void MainDialogPrivate::ReadSettings(QSettings& s) {
	s.beginGroup(SETTINGS_MAIN_DIALOG);

	g_last_geometry = s.value(SETTINGS_MAIN_LAST_GEOMETRY, g_last_geometry).toByteArray();

	s.endGroup();
}

void MainDialogPrivate::Run() {
	if (m_process.state() != QProcess::NotRunning || !m_ui->widget->IsRunEnabled()) {
		return;
	}
	m_running_item = m_ui->widget->GetSelectedItem();
	if (m_running_item == nullptr) {
		return;
	}

	m_running_item->SetRunning(true);

	Configuration info;
	info.CopyFrom(m_running_item->GetInfo());
	info.host_input_mapping = m_ui->widget->GetHostInputMapping();
	m_main_dialog->RunInterpreter(&m_process, info);

	Update();
}

void MainDialogPrivate::Update() {
	m_ui->widget->SetGameRunning(m_process.state() != QProcess::NotRunning);
	const auto* item = m_ui->widget->GetSelectedItem();

	bool run_enabled = (m_process.state() == QProcess::NotRunning && item != nullptr);

	if (run_enabled) {
		const auto& info = item->GetInfo();
		const QFileInfo source(info.basedir);
		run_enabled = !info.basedir.isEmpty() &&
		              (source.isDir() ||
		               (source.isFile() && source.isReadable() &&
		                source.suffix().compare(QStringLiteral("ffpfsc"), Qt::CaseInsensitive) == 0));
	}

	m_ui->widget->SetRunEnabled(run_enabled);
}

void MainDialogPrivate::RunInstall(const QString& file, const QString& flag) {
	if (file.isEmpty()) return;
	if (m_interpreter.isEmpty() || !QFile::exists(m_interpreter)) {
		QMessageBox::critical(m_main_dialog, tr("Error"), tr("Can't find emulator"));
		return;
	}

	QFileInfo f(m_interpreter);
	auto dir = f.absoluteDir();

	QString install_program = m_interpreter;
	QString pkg_out_dir = QDir(dir.path()).filePath(QStringLiteral("pkg_out/pfs_files"));
	QStringList args;
	QFile package(file);
	const bool image_load = flag == QStringLiteral("--load-ffpfsc");
	const QByteArray magic = package.open(QIODevice::ReadOnly) ? package.read(4) : QByteArray();
	bool encrypted_cnt = false;
	if (magic == QByteArray("\x7f" "CNT", 4) && package.seek(0x410)) {
		const QByteArray offset_bytes = package.read(8);
		if (offset_bytes.size() == 8) {
			const quint64 offset = qFromBigEndian<quint64>(reinterpret_cast<const uchar*>(offset_bytes.constData()));
			if (offset <= quint64(package.size()) && quint64(package.size()) - offset >= 0x1e && package.seek(qint64(offset + 0x1c))) {
				const QByteArray mode = package.read(2);
				encrypted_cnt = mode.size() == 2 && (uchar(mode[0]) & 4) != 0;
			}
		}
	}
	const bool helper_package = image_load || (flag == QStringLiteral("--install-pkg") &&
	                          (magic == QByteArray("\x7f" "FIH", 4) || encrypted_cnt));
	bool encrypted_debug = false;
	if (magic == QByteArray("\x7f" "FIH", 4) && package.seek(0)) {
		const QByteArray fih = package.read(0x100);
		if (fih.size() == 0x100 && fih[5] == '\0') {
			quint64 sb = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(fih.constData() + 0x20));
			if (sb == 0) sb = qFromLittleEndian<quint64>(reinterpret_cast<const uchar*>(fih.constData() + 0x10));
			if (sb <= quint64(package.size()) && quint64(package.size()) - sb >= 0x380 && package.seek(qint64(sb))) {
				const QByteArray superblock = package.read(0x380);
				encrypted_debug = superblock.size() == 0x380 && (uchar(superblock[0x1c]) & 4) != 0 &&
				                  superblock.mid(0x370, 16) != QByteArray("PPRPLAIN-NOAUTH!", 16);
			}
		}
	}
	if (helper_package) {
#ifdef _WIN32
		const QString helper_name = QStringLiteral("naps/kyty_naps_extractor.exe");
#else
		const QString helper_name = QStringLiteral("naps/kyty_naps_extractor");
#endif
		const QString helper = dir.filePath(helper_name);
		if (QFile::exists(helper)) {
			install_program = helper;
			// A fresh directory prevents stale or partial installs being reused.
			pkg_out_dir = QDir(dir.path()).filePath(QStringLiteral("pkg_out/naps-%1/pfs_files")
			                  .arg(QString::number(QDateTime::currentMSecsSinceEpoch())));
			args << file << pkg_out_dir;
			if (image_load) args << QStringLiteral("--image");
			if (encrypted_debug &&
			    QMessageBox::question(m_main_dialog, tr("PS5 debug FPKG"),
			        tr("Use the standard all-zero fake-package passcode? Choose No to select a file containing your package's 32-character passcode.")) == QMessageBox::No) {
				const QString passcode_file = QFileDialog::getOpenFileName(m_main_dialog, tr("Select fake-package passcode file"));
				if (passcode_file.isEmpty()) return;
				args << QStringLiteral("--passcode-file") << passcode_file;
			}
		} else {
			QMessageBox::warning(m_main_dialog, tr("Extractor unavailable"),
			                     tr("The game-image / fake-package helper is missing. Build the naps_extractor target or reinstall the launcher bundle."));
			return;
		}
	} else {
		args << flag << file;
	}

	// Keep helper pipes directly attached on every platform.
	QProcess* process = new QProcess(m_main_dialog);
	process->setProgram(install_program);
	process->setArguments(args);
	process->setWorkingDirectory(dir.path());

	auto* progress = new QProgressDialog(tr("Preparing package and reading its file table..."), QString(), 0, 0, m_main_dialog);
	progress->setWindowTitle(tr("Installing package"));
	progress->setWindowModality(Qt::WindowModal);
	progress->setMinimumDuration(0);
	progress->setAutoClose(false);
	progress->setAutoReset(false);
	progress->setCancelButton(nullptr);
	progress->show();
	auto output = std::make_shared<QByteArray>();
	auto pending = std::make_shared<QByteArray>();
	auto drain = [process, progress, output, pending]() {
		const QByteArray bytes = process->readAllStandardOutput() + process->readAllStandardError();
		output->append(bytes);
		// Bound retained diagnostics; important status markers normally arrive at
		// the end, while a large package can print millions of log lines.
		if (output->size() > 4 * 1024 * 1024) output->remove(0, output->size() - 4 * 1024 * 1024);
		pending->append(bytes);
		int end = 0;
		while ((end = pending->indexOf('\n')) >= 0) {
			const QString line = QString::fromUtf8(pending->left(end)).trimmed();
			pending->remove(0, end + 1);
			if (!line.startsWith(QStringLiteral("KYTY_PROGRESS="))) continue;
			const auto fields = line.mid(14).split('/');
			if (fields.size() != 2) continue;
			bool ok1 = false, ok2 = false;
			const double done = fields[0].toDouble(&ok1), total = fields[1].toDouble(&ok2);
			if (!ok1 || !ok2 || total <= 0 || done < 0 || done > total) continue;
			progress->setRange(0, 100);
			progress->setValue(int(done * 100 / total));
			progress->setLabelText(QObject::tr("Extracting package... %1%\n%2 / %3").arg(int(done * 100 / total)).arg(fields[0], fields[1]));
		}
		if (pending->size() > 65536) pending->clear();
	};
	QObject::connect(process, &QProcess::readyReadStandardOutput, progress, drain);
	QObject::connect(process, &QProcess::readyReadStandardError, progress, drain);
	QObject::connect(process, &QProcess::destroyed, progress, &QObject::deleteLater);

	// After launching the install process, move the extracted files to the
	// games directory and rescan the library, using an async signal so we
	// dont block the GUI thread (which was causing the launcher to freeze).
	if (flag == QStringLiteral("--install-pkg") || image_load) {
		// Emulator extracts to <working_dir>/pkg_out/pfs_files (working dir = emulator dir)
		QStringList game_dirs = m_ui->widget->GetGameDirectories();
		// KytyPlus: name the destination from the PKG's real content id, which the
		// emulator prints as PKG_CONTENT_ID=... Use the filename only as a fallback.
		QString content_id = QFileInfo(file).completeBaseName();

		QObject::connect(process, &QProcess::finished, m_main_dialog,
			[this, process, pkg_out_dir, game_dirs, content_id, dir, image_load, progress, output, drain](int exitCode, QProcess::ExitStatus exitStatus) mutable {
				drain();
				progress->hide();
				const QString out = QString::fromLocal8Bit(*output);
				// Keep extractor diagnostics separate from the game's runtime log.
				QFile install_log(dir.filePath(QStringLiteral("_pkg_install.txt")));
				if (install_log.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
					QTextStream log(&install_log);
					log << "extractor=" << process->program() << '\n'
					    << "arguments=" << process->arguments().join(QStringLiteral(" | ")) << '\n'
					    << "exitCode=" << exitCode << " exitStatus=" << int(exitStatus) << '\n'
					    << out;
				}

				// KytyPlus: never report success when the emulator failed. Previously a
				// stale pkg_out/pfs_files from a previous install was copied and a false
				// "Install complete" was shown. Honour the exit code and the explicit
				// PKG_ERROR_* markers instead.
				if (exitStatus != QProcess::NormalExit || exitCode != 0 ||
				    out.contains(QStringLiteral("PKG_ERROR_ENCRYPTED"))) {
					QString reason = tr("The package could not be installed.");
					if (out.contains(QStringLiteral("PKG_ERROR_ENCRYPTED"))) {
						reason = tr("This package is encrypted, which is not supported.\n\n"
						            "Decrypt it externally first (e.g. with a PKG tool), then "
						            "import the plaintext .pkg \u2014 or extract it and add the "
						            "game folder directly.");
					} else if (out.contains(QStringLiteral("PKG_ERROR_EMPTY"))) {
						reason = tr("The package parsed but produced no files (extraction failed).");
					} else if (exitStatus != QProcess::NormalExit) {
						reason = tr("The package extractor crashed before installation completed.");
					}
					for (const QString& line : out.split(QLatin1Char('\n'))) {
						const QString trimmed = line.trimmed();
						if (trimmed.startsWith(QStringLiteral("PKG_ERROR_REASON="))) {
							reason = trimmed.mid(QStringLiteral("PKG_ERROR_REASON=").size());
							break;
						}
					}
					QMessageBox::warning(m_main_dialog, tr("Install failed"), reason);
					process->deleteLater();
					return;
				}

				// Prefer the content id reported by the emulator.
				for (const QString& line : out.split(QLatin1Char('\n'))) {
					const QString trimmed = line.trimmed();
					if (trimmed.startsWith(QStringLiteral("PKG_CONTENT_ID="))) {
						const QString id = trimmed.mid(QStringLiteral("PKG_CONTENT_ID=").size()).trimmed();
						if (!id.isEmpty()) {
							content_id = id;
						}
					}
				}

				if (!QDir(pkg_out_dir).exists()) {
					QMessageBox::warning(m_main_dialog, tr("Install failed"),
					                     tr("Extraction produced no output to copy."));
					process->deleteLater();
					return;
				}

				// KytyPlus: refuse to copy an empty tree (a stale/blank pkg_out must not
				// masquerade as a successful install).
				const QDir src_check(pkg_out_dir);
				if (src_check.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot).isEmpty()) {
					QMessageBox::warning(m_main_dialog, tr("Install failed"),
					                     tr("Extraction directory is empty; nothing to install."));
					process->deleteLater();
					return;
				}

				if (image_load) {
					if (!QFileInfo(QDir(pkg_out_dir).filePath(QStringLiteral("eboot.bin"))).isFile()) {
						QMessageBox::warning(m_main_dialog, tr("Load failed"), tr("The image does not contain eboot.bin."));
						process->deleteLater();
						return;
					}
					Configuration info;
					info.host_input_mapping = m_ui->widget->GetHostInputMapping();
					info.basedir = pkg_out_dir;
					info.elf = QStringLiteral("eboot.bin");
					m_running_item = nullptr;
					m_main_dialog->RunInterpreter(&m_process, info);
					Update();
					process->deleteLater();
					return;
				}
				// Content IDs and filename fallbacks may not escape the game library.
				content_id.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_.-]")), QStringLiteral("_"));
				if (content_id.isEmpty() || content_id == QStringLiteral(".") || content_id == QStringLiteral(".."))
					content_id = QStringLiteral("imported-game");
				QString games_dir;
				if (game_dirs.isEmpty()) {
					games_dir = QDir(dir.path()).filePath(QStringLiteral("games"));
					QDir().mkpath(games_dir);
					m_ui->widget->AddGameDirectory(games_dir);
				} else {
					games_dir = game_dirs.first();
				}
				if (!games_dir.isEmpty() && QDir(games_dir).exists()) {
					QString dest_dir = QDir(games_dir).filePath(content_id);
					QDir().mkpath(dest_dir);

					progress->setRange(0, 0);
					progress->setLabelText(tr("Copying extracted files into your game library..."));
					progress->show();
					auto* watcher = new QFutureWatcher<int>(m_main_dialog);
					QObject::connect(watcher, &QFutureWatcher<int>::finished, m_main_dialog,
					    [this, watcher, process, progress, dest_dir]() {
						const int moved = watcher->result();
						progress->hide();
						m_ui->widget->ScanGameDirectory();
						if (moved <= 0) QMessageBox::warning(m_main_dialog, tr("Install failed"), tr("No files could be copied to:\n%1").arg(dest_dir));
						else QMessageBox::information(m_main_dialog, tr("Install complete"),
						    tr("Installed %1 file(s) to:\n%2\n\nThe game should now appear in the list.").arg(moved).arg(dest_dir));
						watcher->deleteLater();
						process->deleteLater();
					    });
					watcher->setFuture(QtConcurrent::run([pkg_out_dir, dest_dir]() {
						std::function<int(const QString&, const QString&)> copy;
						copy = [&copy](const QString& srcPath, const QString& dstPath) -> int {
							QDir src(srcPath);
							if (!src.exists() || !QDir().mkpath(dstPath)) return -1;
							int count = 0;
							for (const auto& entry: src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
								if (entry.isSymLink()) return -1;
								const QString target = QDir(dstPath).filePath(entry.fileName());
								if (QFileInfo(target).isSymLink()) return -1;
								if (entry.isDir()) { const int n = copy(entry.absoluteFilePath(), target); if (n < 0) return -1; count += n; }
								else { if (QFile::exists(target) && !QFile::remove(target)) return -1; if (!QFile::copy(entry.absoluteFilePath(), target)) return -1; ++count; }
							}
							return count;
						};
						return copy(pkg_out_dir, dest_dir);
					}));
					return;
				}
				process->deleteLater();
			});
	} else {
		// For non-pkg installs, auto-cleanup the process
		QObject::connect(process, &QProcess::finished, process, &QProcess::deleteLater);
	}
	QObject::connect(process, &QProcess::errorOccurred, m_main_dialog,
	                 [this, process, progress](QProcess::ProcessError error) {
		if (error == QProcess::FailedToStart) {
			progress->hide();
			QMessageBox::critical(m_main_dialog, tr("Install failed"),
			                     tr("Failed to start the package extractor:\n%1").arg(process->errorString()));
			process->deleteLater();
		}
	});
	// Connect first: malformed packages can fail before a late connection.
	process->start();
}

void MainDialogPrivate::LoadGame() {
	if (m_process.state() != QProcess::NotRunning) {
		QMessageBox::information(m_main_dialog, tr("Load Game"),
		                         tr("Close the running game before opening another game."));
		return;
	}
	// Keep compressed game images separate from ELF and folder loading.
	QMessageBox msg(m_main_dialog);
	msg.setWindowTitle(tr("Load Game"));
	msg.setText(tr("What do you want to load?"));
	auto* btn_file   = msg.addButton(tr("Single ELF / eboot..."), QMessageBox::ActionRole);
	auto* btn_folder = msg.addButton(tr("Game folder (eboot.bin inside)..."), QMessageBox::ActionRole);
	auto* btn_image  = msg.addButton(tr("Compressed game image (.ffpfsc)..."), QMessageBox::ActionRole);
	msg.addButton(tr("Cancel"), QMessageBox::RejectRole);
	msg.exec();

	Configuration info;
	info.host_input_mapping = m_ui->widget->GetHostInputMapping();

	if (msg.clickedButton() == btn_file) {
		const QString game = QFileDialog::getOpenFileName(m_main_dialog, tr("Load ELF"), QString(),
		                                                  tr("PS5 ELF (*.elf *.bin);;All Files (*.*)"));
		if (game.isEmpty()) return;
		info.basedir = QFileInfo(game).absoluteDir().absolutePath();
		info.elf     = QFileInfo(game).fileName();
	} else if (msg.clickedButton() == btn_folder) {
		const QString dir = QFileDialog::getExistingDirectory(m_main_dialog, tr("Select game folder"));
		if (dir.isEmpty()) return;
		info.basedir = dir;
		info.elf     = QStringLiteral("eboot.bin");
	} else if (msg.clickedButton() == btn_image) {
		const QString image = QFileDialog::getOpenFileName(m_main_dialog, tr("Load compressed game image"), QString(),
		                                                 tr("Compressed game image (*.ffpfsc);;All Files (*.*)"));
		if (image.isEmpty()) return;
		if (m_ui->widget->SelectGameImage(image)) {
			Update();
			Run();
		}
		return;
	} else {
		return;
	}

	m_running_item = nullptr;
	m_main_dialog->RunInterpreter(&m_process, info);
	Update();
}

void MainDialogPrivate::InstallPkg() {
	const QString file = QFileDialog::getOpenFileName(m_main_dialog, tr("Install Package"), QString(), tr("PS4/PS5 Package (*.pkg);;All Files (*.*)"));
	RunInstall(file, QStringLiteral("--install-pkg"));
}

void MainDialogPrivate::OpenGlobalSettings() {
	m_ui->widget->edit_global_settings();
}

#include "mainDialog.moc"
