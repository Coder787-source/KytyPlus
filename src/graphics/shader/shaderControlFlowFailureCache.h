// SPDX-License-Identifier: MIT
#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Libs::Graphics {

// Cache only CFG decode failures and static unknown-SSA descriptor provenance.
// These precede runtime descriptor materialization. Runtime resource, validation,
// specialization and emission failures remain retryable. Exact code and compute
// layout comparisons make replacing code or changing stage registers safe.
class ShaderControlFlowFailureCache {
public:
    bool Contains(uint64_t address, std::span<const uint32_t> code,
                  std::span<const uint32_t> layout = {}) {
        std::scoped_lock lock(m_mutex);
        const auto found = m_entries.find(address);
        if (found == m_entries.end()) return false;
        if (found->second.code.size() == code.size() &&
            std::equal(code.begin(), code.end(), found->second.code.begin()) &&
            found->second.layout.size() == layout.size() &&
            std::equal(layout.begin(), layout.end(), found->second.layout.begin())) return true;
        m_words -= found->second.code.size();
        m_entries.erase(found);
        return false;
    }

    bool Remember(uint64_t address, std::span<const uint32_t> code, std::string_view error,
                  std::span<const uint32_t> layout = {}) {
        const bool cfg = error.starts_with("unsupported control-flow instruction in CFG at pc ");
        const bool static_provenance = error.starts_with("shader resource tracking: ") &&
            error.find(" contains an unknown value ") != std::string_view::npos;
        if ((!cfg && !static_provenance) || code.empty() || code.size() > MaxWordsPerShader ||
            layout.size() > 64) return false;
        std::scoped_lock lock(m_mutex);
        const auto existing = m_entries.find(address);
        if (existing != m_entries.end()) {
            m_words -= existing->second.code.size();
            m_entries.erase(existing);
        }
        if (m_entries.size() >= MaxEntries || m_words + code.size() > MaxWords) {
            m_entries.clear();
            m_words = 0;
        }
        m_entries.emplace(address, Entry {std::vector<uint32_t>(code.begin(), code.end()),
                                         std::vector<uint32_t>(layout.begin(), layout.end())});
        m_words += code.size();
        return true;
    }

private:
    static constexpr size_t MaxEntries = 128;
    static constexpr size_t MaxWordsPerShader = 256 * 1024;
    static constexpr size_t MaxWords = 4 * 1024 * 1024;
    std::mutex m_mutex;
    struct Entry { std::vector<uint32_t> code; std::vector<uint32_t> layout; };
    std::unordered_map<uint64_t, Entry> m_entries;
    size_t m_words = 0;
};

} // namespace Libs::Graphics
