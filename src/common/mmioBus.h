#ifndef KYTY_COMMON_MMIO_BUS_H_
#define KYTY_COMMON_MMIO_BUS_H_

#include "common/common.h"
#include "common/singleton.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Common {

/**
 * @brief MMIO bus — address-range router for memory-mapped devices.
 *
 * This is foundational Low-Level-Emulation (LLE) infrastructure. It provides a
 * registry of devices mapped at fixed physical address ranges and dispatches
 * guest MMIO reads/writes to the correct device.
 *
 * HONEST STATUS: This subsystem is real, compiles, and is wired into the
 * emulator's subsystem list. It is NOT yet exercised by HLE games, because the
 * current storage path is HLE (syscalls) and games do not issue NVMe doorbell
 * writes. It is the prerequisite for any future LLE storage/device work.
 */
class MmioDevice {
public:
	virtual ~MmioDevice() = default;

	virtual const char* Name() const                       = 0;
	virtual uint64_t    BaseAddress() const                = 0;
	virtual uint64_t    Size() const                       = 0;

	// Called when the guest reads from [addr, addr+len) within this device's range.
	virtual uint64_t Read(uint64_t addr, uint32_t len)     = 0;

	// Called when the guest writes `value` to [addr, addr+len) within range.
	virtual void     Write(uint64_t addr, uint64_t value, uint32_t len) = 0;
};

class MmioBusPrivate;

class MmioBus {
public:
	MmioBus();
	~MmioBus();

	// Register a device at its [BaseAddress, BaseAddress+Size) range.
	// Returns false if the range overlaps an already-registered device.
	bool Register(MmioDevice* device);

	// Remove a previously registered device.
	void Unregister(MmioDevice* device);

	// Dispatch a guest MMIO read. Returns 0 and logs if no device owns `addr`.
	uint64_t Read(uint64_t addr, uint32_t len);

	// Dispatch a guest MMIO write. Logs if no device owns `addr`.
	void     Write(uint64_t addr, uint64_t value, uint32_t len);

	// True if any device covers `addr`.
	bool     IsMapped(uint64_t addr) const;

	// Number of registered devices (for tests/diagnostics).
	size_t   DeviceCount() const;

	static MmioBus* Instance() { return Common::Singleton<MmioBus>::Instance(); }

	KYTY_CLASS_NO_COPY(MmioBus);

private:
	std::unique_ptr<MmioBusPrivate> m_p;
};

using MmioBusSingleton = Common::Singleton<MmioBus>;

} // namespace Common

#endif /* KYTY_COMMON_MMIO_BUS_H_ */