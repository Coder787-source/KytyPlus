#include "graphics/presentation/window.h"

#include <cstdlib>

#include "SDL.h"
#include "SDL_error.h"
#include "SDL_events.h"
#include "SDL_gamecontroller.h"
#include "SDL_hints.h"
#include "SDL_joystick.h"
#include "SDL_keyboard.h"
#include "SDL_keycode.h"
#include "SDL_mouse.h"
#include "SDL_pixels.h"
#include "SDL_rwops.h"
#include "SDL_stdinc.h"
#include "SDL_surface.h"
#include "SDL_thread.h"
#include "SDL_touch.h"
#include "SDL_video.h"
#include "SDL_vulkan.h"
#include "common/assert.h"
#include "common/common.h"
#include "common/emulatorConfig.h"
#include "common/file.h"
#include "common/logging/log.h"
#include "common/profiler.h"
#include "common/stringUtils.h"
#include "common/systemInfo.h"
#include "common/threads.h"
#include "common/timer.h"
#include "graphics/host_gpu/graphicContext.h"
#include "graphics/host_gpu/renderer/render.h"
#include "graphics/host_gpu/vma.h"
#include "graphics/host_gpu/vulkanCommon.h"
#include "graphics/presentation/renderDoc.h"
#include "graphics/presentation/window/windowInternal.h"
#include "libs/controller.h"
#include "loader/systemContent.h"

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <vulkan/vk_platform.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#include "stb_image.h"

#include <fmt/format.h>

// IWYU pragma: no_include <intrin.h>

#define KYTY_ENABLE_DEBUG_PRINTF
#define KYTY_DBG_INPUT

namespace Libs::Graphics {

constexpr int   KEYBOARD_CONTROLLER_ID = -1000;

struct EventKeyboard {
	bool     down;
	bool     up;
	bool     pressed;
	bool     released;
	bool     repeat;
	int      scan_code;
	int      key_code;
	uint16_t mod;
	double   timestamp_seconds;
};

static uint32_t KeyboardKeyToPadButton(int key_code) {
	switch (key_code) {
		case SDLK_w: return Controller::PAD_BUTTON_UP;
		case SDLK_a: return Controller::PAD_BUTTON_LEFT;
		case SDLK_s: return Controller::PAD_BUTTON_DOWN;
		case SDLK_d: return Controller::PAD_BUTTON_RIGHT;
		case SDLK_j: return Controller::PAD_BUTTON_CROSS;
		case SDLK_i: return Controller::PAD_BUTTON_TRIANGLE;
		case SDLK_k: return Controller::PAD_BUTTON_SQUARE;
		case SDLK_l: return Controller::PAD_BUTTON_CIRCLE;
		case SDLK_q: return Controller::PAD_BUTTON_L1;
		case SDLK_e: return Controller::PAD_BUTTON_R1;
		case SDLK_RETURN:
		case SDLK_RETURN2: return Controller::PAD_BUTTON_OPTIONS;
		case SDLK_BACKSPACE:
		case SDLK_TAB: return Controller::PAD_BUTTON_TOUCH_PAD;
		default: return 0;
	}
}

static uint32_t ControllerButtonToPadButton(int button) {
	switch (button) {
		case SDL_CONTROLLER_BUTTON_A: return Controller::PAD_BUTTON_CROSS;
		case SDL_CONTROLLER_BUTTON_B: return Controller::PAD_BUTTON_CIRCLE;
		case SDL_CONTROLLER_BUTTON_X: return Controller::PAD_BUTTON_SQUARE;
		case SDL_CONTROLLER_BUTTON_Y: return Controller::PAD_BUTTON_TRIANGLE;
		case SDL_CONTROLLER_BUTTON_BACK: return Controller::PAD_BUTTON_TOUCH_PAD;
		case SDL_CONTROLLER_BUTTON_START: return Controller::PAD_BUTTON_OPTIONS;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK: return Controller::PAD_BUTTON_L3;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Controller::PAD_BUTTON_R3;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Controller::PAD_BUTTON_L1;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Controller::PAD_BUTTON_R1;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return Controller::PAD_BUTTON_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Controller::PAD_BUTTON_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Controller::PAD_BUTTON_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Controller::PAD_BUTTON_RIGHT;
		default: return 0;
	}
}

static Controller::Axis ControllerAxisFromSdl(int axis_id) {
	switch (axis_id) {
		case SDL_CONTROLLER_AXIS_LEFTX: return Controller::Axis::LeftX;
		case SDL_CONTROLLER_AXIS_LEFTY: return Controller::Axis::LeftY;
		case SDL_CONTROLLER_AXIS_RIGHTX: return Controller::Axis::RightX;
		case SDL_CONTROLLER_AXIS_RIGHTY: return Controller::Axis::RightY;
		case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return Controller::Axis::TriggerLeft;
		case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return Controller::Axis::TriggerRight;
		default: return Controller::Axis::AxisMax;
	}
}

static bool ControllerAxisIsTrigger(int axis_id) {
	return axis_id == SDL_CONTROLLER_AXIS_TRIGGERLEFT ||
	       axis_id == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
}

static int ControllerAxisValueFromSdl(int axis_id, int axis_value) {
	return ControllerAxisIsTrigger(axis_id)
	           ? Controller::controller_get_axis(0, SDL_JOYSTICK_AXIS_MAX, axis_value)
	           : Controller::controller_get_axis(SDL_JOYSTICK_AXIS_MIN, SDL_JOYSTICK_AXIS_MAX,
	                                             axis_value);
}

struct EventMouse {
	bool   down;
	bool   up;
	bool   left;
	bool   middle;
	bool   right;
	bool   x1;
	bool   x2;
	bool   touch;
	bool   pressed;
	bool   released;
	int    num_of_clicks;
	bool   wheel;
	int    x;
	int    y;
	bool   motion;
	int    motion_x;
	int    motion_y;
	double timestamp_seconds;
};

struct EventFinger {
	bool   down;
	bool   up;
	bool   motion;
	int    touch_id;
	int    finger_id;
	float  x;
	float  y;
	float  dx;
	float  dy;
	float  pressure;
	double timestamp_seconds;
};

struct EventController {
	int    id;
	int    button;
	int    axis_id;
	int    axis_value;
	bool   down;
	bool   up;
	bool   added;
	bool   removed;
	bool   remapped;
	bool   axis;
	bool   pressed;
	bool   released;
	double timestamp_seconds;
};

enum class DisplayOrientation {
	Unknown,   /* The display orientation can't be determined */
	Landscape, /* The display is in landscape mode, with the right side up, relative to portrait
	              mode */
	LandscapeFlipped, /* The display is in landscape mode, with the left side up, relative to
	                     portrait mode */
	Portrait,         /* The display is in portrait mode */
	PortraitFlipped,  /* The display is in portrait mode, upside down */

