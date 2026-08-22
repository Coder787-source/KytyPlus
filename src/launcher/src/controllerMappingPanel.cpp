#include "controllerMappingPanel.h"

#include <QGridLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>

// Mirrors the ControlInfo table in hostInput.cpp.
// Use readable display names alongside the internal keymap names.
struct ControlEntry {
	QString display_name; // What the user sees
	QString keymap_name;  // Internal name for the keymap string
};

static const QList<ControlEntry> CONTROLS = {
    // Face buttons
    {"Cross", "Cross"},
    {"Circle", "Circle"},
    {"Square", "Square"},
    {"Triangle", "Triangle"},
    // D-pad
    {"D-Pad Up", "Up"},
    {"D-Pad Down", "Down"},
    {"D-Pad Left", "Left"},
    {"D-Pad Right", "Right"},
    // Shoulders
    {"L1", "L1"},
    {"R1", "R1"},
    {"L2", "L2"},
    {"R2", "R2"},
    // Sticks (click)
    {"L3 (Left Stick Click)", "L3"},
    {"R3 (Right Stick Click)", "R3"},
    // Center buttons
    {"Options", "Options"},
    {"TouchPad", "TouchPad"},
    // Left stick axes
    {"Left Stick Up", "LeftStickUp"},
    {"Left Stick Down", "LeftStickDown"},
    {"Left Stick Left", "LeftStickLeft"},
    {"Left Stick Right", "LeftStickRight"},
    // Right stick axes
    {"Right Stick Up", "RightStickUp"},
    {"Right Stick Down", "RightStickDown"},
    {"Right Stick Left", "RightStickLeft"},
    {"Right Stick Right", "RightStickRight"},
};

// SDL key name → human-readable "Key: X" text.
static QString KeyToLabel(const QString& key_name) {
	if (key_name.isEmpty()) return QStringLiteral("Unbound");
	return QStringLiteral("Key: %1").arg(key_name);
}

// SDL gamepad button name → human-readable "Pad: X" text.
static QString PadToLabel(const QString& btn_name) {
	if (btn_name.isEmpty()) return QString();
	return QStringLiteral("Pad: %1").arg(btn_name);
}

// Raw keymap entry → display label.
// Accepted forms: "Cross=Space", "Cross=Pad:A", "Cross=Mouse:Left"
static QString MappingToLabel(const QString& raw) {
	const auto eq = raw.indexOf('=');
	if (eq < 0) return raw;
	const auto value = raw.mid(eq + 1).trimmed();
	if (value.startsWith("Pad:"))
		return PadToLabel(value.mid(4));
	if (value.startsWith("Mouse:"))
		return QStringLiteral("Mouse: %1").arg(value.mid(6));
	return KeyToLabel(value);
}

// Index into CONTROLS by keymap name.
static int ControlIndex(const QString& name) {
	for (int i = 0; i < CONTROLS.size(); ++i) {
		if (CONTROLS[i].keymap_name == name) return i;
	}
	return -1;
}

ControllerMappingPanel::ControllerMappingPanel(QWidget* parent)
    : QWidget(parent) {
	BuildLayout();
}

void ControllerMappingPanel::BuildLayout() {
	m_grid = new QGridLayout(this);
	m_grid->setContentsMargins(8, 8, 8, 8);
	m_grid->setSpacing(4);
	setLayout(m_grid);

	// Column headers
	m_grid->addWidget(new QLabel(tr("Control"), this), 0, 0);
	m_grid->addWidget(new QLabel(tr("Binding"), this), 0, 1);

	m_rows.reserve(CONTROLS.size());
	for (int i = 0; i < CONTROLS.size(); ++i) {
		ControlRow row;
		row.control_name = CONTROLS[i].keymap_name;
		row.bind_button  = new QPushButton(tr("Unbound"), this);
		row.bind_button->setMinimumWidth(180);
		// Capture on click: enter listening mode for next key/button.
		connect(row.bind_button, &QPushButton::clicked, this, [this, idx = i]() {
			if (idx >= 0 && idx < m_rows.size()) SetCaptureMode(m_rows[idx]);
		});
		m_grid->addWidget(new QLabel(CONTROLS[i].display_name, this), i + 1, 0);
		m_grid->addWidget(row.bind_button, i + 1, 1);
		m_rows.append(row);
	}
}

void ControllerMappingPanel::SetCaptureMode(ControlRow& row) {
	if (m_capturing != nullptr) {
		// Cancel previous capture.
		m_capturing->bind_button->setStyleSheet(QString());
	}
	m_capturing            = &row;
	row.bind_button->setText(tr("... press key or button ..."));
	row.bind_button->setStyleSheet(QStringLiteral("background-color: #ddeeff;"));
	setFocus(); // grab keyboard focus so we receive key events
}

