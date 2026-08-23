#ifndef CONTROLLER_MAPPING_PANEL_H
#define CONTROLLER_MAPPING_PANEL_H

#include "common.h"

#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QWidget>

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

	QTableWidget* m_table      = nullptr;
	int           m_capturing  = -1; // row index currently listening, -1 = none
	QStringList   m_key_names;       // internal keymap names per row
};

#endif // CONTROLLER_MAPPING_PANEL_H