	DisplayEventOrientation = 0xF0
};

struct EventDisplay {
	DisplayOrientation orientation;
};

constexpr uint32_t KYTY_SDL_BUTTON_LMASK  = SDL_BUTTON_LMASK;  // NOLINT(hicpp-signed-bitwise)
constexpr uint32_t KYTY_SDL_BUTTON_MMASK  = SDL_BUTTON_MMASK;  // NOLINT(hicpp-signed-bitwise)
constexpr uint32_t KYTY_SDL_BUTTON_RMASK  = SDL_BUTTON_RMASK;  // NOLINT(hicpp-signed-bitwise)
constexpr uint32_t KYTY_SDL_BUTTON_X1MASK = SDL_BUTTON_X1MASK; // NOLINT(hicpp-signed-bitwise)
constexpr uint32_t KYTY_SDL_BUTTON_X2MASK = SDL_BUTTON_X2MASK; // NOLINT(hicpp-signed-bitwise)

namespace {

std::unique_ptr<WindowContext> g_window;

} // namespace

constexpr const char* KYTY_SDL_WINDOW_CAPTION = "Game";
constexpr uint32_t    KYTY_SDL_WINDOW_FLAGS =
    (static_cast<uint32_t>(SDL_WINDOW_HIDDEN) | static_cast<uint32_t>(SDL_WINDOW_VULKAN));
constexpr int KYTY_SDL_WINDOWPOS_CENTERED = SDL_WINDOWPOS_CENTERED; /*NOLINT(hicpp-signed-bitwise)*/

static void SetPause(WindowLoopState& game, bool flag) {
	LOGF("Pause: %s\n", flag ? "true" : "false");

	game.paused.store(flag, std::memory_order_release);
}

static void GameEventQuit(WindowLoopState& game) {
	LOGF("Event: quit\n");

	game.need_exit = true;
}

static void GameEventTerminate(WindowLoopState& game) {
	LOGF("Event: terminate\n");

	game.need_exit = true;
}

static void GameEventKeyboard(WindowLoopState& game, const EventKeyboard& key) {
#ifdef KYTY_DBG_INPUT
	LOGF("Key: time = %.04f, %s%s, %s%s, %s, scan = %d, key = %d, mod = %04" PRIx16 "\n",
	     key.timestamp_seconds, (key.down ? "down" : ""), (key.up ? "up" : ""),
	     (key.pressed ? "pressed" : ""), (key.released ? "released" : ""),
	     (key.repeat ? "repeat" : ""), key.scan_code, key.key_code, key.mod);
#endif

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS || KYTY_PLATFORM == KYTY_PLATFORM_LINUX
	if (key.down) {
		switch (key.key_code) {
			case SDLK_ESCAPE: game.need_exit = true; break;
			case SDLK_SPACE:
				SetPause(game, !game.paused.load(std::memory_order_acquire));
				break;
			case SDLK_F1:
				if (!key.repeat) {
					RenderDocRequestCapture();
				}
				break;
			default: break;
		}
	}

	const auto button = KeyboardKeyToPadButton(key.key_code);
	if (button != 0 && (key.down || key.up) && !key.repeat) {
		static bool keyboard_connected = false;
		if (!keyboard_connected) {
			Controller::ControllerConnect(KEYBOARD_CONTROLLER_ID);
			keyboard_connected = true;
		}
		Controller::ControllerButton(KEYBOARD_CONTROLLER_ID, button, key.down);
	}
#endif
}

static void GameEventMouse([[maybe_unused]] const EventMouse& mb) {
#ifdef KYTY_DBG_INPUT
	if (mb.wheel) {
		LOGF("Mouse wheel: time = %.04f, %s[%d, %d]\n", mb.timestamp_seconds,
		     (mb.touch ? "touch, " : ""), mb.x, mb.y);
	} else if (mb.motion) {
		LOGF("Mouse motion: time = %.04f, %s%s%s%s%s%s, [%d, %d], (%d, %d)\n", mb.timestamp_seconds,
		     (mb.left ? "left" : ""), (mb.middle ? "middle" : ""), (mb.right ? "right" : ""),
		     (mb.x1 ? "x1" : ""), (mb.x2 ? "x2" : ""), (mb.touch ? "_touch" : ""), mb.x, mb.y,
		     mb.motion_x, mb.motion_y);
	} else {
		LOGF("Mouse click: time = %.04f, %d, %s%s%s%s%s%s, %s%s, %s%s, [%d, %d]\n",
		     mb.timestamp_seconds, mb.num_of_clicks, (mb.left ? "left" : ""),
		     (mb.middle ? "middle" : ""), (mb.right ? "right" : ""), (mb.x1 ? "x1" : ""),
		     (mb.x2 ? "x2" : ""), (mb.touch ? "_touch" : ""), (mb.down ? "down" : ""),
		     (mb.up ? "up" : ""), (mb.pressed ? "pressed" : ""), (mb.released ? "released" : ""),
		     mb.x, mb.y);
	}
#endif
}

static void GameEventFinger([[maybe_unused]] const EventFinger& f) {
#ifdef KYTY_DBG_INPUT
	if (f.motion) {
		LOGF("Finger motion: time = %.04f, %d, %d, (x,y) = [%f, %f], (dx,dy) = [%f, %f], pressure "
		     "= %f\n",
		     f.timestamp_seconds, f.touch_id, f.finger_id, f.x, f.y, f.dx, f.dy, f.pressure);
	} else {
		LOGF("Finger press: time = %.04f, %d, %d, %s%s, (x,y) = [%f, %f], (dx,dy) = [%f, %f], "
		     "pressure = %f\n",
		     f.timestamp_seconds, f.touch_id, f.finger_id, (f.down ? "down" : ""),
		     (f.up ? "up" : ""), f.x, f.y, f.dx, f.dy, f.pressure);
	}
#endif
}

static void GameEventController([[maybe_unused]] const EventController& f) {
	EXIT_NOT_IMPLEMENTED(f.remapped);

#ifdef KYTY_DBG_INPUT
	if (f.added || f.removed) {
		LOGF("Controller %s: %d, time = %.04f\n", (f.added ? "added" : "removed"), f.id,
		     f.timestamp_seconds);
	} else if (f.axis) {
		LOGF("Controller axis: %d, axis = %d, value = %d, time = %.04f\n", f.id, f.axis_id,
		     f.axis_value, f.timestamp_seconds);
	} else {
		LOGF("Controller button: "
		     "%d, %s%s, %s%s, button = %d, time = %.04f\n",
		     f.id, (f.down ? "down" : ""), (f.up ? "up" : ""), (f.pressed ? "pressed" : ""),
		     (f.released ? "released" : ""), f.button, f.timestamp_seconds);
	}
#endif

	if (f.added) {
		auto* pad = SDL_GameControllerOpen(f.id);
		EXIT_NOT_IMPLEMENTED(pad == nullptr);
		int id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad));
		Controller::ControllerConnect(id);
	}

	if (f.removed) {
		Controller::ControllerDisconnect(f.id);
		SDL_GameControllerClose(SDL_GameControllerFromInstanceID(f.id));
	}

	if (f.down || f.up) {
		const auto button = ControllerButtonToPadButton(f.button);
		if (button != 0) {
			Controller::ControllerButton(f.id, button, f.down);
		}
	}

	if (f.axis) {
		const auto axis = ControllerAxisFromSdl(f.axis_id);
		if (axis != Controller::Axis::AxisMax) {
			Controller::ControllerAxis(f.id, axis,
			                           ControllerAxisValueFromSdl(f.axis_id, f.axis_value));
		}
	}
}

static void GameEventLowMemory() {
	LOGF("Event: low_memory\n");
}

static void GameEventWillEnterBackground(WindowLoopState& game) {
	LOGF("Event: will_enter_background\n");

	SetPause(game, true);
}

static void GameEventDidEnterBackground() {
	LOGF("Event: did_enter_background\n");
}

static void GameEventWillEnterForeground() {
	LOGF("Event: will_enter_foreground\n");
}

static void GameEventDidEnterForeground(WindowLoopState& game) {
	LOGF("Event: did_enter_foreground\n");

	SetPause(game, false);
}

