#ifndef LOG_VIEWER_DIALOG_H
#define LOG_VIEWER_DIALOG_H

#include "common.h"

#include <QDialog>

class Configuration;
class QPlainTextEdit;

class LogViewerDialog: public QDialog {
	KYTY_QT_CLASS_NO_COPY(LogViewerDialog);

public:
	explicit LogViewerDialog(QWidget* parent = nullptr);

	static void ShowForGame(const Configuration* info, QWidget* parent);
	static void ShowGlobal(QWidget* parent);

private:
	void LoadLog(const QString& path);

	QPlainTextEdit* m_log = nullptr;
};

#endif // LOG_VIEWER_DIALOG_H