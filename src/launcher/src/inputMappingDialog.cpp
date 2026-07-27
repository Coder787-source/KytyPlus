#include "inputMappingDialog.h"

#include "configuration.h"
#include "keyCaptureLineEdit.h"

#include <QByteArray>
#include <QComboBox>
#include <QHeaderView>
#include <QLayout>
#include <QMoveEvent>
#include <QPushButton>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVariant>

#include <cstdint>
#include <utility>
#include <vector>

#include "ui_input_mapping_dialog.h"

constexpr char SETTINGS_INPUT_MAPPING_DIALOG[]        = "InputMappingDialog";
constexpr char SETTINGS_INPUT_MAPPING_LAST_GEOMETRY[] = "geometry";

namespace {

// PAD_BUTTON_* values duplicated from Libs::Controller (src/libs/controller.h). Kept as plain
// constants here (rather than including controller.h) because the launcher doesn't otherwise
// depend on the emulator's core headers/build graph -- it only needs to speak the shared
// "host_code:pad_button" text format, not link against the code that defines these.
constexpr uint32_t PAD_BUTTON_L3        = 0x00000002;
constexpr uint32_t PAD_BUTTON_R3        = 0x00000004;
constexpr uint32_t PAD_BUTTON_OPTIONS   = 0x00000008;
constexpr uint32_t PAD_BUTTON_UP        = 0x00000010;
constexpr uint32_t PAD_BUTTON_RIGHT     = 0x00000020;
constexpr uint32_t PAD_BUTTON_DOWN      = 0x00000040;
constexpr uint32_t PAD_BUTTON_LEFT      = 0x00000080;
constexpr uint32_t PAD_BUTTON_L2        = 0x00000100;
constexpr uint32_t PAD_BUTTON_R2        = 0x00000200;
constexpr uint32_t PAD_BUTTON_L1        = 0x00000400;
constexpr uint32_t PAD_BUTTON_R1        = 0x00000800;
constexpr uint32_t PAD_BUTTON_TRIANGLE  = 0x00001000;
constexpr uint32_t PAD_BUTTON_CIRCLE    = 0x00002000;
constexpr uint32_t PAD_BUTTON_CROSS     = 0x00004000;
constexpr uint32_t PAD_BUTTON_SQUARE    = 0x00008000;
constexpr uint32_t PAD_BUTTON_TOUCH_PAD = 0x00100000;

struct PadButtonRow {
	uint32_t pad_button;
	QString  display_name;
};

// One row per bindable PS5 button, in the order shown in both tabs. L2/R2 are included even
// though the real DualSense reports them as analog triggers (see PadData in controller.h) --
// window.cpp's synthetic keyboard/controller-button remap path (what this dialog configures)
// only ever produces a digital press/release for them, same as every other bound button here.
const std::vector<PadButtonRow>& PadButtonRows() {
	static const std::vector<PadButtonRow> rows = {
	    {PAD_BUTTON_UP, QObject::tr("D-Pad Up")},
	    {PAD_BUTTON_DOWN, QObject::tr("D-Pad Down")},
	    {PAD_BUTTON_LEFT, QObject::tr("D-Pad Left")},
	    {PAD_BUTTON_RIGHT, QObject::tr("D-Pad Right")},
	    {PAD_BUTTON_TRIANGLE, QObject::tr("Triangle")},
	    {PAD_BUTTON_CIRCLE, QObject::tr("Circle")},
	    {PAD_BUTTON_CROSS, QObject::tr("Cross")},
	    {PAD_BUTTON_SQUARE, QObject::tr("Square")},
	    {PAD_BUTTON_L1, QObject::tr("L1")},
	    {PAD_BUTTON_R1, QObject::tr("R1")},
	    {PAD_BUTTON_L2, QObject::tr("L2")},
	    {PAD_BUTTON_R2, QObject::tr("R2")},
	    {PAD_BUTTON_L3, QObject::tr("L3 (Left Stick Click)")},
	    {PAD_BUTTON_R3, QObject::tr("R3 (Right Stick Click)")},
	    {PAD_BUTTON_OPTIONS, QObject::tr("Options")},
	    {PAD_BUTTON_TOUCH_PAD, QObject::tr("Touch Pad")},
	};
	return rows;
}

// Keyboard default bindings, mirrored from Libs::Graphics::DefaultKeyboardBindings() in
// window.cpp. SDL_Keycode values for plain letters/Enter/Backspace/Tab are their ASCII value
// (see keyCaptureLineEdit.cpp's QtKeyToSdlKeycode for the same fact used in reverse).
struct DefaultBinding {
	uint32_t pad_button;
	int      host_code;
};

const std::vector<DefaultBinding>& DefaultKeyboardBindingsMirror() {
	static const std::vector<DefaultBinding> bindings = {
	    {PAD_BUTTON_UP, 'w'},
	    {PAD_BUTTON_LEFT, 'a'},
	    {PAD_BUTTON_DOWN, 's'},
	    {PAD_BUTTON_RIGHT, 'd'},
	    {PAD_BUTTON_CROSS, 'j'},
	    {PAD_BUTTON_TRIANGLE, 'i'},
	    {PAD_BUTTON_SQUARE, 'k'},
	    {PAD_BUTTON_CIRCLE, 'l'},
	    {PAD_BUTTON_L1, 'q'},
	    {PAD_BUTTON_R1, 'e'},
	    {PAD_BUTTON_OPTIONS, '\r'},     // SDLK_RETURN
	    {PAD_BUTTON_TOUCH_PAD, '\b'},   // SDLK_BACKSPACE
	};
	return bindings;
}

// Controller default bindings, mirrored from Libs::Graphics::DefaultControllerBindings() in
// window.cpp. SDL_GameControllerButton numeric values are copied from SDL_gamecontroller.h.
enum SdlControllerButton {
	SDL_CB_A              = 0,
	SDL_CB_B              = 1,
	SDL_CB_X              = 2,
	SDL_CB_Y              = 3,
	SDL_CB_BACK           = 4,
	SDL_CB_GUIDE          = 5,
	SDL_CB_START          = 6,
	SDL_CB_LEFTSTICK      = 7,
	SDL_CB_RIGHTSTICK     = 8,
	SDL_CB_LEFTSHOULDER   = 9,
	SDL_CB_RIGHTSHOULDER  = 10,
	SDL_CB_DPAD_UP        = 11,
	SDL_CB_DPAD_DOWN      = 12,
	SDL_CB_DPAD_LEFT      = 13,
	SDL_CB_DPAD_RIGHT     = 14,
	SDL_CB_MISC1          = 15,
	SDL_CB_PADDLE1        = 16,
	SDL_CB_PADDLE2        = 17,
	SDL_CB_PADDLE3        = 18,
	SDL_CB_PADDLE4        = 19,
	SDL_CB_TOUCHPAD       = 20,
};

const std::vector<DefaultBinding>& DefaultControllerBindingsMirror() {
	static const std::vector<DefaultBinding> bindings = {
	    {PAD_BUTTON_CROSS, SDL_CB_A},
	    {PAD_BUTTON_CIRCLE, SDL_CB_B},
	    {PAD_BUTTON_SQUARE, SDL_CB_X},
	    {PAD_BUTTON_TRIANGLE, SDL_CB_Y},
	    {PAD_BUTTON_TOUCH_PAD, SDL_CB_BACK},
	    {PAD_BUTTON_OPTIONS, SDL_CB_START},
	    {PAD_BUTTON_L3, SDL_CB_LEFTSTICK},
	    {PAD_BUTTON_R3, SDL_CB_RIGHTSTICK},
	    {PAD_BUTTON_L1, SDL_CB_LEFTSHOULDER},
	    {PAD_BUTTON_R1, SDL_CB_RIGHTSHOULDER},
	    {PAD_BUTTON_UP, SDL_CB_DPAD_UP},
	    {PAD_BUTTON_DOWN, SDL_CB_DPAD_DOWN},
	    {PAD_BUTTON_LEFT, SDL_CB_DPAD_LEFT},
	    {PAD_BUTTON_RIGHT, SDL_CB_DPAD_RIGHT},
	    {PAD_BUTTON_TOUCH_PAD, SDL_CB_TOUCHPAD},
	};
	return bindings;
}

struct ControllerButtonOption {
	int     value;
	QString name;
};

const std::vector<ControllerButtonOption>& ControllerButtonOptions() {
	static const std::vector<ControllerButtonOption> options = {
	    {SDL_CB_A, QObject::tr("A")},
	    {SDL_CB_B, QObject::tr("B")},
	    {SDL_CB_X, QObject::tr("X")},
	    {SDL_CB_Y, QObject::tr("Y")},
	    {SDL_CB_BACK, QObject::tr("Back")},
	    {SDL_CB_GUIDE, QObject::tr("Guide")},
	    {SDL_CB_START, QObject::tr("Start")},
	    {SDL_CB_LEFTSTICK, QObject::tr("Left Stick Click")},
	    {SDL_CB_RIGHTSTICK, QObject::tr("Right Stick Click")},
	    {SDL_CB_LEFTSHOULDER, QObject::tr("Left Shoulder (LB)")},
	    {SDL_CB_RIGHTSHOULDER, QObject::tr("Right Shoulder (RB)")},
	    {SDL_CB_DPAD_UP, QObject::tr("D-Pad Up")},
	    {SDL_CB_DPAD_DOWN, QObject::tr("D-Pad Down")},
	    {SDL_CB_DPAD_LEFT, QObject::tr("D-Pad Left")},
	    {SDL_CB_DPAD_RIGHT, QObject::tr("D-Pad Right")},
	    {SDL_CB_MISC1, QObject::tr("Misc 1")},
	    {SDL_CB_PADDLE1, QObject::tr("Paddle 1")},
	    {SDL_CB_PADDLE2, QObject::tr("Paddle 2")},
	    {SDL_CB_PADDLE3, QObject::tr("Paddle 3")},
	    {SDL_CB_PADDLE4, QObject::tr("Paddle 4")},
	    {SDL_CB_TOUCHPAD, QObject::tr("Touchpad Click")},
	};
	return options;
}

// Parses/serializes using the exact same "host_code:pad_button,..." text format as
// Libs::Controller::ParseInputBindingList/SerializeInputBindingList in the emulator, so no
// translation layer is needed between what this dialog writes and what the emulator reads.
std::vector<DefaultBinding> ParseBindingList(const QString& serialized) {
	std::vector<DefaultBinding> bindings;
	const auto                  entries = serialized.split(QLatin1Char(','), Qt::SkipEmptyParts);
	for (const auto& entry: entries) {
		const auto colon = entry.indexOf(QLatin1Char(':'));
		if (colon <= 0 || colon + 1 >= entry.size()) {
			continue;
		}
		bool host_ok  = false;
		bool pad_ok   = false;
		int  host_code = entry.left(colon).toInt(&host_ok);
		auto pad_button = entry.mid(colon + 1).toUInt(&pad_ok);
		if (host_ok && pad_ok) {
			bindings.push_back({static_cast<uint32_t>(pad_button), host_code});
		}
	}
	return bindings;
}

QString SerializeBindingList(const std::vector<DefaultBinding>& bindings) {
	QStringList parts;
	for (const auto& binding: bindings) {
		parts << QStringLiteral("%1:%2").arg(binding.host_code).arg(binding.pad_button);
	}
	return parts.join(QLatin1Char(','));
}

// Finds the configured host_code for a given pad_button in a parsed binding list, or the
// default if the list doesn't explicitly mention it (matching how a partial/hand-edited map is
// interpreted at lookup time in Libs::Controller::LookupKeyboardPadButton/
// LookupControllerPadButton -- an *empty* map means "use built-in defaults", but a *non-empty*
// map that simply omits one button falls back to 0/unbound there, not to the default; this
// dialog instead always shows a concrete value per row by falling back to the default entry when
// the row isn't present, since "leave this row exactly as the built-in default" and "explicitly
// unbind this row" are both useful and the row's widget can only represent one value at a time --
// explicitly saving will still write out one entry per row either way).
int FindHostCodeOrDefault(const std::vector<DefaultBinding>& parsed, uint32_t pad_button,
                          const std::vector<DefaultBinding>& defaults) {
	for (const auto& binding: parsed) {
		if (binding.pad_button == pad_button) {
			return binding.host_code;
		}
	}
	for (const auto& binding: defaults) {
		if (binding.pad_button == pad_button) {
			return binding.host_code;
		}
	}
	return 0;
}

} // namespace