void WindowContext::Resize(uint32_t new_width, uint32_t new_height) {
	// Minimized Win32 windows fire SIZE_CHANGED/RESIZED with 0x0 (Astro Bot splash). Ignore the
	// degenerate size; last known-good screen size stays until a real restore arrives.
	if (new_width == 0 || new_height == 0) {
		static std::atomic<uint32_t> zero_resize_logs {0};
		if (zero_resize_logs.fetch_add(1, std::memory_order_relaxed) < 8) {
			LOGF_COLOR(Log::Color::Yellow,
			           "GameEventResize: ignoring degenerate resize %" PRIu32 "x%" PRIu32
			           " (window likely minimized)\n",
			           new_width, new_height);
		}
		return;
	}
	graphic_ctx.screen_width  = new_width;
	graphic_ctx.screen_height = new_height;
}

void WindowContext::ProcessWindowEvent(const SDL_WindowEvent& event) {
	const auto& window_event = event;
	switch (window_event.event) {
		case SDL_WINDOWEVENT_SHOWN: LOGF("Window %" PRIu32 " shown\n", window_event.windowID); break;

		case SDL_WINDOWEVENT_HIDDEN:
			LOGF("Window %" PRIu32 " hidden\n", window_event.windowID);
			break;

		case SDL_WINDOWEVENT_EXPOSED:
			LOGF("Window %" PRIu32 " exposed\n", window_event.windowID);
			break;

		case SDL_WINDOWEVENT_MOVED:
			LOGF("Window %" PRIu32 " moved to %" PRId32 ",%" PRId32 "\n",
			     window_event.windowID, window_event.data1, window_event.data2);
			break;

		case SDL_WINDOWEVENT_RESIZED:
			LOGF("Window %" PRIu32 " resized to %" PRId32 "x%" PRId32 "\n",
			     window_event.windowID, window_event.data1, window_event.data2);

			LOGF("m: %d\n", static_cast<int>(SDL_ThreadID()));
			Resize(window_event.data1, window_event.data2);

			break;

		case SDL_WINDOWEVENT_SIZE_CHANGED:
			LOGF("Window %" PRIu32 " size changed to %" PRId32 "x%" PRId32 "\n",
			     window_event.windowID, window_event.data1, window_event.data2);

			LOGF("m: %d\n", static_cast<int>(SDL_ThreadID()));
			Resize(window_event.data1, window_event.data2);

			break;

		case SDL_WINDOWEVENT_MINIMIZED:
			LOGF("Window %" PRIu32 " minimized\n", window_event.windowID);
			window_minimized = true;
			break;
		case SDL_WINDOWEVENT_MAXIMIZED:
			LOGF("Window %" PRIu32 " maximized\n", window_event.windowID);
			window_minimized = false;
			break;
		case SDL_WINDOWEVENT_RESTORED:
			LOGF("Window %" PRIu32 " restored\n", window_event.windowID);
			window_minimized = false;
			break;
		case SDL_WINDOWEVENT_ENTER:
			LOGF("Mouse entered window %" PRIu32 "\n", window_event.windowID);
			break;
		case SDL_WINDOWEVENT_LEAVE:
			LOGF("Mouse left window %" PRIu32 "\n", window_event.windowID);
			break;
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			LOGF("Window %" PRIu32 " gained keyboard focus\n", window_event.windowID);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			LOGF("Window %" PRIu32 " lost keyboard focus\n", window_event.windowID);
			break;
		case SDL_WINDOWEVENT_CLOSE:
			LOGF("Window %" PRIu32 " closed\n", window_event.windowID);
			break;
		default:
			LOGF("Window %" PRIu32 " got unknown event %" PRIu8 "\n", window_event.windowID,
			     window_event.event);
			break;
	}
}

void WindowContext::ProcessDisplayEvent(const SDL_DisplayEvent& display) {
	bool sdl = false;

	switch (display.event) {
		case SDL_DISPLAYEVENT_ORIENTATION: sdl = true; [[fallthrough]];
		case static_cast<Uint8>(DisplayOrientation::DisplayEventOrientation): {
			LOGF("Display %" PRIu32 "[%s] changed orientation to %d - ", display.display,
			     sdl ? "SDL" : "Kyty", static_cast<int>(display.data1));

			switch (display.data1) {
				case SDL_ORIENTATION_UNKNOWN: LOGF("UNKNOWN\n"); break;
				case SDL_ORIENTATION_LANDSCAPE: LOGF("LANDSCAPE\n"); break;
				case SDL_ORIENTATION_LANDSCAPE_FLIPPED: LOGF("LANDSCAPE_FLIPPED\n"); break;
				case SDL_ORIENTATION_PORTRAIT: LOGF("PORTRAIT\n"); break;
				case SDL_ORIENTATION_PORTRAIT_FLIPPED: LOGF("PORTRAIT_FLIPPED\n"); break;
				default: LOGF("???\n");
			}

			break;
		}
		default:
			LOGF("Display %" PRIu32 " got unknown event 0x%" PRIx8 "\n", display.display,
			     display.event);
			break;
	}
}

