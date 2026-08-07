#ifndef EMULATOR_SRC_GRAPHICS_PRESENTATION_PRESENTER_H_
#define EMULATOR_SRC_GRAPHICS_PRESENTATION_PRESENTER_H_

#include "common/common.h"

#include <atomic>
#include <memory>
#include <string>

namespace Libs::Graphics {

class CommandBuffer;
class RenderContext;
struct ImageInfo;
struct WindowContext;

class Presenter final {
public:
	struct Frame;

	explicit Presenter(WindowContext& window);
	~Presenter();
	KYTY_CLASS_NO_COPY(Presenter);

	[[nodiscard]] Frame& PrepareFrame(CommandBuffer& command, const ImageInfo& info);
	[[nodiscard]] Frame& PrepareBlankFrame(uint32_t width, uint32_t height, bool opaque,
	                                       CommandBuffer* producer = nullptr);
	[[nodiscard]] Frame* PrepareLastFrame();
	[[nodiscard]] bool   IsGuestPaused() const noexcept;
	[[nodiscard]] RenderContext& Renderer() const noexcept;
	void                 Present(Frame& frame, bool reuse = false);
	void                 Discard(Frame& frame);

	// Captures the current swapchain image to a PNG. Returns the written path, or empty on
	// failure. Must be called on the presentation thread after a frame has been presented.
	[[nodiscard]] std::string CaptureScreenshot();

	// Marks a screenshot as requested for the next present. Thread-safe; the actual
	// capture runs on the presentation thread immediately after it presents a frame.
	void RequestScreenshot() noexcept { m_screenshot_requested.store(true, std::memory_order_release); }
	// Atomically tests-and-clears the pending-screenshot flag. Called by the presentation
	// thread after it has just presented a frame.
	[[nodiscard]] bool IsScreenshotRequested() noexcept {
		return m_screenshot_requested.exchange(false, std::memory_order_acq_rel);
	}

private:
	struct Impl;
	std::unique_ptr<Impl> m_impl;

	std::atomic_bool m_screenshot_requested {false};
};
} // namespace Libs::Graphics

#endif // EMULATOR_SRC_GRAPHICS_PRESENTATION_PRESENTER_H_
