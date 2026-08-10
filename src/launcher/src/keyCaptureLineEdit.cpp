#include "keyCaptureLineEdit.h"

#include <QEvent>
#include <QKeyEvent>
#include <Qt>

namespace {

// Mirrors SDL_SCANCODE_TO_KEYCODE(x) = x | (1<<30) from SDL_keycode.h, without including SDL.
constexpr int SDLK_SCANCODE_MASK = 1 << 30;

constexpr int SdlScancodeToKeycode(int scancode) { return scancode | SDLK_SCANCODE_MASK; }

// Scancode values copied from SDL_scancode.h (usage page 0x07, the stable USB HID keyboard
// page -- these numeric values are part of SDL2's public, versioned ABI and are not expected to
// change).
constexpr int SDL_SCANCODE_F1        = 58;
constexpr int SDL_SCANCODE_F12       = 69;
constexpr int SDL_SCANCODE_CAPSLOCK  = 57;
constexpr int SDL_SCANCODE_PRINTSCREEN = 70;
constexpr int SDL_SCANCODE_SCROLLLOCK  = 71;
constexpr int SDL_SCANCODE_PAUSE       = 72;
constexpr int SDL_SCANCODE_INSERT      = 73;
constexpr int SDL_SCANCODE_HOME        = 74;
constexpr int SDL_SCANCODE_PAGEUP      = 75;
constexpr int SDL_SCANCODE_END         = 77;
constexpr int SDL_SCANCODE_PAGEDOWN    = 78;
constexpr int SDL_SCANCODE_RIGHT       = 79;
constexpr int SDL_SCANCODE_LEFT        = 80;
constexpr int SDL_SCANCODE_DOWN        = 81;
constexpr int SDL_SCANCODE_UP          = 82;
constexpr int SDL_SCANCODE_NUMLOCKCLEAR = 83;
constexpr int SDL_SCANCODE_APPLICATION  = 101;
constexpr int SDL_SCANCODE_MENU         = 118;
constexpr int SDL_SCANCODE_LCTRL        = 224;
constexpr int SDL_SCANCODE_LSHIFT       = 225;
constexpr int SDL_SCANCODE_LALT         = 226;
constexpr int SDL_SCANCODE_LGUI         = 227;
constexpr int SDL_SCANCODE_RCTRL        = 228;
constexpr int SDL_SCANCODE_RSHIFT       = 229;
constexpr int SDL_SCANCODE_RALT         = 230;
constexpr int SDL_SCANCODE_RGUI         = 231;

// SDLK_* constants that are plain ASCII values in SDL_keycode.h.
constexpr int SDLK_RETURN    = '\r';
constexpr int SDLK_ESCAPE    = '\x1B';
constexpr int SDLK_BACKSPACE = '\b';
constexpr int SDLK_TAB       = '\t';
constexpr int SDLK_SPACE     = ' ';
constexpr int SDLK_DELETE    = '\x7F';

// Converts a Qt::Key into the matching SDL_Keycode value, so the emulator's
// Libs::Controller::InputBinding::host_code (which is always an SDL_Keycode for keyboard
// bindings) stores exactly what the SDL-backed emulator process will see, without the launcher
// needing to link SDL2. Returns 0 for keys with no reasonable SDL_Keycode equivalent (e.g.
// pure modifier-only chords report via their own Qt::Key_Control etc, which are excluded
// on purpose -- see keyPressEvent()).
int QtKeyToSdlKeycode(int qt_key) {
	// Printable ASCII keys: Qt::Key_A..Z/0..9/punctuation use the same values as their
	// uppercase ASCII character, while SDL_Keycode for letters is lowercase ASCII. Qt's
	// Key_Space..Key_AsciiTilde block (0x20-0x7E) is otherwise already identical to ASCII/SDLK.
	if (qt_key >= Qt::Key_A && qt_key <= Qt::Key_Z) {
		return 'a' + (qt_key - Qt::Key_A);
	}
	if (qt_key >= Qt::Key_Space && qt_key <= Qt::Key_AsciiTilde) {
		return qt_key;
	}

	switch (qt_key) {
		case Qt::Key_Return:
		case Qt::Key_Enter: return SDLK_RETURN;
		case Qt::Key_Escape: return SDLK_ESCAPE;
		case Qt::Key_Backspace: return SDLK_BACKSPACE;
		case Qt::Key_Tab:
		case Qt::Key_Backtab: return SDLK_TAB;
		case Qt::Key_Delete: return SDLK_DELETE;
		case Qt::Key_Insert: return SdlScancodeToKeycode(SDL_SCANCODE_INSERT);
		case Qt::Key_Home: return SdlScancodeToKeycode(SDL_SCANCODE_HOME);
		case Qt::Key_End: return SdlScancodeToKeycode(SDL_SCANCODE_END);
		case Qt::Key_PageUp: return SdlScancodeToKeycode(SDL_SCANCODE_PAGEUP);
		case Qt::Key_PageDown: return SdlScancodeToKeycode(SDL_SCANCODE_PAGEDOWN);
		case Qt::Key_Left: return SdlScancodeToKeycode(SDL_SCANCODE_LEFT);
		case Qt::Key_Right: return SdlScancodeToKeycode(SDL_SCANCODE_RIGHT);
		case Qt::Key_Up: return SdlScancodeToKeycode(SDL_SCANCODE_UP);
		case Qt::Key_Down: return SdlScancodeToKeycode(SDL_SCANCODE_DOWN);
		case Qt::Key_CapsLock: return SdlScancodeToKeycode(SDL_SCANCODE_CAPSLOCK);
		case Qt::Key_NumLock: return SdlScancodeToKeycode(SDL_SCANCODE_NUMLOCKCLEAR);
		case Qt::Key_ScrollLock: return SdlScancodeToKeycode(SDL_SCANCODE_SCROLLLOCK);
		case Qt::Key_Pause: return SdlScancodeToKeycode(SDL_SCANCODE_PAUSE);
		case Qt::Key_Print: return SdlScancodeToKeycode(SDL_SCANCODE_PRINTSCREEN);
		case Qt::Key_Menu: return SdlScancodeToKeycode(SDL_SCANCODE_MENU);
		case Qt::Key_Meta: return SdlScancodeToKeycode(SDL_SCANCODE_LGUI);
		case Qt::Key_Control: return SdlScancodeToKeycode(SDL_SCANCODE_LCTRL);
		case Qt::Key_Shift: return SdlScancodeToKeycode(SDL_SCANCODE_LSHIFT);
		case Qt::Key_Alt: return SdlScancodeToKeycode(SDL_SCANCODE_LALT);
		case Qt::Key_AltGr: return SdlScancodeToKeycode(SDL_SCANCODE_RALT);
		default: break;
	}

	if (qt_key >= Qt::Key_F1 && qt_key <= Qt::Key_F12) {
		return SdlScancodeToKeycode(SDL_SCANCODE_F1 + (qt_key - Qt::Key_F1));
	}

	return 0;
}

QString SdlKeycodeToDisplayText(int sdl_keycode) {
	if (sdl_keycode == 0) {
		return QObject::tr("(unbound)");
	}
	if (sdl_keycode == SDLK_RETURN) {
		return QObject::tr("Enter");
	}
	if (sdl_keycode == SDLK_ESCAPE) {
		return QObject::tr("Escape");
	}
	if (sdl_keycode == SDLK_BACKSPACE) {
		return QObject::tr("Backspace");
	}
	if (sdl_keycode == SDLK_TAB) {
		return QObject::tr("Tab");
	}
	if (sdl_keycode == SDLK_SPACE) {
		return QObject::tr("Space");
	}
	if (sdl_keycode == SDLK_DELETE) {
		return QObject::tr("Delete");
	}
	if ((sdl_keycode & SDLK_SCANCODE_MASK) != 0) {
		const int scancode = sdl_keycode & ~SDLK_SCANCODE_MASK;
		if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
			return QStringLiteral("F%1").arg(scancode - SDL_SCANCODE_F1 + 1);
		}
		switch (scancode) {
			case SDL_SCANCODE_INSERT: return QObject::tr("Insert");
			case SDL_SCANCODE_HOME: return QObject::tr("Home");
			case SDL_SCANCODE_END: return QObject::tr("End");
			case SDL_SCANCODE_PAGEUP: return QObject::tr("Page Up");
			case SDL_SCANCODE_PAGEDOWN: return QObject::tr("Page Down");
			case SDL_SCANCODE_LEFT: return QObject::tr("Left");
			case SDL_SCANCODE_RIGHT: return QObject::tr("Right");
			case SDL_SCANCODE_UP: return QObject::tr("Up");
			case SDL_SCANCODE_DOWN: return QObject::tr("Down");
			case SDL_SCANCODE_CAPSLOCK: return QObject::tr("Caps Lock");
			case SDL_SCANCODE_NUMLOCKCLEAR: return QObject::tr("Num Lock");
			case SDL_SCANCODE_SCROLLLOCK: return QObject::tr("Scroll Lock");
			case SDL_SCANCODE_PAUSE: return QObject::tr("Pause");
			case SDL_SCANCODE_PRINTSCREEN: return QObject::tr("Print Screen");
			case SDL_SCANCODE_MENU: return QObject::tr("Menu");
			case SDL_SCANCODE_APPLICATION: return QObject::tr("Application");
			case SDL_SCANCODE_LCTRL: return QObject::tr("Left Ctrl");
			case SDL_SCANCODE_RCTRL: return QObject::tr("Right Ctrl");
			case SDL_SCANCODE_LSHIFT: return QObject::tr("Left Shift");
			case SDL_SCANCODE_RSHIFT: return QObject::tr("Right Shift");
			case SDL_SCANCODE_LALT: return QObject::tr("Left Alt");
			case SDL_SCANCODE_RALT: return QObject::tr("Right Alt");
			case SDL_SCANCODE_LGUI: return QObject::tr("Left Meta");
			case SDL_SCANCODE_RGUI: return QObject::tr("Right Meta");
			default: return QStringLiteral("Scancode %1").arg(scancode);
		}
	}
	if (sdl_keycode >= 'a' && sdl_keycode <= 'z') {
		return QString(QChar(sdl_keycode)).toUpper();
	}
	if (sdl_keycode >= 0x20 && sdl_keycode < 0x7F) {
		return QString(QChar(sdl_keycode));
	}
	return QStringLiteral("Code %1").arg(sdl_keycode);
}

} // namespace