void WindowContext::ProcessEvent(double time_s) {
	auto& game  = loop;
	auto* event = &game.event;
	EXIT_IF(SDL_GetEventState(SDL_DISPLAYEVENT) != SDL_ENABLE);

	switch (event->type) {
		case SDL_QUIT: GameEventQuit(game); break;

		case SDL_APP_TERMINATING: GameEventTerminate(game); break;

		case SDL_APP_LOWMEMORY: GameEventLowMemory(); break;

		case SDL_APP_WILLENTERBACKGROUND: GameEventWillEnterBackground(game); break;

		case SDL_APP_DIDENTERBACKGROUND: GameEventDidEnterBackground(); break;

		case SDL_APP_WILLENTERFOREGROUND: GameEventWillEnterForeground(); break;

		case SDL_APP_DIDENTERFOREGROUND: GameEventDidEnterForeground(game); break;

		case SDL_KEYDOWN:
		case SDL_KEYUP: {
			EventKeyboard key {};

			key.down              = (event->type == SDL_KEYDOWN);
			key.up                = (event->type == SDL_KEYUP);
			key.pressed           = (event->key.state == SDL_PRESSED);
			key.released          = (event->key.state == SDL_RELEASED);
			key.repeat            = (event->key.repeat != 0u);
			key.scan_code         = event->key.keysym.scancode;
			key.key_code          = event->key.keysym.sym;
			key.mod               = event->key.keysym.mod;
			key.timestamp_seconds = time_s;

			GameEventKeyboard(game, key);

			break;
		}

		case SDL_WINDOWEVENT: ProcessWindowEvent(event->window); break;

		case SDL_DISPLAYEVENT: ProcessDisplayEvent(event->display); break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP: {
			EventMouse mb {};

			mb.down              = (event->button.type == SDL_MOUSEBUTTONDOWN);
			mb.up                = (event->button.type == SDL_MOUSEBUTTONUP);
			mb.left              = (event->button.button == SDL_BUTTON_LEFT);
			mb.middle            = (event->button.button == SDL_BUTTON_MIDDLE);
			mb.right             = (event->button.button == SDL_BUTTON_RIGHT);
			mb.x1                = (event->button.button == SDL_BUTTON_X1);
			mb.x2                = (event->button.button == SDL_BUTTON_X2);
			mb.touch             = (event->button.which == SDL_TOUCH_MOUSEID);
			mb.pressed           = (event->button.state == SDL_PRESSED);
			mb.released          = (event->button.state == SDL_RELEASED);
			mb.num_of_clicks     = event->button.clicks;
			mb.wheel             = false;
			mb.x                 = event->button.x;
			mb.y                 = event->button.y;
			mb.motion            = false;
			mb.motion_x          = 0;
			mb.motion_y          = 0;
			mb.timestamp_seconds = time_s;

			GameEventMouse(mb);

			break;
		}

		case SDL_MOUSEWHEEL: {
			EventMouse mb {};

			mb.down              = false;
			mb.up                = false;
			mb.left              = false;
			mb.middle            = false;
			mb.right             = false;
			mb.x1                = false;
			mb.x2                = false;
			mb.touch             = (event->wheel.which == SDL_TOUCH_MOUSEID);
			mb.pressed           = false;
			mb.released          = false;
			mb.num_of_clicks     = 0;
			mb.wheel             = true;
			mb.x                 = event->wheel.x;
			mb.y                 = event->wheel.y;
			mb.motion            = false;
			mb.motion_x          = 0;
			mb.motion_y          = 0;
			mb.timestamp_seconds = time_s;

			GameEventMouse(mb);

			break;
		}

		case SDL_MOUSEMOTION: {
			EventMouse mb {};

			mb.down              = false;
			mb.up                = false;
			mb.left              = ((event->motion.state & KYTY_SDL_BUTTON_LMASK) != 0u);
			mb.middle            = ((event->motion.state & KYTY_SDL_BUTTON_MMASK) != 0u);
			mb.right             = ((event->motion.state & KYTY_SDL_BUTTON_RMASK) != 0u);
			mb.x1                = ((event->motion.state & KYTY_SDL_BUTTON_X1MASK) != 0u);
			mb.x2                = ((event->motion.state & KYTY_SDL_BUTTON_X2MASK) != 0u);
			mb.touch             = (event->motion.which == SDL_TOUCH_MOUSEID);
			mb.pressed           = false;
			mb.released          = false;
			mb.num_of_clicks     = 0;
			mb.wheel             = false;
			mb.x                 = event->motion.x;
			mb.y                 = event->motion.y;
			mb.motion            = true;
			mb.motion_x          = event->motion.xrel;
			mb.motion_y          = event->motion.yrel;
			mb.timestamp_seconds = time_s;

			GameEventMouse(mb);

			break;
		}

		case SDL_FINGERMOTION:
		case SDL_FINGERDOWN:
		case SDL_FINGERUP: {
			EventFinger f {};

			f.down              = (event->tfinger.type == SDL_FINGERDOWN);
			f.up                = (event->tfinger.type == SDL_FINGERUP);
			f.motion            = (event->tfinger.type == SDL_FINGERMOTION);
			f.finger_id         = static_cast<int>(event->tfinger.fingerId);
			f.touch_id          = static_cast<int>(event->tfinger.touchId);
			f.x                 = event->tfinger.x;
			f.y                 = event->tfinger.y;
			f.dx                = event->tfinger.dx;
			f.dy                = event->tfinger.dy;
			f.pressure          = event->tfinger.pressure;
			f.timestamp_seconds = time_s;

			GameEventFinger(f);

			break;
		}

		case SDL_CONTROLLERAXISMOTION: {
			EventController c {};

			c.id                = event->caxis.which;
			c.button            = SDL_CONTROLLER_BUTTON_INVALID;
			c.axis_id           = event->caxis.axis;
			c.axis_value        = event->caxis.value;
			c.down              = false;
			c.up                = false;
			c.added             = false;
			c.removed           = false;
			c.remapped          = false;
			c.axis              = true;
			c.pressed           = false;
			c.released          = false;
			c.timestamp_seconds = time_s;

			GameEventController(c);

			break;
		}

		case SDL_CONTROLLERBUTTONDOWN:
		case SDL_CONTROLLERBUTTONUP: {
			EventController c {};

			c.id                = event->cbutton.which;
			c.button            = event->cbutton.button;
			c.axis_id           = SDL_CONTROLLER_AXIS_INVALID;
			c.axis_value        = 0;
			c.down              = (event->cbutton.type == SDL_CONTROLLERBUTTONDOWN);
			c.up                = (event->cbutton.type == SDL_CONTROLLERBUTTONUP);
			c.added             = false;
			c.removed           = false;
			c.remapped          = false;
			c.axis              = false;
			c.pressed           = (event->cbutton.state == SDL_PRESSED);
			c.released          = (event->cbutton.state == SDL_RELEASED);
			c.timestamp_seconds = time_s;

			GameEventController(c);

			break;
		}

		case SDL_CONTROLLERDEVICEADDED:
		case SDL_CONTROLLERDEVICEREMOVED:
		case SDL_CONTROLLERDEVICEREMAPPED: {
			EventController c {};

			c.id                = event->cdevice.which;
			c.button            = SDL_CONTROLLER_BUTTON_INVALID;
			c.axis_id           = SDL_CONTROLLER_AXIS_INVALID;
			c.axis_value        = 0;
			c.down              = false;
			c.up                = false;
			c.added             = (event->cdevice.type == SDL_CONTROLLERDEVICEADDED);
			c.removed           = (event->cdevice.type == SDL_CONTROLLERDEVICEREMOVED);
			c.remapped          = (event->cdevice.type == SDL_CONTROLLERDEVICEREMAPPED);
			c.axis              = false;
			c.pressed           = false;
			c.released          = false;
			c.timestamp_seconds = time_s;

			GameEventController(c);

			break;
		}
	}
}

#if defined(__APPLE__)
void WindowContext::RunOnMainThread(std::function<void()> task, bool wait) {
	if (Common::Thread::IsMainThread()) {
		task();
		return;
	}

	uint64_t ticket = 0;
	{
		Common::LockGuard lock(main_task_mutex);
		main_tasks.push_back(std::move(task));
		ticket = ++main_tasks_queued;
	}

	// Wake the main loop in case it is blocked in SDL_WaitEvent.
	SDL_Event event {};
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);

	if (!wait) {
		return;
	}
	Common::LockGuard lock(main_task_mutex);
	while (main_tasks_run < ticket) {
		main_task_done.Wait(&main_task_mutex);
	}
}

void WindowContext::DrainMainThreadTasks() {
	std::vector<std::function<void()>> tasks;
	{
		Common::LockGuard lock(main_task_mutex);
		tasks.swap(main_tasks);
	}
	if (tasks.empty()) {
		return;
	}
	for (auto& task: tasks) {
		task();
	}
	Common::LockGuard lock(main_task_mutex);
	main_tasks_run += tasks.size();
	main_task_done.SignalAll();
}
#endif

void WindowContext::Run() {
	Common::Timer timer;
	timer.Start();

	loop.event     = {};
	loop.need_exit = false;
	loop.paused.store(false, std::memory_order_release);

	// Frame pacing — target 60 FPS (16.67 ms per frame).
	// Adaptive: tracks recent frame times and adjusts sleep to maintain
	// consistent cadence even under variable GPU load.
	constexpr double kTargetFrameTime = 1.0 / 60.0;
	constexpr double kMinSleep        = 0.0005;  // 0.5 ms minimum sleep to avoid busy-wait
	constexpr double kMaxFrameTime    = 1.0 / 30.0; // 30 FPS floor before we stop sleeping
	constexpr int    kHistorySize     = 10;
	double           last_frame_time  = timer.GetTimeS();
	double           frame_history[kHistorySize] = {};
	int              frame_index     = 0;
	int              frame_count     = 0;

	while (!loop.need_exit) {
#if defined(__APPLE__)
		DrainMainThreadTasks();
#endif
		if (SDL_PollEvent(&loop.event) != 0) {
			ProcessEvent(timer.GetTimeS());
			continue;
		}

		if (loop.paused.load(std::memory_order_acquire)) {
			if (!timer.IsPaused()) {
				timer.Pause();
			}
			if (SDL_WaitEvent(&loop.event) == 0) {
				EXIT("%s\n", SDL_GetError());
			}
			ProcessEvent(timer.GetTimeS());
			continue;
		}

		if (timer.IsPaused()) {
			timer.Resume();
		}

		// Adaptive frame pacing.
		double now       = timer.GetTimeS();
		double elapsed   = now - last_frame_time;

		// Track frame time in history ring buffer.
		frame_history[frame_index % kHistorySize] = elapsed;
		frame_index++;
		if (frame_count < kHistorySize) { frame_count++; }

		// Compute average frame time over history.
		double avg_frame_time = 0.0;
		for (int i = 0; i < frame_count; i++) {
			avg_frame_time += frame_history[i];
		}
		if (frame_count > 0) {
			avg_frame_time /= static_cast<double>(frame_count);
		} else {
			avg_frame_time = kTargetFrameTime; // first frame — assume ideal
		}

		// If we're consistently below 30 FPS, skip sleeping to let the GPU catch up.
		if (avg_frame_time < kMaxFrameTime) {
			double remaining = kTargetFrameTime - elapsed;
			// Only sleep if we have at least kMinSleep worth of headroom.
			if (remaining > kMinSleep) {
				Common::Thread::SleepMicro(static_cast<uint64_t>(remaining * 1'000'000.0));
			}
		}
		last_frame_time = timer.GetTimeS();
	}
}

