#include "controllerMappingPanel.h"

#include <QHeaderView>
#include <QKeyEvent>
#include <QTableWidgetItem>
#include <QTimer>
#include <QVBoxLayout>

#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

// Mirrors the ControlInfo table in hostInput.cpp.
struct ControlEntry {
	QString display_name;
	QString keymap_name;
};

static const QList<ControlEntry> CONTROLS = {
    {"Cross", "Cross"},
    {"Circle", "Circle"},
    {"Square", "Square"},
    {"Triangle", "Triangle"},
    {"D-Pad Up", "Up"},
    {"D-Pad Down", "Down"},
    {"D-Pad Left", "Left"},
    {"D-Pad Right", "Right"},
    {"L1", "L1"},
    {"R1", "R1"},
    {"L2", "L2"},
    {"R2", "R2"},
    {"L3 (Left Stick Click)", "L3"},
    {"R3 (Right Stick Click)", "R3"},
    {"Options", "Options"},
    {"TouchPad", "TouchPad"},
    {"Left Stick Up", "LeftStickUp"},
    {"Left Stick Down", "LeftStickDown"},
    {"Left Stick Left", "LeftStickLeft"},
    {"Left Stick Right", "LeftStickRight"},
    {"Right Stick Up", "RightStickUp"},
    {"Right Stick Down", "RightStickDown"},
    {"Right Stick Left", "RightStickLeft"},
    {"Right Stick Right", "RightStickRight"},
};

static QString KeyToLabel(const QString& key_name) {
	if (key_name.isEmpty()) return QStringLiteral("Unbound");
	return QStringLiteral("Key: %1").arg(key_name);
}

static QString MappingToLabel(const QString& raw) {
	const auto eq = raw.indexOf('=');
	if (eq < 0) return raw;
	const auto value = raw.mid(eq + 1).trimmed();
	if (value.startsWith("Pad:"))
		return QStringLiteral("Pad: %1").arg(value.mid(4));
	if (value.startsWith("Mouse:"))
		return QStringLiteral("Mouse: %1").arg(value.mid(6));
	return KeyToLabel(value);
}

static int ControlIndex(const QString& name) {
	for (int i = 0; i < CONTROLS.size(); ++i) {
		if (CONTROLS[i].keymap_name == name) return i;
	}
	return -1;
}

ControllerMappingPanel::ControllerMappingPanel(QWidget* parent)
    : QWidget(parent) {
	BuildTable();

	// Poll for gamepad button presses during capture mode.
	m_poll_timer = new QTimer(this);
	m_poll_timer->setInterval(16); // ~60 Hz
	connect(m_poll_timer, &QTimer::timeout, this, &ControllerMappingPanel::PollGamepad);
}

void ControllerMappingPanel::BuildTable() {
	m_table = new QTableWidget(CONTROLS.size(), 2, this);
	m_table->setHorizontalHeaderLabels({tr("Control"), tr("Binding")});
	m_table->horizontalHeader()->setStretchLastSection(true);
	m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	m_table->verticalHeader()->hide();
	m_table->setSelectionMode(QAbstractItemView::NoSelection);
	m_table->setFocusPolicy(Qt::StrongFocus);
	m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	m_table->setMinimumHeight(200);

	for (int i = 0; i < CONTROLS.size(); ++i) {
		m_key_names.append(CONTROLS[i].keymap_name);

		auto* name_item = new QTableWidgetItem(CONTROLS[i].display_name);
		name_item->setFlags(Qt::ItemIsEnabled);
		m_table->setItem(i, 0, name_item);

		auto* bind_item = new QTableWidgetItem(tr("Unbound"));
		bind_item->setFlags(Qt::ItemIsEnabled);
		m_table->setItem(i, 1, bind_item);
	}

	// Click a binding cell to start capture.
	connect(m_table, &QTableWidget::cellClicked, this,
	        [this](int row, int /*col*/) {
		        if (row < 0 || row >= m_key_names.size()) return;
		        // Cancel previous capture.
		        if (m_capturing >= 0) {
			        m_table->item(m_capturing, 1)->setBackground(Qt::white);
		        }
		        m_capturing                        = row;
		        m_prev_buttons                     = 0;
		        m_table->item(row, 1)->setText(tr("... press key or button ..."));
		        m_table->item(row, 1)->setBackground(QColor(0xDD, 0xEE, 0xFF));
		        m_table->setFocus();
		        m_poll_timer->start();
	        });

	// Fill remaining space.
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->addWidget(m_table);
}

