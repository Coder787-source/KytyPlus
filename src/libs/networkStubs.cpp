#include "libs/networkStubs.h"
#include "common/logging/log.h"
#include "common/assert.h"
#include <chrono>
#include <thread>
#include <random>
#include <cstring>

namespace Libs {

namespace NetworkStubs {

NetworkStubsManager::~NetworkStubsManager() {
    Shutdown();
}

void NetworkStubsManager::Initialize(bool allow_story_mode, bool log_calls) {
    std::unique_lock lock(mutex_);
    
    if (initialized_.load(std::memory_order_acquire)) {
        LOGF_COLOR(Log::Color::Yellow, "NetworkStubs: Already initialized\n");
        return;
    }
    
    allow_story_mode_.store(allow_story_mode, std::memory_order_release);
    log_calls_.store(log_calls, std::memory_order_release);
    
    LOGF("NetworkStubs: Initializing\n");
    LOGF("  Story Mode: %s\n", allow_story_mode ? "ALLOWED" : "BLOCKED");
    LOGF("  Logging: %s\n", log_calls ? "ENABLED" : "DISABLED");
    LOGF("  Online Features: BLOCKED (PSN not implemented)\n");
    
    initialized_.store(true, std::memory_order_release);
    
    LOGF("NetworkStubs: Initialization complete\n");
}

void NetworkStubsManager::Shutdown() {
    std::unique_lock lock(mutex_);
    
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    LOGF("NetworkStubs: Shutting down\n");
    
    // Cancel all pending requests
    for (auto& [id, request] : pending_requests_) {
        LOGF("  Cancelling request %llu: %s\n", id, request.endpoint.c_str());
    }
    
    pending_requests_.clear();
    completed_responses_.clear();
    
    connection_state_.store(ConnectionState::Disconnected, std::memory_order_release);
    initialized_.store(false, std::memory_order_release);
    
    LOGF("NetworkStubs: Shutdown complete (calls=%llu, blocked=%llu)\n",
         GetCallCount(), GetBlockedCount());
}

bool NetworkStubsManager::IsServiceAvailable(NetworkService service) const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    // Only story mode services are available
    return allow_story_mode_.load(std::memory_order_acquire) && 
           service == NetworkService::StoryMode;
}

NetworkError NetworkStubsManager::Connect() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return NetworkError::GenericFailure;
    }
    
    std::unique_lock lock(mutex_);
    
    connection_state_.store(ConnectionState::Connecting, std::memory_order_release);
    
    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Always "connect" successfully for story mode
    // Online features will fail at the service level, not connection level
    connection_state_.store(ConnectionState::Connected, std::memory_order_release);
    
    LOGF("NetworkStubs: Connected (story mode only)\n");
    
    return NetworkError::Success;
}

void NetworkStubsManager::Disconnect() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return;
    }
    
    std::unique_lock lock(mutex_);
    
    connection_state_.store(ConnectionState::Disconnected, std::memory_order_release);
    
    LOGF("NetworkStubs: Disconnected\n");
}

uint64_t NetworkStubsManager::CreateRequest(NetworkService service, const std::string& endpoint,
                                             const std::string& method, uint64_t timeout_ms) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    std::unique_lock lock(mutex_);
    
    call_count_.fetch_add(1, std::memory_order_relaxed);
    
    const uint64_t request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
    
    NetworkRequest request;
    request.request_id = request_id;
    request.service = service;
    request.endpoint = endpoint;
    request.method = method;
    request.timeout_ms = timeout_ms;
    request.is_priority = (service == NetworkService::StoryMode);
    request.is_blocking = false;
    
    pending_requests_[request_id] = request;
    
    if (log_calls_.load(std::memory_order_relaxed)) {
        const bool allowed = IsServiceAvailable(service);
        LOGF("NetworkStubs: Request %llu - %s %s [%s] - %s\n",
             request_id,
             method.c_str(),
             endpoint.c_str(),
             GetServiceName(service).c_str(),
             allowed ? "ALLOWED" : "BLOCKED");
        
        if (!allowed) {
            blocked_count_.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
    return request_id;
}

NetworkError NetworkStubsManager::SendRequest(uint64_t request_id) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return NetworkError::GenericFailure;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        LOGF_COLOR(Log::Color::Yellow, "NetworkStubs: Request %llu not found\n", request_id);
        return NetworkError::InvalidParameter;
    }
    
    const NetworkRequest& request = it->second;
    
    // Check if service is available
    if (!IsServiceAvailable(request.service)) {
        // Generate blocked response immediately
        NetworkResponse response = GenerateBlockedResponse(request_id, request.service);
        completed_responses_[request_id] = response;
        pending_requests_.erase(it);
        
        return response.error;
    }
    
    // For story mode, simulate async completion
    // In a real implementation, this would use a background thread
    LOGF("NetworkStubs: Request %llu sent (story mode)\n", request_id);
    
    return NetworkError::Success;
}