static void WindowCreate(WindowContext& context) {
	EXIT_IF(context.window != nullptr);
	EXIT_IF(context.graphic_ctx.screen_width == 0);
	EXIT_IF(context.graphic_ctx.screen_height == 0);

	int width  = static_cast<int>(context.graphic_ctx.screen_width);
	int height = static_cast<int>(context.graphic_ctx.screen_height);

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS
	SDL_SetHint(SDL_HINT_WINDOWS_DPI_SCALING, "0");
#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) < 0) {
		EXIT("%s\n", SDL_GetError());
	}

	LOGF("WindowCreate(): width = %d, height = %d\n", width, height);

	uint32_t window_flags = KYTY_SDL_WINDOW_FLAGS;
#if defined(__APPLE__)
	// macOS 26 window chrome (CoreUI asset decode, SwiftUI titlebar) has been observed
	// throwing NSExceptions under Rosetta during the first CATransaction commit. A
	// borderless window skips that machinery entirely.
	if (std::getenv("KYTY_BORDERLESS") != nullptr) {
		window_flags |= static_cast<uint32_t>(SDL_WINDOW_BORDERLESS);
	}
#endif
	context.window =
	    SDL_CreateWindow(KYTY_SDL_WINDOW_CAPTION, KYTY_SDL_WINDOWPOS_CENTERED,
	                     KYTY_SDL_WINDOWPOS_CENTERED, width, height, window_flags);

	context.window_hidden = true;

	if (context.window == nullptr) {
		EXIT("%s\n", SDL_GetError());
	}

	// Apply fullscreen if requested via --fullscreen / -f flag.
	if (Config::FullscreenEnabled()) {
		SDL_SetWindowFullscreen(context.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}

	SDL_SetWindowResizable(context.window, SDL_FALSE);
}

Presenter& WindowInit(uint32_t width, uint32_t height) {
	EXIT_NOT_IMPLEMENTED(!Common::Thread::IsMainThread());
	EXIT_IF(g_window != nullptr);

	auto window = std::make_unique<WindowContext>();

	window->graphic_ctx.screen_width  = width;
	window->graphic_ctx.screen_height = height;

	WindowCreate(*window);
	window->CreateVulkan();
	auto& presenter = *window->presenter;
	g_window         = std::move(window);
	return presenter;
}

void WindowRun() {
	KYTY_PROFILER_THREAD("Thread_Window");
	EXIT_IF(g_window == nullptr);

	g_window->Run();
}

void WindowShutdown() {
	if (g_window != nullptr) {
		g_window.reset();
	}
}

static int WindowIconRead(void* user, char* data, int size) {
	auto*    src        = static_cast<Common::File*>(user);
	uint32_t bytes_read = 0;
	src->Read(data, static_cast<uint32_t>(size), &bytes_read);
	return static_cast<int>(bytes_read);
}

static void WindowIconSkip(void* user, int n) {
	auto*          src      = static_cast<Common::File*>(user);
	const uint64_t position = src->Tell();

	if (n >= 0) {
		src->Seek(position + static_cast<uint64_t>(n));
	} else {
		const uint64_t distance = static_cast<uint64_t>(-static_cast<int64_t>(n));
		EXIT_IF(distance > position);
		src->Seek(position - distance);
	}
}

static int WindowIconEof(void* user) {
	auto* src = static_cast<Common::File*>(user);
	return src->IsEOF() ? 1 : 0;
}

struct WindowIcon {
	SDL_Surface* surface = nullptr;
	void*        pixels  = nullptr;

	~WindowIcon() {
		SDL_FreeSurface(surface);
		stbi_image_free(pixels);
	}
};

static void WindowLoadPngIcon(const std::string& path, WindowIcon* icon) {
	Common::File f;
	if (!f.Open(path, Common::File::Mode::Read)) {
		EXIT("Can't open icon file %s\n", path.c_str());
	}

	int width  = 0;
	int height = 0;

	stbi_io_callbacks cb {};
	cb.read = WindowIconRead;
	cb.skip = WindowIconSkip;
	cb.eof  = WindowIconEof;

	icon->pixels = stbi_load_from_callbacks(&cb, &f, &width, &height, nullptr, 4);
	f.Close();

	EXIT_IF(icon->pixels == nullptr);

	icon->surface = SDL_CreateRGBSurfaceWithFormatFrom(icon->pixels, width, height, 32, width * 4,
	                                                   SDL_PIXELFORMAT_RGBA32);
	EXIT_NOT_IMPLEMENTED(icon->surface == nullptr);
}

void WindowContext::UpdateIcon() {
	static WindowIcon icon;
	static bool       icon_loaded = false;

	if (!icon_loaded) {
		std::string icon_path;
		if (Loader::SystemContentGetIconPath(&icon_path)) {
			WindowLoadPngIcon(icon_path, &icon);
		}
		icon_loaded = true;
	}

	if (icon.surface != nullptr) {
		SDL_SetWindowIcon(window, icon.surface);
	}
}

void WindowContext::UpdateTitle() {
	static char title[128];
	static char title_id[12];
	static char app_ver[12];
	static bool has_title = Loader::SystemContentParamSfoGetString("TITLE", title, sizeof(title));
	static bool has_title_id =
	    Loader::SystemContentParamSfoGetString("TITLE_ID", title_id, sizeof(title_id));
	static bool has_app_ver =
	    Loader::SystemContentParamSfoGetString("APP_VER", app_ver, sizeof(app_ver));
	static uint64_t fps_start = Common::Timer::QueryPerformanceCounter();
	static uint64_t frame_num = 0;
	static uint64_t fps_frames = 0;
	static double   current_fps = 0.0;

	const auto now       = Common::Timer::QueryPerformanceCounter();
	const auto frequency = Common::Timer::QueryPerformanceFrequency();
	frame_num++;
	fps_frames++;
	if (now - fps_start >= frequency) {
		current_fps = static_cast<double>(fps_frames) * static_cast<double>(frequency) /
		              static_cast<double>(now - fps_start);
		fps_start  = now;
		fps_frames = 0;
	}

	auto fps = fmt::format("{}{}{}{}{}{}[{}] [{}], frame: {}, fps: {:f}", (has_title ? title : ""),
	                       (has_title ? ", " : ""), (has_title_id ? title_id : ""),
	                       (has_title_id ? ", " : ""), (has_app_ver ? app_ver : ""),
	                       (has_app_ver ? " " : ""), device_name, processor_name,
	                       frame_num, current_fps);

#if defined(__APPLE__)
	// AppKit traps on title changes off the main thread; fire-and-forget keeps present pacing.
	RunOnMainThread([this, fps = std::move(fps)] { SDL_SetWindowTitle(window, fps.c_str()); },
	                false);
#else
	SDL_SetWindowTitle(window, fps.c_str());
#endif
}

} // namespace Libs::Graphics