QByteArray InputMappingDialog::g_last_geometry;

InputMappingDialog::InputMappingDialog(Configuration& info, QWidget* parent)
    : QDialog(parent, Qt::WindowCloseButtonHint), m_ui(new Ui::InputMappingDialog), m_info(info) {
	m_ui->setupUi(this);

	m_ui->table_keyboard->horizontalHeader()->setStretchLastSection(true);
	m_ui->table_controller->horizontalHeader()->setStretchLastSection(true);

	connect(m_ui->ok_button, &QPushButton::clicked, this, &InputMappingDialog::save);
	connect(m_ui->reset_defaults_button, &QPushButton::clicked, this,
	        &InputMappingDialog::reset_defaults);

	layout()->setSizeConstraint(QLayout::SetFixedSize);
	restoreGeometry(g_last_geometry);

	Init(info);
}

InputMappingDialog::~InputMappingDialog() {
	delete m_ui;
}

void InputMappingDialog::WriteSettings(QSettings& s) {
	s.beginGroup(SETTINGS_INPUT_MAPPING_DIALOG);
	if (!g_last_geometry.isEmpty()) {
		s.setValue(SETTINGS_INPUT_MAPPING_LAST_GEOMETRY, g_last_geometry);
	}
	s.endGroup();
}

void InputMappingDialog::ReadSettings(QSettings& s) {
	s.beginGroup(SETTINGS_INPUT_MAPPING_DIALOG);
	g_last_geometry = s.value(SETTINGS_INPUT_MAPPING_LAST_GEOMETRY, g_last_geometry).toByteArray();
	s.endGroup();
}

