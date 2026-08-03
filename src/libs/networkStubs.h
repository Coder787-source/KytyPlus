#pragma once

#include "common/common.h"
#include <cstdint>
#include <string>
#include <functional>

namespace Libs {

/**
 * @brief Enhanced Network Stubs for Rockstar Games
 * 
 * Provides improved network stubs that allow story mode to function
 * while gracefully blocking online features that require PSN.
 * 
 * Features:
 * - Story mode network calls return success
 * - Online features return "unavailable" errors
 * - No crashes on network calls
 * - Logging for debugging
 */
namespace NetworkStubs {

/**
 * @brief Network service types
 */
enum class NetworkService : uint32_t {
    StoryMode       = 0,  ///< Story mode features (allowed)
    Multiplayer     = 1,  ///< Multiplayer features (blocked)
    SocialClub      = 2,  ///< Rockstar Social Club (blocked)
    DLCDownload     = 3,  ///< DLC downloads (blocked)
    CloudSaves      = 4,  ///< Cloud saves (blocked)
    Leaderboards    = 5,  ///< Leaderboards (blocked)
    Telemetry       = 6,  ///< Game telemetry (blocked)
    Matchmaking     = 7   ///< Matchmaking (blocked)
};

/**
 * @brief Network connection states
 */
enum class ConnectionState : uint32_t {
    Disconnected    = 0,
    Connecting      = 1,
    Connected       = 2,
    Failed          = 3,
    Timeout         = 4
};

/**
 * @brief Network error codes (Rockstar-style)
 */
enum class NetworkError : int32_t {
    Success                 = 0,
    GenericFailure          = -1,
    NotConnected            = -2,
    ConnectionTimeout       = -3,
    ServiceUnavailable      = -4,
    InvalidParameter        = -5,
    AuthenticationFailed    = -6,
    ServerBusy              = -7,
    MaintenanceMode         = -8,
    RegionLocked            = -9,
    VersionMismatch         = -10,
    StoryModeOnly           = -100  ///< Custom error for story-mode-only games
};

/**
 * @brief Network request information
 */
struct NetworkRequest {
    uint64_t        request_id;
    NetworkService  service;
    std::string     endpoint;
    std::string     method;  ///< GET, POST, etc.
    uint64_t        timeout_ms;
    bool            is_priority;
    bool            is_blocking;
    
    NetworkRequest() 
        : request_id(0), service(NetworkService::StoryMode),
          timeout_ms(5000), is_priority(false), is_blocking(false) {}
};

/**
 * @brief Network response information
 */
struct NetworkResponse {
    uint64_t        request_id;
    NetworkError    error;
    int32_t         status_code;  ///< HTTP status code
    std::string     body;
    uint64_t        response_time_ms;
    bool            is_cached;
    
    NetworkResponse() 
        : request_id(0), error(NetworkError::Success), 
          status_code(0), response_time_ms(0), is_cached(false) {}
};

/**
 * @brief Network stubs manager singleton
 */
class NetworkStubsManager {
public:
    static NetworkStubsManager& Instance() {
        static NetworkStubsManager instance;
        return instance;
    }
    
    /**
     * @brief Initialize network stubs
     * @param allow_story_mode Allow story mode network features
     * @param log_calls Log network calls for debugging
     */
    void Initialize(bool allow_story_mode = true, bool log_calls = true);
    
    /**
     * @brief Shutdown network stubs
     */
    void Shutdown();
    
    /**
     * @brief Check if a network service is available
     * @param service Service type
     * @return true if available (story mode only)
     */
    bool IsServiceAvailable(NetworkService service) const;
    
    /**
     * @brief Get connection state
     * @return Current connection state
     */
    ConnectionState GetConnectionState() const { return connection_state_; }
    
    /**
     * @brief Simulate network connection (for story mode)
     * @return Success or error code
     */
    NetworkError Connect();
    
    /**
     * @brief Simulate network disconnection
     */
    void Disconnect();
    
    /**
     * @brief Create a network request
     * @param service Service type
     * @param endpoint API endpoint
     * @param method HTTP method
     * @param timeout_ms Timeout in milliseconds
     * @return Request ID or 0 on failure
     */
    uint64_t CreateRequest(NetworkService service, const std::string& endpoint,
                           const std::string& method = "GET",
                           uint64_t timeout_ms = 5000);
    
    /**
     * @brief Send a network request (asynchronous)
     * @param request_id Request ID
     * @return Success or error code
     */
    NetworkError SendRequest(uint64_t request_id);
    
    /**
     * @brief Send a network request (synchronous)
     * @param request_id Request ID
     * @param response Response output
     * @return Success or error code
     */
    NetworkError SendRequestSync(uint64_t request_id, NetworkResponse& response);
    
    /**
     * @brief Check if request is complete
     * @param request_id Request ID
     * @return true if complete
     */
    bool IsRequestComplete(uint64_t request_id) const;
    
    /**
     * @brief Get request response
     * @param request_id Request ID
     * @param response Response output
     * @return Success or error code
     */
    NetworkError GetResponse(uint64_t request_id, NetworkResponse& response);
    
    /**
     * @brief Cancel a pending request
     * @param request_id Request ID
     * @return true if cancelled
     */
    bool CancelRequest(uint64_t request_id);
    
    /**
     * @brief Get error message for error code
     * @param error Error code
     * @return Error message string
     */
    static std::string GetErrorMessage(NetworkError error);
    
    /**
     * @brief Get service name
     * @param service Service type
     * @return Service name string
     */
    static std::string GetServiceName(NetworkService service);
    
    /**
     * @brief Check if story mode is allowed
     */
    bool IsStoryModeAllowed() const { return allow_story_mode_; }
    
    /**
     * @brief Get number of network calls made
     */
    uint64_t GetCallCount() const { return call_count_.load(std::memory_order_relaxed); }
    
    /**
     * @brief Get number of blocked calls
     */
    uint64_t GetBlockedCount() const { return blocked_count_.load(std::memory_order_relaxed); }
    
private:
    NetworkStubsManager() = default;
    ~NetworkStubsManager();
    
    // Prevent copying
    NetworkStubsManager(const NetworkStubsManager&) = delete;
    NetworkStubsManager& operator=(const NetworkStubsManager&) = delete;
    
    /**
     * @brief Generate fake response for story mode requests
     */
    NetworkResponse GenerateStoryModeResponse(uint64_t request_id);
    
    /**
     * @brief Generate error response for blocked services
     */
    NetworkResponse GenerateBlockedResponse(uint64_t request_id, NetworkService service);
    
    mutable std::mutex mutex_;
    std::atomic<bool> initialized_ {false};
    std::atomic<bool> allow_story_mode_ {true};
    std::atomic<bool> log_calls_ {true};
    
    std::atomic<ConnectionState> connection_state_ {ConnectionState::Disconnected};
    
    std::unordered_map<uint64_t, NetworkRequest> pending_requests_;
    std::unordered_map<uint64_t, NetworkResponse> completed_responses_;
    
    std::atomic<uint64_t> next_request_id_ {0x1000};
    std::atomic<uint64_t> call_count_ {0};
    std::atomic<uint64_t> blocked_count_ {0};
};

/**
 * @brief Helper function to check if online features should be blocked
 */
inline bool ShouldBlockOnlineFeature() {
    return !NetworkStubsManager::Instance().IsStoryModeAllowed();
}

/**
 * @brief Helper function to log network call
 */
void LogNetworkCall(const char* function, NetworkService service, bool allowed);

} // namespace NetworkStubs

} // namespace Libs