namespace Libs::Controller {

// This is the built-in fallback used whenever the user hasn't set a custom keyboard mapping in
// the Qt launcher's Input Mapping dialog (SetKeyboardButtonMap was never called, or was called
// with an empty list to explicitly reset to defaults). Defined here (rather than in
// controller.cpp) because the binding codes are SDL_Keycode values, which only this windowing
// backend file already depends on; declared in controller.h so callers elsewhere don't need to
// know that.
uint32_t DefaultKeyboardPadButton(int key_code) {
	switch (key_code) {
		case SDLK_w: return Controller::PAD_BUTTON_UP;
		case SDLK_a: return Controller::PAD_BUTTON_LEFT;
		case SDLK_s: return Controller::PAD_BUTTON_DOWN;
		case SDLK_d: return Controller::PAD_BUTTON_RIGHT;
		case SDLK_j: return Controller::PAD_BUTTON_CROSS;
		case SDLK_i: return Controller::PAD_BUTTON_TRIANGLE;
		case SDLK_k: return Controller::PAD_BUTTON_SQUARE;
		case SDLK_l: return Controller::PAD_BUTTON_CIRCLE;
		case SDLK_q: return Controller::PAD_BUTTON_L1;
		case SDLK_e: return Controller::PAD_BUTTON_R1;
		case SDLK_RETURN:
		case SDLK_RETURN2: return Controller::PAD_BUTTON_OPTIONS;
		case SDLK_BACKSPACE:
		case SDLK_TAB: return Controller::PAD_BUTTON_TOUCH_PAD;
		default: return 0;
	}
}

const std::vector<InputBinding>& DefaultKeyboardBindings() {
	static const std::vector<InputBinding> bindings = {
	    {SDLK_w, PAD_BUTTON_UP},        {SDLK_a, PAD_BUTTON_LEFT},
	    {SDLK_s, PAD_BUTTON_DOWN},      {SDLK_d, PAD_BUTTON_RIGHT},
	    {SDLK_j, PAD_BUTTON_CROSS},     {SDLK_i, PAD_BUTTON_TRIANGLE},
	    {SDLK_k, PAD_BUTTON_SQUARE},    {SDLK_l, PAD_BUTTON_CIRCLE},
	    {SDLK_q, PAD_BUTTON_L1},        {SDLK_e, PAD_BUTTON_R1},
	    {SDLK_RETURN, PAD_BUTTON_OPTIONS},
	    {SDLK_RETURN2, PAD_BUTTON_OPTIONS},
	    {SDLK_BACKSPACE, PAD_BUTTON_TOUCH_PAD},
	    {SDLK_TAB, PAD_BUTTON_TOUCH_PAD},
	};
	return bindings;
}

uint32_t DefaultControllerPadButton(int button) {
	switch (button) {
		case SDL_CONTROLLER_BUTTON_A: return Controller::PAD_BUTTON_CROSS;
		case SDL_CONTROLLER_BUTTON_B: return Controller::PAD_BUTTON_CIRCLE;
		case SDL_CONTROLLER_BUTTON_X: return Controller::PAD_BUTTON_SQUARE;
		case SDL_CONTROLLER_BUTTON_Y: return Controller::PAD_BUTTON_TRIANGLE;
		case SDL_CONTROLLER_BUTTON_BACK: return Controller::PAD_BUTTON_TOUCH_PAD;
		case SDL_CONTROLLER_BUTTON_START: return Controller::PAD_BUTTON_OPTIONS;
		case SDL_CONTROLLER_BUTTON_LEFTSTICK: return Controller::PAD_BUTTON_L3;
		case SDL_CONTROLLER_BUTTON_RIGHTSTICK: return Controller::PAD_BUTTON_R3;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Controller::PAD_BUTTON_L1;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Controller::PAD_BUTTON_R1;
		case SDL_CONTROLLER_BUTTON_DPAD_UP: return Controller::PAD_BUTTON_UP;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Controller::PAD_BUTTON_DOWN;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Controller::PAD_BUTTON_LEFT;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Controller::PAD_BUTTON_RIGHT;
		case SDL_CONTROLLER_BUTTON_TOUCHPAD: return Controller::PAD_BUTTON_TOUCH_PAD;
		default: return 0;
	}
}

const std::vector<InputBinding>& DefaultControllerBindings() {
	static const std::vector<InputBinding> bindings = {
	    {SDL_CONTROLLER_BUTTON_A, PAD_BUTTON_CROSS},
	    {SDL_CONTROLLER_BUTTON_B, PAD_BUTTON_CIRCLE},
	    {SDL_CONTROLLER_BUTTON_X, PAD_BUTTON_SQUARE},
	    {SDL_CONTROLLER_BUTTON_Y, PAD_BUTTON_TRIANGLE},
	    {SDL_CONTROLLER_BUTTON_BACK, PAD_BUTTON_TOUCH_PAD},
	    {SDL_CONTROLLER_BUTTON_START, PAD_BUTTON_OPTIONS},
	    {SDL_CONTROLLER_BUTTON_LEFTSTICK, PAD_BUTTON_L3},
	    {SDL_CONTROLLER_BUTTON_RIGHTSTICK, PAD_BUTTON_R3},
	    {SDL_CONTROLLER_BUTTON_LEFTSHOULDER, PAD_BUTTON_L1},
	    {SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, PAD_BUTTON_R1},
	    {SDL_CONTROLLER_BUTTON_DPAD_UP, PAD_BUTTON_UP},
	    {SDL_CONTROLLER_BUTTON_DPAD_DOWN, PAD_BUTTON_DOWN},
	    {SDL_CONTROLLER_BUTTON_DPAD_LEFT, PAD_BUTTON_LEFT},
	    {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, PAD_BUTTON_RIGHT},
	    {SDL_CONTROLLER_BUTTON_TOUCHPAD, PAD_BUTTON_TOUCH_PAD},
	};
	return bindings;
}

} // namespace Libs::Controller