void InputMappingDialog::Init(const Configuration& info) {
	InitKeyboardTab(info.keyboard_button_map);
	InitControllerTab(info.controller_button_map);
}

void InputMappingDialog::InitKeyboardTab(const QString& serialized_map) {
	const auto parsed   = ParseBindingList(serialized_map);
	const auto defaults = DefaultKeyboardBindingsMirror();
	const auto& rows    = PadButtonRows();

	m_ui->table_keyboard->setRowCount(static_cast<int>(rows.size()));
	for (int row = 0; row < static_cast<int>(rows.size()); row++) {
		const auto& pad_row = rows[static_cast<size_t>(row)];

		auto* name_item = new QTableWidgetItem(pad_row.display_name);
		name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
		m_ui->table_keyboard->setItem(row, 0, name_item);

		auto* capture = new KeyCaptureLineEdit(m_ui->table_keyboard);
		capture->SetSdlKeycode(FindHostCodeOrDefault(parsed, pad_row.pad_button, defaults));
		capture->setProperty("pad_button", QVariant::fromValue<uint>(pad_row.pad_button));
		m_ui->table_keyboard->setCellWidget(row, 1, capture);
	}
}

void InputMappingDialog::InitControllerTab(const QString& serialized_map) {
	const auto parsed   = ParseBindingList(serialized_map);
	const auto defaults = DefaultControllerBindingsMirror();
	const auto& rows    = PadButtonRows();
	const auto& options = ControllerButtonOptions();

	m_ui->table_controller->setRowCount(static_cast<int>(rows.size()));
	for (int row = 0; row < static_cast<int>(rows.size()); row++) {
		const auto& pad_row = rows[static_cast<size_t>(row)];

		auto* name_item = new QTableWidgetItem(pad_row.display_name);
		name_item->setFlags(name_item->flags() & ~Qt::ItemIsEditable);
		m_ui->table_controller->setItem(row, 0, name_item);

		auto* combo = new QComboBox(m_ui->table_controller);
		combo->addItem(tr("(unbound)"), QVariant::fromValue(-1));
		for (const auto& option: options) {
			combo->addItem(option.name, QVariant::fromValue(option.value));
		}

		const int current = FindHostCodeOrDefault(parsed, pad_row.pad_button, defaults);
		const int index    = combo->findData(QVariant::fromValue(current));
		combo->setCurrentIndex(index >= 0 ? index : 0);
		combo->setProperty("pad_button", QVariant::fromValue<uint>(pad_row.pad_button));
		m_ui->table_controller->setCellWidget(row, 1, combo);
	}
}