KeyCaptureLineEdit::KeyCaptureLineEdit(QWidget* parent): QLineEdit(parent) {
	setReadOnly(true);
	setAlignment(Qt::AlignCenter);
	setPlaceholderText(tr("Click, then press a key..."));
	setText(SdlKeycodeToDisplayText(m_sdl_keycode));
}

void KeyCaptureLineEdit::SetSdlKeycode(int sdl_keycode) {
	m_sdl_keycode = sdl_keycode;
	setText(SdlKeycodeToDisplayText(m_sdl_keycode));
}

bool KeyCaptureLineEdit::event(QEvent* e) {
	// Tab/Shift+Tab normally move focus before reaching keyPressEvent; intercept them here at
	// the event() level (same technique Qt itself recommends for capturing Tab) so users can
	// bind Tab like any other key while this field has focus.
	if (e->type() == QEvent::KeyPress) {
		auto* key_event = static_cast<QKeyEvent*>(e);
		if (key_event->key() == Qt::Key_Tab || key_event->key() == Qt::Key_Backtab) {
			keyPressEvent(key_event);
			return true;
		}
	}
	return QLineEdit::event(e);
}

void KeyCaptureLineEdit::keyPressEvent(QKeyEvent* event) {
	const int key = event->key();

	if (key == Qt::Key_Escape) {
		SetSdlKeycode(0);
		event->accept();
		return;
	}

	// Bare modifier presses (nothing else held yet) aren't useful bindings on their own -- let
	// them fall through so focus/navigation behaves normally instead of silently binding to
	// e.g. "Shift" whenever the user taps it while aiming for a combo.
	if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
	    key == Qt::Key_AltGr || key == Qt::Key_Meta) {
		QLineEdit::keyPressEvent(event);
		return;
	}

	const int sdl_keycode = QtKeyToSdlKeycode(key);
	if (sdl_keycode != 0) {
		SetSdlKeycode(sdl_keycode);
		event->accept();
		return;
	}

	QLineEdit::keyPressEvent(event);
}