namespace Libs::Controller {

// Mirrors DS5EffectsState_t from SDL's bundled hidapi PS5 driver (SDL_hidapi_ps5.c), which in
// turn is verified against Sony's own upstreamed Linux kernel driver's
// dualsense_output_report_common (hid-playstation.c, GPL-2.0-or-later). Both agree on this exact
// 47-byte layout. Sent as-is through SDL_GameControllerSendEffect(), which lets SDL2's own
// HIDAPI PS5 driver handle the USB/Bluetooth framing (headers, CRC32) transparently -- so this
// works unmodified on both Windows and Linux.
struct DualSenseEffectsReport {
	uint8_t enable_bits1               = 0;
	uint8_t enable_bits2               = 0;
	uint8_t rumble_right               = 0;
	uint8_t rumble_left                = 0;
	uint8_t headphone_volume           = 0;
	uint8_t speaker_volume             = 0;
	uint8_t microphone_volume          = 0;
	uint8_t audio_enable_bits          = 0;
	uint8_t mic_light_mode             = 0;
	uint8_t audio_mute_bits            = 0;
	uint8_t right_trigger_effect[11]   = {};
	uint8_t left_trigger_effect[11]    = {};
	uint8_t unknown1[6]                = {};
	uint8_t enable_bits3               = 0;
	uint8_t unknown2[2]                = {};
	uint8_t led_anim                   = 0;
	uint8_t led_brightness             = 0;
	uint8_t pad_lights                 = 0;
	uint8_t led_red                    = 0;
	uint8_t led_green                  = 0;
	uint8_t led_blue                   = 0;
};

static_assert(sizeof(DualSenseEffectsReport) == 47);

// Enable-bit flags for enable_bits1/enable_bits2/audio_mute_bits. The trigger-effect and
// mic-light bits are cross-referenced against widely-corroborated community reverse-engineering
// of the DualSense output report and against SDL_hidapi_ps5.c's own k_EDS5Effect* enum. The
// audio-related bits are directly verified against Linux's hid-playstation.c
// (DS_OUTPUT_VALID_FLAG0_SPEAKER_VOLUME_ENABLE/MIC_VOLUME_ENABLE,
// DS_OUTPUT_VALID_FLAG1_MIC_MUTE_LED_CONTROL_ENABLE/POWER_SAVE_CONTROL_ENABLE, and
// DS_OUTPUT_POWER_SAVE_CONTROL_MIC_MUTE), which is an authoritative Sony-authored source.
constexpr uint8_t DS5_ENABLE1_RIGHT_TRIGGER_EFFECT = 0x04;
constexpr uint8_t DS5_ENABLE1_LEFT_TRIGGER_EFFECT  = 0x08;
constexpr uint8_t DS5_ENABLE1_SPEAKER_VOLUME       = 0x20;
constexpr uint8_t DS5_ENABLE1_MIC_VOLUME           = 0x40;
constexpr uint8_t DS5_ENABLE2_MIC_LIGHT            = 0x01;
constexpr uint8_t DS5_ENABLE2_POWER_SAVE_CONTROL   = 0x02;
constexpr uint8_t DS5_POWER_SAVE_MIC_MUTE          = 0x10;

// HD Haptics enable bit for enable_bits3. When set, the unknown1[6] field in the
// DualSenseEffectsReport is interpreted as custom haptics frequency/amplitude data for the
// voice-coil actuators. Verified against widely-corroborated community reverse-engineering
// of the DualSense HID output report (same sources that identified the adaptive trigger layout).
constexpr uint8_t DS5_ENABLE3_CUSTOM_HAPTICS = 0x04;

// Custom haptics data layout within the unknown1[6] field (byte 24-29 of the 47-byte report).
// Each voice-coil actuator gets a 16-bit frequency (Hz, little-endian) and 8-bit amplitude.
// The DualSense's voice-coil actuators support approximately 20-500 Hz, with the most
// perceptible haptic detail in the 40-200 Hz range.
constexpr uint8_t DS5_HAPTICS_LEFT_FREQ_LO  = 0;
constexpr uint8_t DS5_HAPTICS_LEFT_FREQ_HI  = 1;
constexpr uint8_t DS5_HAPTICS_RIGHT_FREQ_LO = 2;
constexpr uint8_t DS5_HAPTICS_RIGHT_FREQ_HI = 3;
constexpr uint8_t DS5_HAPTICS_LEFT_AMP      = 4;
constexpr uint8_t DS5_HAPTICS_RIGHT_AMP     = 5;

// Default haptic frequency used when a game only sets amplitude via the standard rumble API
// (PadSetVibration) without specifying a frequency. 150 Hz is in the middle of the DualSense's
// most perceptible range and produces a clean, crisp feel — close to what first-party titles
// use for general-purpose haptic feedback (e.g. footsteps, UI interactions).
constexpr uint16_t DS5_HAPTICS_DEFAULT_FREQ_HZ = 150;

// Per-controller HD haptics state: tracks the last-set frequency and amplitude for each
// voice-coil actuator. This allows games that call PadSetVibration (which only sets amplitude)
// to still get frequency-rich HD haptics rather than falling back to the old rumble-only path.
struct HapticState {
	uint16_t left_freq_hz  = DS5_HAPTICS_DEFAULT_FREQ_HZ;
	uint16_t right_freq_hz = DS5_HAPTICS_DEFAULT_FREQ_HZ;
	uint8_t  left_amp      = 0;
	uint8_t  right_amp     = 0;
};
static std::unordered_map<int, HapticState> g_haptic_state;
static std::mutex                           g_haptic_mutex;

// Sends a full DualSense HID output report with the current haptic frequency/amplitude for
// both voice-coil actuators. Called by ControllerSetRumble (amplitude-only) and
// ControllerSetHapticEffect (frequency + amplitude). The enable_bits3 flag gates the custom
// haptics data; when both amplitudes are 0, the flag is cleared to let the controller idle.
static void DualSenseSendHapticReport(SDL_GameController* pad, int id) {
	std::lock_guard<std::mutex> lock(g_haptic_mutex);
	auto it = g_haptic_state.find(id);
	if (it == g_haptic_state.end()) return;

	DualSenseEffectsReport report {};
	const auto& state = it->second;

	// Pack custom haptics frequency/amplitude into the unknown1[6] field
	report.unknown1[DS5_HAPTICS_LEFT_FREQ_LO]  = static_cast<uint8_t>(state.left_freq_hz & 0xFF);
	report.unknown1[DS5_HAPTICS_LEFT_FREQ_HI]  = static_cast<uint8_t>((state.left_freq_hz >> 8) & 0xFF);
	report.unknown1[DS5_HAPTICS_RIGHT_FREQ_LO] = static_cast<uint8_t>(state.right_freq_hz & 0xFF);
	report.unknown1[DS5_HAPTICS_RIGHT_FREQ_HI] = static_cast<uint8_t>((state.right_freq_hz >> 8) & 0xFF);
	report.unknown1[DS5_HAPTICS_LEFT_AMP]      = state.left_amp;
	report.unknown1[DS5_HAPTICS_RIGHT_AMP]     = state.right_amp;

	// Only set the custom haptics enable bit if at least one actuator has non-zero amplitude.
	// This lets the controller fall back to its own idle/low-power state when silent.
	if (state.left_amp > 0 || state.right_amp > 0) {
		report.enable_bits3 = DS5_ENABLE3_CUSTOM_HAPTICS;
	}

	SDL_GameControllerSendEffect(pad, &report, sizeof(report));
}

void ControllerSetRumble(int id, uint8_t large_motor, uint8_t small_motor) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	// Map the legacy large/small motor values to the DualSense's voice-coil actuators.
	// On real hardware, the "large motor" (low-frequency) maps to the left actuator and
	// "small motor" (high-frequency) maps to the right actuator. We preserve the frequency
	// that was last set (or the default 150 Hz) and only update the amplitude, giving games
	// that use the standard PadSetVibration API frequency-rich HD haptics automatically.
	// This replaces the old SDL_GameControllerRumble path, which only produced basic
	// on/off rumble without frequency control.
	{
		std::lock_guard<std::mutex> lock(g_haptic_mutex);
		auto& state = g_haptic_state[id];
		state.left_amp  = large_motor;
		state.right_amp = small_motor;
	}

	DualSenseSendHapticReport(pad, id);
}

// Sets HD haptics with explicit frequency and amplitude for each voice-coil actuator.
// This is the full-featured API that games can use for fine-grained haptic feedback.
// frequency_hz: 20-500 Hz range (40-200 Hz is most perceptible)
// left_amp / right_amp: 0-255 amplitude for each actuator
// When called with amplitude 0, the actuator is silenced but the frequency is preserved
// for the next non-zero call.
void ControllerSetHapticEffect(int id, uint16_t left_freq_hz, uint8_t left_amp,
                                uint16_t right_freq_hz, uint8_t right_amp) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	{
		std::lock_guard<std::mutex> lock(g_haptic_mutex);
		auto& state = g_haptic_state[id];
		state.left_freq_hz  = left_freq_hz;
		state.left_amp      = left_amp;
		state.right_freq_hz = right_freq_hz;
		state.right_amp     = right_amp;
	}

	DualSenseSendHapticReport(pad, id);
}