QStringList ControllerMappingPanel::GetMappings() const {
	QStringList result;
	for (const auto& row: m_rows) {
		const QString text = row.bind_button->text();
		if (text.isEmpty() || text == tr("Unbound") || text == tr("... press key or button ..."))
			continue;
		// Extract the raw keymap value from the display text.
		if (text.startsWith("Pad: ")) {
			result.append(QStringLiteral("%1=Pad:%2").arg(row.control_name, text.mid(5)));
		} else if (text.startsWith("Mouse: ")) {
			result.append(QStringLiteral("%1=Mouse:%2").arg(row.control_name, text.mid(7)));
		} else if (text.startsWith("Key: ")) {
			result.append(QStringLiteral("%1=%2").arg(row.control_name, text.mid(5)));
		}
	}
	return result;
}

void ControllerMappingPanel::SetMappings(const QStringList& list) {
	// Reset all bindings.
	for (auto& row: m_rows) {
		row.bind_button->setText(tr("Unbound"));
	}

	for (const auto& entry: list) {
		const auto eq = entry.indexOf('=');
		if (eq < 0) continue;
		const auto name  = entry.left(eq).trimmed();
		const int  index = ControlIndex(name);
		if (index < 0 || index >= m_rows.size()) continue;
		m_rows[index].bind_button->setText(MappingToLabel(entry));
	}
}

// Override keyPressEvent to capture keyboard input during rebind.
void ControllerMappingPanel::keyPressEvent(QKeyEvent* event) {
	if (m_capturing == nullptr) {
		QWidget::keyPressEvent(event);
		return;
	}

	// Ignore Escape and other modifiers-alone — not mappable.
	if (event->key() == Qt::Key_Escape) {
		m_capturing->bind_button->setStyleSheet(QString());
		m_capturing->bind_button->setText(tr("Unbound"));
		m_capturing = nullptr;
		return;
	}

	// Build an SDL-style key name.
	int sdl_keycode = 0;
	switch (event->key()) {
		case Qt::Key_Up: sdl_keycode = 1073741906; break;       // SDLK_UP
		case Qt::Key_Down: sdl_keycode = 1073741905; break;     // SDLK_DOWN
		case Qt::Key_Left: sdl_keycode = 1073741904; break;     // SDLK_LEFT
		case Qt::Key_Right: sdl_keycode = 1073741903; break;    // SDLK_RIGHT
		case Qt::Key_Return: sdl_keycode = 13; break;
		case Qt::Key_Enter: sdl_keycode = 13; break;
		case Qt::Key_Space: sdl_keycode = 32; break;
		case Qt::Key_Tab: sdl_keycode = 9; break;
		case Qt::Key_Backspace: sdl_keycode = 8; break;
		case Qt::Key_Shift: sdl_keycode = 1073742049; break;    // SDLK_LSHIFT
		case Qt::Key_Control: sdl_keycode = 1073742048; break;  // SDLK_LCTRL
		case Qt::Key_Alt: sdl_keycode = 1073742050; break;      // SDLK_LALT
		case Qt::Key_Escape: break;
		default: {
			// For regular letters/numbers, use the text.
			if (!event->text().isEmpty()) {
				QString name = event->text().toUpper();
				m_capturing->bind_button->setStyleSheet(QString());
				m_capturing->bind_button->setText(KeyToLabel(name));
				m_capturing = nullptr;
				return;
			}
			QWidget::keyPressEvent(event);
			return;
		}
	}

	if (sdl_keycode != 0) {
		m_capturing->bind_button->setStyleSheet(QString());
		// Common SDL key names for non-alpha keys.
		switch (sdl_keycode) {
			case 1073741906: m_capturing->bind_button->setText(KeyToLabel("Up")); break;
			case 1073741905: m_capturing->bind_button->setText(KeyToLabel("Down")); break;
			case 1073741904: m_capturing->bind_button->setText(KeyToLabel("Left")); break;
			case 1073741903: m_capturing->bind_button->setText(KeyToLabel("Right")); break;
			case 13: m_capturing->bind_button->setText(KeyToLabel("Return")); break;
			case 32: m_capturing->bind_button->setText(KeyToLabel("Space")); break;
			case 9: m_capturing->bind_button->setText(KeyToLabel("Tab")); break;
			case 8: m_capturing->bind_button->setText(KeyToLabel("Backspace")); break;
			case 1073742049: m_capturing->bind_button->setText(KeyToLabel("Left Shift")); break;
			case 1073742048: m_capturing->bind_button->setText(KeyToLabel("Left Ctrl")); break;
			case 1073742050: m_capturing->bind_button->setText(KeyToLabel("Left Alt")); break;
			default: break;
		}
		m_capturing = nullptr;
		return;
	}

	QWidget::keyPressEvent(event);
}