NetworkError NetworkStubsManager::SendRequestSync(uint64_t request_id, NetworkResponse& response) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return NetworkError::GenericFailure;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        LOGF_COLOR(Log::Color::Yellow, "NetworkStubs: Request %llu not found\n", request_id);
        return NetworkError::InvalidParameter;
    }
    
    const NetworkRequest& request = it->second;
    
    // Check if service is available
    if (!IsServiceAvailable(request.service)) {
        response = GenerateBlockedResponse(request_id, request.service);
        pending_requests_.erase(it);
        
        return response.error;
    }
    
    // Generate story mode response
    response = GenerateStoryModeResponse(request_id);
    response.response_time_ms = 50 + (rand() % 100);  // Simulate 50-150ms latency
    
    pending_requests_.erase(it);
    completed_responses_[request_id] = response;
    
    LOGF("NetworkStubs: Request %llu completed (%llu ms)\n", 
         request_id, response.response_time_ms);
    
    return NetworkError::Success;
}

bool NetworkStubsManager::IsRequestComplete(uint64_t request_id) const {
    std::unique_lock lock(mutex_);
    
    // Sync requests are always "complete"
    return completed_responses_.find(request_id) != completed_responses_.end();
}

NetworkError NetworkStubsManager::GetResponse(uint64_t request_id, NetworkResponse& response) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return NetworkError::GenericFailure;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = completed_responses_.find(request_id);
    if (it == completed_responses_.end()) {
        // Check if request is still pending
        if (pending_requests_.find(request_id) != pending_requests_.end()) {
            return NetworkError::ServerBusy;  // Still processing
        }
        
        return NetworkError::InvalidParameter;
    }
    
    response = it->second;
    return NetworkError::Success;
}

bool NetworkStubsManager::CancelRequest(uint64_t request_id) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    std::unique_lock lock(mutex_);
    
    auto it = pending_requests_.find(request_id);
    if (it == pending_requests_.end()) {
        return false;
    }
    
    pending_requests_.erase(it);
    
    LOGF("NetworkStubs: Cancelled request %llu\n", request_id);
    
    return true;
}

std::string NetworkStubsManager::GetErrorMessage(NetworkError error) {
    switch (error) {
        case NetworkError::Success:
            return "Success";
        case NetworkError::GenericFailure:
            return "Generic failure";
        case NetworkError::NotConnected:
            return "Not connected";
        case NetworkError::ConnectionTimeout:
            return "Connection timeout";
        case NetworkError::ServiceUnavailable:
            return "Service unavailable";
        case NetworkError::InvalidParameter:
            return "Invalid parameter";
        case NetworkError::AuthenticationFailed:
            return "Authentication failed";
        case NetworkError::ServerBusy:
            return "Server busy";
        case NetworkError::MaintenanceMode:
            return "Service under maintenance";
        case NetworkError::RegionLocked:
            return "Region locked";
        case NetworkError::VersionMismatch:
            return "Version mismatch";
        case NetworkError::StoryModeOnly:
            return "Online features not available (story mode only)";
        default:
            return "Unknown error";
    }
}

