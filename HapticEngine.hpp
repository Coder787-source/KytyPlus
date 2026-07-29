#pragma once
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include "DualSenseEmulator.hpp"

class HapticEngine {
public:
    HapticEngine(IDualSenseHardware* hardware) 
        : m_hardware(hardware), m_running(true) {
        m_worker = std::thread(&HapticEngine::ProcessQueue, this);
    }

    ~HapticEngine() {
        m_running = false;
        m_cv.notify_all();
        if (m_worker.joinable()) m_worker.join();
    }

    void QueueWaveform(const HapticWaveform& waveform) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_waveformQueue.push(waveform);
        }
        m_cv.notify_one();
    }

private:
    void ProcessQueue() {
        while (m_running) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this] { return !m_waveformQueue.empty() || !m_running; });

            if (!m_running) break;

            HapticWaveform currentWave = m_waveformQueue.front();
            m_waveformQueue.pop();
            lock.unlock();

            if (m_hardware) {
                m_hardware->StreamHapticData(currentWave);
            }
        }
    }

    IDualSenseHardware* m_hardware;
    std::queue<HapticWaveform> m_waveformQueue;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<bool> m_running;
    std::thread m_worker;
};
