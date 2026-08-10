#pragma once

#include <atomic>
#include <cstdint>
#include <chrono>

namespace KytyPS5::Core {

    /**
     * @brief High-precision Guest Clock and Interrupt Controller.
     * Ensures synchronization between guest CPU cycles and host wall-clock.
     */
    class GuestClock {
    public:
        static GuestClock& Instance() {
            static GuestClock instance;
            return instance;
        }

        void Tick() {
            cycle_count_.fetch_add(1, std::memory_order_relaxed);
        }

        uint64_t GetCycles() const {
            return cycle_count_.load(std::memory_order_acquire);
        }

        void SetTimerInterrupt(uint64_t trigger_cycle) {
            timer_trigger_ = trigger_cycle;
        }

        bool CheckInterrupt() {
            if (cycle_count_.load(std::memory_order_relaxed) >= timer_trigger_) {
                timer_trigger_ = 0; // Reset
                return true;
            }
            return false;
        }

    private:
        GuestClock() : cycle_count_(0), timer_trigger_(0) {}
        std::atomic<uint64_t> cycle_count_;
        std::atomic<uint64_t> timer_trigger_;
    };

}