void ControllerSetLightBar(int id, uint8_t r, uint8_t g, uint8_t b) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr || !SDL_GameControllerHasLED(pad)) {
		return;
	}

	SDL_GameControllerSetLED(pad, r, g, b);
}

void ControllerSetPlayerIndex(int id, int player_index) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	// SDL's PS5 HIDAPI driver reproduces the same 5-LED centered mapping the console itself
	// uses (see SetLightsForPlayerIndex()/dualsense_set_player_leds() in hid-playstation.c),
	// triggered automatically whenever the player index changes.
	SDL_GameControllerSetPlayerIndex(pad, player_index);
}

static void WriteTriggerCommand(uint8_t* dest, const Controller::DualSenseTriggerCommand& cmd) {
	dest[0] = cmd.mode;
	std::memcpy(dest + 1, cmd.param, sizeof(cmd.param));
}

void DualSenseSetTriggerEffect(int id, const Controller::DualSenseTriggerCommand& left,
                               const Controller::DualSenseTriggerCommand& right) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	DualSenseEffectsReport report {};
	report.enable_bits1 = DS5_ENABLE1_RIGHT_TRIGGER_EFFECT | DS5_ENABLE1_LEFT_TRIGGER_EFFECT;
	WriteTriggerCommand(report.right_trigger_effect, right);
	WriteTriggerCommand(report.left_trigger_effect, left);

	SDL_GameControllerSendEffect(pad, &report, sizeof(report));
}

void DualSenseSetMicMuted(int id, bool muted, uint8_t led_mode) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	DualSenseEffectsReport report {};
	// valid_flag1 bits: enable both the mic-mute LED control and the power-save control that
	// actually gates the microphone hardware (see hid-playstation.c's dualsense_output_worker).
	report.enable_bits2   = static_cast<uint8_t>(DS5_ENABLE2_MIC_LIGHT | DS5_ENABLE2_POWER_SAVE_CONTROL);
	report.mic_light_mode = led_mode;
	report.audio_mute_bits =
	    static_cast<uint8_t>(muted ? DS5_POWER_SAVE_MIC_MUTE : 0);

	SDL_GameControllerSendEffect(pad, &report, sizeof(report));
}

void DualSenseSetAudioVolume(int id, uint8_t speaker_volume, uint8_t mic_volume) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	DualSenseEffectsReport report {};
	report.enable_bits1      = static_cast<uint8_t>(DS5_ENABLE1_SPEAKER_VOLUME | DS5_ENABLE1_MIC_VOLUME);
	report.speaker_volume    = speaker_volume;
	report.microphone_volume = mic_volume;

	SDL_GameControllerSendEffect(pad, &report, sizeof(report));
}

void ControllerSetMotionSensorsEnabled(int id, bool enabled) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	if (SDL_GameControllerHasSensor(pad, SDL_SENSOR_GYRO)) {
		SDL_GameControllerSetSensorEnabled(pad, SDL_SENSOR_GYRO, enabled ? SDL_TRUE : SDL_FALSE);
	}
	if (SDL_GameControllerHasSensor(pad, SDL_SENSOR_ACCEL)) {
		SDL_GameControllerSetSensorEnabled(pad, SDL_SENSOR_ACCEL, enabled ? SDL_TRUE : SDL_FALSE);
	}
}

// Touchpad resolution matches what PadGetControllerInformation already reports to guests.
constexpr float TOUCHPAD_RESOLUTION_X = 1920.0f;
constexpr float TOUCHPAD_RESOLUTION_Y = 943.0f;

static uint16_t NormalizedTouchToPixel(float normalized, float resolution) {
	const float clamped = (normalized < 0.0f ? 0.0f : (normalized > 1.0f ? 1.0f : normalized));
	return static_cast<uint16_t>(clamped * resolution);
}

void ControllerPollExtendedState(int id, bool motion_enabled, ControllerExtendedState* out) {
	EXIT_IF(out == nullptr);

	*out = ControllerExtendedState {};

	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return;
	}

	if (SDL_GameControllerGetNumTouchpads(pad) > 0) {
		Uint8 state    = 0;
		float x        = 0.0f;
		float y        = 0.0f;
		float pressure = 0.0f;
		if (SDL_GameControllerGetTouchpadFinger(pad, 0, 0, &state, &x, &y, &pressure) == 0 &&
		    state != 0) {
			out->touch0_active = true;
			out->touch0_x      = NormalizedTouchToPixel(x, TOUCHPAD_RESOLUTION_X);
			out->touch0_y      = NormalizedTouchToPixel(y, TOUCHPAD_RESOLUTION_Y);
		}
		if (SDL_GameControllerGetTouchpadFinger(pad, 0, 1, &state, &x, &y, &pressure) == 0 &&
		    state != 0) {
			out->touch1_active = true;
			out->touch1_x      = NormalizedTouchToPixel(x, TOUCHPAD_RESOLUTION_X);
			out->touch1_y      = NormalizedTouchToPixel(y, TOUCHPAD_RESOLUTION_Y);
		}
	}

	// DualSense Edge extras: SDL's builtin PS5 mapping (SDL_gamecontroller.c) only assigns these
	// PADDLE1..4 slots when the connected device is actually detected as an Edge, so this is a
	// harmless SDL_FALSE/no-op query on a standard DualSense.
	if (SDL_GameControllerHasButton(pad, SDL_CONTROLLER_BUTTON_PADDLE1)) {
		out->edge_paddle_right = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_PADDLE1) != 0;
		out->edge_paddle_left  = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_PADDLE2) != 0;
		out->edge_function_right =
		    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_PADDLE3) != 0;
		out->edge_function_left =
		    SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_PADDLE4) != 0;
	}

	if (motion_enabled) {
		float gyro[3]  = {0.0f, 0.0f, 0.0f};
		float accel[3] = {0.0f, 0.0f, 0.0f};
		// SDL's gyro (rad/s) and accel (m/s^2) use the same physical units as ScePadData's
		// angular_velocity/acceleration fields, so no conversion is needed here.
		const bool has_gyro  = SDL_GameControllerGetSensorData(pad, SDL_SENSOR_GYRO, gyro, 3) == 0;
		const bool has_accel = SDL_GameControllerGetSensorData(pad, SDL_SENSOR_ACCEL, accel, 3) == 0;
		if (has_gyro || has_accel) {
			out->motion_valid = true;
			out->gyro_x       = gyro[0];
			out->gyro_y       = gyro[1];
			out->gyro_z       = gyro[2];
			out->accel_x      = accel[0];
			out->accel_y      = accel[1];
			out->accel_z      = accel[2];
		}
	}
}

BatteryLevel ControllerGetBatteryLevel(int id) {
	auto* pad = SDL_GameControllerFromInstanceID(id);
	if (pad == nullptr) {
		return BatteryLevel::Unknown;
	}

	auto* joystick = SDL_GameControllerGetJoystick(pad);
	if (joystick == nullptr) {
		return BatteryLevel::Unknown;
	}

	switch (SDL_JoystickCurrentPowerLevel(joystick)) {
		case SDL_JOYSTICK_POWER_EMPTY: return BatteryLevel::Empty;
		case SDL_JOYSTICK_POWER_LOW: return BatteryLevel::Low;
		case SDL_JOYSTICK_POWER_MEDIUM: return BatteryLevel::Medium;
		case SDL_JOYSTICK_POWER_FULL: return BatteryLevel::Full;
		case SDL_JOYSTICK_POWER_WIRED: return BatteryLevel::Wired;
		default: return BatteryLevel::Unknown;
	}
}

} // namespace Libs::Controller