QStringList ControllerMappingPanel::GetMappings() const {
	QStringList result;
	for (int i = 0; i < m_key_names.size(); ++i) {
		const auto* item = m_table->item(i, 1);
		if (item == nullptr) continue;
		const QString text = item->text();
		if (text.isEmpty() || text == tr("Unbound") ||
		    text == tr("... press key or button ..."))
			continue;
		if (text.startsWith("Pad: ")) {
			result.append(QStringLiteral("%1=Pad:%2").arg(m_key_names[i], text.mid(5)));
		} else if (text.startsWith("Mouse: ")) {
			result.append(QStringLiteral("%1=Mouse:%2").arg(m_key_names[i], text.mid(7)));
		} else if (text.startsWith("Key: ")) {
			result.append(QStringLiteral("%1=%2").arg(m_key_names[i], text.mid(5)));
		}
	}
	return result;
}

void ControllerMappingPanel::SetMappings(const QStringList& list) {
	// Reset all.
	for (int i = 0; i < m_key_names.size(); ++i) {
		m_table->item(i, 1)->setText(tr("Unbound"));
	}

	for (const auto& entry: list) {
		const auto eq = entry.indexOf('=');
		if (eq < 0) continue;
		const auto name  = entry.left(eq).trimmed();
		const int  index = ControlIndex(name);
		if (index < 0 || index >= m_key_names.size()) continue;
		m_table->item(index, 1)->setText(MappingToLabel(entry));
	}
}

void ControllerMappingPanel::keyPressEvent(QKeyEvent* event) {
	if (m_capturing < 0) {
		QWidget::keyPressEvent(event);
		return;
	}

	if (event->key() == Qt::Key_Escape) {
		m_table->item(m_capturing, 1)->setBackground(Qt::white);
		m_table->item(m_capturing, 1)->setText(tr("Unbound"));
		m_capturing = -1;
		return;
	}

	// Build SDL-style key name for special keys.
	QString key_name;
	switch (event->key()) {
		case Qt::Key_Up:    key_name = QStringLiteral("Up"); break;
		case Qt::Key_Down:  key_name = QStringLiteral("Down"); break;
		case Qt::Key_Left:  key_name = QStringLiteral("Left"); break;
		case Qt::Key_Right: key_name = QStringLiteral("Right"); break;
		case Qt::Key_Return:
		case Qt::Key_Enter: key_name = QStringLiteral("Return"); break;
		case Qt::Key_Space: key_name = QStringLiteral("Space"); break;
		case Qt::Key_Tab:   key_name = QStringLiteral("Tab"); break;
		case Qt::Key_Backspace: key_name = QStringLiteral("Backspace"); break;
		case Qt::Key_Shift:   key_name = QStringLiteral("Left Shift"); break;
		case Qt::Key_Control: key_name = QStringLiteral("Left Ctrl"); break;
		case Qt::Key_Alt:     key_name = QStringLiteral("Left Alt"); break;
		case Qt::Key_Escape: break;
		default:
			if (!event->text().isEmpty()) {
				key_name = event->text().toUpper();
			}
			break;
	}

	if (!key_name.isEmpty()) {
		CaptureInput(KeyToLabel(key_name));
		return;
	}

	QWidget::keyPressEvent(event);
}

void ControllerMappingPanel::CaptureInput(const QString& label) {
	if (m_capturing < 0) return;
	m_poll_timer->stop();
	m_table->item(m_capturing, 1)->setBackground(Qt::white);
	m_table->item(m_capturing, 1)->setText(label);
	m_capturing = -1;
}