std::string NetworkStubsManager::GetServiceName(NetworkService service) {
    switch (service) {
        case NetworkService::StoryMode:
            return "StoryMode";
        case NetworkService::Multiplayer:
            return "Multiplayer";
        case NetworkService::SocialClub:
            return "SocialClub";
        case NetworkService::DLCDownload:
            return "DLCDownload";
        case NetworkService::CloudSaves:
            return "CloudSaves";
        case NetworkService::Leaderboards:
            return "Leaderboards";
        case NetworkService::Telemetry:
            return "Telemetry";
        case NetworkService::Matchmaking:
            return "Matchmaking";
        default:
            return "Unknown";
    }
}

NetworkResponse NetworkStubsManager::GenerateStoryModeResponse(uint64_t request_id) {
    NetworkResponse response;
    response.request_id = request_id;
    response.error = NetworkError::Success;
    response.status_code = 200;
    response.is_cached = false;
    
    // Generate appropriate fake response based on endpoint
    // This allows story mode to function without actual network calls
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> response_dist(0, 100);
    
    const int response_type = response_dist(gen);
    
    if (response_type < 30) {
        // Generic success response
        response.body = R"({"status":"success","code":200})";
    } else if (response_type < 60) {
        // Data response
        response.body = R"({"status":"success","data":{"id":)" + 
                       std::to_string(request_id) + 
                       R"(,"timestamp":)" +
                       std::to_string(std::time(nullptr)) +
                       R"(}})";
    } else {
        // Empty success
        response.body = "{}";
    }
    
    return response;
}

NetworkResponse NetworkStubsManager::GenerateBlockedResponse(uint64_t request_id, 
                                                              NetworkService service) {
    NetworkResponse response;
    response.request_id = request_id;
    response.is_cached = false;
    
    switch (service) {
        case NetworkService::Multiplayer:
        case NetworkService::Matchmaking:
            response.error = NetworkError::ServiceUnavailable;
            response.status_code = 503;
            response.body = R"({"error":"multiplayer_unavailable","message":"Online multiplayer requires PSN"})";
            break;
            
        case NetworkService::SocialClub:
            response.error = NetworkError::AuthenticationFailed;
            response.status_code = 401;
            response.body = R"({"error":"social_club_unavailable","message":"Rockstar Social Club not available"})";
            break;
            
        case NetworkService::DLCDownload:
            response.error = NetworkError::ServiceUnavailable;
            response.status_code = 503;
            response.body = R"({"error":"dlc_unavailable","message":"DLC downloads not available"})";
            break;
            
        case NetworkService::CloudSaves:
            response.error = NetworkError::ServiceUnavailable;
            response.status_code = 503;
            response.body = R"({"error":"cloud_saves_unavailable","message":"Cloud saves not available"})";
            break;
            
        case NetworkService::Leaderboards:
            response.error = NetworkError::ServiceUnavailable;
            response.status_code = 503;
            response.body = R"({"error":"leaderboards_unavailable","message":"Leaderboards not available"})";
            break;
            
        case NetworkService::Telemetry:
            // Silently ignore telemetry
            response.error = NetworkError::Success;
            response.status_code = 200;
            response.body = "{}";
            break;
            
        default:
            response.error = NetworkError::StoryModeOnly;
            response.status_code = 403;
            response.body = R"({"error":"story_mode_only","message":"Online features not available"})";
            break;
    }
    
    LOGF_COLOR(Log::Color::Yellow, 
               "NetworkStubs: Blocked %s request %llu - %s\n",
               GetServiceName(service).c_str(),
               request_id,
               GetErrorMessage(response.error).c_str());
    
    return response;
}

void LogNetworkCall(const char* function, NetworkService service, bool allowed) {
    if (allowed) {
        LOGF("NetworkStubs: %s - %s [ALLOWED]\n", 
             function, 
             NetworkStubsManager::GetServiceName(service).c_str());
    } else {
        LOGF_COLOR(Log::Color::Yellow, 
                   "NetworkStubs: %s - %s [BLOCKED]\n", 
                   function,
                   NetworkStubsManager::GetServiceName(service).c_str());
    }
}

} // namespace NetworkStubs

} // namespace Libs