void InputMappingDialog::moveEvent(QMoveEvent* event) {
	QDialog::moveEvent(event);
	g_last_geometry = saveGeometry();
}

void InputMappingDialog::save() {
	std::vector<DefaultBinding> keyboard_bindings;
	for (int row = 0; row < m_ui->table_keyboard->rowCount(); row++) {
		auto* capture = qobject_cast<KeyCaptureLineEdit*>(m_ui->table_keyboard->cellWidget(row, 1));
		if (capture == nullptr) {
			continue;
		}
		const auto pad_button = capture->property("pad_button").value<uint>();
		if (capture->GetSdlKeycode() != 0) {
			keyboard_bindings.push_back({pad_button, capture->GetSdlKeycode()});
		}
	}

	std::vector<DefaultBinding> controller_bindings;
	for (int row = 0; row < m_ui->table_controller->rowCount(); row++) {
		auto* combo = qobject_cast<QComboBox*>(m_ui->table_controller->cellWidget(row, 1));
		if (combo == nullptr) {
			continue;
		}
		const auto pad_button = combo->property("pad_button").value<uint>();
		const int  button     = combo->currentData().toInt();
		if (button >= 0) {
			controller_bindings.push_back({pad_button, button});
		}
	}

	m_info.keyboard_button_map   = SerializeBindingList(keyboard_bindings);
	m_info.controller_button_map = SerializeBindingList(controller_bindings);

	emit accept();
}

void InputMappingDialog::reset_defaults() {
	InitKeyboardTab(QString());
	InitControllerTab(QString());
}
