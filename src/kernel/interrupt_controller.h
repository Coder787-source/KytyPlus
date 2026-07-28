#ifndef KYTY_INTERRUPT_CONTROLLER_H
#define KYTY_INTERRUPT_CONTROLLER_H

#include <vector>
#include <mutex>
#include <queue>
#include <functional>
#include <iostream>
#include <atomic>

namespace Emulator {

/**
 * @brief Handles Guest Hardware Interrupts.
 * This is essential for timing-sensitive game loops and I/O.
 */
class InterruptController {
public:
    InterruptController() : running_(true) {}
    ~InterruptController() { Stop(); }

    /**
     * @brief Triggers a hardware interrupt.
     * @param irq The Interrupt Request line.
     */
    void RaiseInterrupt(uint32_t irq) {
        std::lock_guard<std::mutex> lock(mutex_);
        interrupt_queue_.push(irq);
        std::cout << "[IRQ] Interrupt Raised: " << irq << std::endl;
    }

    /**
     * @brief Checks if there are pending interrupts for the CPU.
     */
    bool HasPendingInterrupts() {
        return !interrupt_queue_.empty();
    }

    /**
     * @brief Consumes the next pending interrupt.
     */
    uint32_t AcknowledgeInterrupt() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (interrupt_queue_.empty()) return 0;
        uint32_t irq = interrupt_queue_.front();
        interrupt_queue_.pop();
        return irq;
    }

    void Stop() { running_ = false; }

private:
    std::mutex mutex_;
    std::queue<uint32_t> interrupt_queue_;
    std::atomic<bool> running_;
};

} // namespace Emulator

#endif // KYTY_INTERRUPT_CONTROLLER_H
