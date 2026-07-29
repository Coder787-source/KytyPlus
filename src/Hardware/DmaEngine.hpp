#pragma once

#include <vector>
#include <queue>
#include <mutex>
#include <future>
#include <expected>
#include "Kernel/SyscallDispatcher.hpp"

namespace KytyPS5::Hardware {

    struct DmaRequest {
        uint64_t source_addr;
        uint64_t dest_addr;
        size_t size;
        std::promise<std::expected<void, std::string>> promise;
    };

    /**
     * @brief Simulates PS5 DMA Engine for asynchronous memory transfers.
     * Decouples I/O from CPU execution to prevent pipeline stalls.
     */
    class DmaEngine {
    public:
        static DmaEngine& Instance() {
            static DmaEngine instance;
            return instance;
        }

        std::future<std::expected<void, std::string>> Transfer(uint64_t src, uint64_t dst, size_t size) {
            auto request = std::make_unique<DmaRequest>(src, dst, size);
            auto future = request->promise.get_future();
            
            {
                std::lock_guard lock(queue_mutex_);
                request_queue_.push(std::move(request));
            }
            cv_.notify_one();
            return future;
        }

        void WorkerThread() {
            while (running_) {
                std::unique_lock lock(queue_mutex_);
                cv_.wait(lock, [this] { return !request_queue_.empty() || !running_; });

                if (!running_) break;

                auto req = std::move(request_queue_.front());
                request_queue_.pop();
                lock.unlock();

                ProcessTransfer(std::move(req));
            }
        }

        void Shutdown() {
            running_ = false;
            cv_.notify_all();
        }

    private:
        DmaEngine() : running_(true) {}
        
        void ProcessTransfer(std::unique_ptr<DmaRequest> req) {
            auto src_ptr = Kernel::MemoryManager::Instance().Translate(req->source_addr);
            auto dst_ptr = Kernel::MemoryManager::Instance().Translate(req->dest_addr);

            if (!src_ptr || !dst_ptr) {
                req->promise.set_value(std::unexpected("DMA Address Translation Failed"));
                return;
            }

            std::memcpy(dst_ptr.value(), src_ptr.value(), req->size);
            req->promise.set_value({});
        }

        std::atomic<bool> running_;
        std::queue<std::unique_ptr<DmaRequest>> request_queue_;
        std::mutex queue_mutex_;
        std::condition_variable cv_;
    };

}
