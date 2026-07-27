#ifndef KEY_CAPTURE_LINEEDIT_H
#define KEY_CAPTURE_LINEEDIT_H

#include "common.h"

#include <QLineEdit>

class QEvent;
class QKeyEvent;
class QWidget;

// A read-only QLineEdit that captures the next physical key press instead of accepting typed
// text, and stores it as an SDL_Keycode-compatible integer (matching the host_code field of
// Libs::Controller::InputBinding in the emulator, see controller.h). Used by the keyboard tab of
// the Input Mapping dialog.
//
// The launcher does not link SDL2 (see the controller-button combo box in inputMappingDialog.cpp
// for the same rationale), so the Qt-key <-> SDL_Keycode conversion is done locally by mirroring
// the small, stable parts of SDL_keycode.h/SDL_scancode.h that the emulator's default bindings
// actually use (ASCII-valued keys are identical between the two; the handful of non-ASCII keys
// SDL represents via SDL_SCANCODE_TO_KEYCODE are hand-mapped in the .cpp).
//
// Escape clears/unbinds the field instead of being captured as a bindable key, matching the
// dialog's on-screen hint text ("Press Escape to unbind").
class KeyCaptureLineEdit: public QLineEdit {
	Q_OBJECT
	KYTY_QT_CLASS_NO_COPY(KeyCaptureLineEdit);

public:
	explicit KeyCaptureLineEdit(QWidget* parent = nullptr);

	// 0 means "unbound".
	[[nodiscard]] int GetSdlKeycode() const { return m_sdl_keycode; }
	void              SetSdlKeycode(int sdl_keycode);

protected:
	bool event(QEvent* e) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	int m_sdl_keycode = 0;
};

#endif // KEY_CAPTURE_LINEEDIT_H
