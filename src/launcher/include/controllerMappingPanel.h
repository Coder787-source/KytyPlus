#ifndef CONTROLLER_MAPPING_PANEL_H
#define CONTROLLER_MAPPING_PANEL_H

#include "common.h"

#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QWidget>

class QTimer;

// Editable table: each PS5 control gets a bindable cell.
// Click a row to capture the next keyboard key, mouse button, or gamepad button press.
class ControllerMappingPanel: public QWidget {
	Q_OBJECT
	KYTY_QT_CLASS_NO_COPY(ControllerMappingPanel);

public:
	explicit ControllerMappingPanel(QWidget* parent = nullptr);

	// Get/set the mapping list in the emulator's keymap format.
	// Each entry is like "Cross=Pad:A" or "Cross=Key:Space" or "Cross=Mouse:Left".
	[[nodiscard]] QStringList GetMappings() const;
	void                     SetMappings(const QStringList& list);

private:
	void BuildTable();
	void keyPressEvent(QKeyEvent* event) override;
	void CaptureInput(const QString& label);
	void PollGamepad();

	QTableWidget* m_table        = nullptr;
	QTimer*       m_poll_timer   = nullptr;
	int           m_capturing    = -1;
	uint32_t      m_prev_buttons = 0;
	QStringList   m_key_names;
};

#endif // CONTROLLER_MAPPING_PANEL_H