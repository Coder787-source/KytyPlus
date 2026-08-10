#pragma once

#include <memory>
#include <vector>

#include "ICache.hpp"
#include "JitDispatcher.hpp"

namespace KytyPS5::Core {

class JitEngine {
public:
	JitEngine()
	    : icache_(std::make_unique<ICache>()), dispatcher_(std::make_unique<JitDispatcher>()) {}

	JitDispatcher& Dispatcher() { return *dispatcher_; }
	ICache& Cache() { return *icache_; }

private:
	std::unique_ptr<ICache> icache_;
	std::unique_ptr<JitDispatcher> dispatcher_;
};

} // namespace KytyPS5::Core
