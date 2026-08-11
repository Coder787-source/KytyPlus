#include "logViewerDialog.h"

#include "configuration.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

// Resolve the directory the emulator runs from (where _kyty.txt is written).
QString EmulatorDir(QWidget* parent) {
	auto* main = qobject_cast<QWidget*>(parent);
	QString dir = QApplication::applicationDirPath();

	// The emulator typically lives next to the launcher, or one level up.
	if (!QFile::exists(dir + QDir::separator() + "kyty_emulator") &&
	    !QFile::exists(dir + QDir::separator() + "kyty_emulator.exe")) {
		QDir up(dir);
		if (up.cdUp()) {
			dir = up.absolutePath();
		}
	}

	Q_UNUSED(main);
	return dir;
}

// Collect host system info to bundle with a shared log (no PII).
QString CollectSystemInfo() {
	QString info;
	info += QStringLiteral("== KytyPlus bug report ==\n");

#if defined(_WIN32)
	OSVERSIONINFOEXW osvi{};
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (GetVersionExW(reinterpret_cast<OSVERSIONINFOW*>(&osvi))) {
		info += QStringLiteral("OS: Windows %1.%2 build %3\n")
		            .arg(osvi.dwMajorVersion)
		            .arg(osvi.dwMinorVersion)
		            .arg(osvi.dwBuildNumber);
	} else {
		info += QStringLiteral("OS: Windows (unknown version)\n");
	}
#else
	info += QStringLiteral("OS: Unix-like\n");
#endif

	info += QStringLiteral("Qt: %1\n").arg(QStringLiteral(QT_VERSION_STR));
	info += QStringLiteral("App dir: %1\n").arg(QApplication::applicationDirPath());
	info += QStringLiteral("\n== Log below ==\n");
	return info;
}

} // namespace

LogViewerDialog::LogViewerDialog(QWidget* parent): QDialog(parent) {
	setWindowTitle(tr("Log Viewer"));
	resize(720, 520);

	auto* layout = new QVBoxLayout(this);

	auto* hint = new QLabel(
	    tr("Emulator log (<b>_kyty.txt</b>). Use <b>Share Log</b> to copy it to your clipboard, "
	       "then paste it into a GitHub issue."),
	    this);
	hint->setWordWrap(true);
	layout->addWidget(hint);

	m_log = new QPlainTextEdit(this);
	m_log->setReadOnly(true);
	m_log->setLineWrapMode(QPlainTextEdit::NoWrap);
	m_log->setFont(QFont(QStringLiteral("Consolas"), 9));
	layout->addWidget(m_log);

	auto* buttons = new QDialogButtonBox(Qt::Horizontal, this);

	auto* share = new QPushButton(tr("Share Log"), buttons);
	share->setToolTip(tr("Copy the log and your system info to the clipboard so you can paste "
	                     "it into a GitHub issue."));
	buttons->addButton(share, QDialogButtonBox::ActionRole);

	auto* open_folder = new QPushButton(tr("Open Log Folder"), buttons);
	open_folder->setToolTip(tr("Open the folder that contains the log file."));
	buttons->addButton(open_folder, QDialogButtonBox::ActionRole);

	buttons->addButton(QDialogButtonBox::Close);
	layout->addWidget(buttons);

	connect(share, &QPushButton::clicked, this, [this]() {
		auto text = m_log->toPlainText();
		if (text.isEmpty()) {
			QMessageBox::information(this, tr("Share Log"), tr("The log is empty — nothing to share."));
			return;
		}
		QGuiApplication::clipboard()->setText(CollectSystemInfo() + text);
		QMessageBox::information(
		    this, tr("Share Log"),
		    tr("Log copied to clipboard.\n\nPlease paste it into a GitHub issue at:\n"
		       "https://github.com/Coder787-source/KytyPlus/issues\n\n"
		       "Include the game title and what you observed (boot, menu, in-game, crash)."));
	});

	connect(open_folder, &QPushButton::clicked, this, [this]() {
		const auto path = property("logDir").toString();
		if (path.isEmpty()) {
			return;
		}
		QDir dir(path);
		if (!dir.exists()) {
			QMessageBox::warning(this, tr("Open Log Folder"), tr("Folder does not exist:\n%1").arg(path));
			return;
		}
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir.absolutePath()));
	});

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void LogViewerDialog::LoadLog(const QString& path) {
	const QFileInfo fi(path);
	setProperty("logDir", fi.absolutePath());

	QFile file(path);
	if (!file.exists()) {
		m_log->setPlainText(tr("No log file found at:\n%1\n\n"
		                       "Run a game first — the emulator writes _kyty.txt when it starts.")
		                        .arg(path));
		return;
	}
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		m_log->setPlainText(tr("Could not open log file:\n%1").arg(path));
		return;
	}
	QTextStream s(&file);
	s.setEncoding(QStringConverter::Utf8);
	const auto content = s.readAll();
	file.close();

	if (content.isEmpty()) {
		m_log->setPlainText(tr("Log file is empty:\n%1\n\n"
		                       "Run a game first — the emulator writes here when it starts.")
		                        .arg(path));
	} else {
		m_log->setPlainText(content);
	}
}

void LogViewerDialog::ShowForGame(const Configuration* info, QWidget* parent) {
	LogViewerDialog dlg(parent);

	QString dir = EmulatorDir(parent);
	if (info != nullptr && !info->basedir.isEmpty()) {
		dir = info->basedir;
	}

	QString log_name = (info != nullptr && !info->printf_output_file.isEmpty())
	                       ? info->printf_output_file
	                       : QStringLiteral("_kyty.txt");

	dlg.LoadLog(QDir(dir).filePath(log_name));
	dlg.exec();
}

void LogViewerDialog::ShowGlobal(QWidget* parent) {
	ShowForGame(nullptr, parent);
}