void ControllerMappingPanel::PollGamepad() {
#ifdef _WIN32
	if (m_capturing < 0) {
		m_poll_timer->stop();
		return;
	}

	JOYINFOEX info = {};
	info.dwSize  = sizeof(info);
	info.dwFlags = JOY_RETURNBUTTONS | JOY_RETURNPOV | JOY_RETURNX | JOY_RETURNY | JOY_RETURNZ | JOY_RETURNR | JOY_RETURNU | JOY_RETURNV;

	// Try joystick 0 first.
	if (joyGetPosEx(JOYSTICKID1, &info) == JOYERR_NOERROR) {
		// --- Digital buttons ---
		uint32_t changed = info.dwButtons ^ m_prev_buttons;
		uint32_t pressed = info.dwButtons & changed;
		m_prev_buttons  = info.dwButtons;

		if (pressed != 0) {
			int btn = 0;
			uint32_t mask = pressed;
			while ((mask & 1) == 0 && btn < 32) {
				mask >>= 1;
				btn++;
			}
			CaptureInput(QStringLiteral("Pad: Button%1").arg(btn + 1));
			return;
		}

		// --- D-pad (POV hat) ---
		const DWORD pov = info.dwPOV;
		if (pov != m_prev_pov && pov != JOY_POVCENTERED) {
			m_prev_pov = pov;
			// POV is in hundredths of a degree: 0=up, 9000=right, 18000=down, 27000=left
			QString dir;
			if (pov < 4500 || pov > 31500)      dir = QStringLiteral("DPadUp");
			else if (pov >= 4500 && pov < 13500) dir = QStringLiteral("DPadRight");
			else if (pov >= 13500 && pov < 22500) dir = QStringLiteral("DPadDown");
			else                                 dir = QStringLiteral("DPadLeft");
			CaptureInput(QStringLiteral("Pad: %1").arg(dir));
			return;
		}
		m_prev_pov = pov;

		// --- Analog sticks and triggers ---
		// Axis order varies by controller. X/Y=left stick on most pads.
		// Z/R or U/V = right stick, depending on the driver.
		// We check all four candidate right-stick axis pairs.
		struct AxisCheck { DWORD val; DWORD prev; const char* name; };
		AxisCheck axes[] = {
		    {info.dwXpos, m_prev_axes[0], "LeftStick"},
		    {info.dwYpos, m_prev_axes[1], "LeftStick"},
		    {info.dwZpos, m_prev_axes[2], "RightStick"},
		    {info.dwRpos, m_prev_axes[3], "RightStick"},
		    {info.dwUpos, m_prev_axes[4], "RightStick"},
		    {info.dwVpos, m_prev_axes[5], "RightStick"},
		};

		const DWORD CENTER  = 32767;
		const DWORD DEADZONE = 12000;  // ~37% of range

		for (int i = 0; i < 6; i++) {
			DWORD diff = (axes[i].val > CENTER) ? (axes[i].val - CENTER) : (CENTER - axes[i].val);
			if (diff > DEADZONE && m_prev_axes[i] <= DEADZONE) {
				m_prev_axes[i] = diff;
				QString dir;
				// Even axis indices = X (Left/Right), odd = Y (Up/Down)
				if ((i % 2) == 0) {
					dir = (axes[i].val > CENTER) ? QStringLiteral("Right") : QStringLiteral("Left");
				} else {
					dir = (axes[i].val > CENTER) ? QStringLiteral("Down")  : QStringLiteral("Up");
				}
				CaptureInput(QStringLiteral("Pad: %1%2").arg(axes[i].name, dir));
				return;
			}
			if (diff <= DEADZONE) m_prev_axes[i] = diff;
		}

		for (int i = 0; i < 6; i++) {
			DWORD diff = (axes[i].val > CENTER) ? (axes[i].val - CENTER) : (CENTER - axes[i].val);
			if (diff > DEADZONE) m_prev_axes[i] = diff;
		}
	}
#else
	(void)this;
#endif
}