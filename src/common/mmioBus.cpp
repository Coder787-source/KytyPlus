#include "common/mmioBus.h"

#include "common/assert.h"

#include <algorithm>

namespace Common {

namespace {

struct MappedDevice {
	MmioDevice* device = nullptr;
	uint64_t    base   = 0;
	uint64_t    size   = 0;
};

bool Overlaps(const MappedDevice& a, const MappedDevice& b) {
	return a.base < (b.base + b.size) && b.base < (a.base + a.size);
}

} // namespace

class MmioBusPrivate {
public:
	std::vector<MappedDevice> devices;
	mutable std::mutex        mutex;

	MappedDevice* Find(uint64_t addr) {
		for (auto& d : devices) {
			if (addr >= d.base && addr < (d.base + d.size)) {
				return &d;
			}
		}
		return nullptr;
	}
};

MmioBus::MmioBus(): m_p(std::make_unique<MmioBusPrivate>()) {}
MmioBus::~MmioBus() = default;

bool MmioBus::Register(MmioDevice* device) {
	EXIT_IF(!device || device->Size() == 0);

	std::lock_guard<std::mutex> lock(m_p->mutex);

	MappedDevice entry { device, device->BaseAddress(), device->Size() };

	for (const auto& existing : m_p->devices) {
		if (Overlaps(entry, existing)) {
			printf("MmioBus: range overlap registering %s [%llx..%llx) with %s\n",
			       device->Name(), (unsigned long long)entry.base,
			       (unsigned long long)(entry.base + entry.size), existing.device->Name());
			return false;
		}
	}

	m_p->devices.push_back(entry);
	return true;
}

void MmioBus::Unregister(MmioDevice* device) {
	std::lock_guard<std::mutex> lock(m_p->mutex);

	m_p->devices.erase(
	    std::remove_if(m_p->devices.begin(), m_p->devices.end(),
	                   [device](const MappedDevice& d) { return d.device == device; }),
	    m_p->devices.end());
}

uint64_t MmioBus::Read(uint64_t addr, uint32_t len) {
	std::lock_guard<std::mutex> lock(m_p->mutex);

	if (auto* entry = m_p->Find(addr)) {
		return entry->device->Read(addr - entry->base, len);
	}

	printf("MmioBus: unmapped read @ %llx (%u bytes)\n", (unsigned long long)addr, len);
	return 0;
}

void MmioBus::Write(uint64_t addr, uint64_t value, uint32_t len) {
	std::lock_guard<std::mutex> lock(m_p->mutex);

	if (auto* entry = m_p->Find(addr)) {
		entry->device->Write(addr - entry->base, value, len);
		return;
	}

	printf("MmioBus: unmapped write @ %llx (%u bytes) = %llx\n", (unsigned long long)addr, len,
	       (unsigned long long)value);
}

bool MmioBus::IsMapped(uint64_t addr) const {
	std::lock_guard<std::mutex> lock(m_p->mutex);

	for (const auto& d : m_p->devices) {
		if (addr >= d.base && addr < (d.base + d.size)) {
			return true;
		}
	}
	return false;
}

size_t MmioBus::DeviceCount() const {
	std::lock_guard<std::mutex> lock(m_p->mutex);
	return m_p->devices.size();
}

} // namespace Common