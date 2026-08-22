#ifndef CONTROLLER_MAPPING_PANEL_H
#define CONTROLLER_MAPPING_PANEL_H

#include "common.h"

#include <QKeyEvent>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>

class QGridLayout;

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
	struct ControlRow {
		QString      control_name; // e.g. "Cross", "LeftStickUp"
		QPushButton* bind_button = nullptr;
	};

	void BuildLayout();
	void SetCaptureMode(ControlRow& row);

	void keyPressEvent(QKeyEvent* event) override;

	QList<ControlRow> m_rows;
	QGridLayout*      m_grid = nullptr;
	ControlRow*        m_capturing = nullptr;
};

#endif // CONTROLLER_MAPPING_PANEL_H