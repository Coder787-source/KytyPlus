#include "libs/networkStubs.h"
#include "common/logging/log.h"
#include "common/assert.h"
#include <chrono>
#include <thread>
#include <random>
#include <cstring>
#include <utility>
#include <vector>

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
    std::vector<std::pair<SignInState, PsnAccount>> transitions;
    {
        std::unique_lock lock(mutex_);
        
        if (!initialized_.load(std::memory_order_acquire)) {
            return;
        }
        
        LOGF("NetworkStubs: Shutting down\n");
        
        // Run the authentic sign-out teardown first so listeners see a clean
        // transition even when the whole subsystem is going away.
        if (sign_in_state_.load(std::memory_order_acquire) != SignInState::SignedOut) {
            sign_in_state_.store(SignInState::SigningOut, std::memory_order_release);
            transitions.emplace_back(SignInState::SigningOut, account_);
            AbortAllPendingRequests(NetworkError::NotConnected);
            account_.Clear();
            sign_in_state_.store(SignInState::SignedOut, std::memory_order_release);
            transitions.emplace_back(SignInState::SignedOut, PsnAccount{});
        }
        
        // Cancel all pending requests
        for (auto& [id, request] : pending_requests_) {
            LOGF("  Cancelling request %llu: %s\n", id, request.endpoint.c_str());
        }
        
        pending_requests_.clear();
        completed_responses_.clear();
        sign_in_cb_ = nullptr;
        
        connection_state_.store(ConnectionState::Disconnected, std::memory_order_release);
        initialized_.store(false, std::memory_order_release);
    }
    
    // Fire listeners outside the lock. Note: sign_in_cb_ was cleared above, so
    // these notify a null callback and are effectively no-ops — but we keep the
    // path for symmetry and so a future "teardown-only" callback hook works.
    for (auto& [st, acct] : transitions) {
        NotifySignInState(st, acct);
    }
    
    LOGF("NetworkStubs: Shutdown complete (calls=%llu, blocked=%llu)\n",
         GetCallCount(), GetBlockedCount());
}

bool NetworkStubsManager::IsServiceAvailable(NetworkService service) const {
    if (!initialized_.load(std::memory_order_acquire)) {
        return false;
    }
    
    // Story mode is always available (single-player must work offline).
    if (service == NetworkService::StoryMode) {
        return allow_story_mode_.load(std::memory_order_acquire);
    }
    
    // Everything else requires an authenticated PSN session. Without real PSN
    // we never reach SignedIn for online services, so they are blocked here
    // rather than deep in the request path. This is the honest gate.
    if (RequiresSignIn(service)) {
        return false;
    }
    
    return allow_story_mode_.load(std::memory_order_acquire);
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

PsnAccount NetworkStubsManager::GetAccount() const {
    std::unique_lock lock(mutex_);
    return account_;
}

SignInStateCallback NetworkStubsManager::SetSignInStateCallback(SignInStateCallback cb) {
    std::unique_lock lock(mutex_);
    SignInStateCallback old = std::move(sign_in_cb_);
    sign_in_cb_ = std::move(cb);
    return old;
}

void NetworkStubsManager::NotifySignInState(SignInState new_state,
                                            const PsnAccount& account) {
    // mutex_ must be held by the caller.
    if (sign_in_cb_) {
        // Exceptions are disabled project-wide (-fno-exceptions): invoke the
        // listener directly. Listeners must not throw.
        sign_in_cb_(new_state, account);
    }
}

void NetworkStubsManager::AbortAllPendingRequests(NetworkError error) {
    // mutex_ must be held by the caller.
    if (pending_requests_.empty()) {
        return;
    }
    
    LOGF("NetworkStubs: Aborting %zu pending request(s) with %s\n",
         pending_requests_.size(), GetErrorMessage(error).c_str());
    
    for (auto& [id, request] : pending_requests_) {
        NetworkResponse resp;
        resp.request_id = id;
        resp.error = error;
        resp.status_code = 0;
        resp.is_cached = false;
        completed_responses_[id] = resp;
        LOGF("  Aborted request %llu: %s\n", id, request.endpoint.c_str());
    }
    pending_requests_.clear();
}

SignInState NetworkStubsManager::SignIn(const std::string& user_id, int32_t region) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return SignInState::SignedOut;
    }
    
    // Collect state transitions here and fire the listener OUTSIDE the lock,
    // so user callbacks can never run while we hold the manager mutex.
    std::vector<std::pair<SignInState, PsnAccount>> transitions;
    SignInState result;
    {
        std::unique_lock lock(mutex_);
        
        // If a sign-out is racing us, abort it: the caller explicitly wants in.
        if (sign_in_state_.load(std::memory_order_acquire) == SignInState::SignedIn) {
            return SignInState::SignedIn;  // idempotent
        }
        
        sign_in_state_.store(SignInState::SigningIn, std::memory_order_release);
        transitions.emplace_back(SignInState::SigningIn, account_);
        
        // Simulate the NP auth handshake. A real implementation would call
        // sceNpAuth* here; for story-mode we synthesize a deterministic local
        // profile so guest code reading NP fields gets sane values.
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        
        account_.user_id   = user_id.empty() ? std::string{"KYTY_LOCAL"} : user_id;
        account_.np_env     = 0x4E5031ULL;          // "NP1"
        account_.np_context = next_request_id_.fetch_add(1, std::memory_order_relaxed);
        account_.auth_token = next_request_id_.fetch_add(1, std::memory_order_relaxed);
        account_.session_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
        account_.region     = region;
        account_.is_plus    = false;
        
        // Story-mode builds always succeed; online services remain blocked
        // by IsServiceAvailable()/RequiresSignIn() regardless.
        sign_in_state_.store(SignInState::SignedIn, std::memory_order_release);
        connection_state_.store(ConnectionState::Connected, std::memory_order_release);
        result = SignInState::SignedIn;
        transitions.emplace_back(SignInState::SignedIn, account_);
    }
    
    LOGF("NetworkStubs: Signed in as '%s' (region=%d, token=0x%016llx, session=0x%016llx)\n",
         transitions.back().second.user_id.c_str(),
         transitions.back().second.region,
         static_cast<unsigned long long>(transitions.back().second.auth_token),
         static_cast<unsigned long long>(transitions.back().second.session_id));
    for (auto& [st, acct] : transitions) {
        NotifySignInState(st, acct);
    }
    
    return result;
}

SignInState NetworkStubsManager::SignOut() {
    if (!initialized_.load(std::memory_order_acquire)) {
        return SignInState::SignedOut;
    }
    
    // Collect state transitions and fire the listener OUTSIDE the lock.
    std::vector<std::pair<SignInState, PsnAccount>> transitions;
    PsnAccount cleared;
    {
        std::unique_lock lock(mutex_);
        
        const SignInState cur = sign_in_state_.load(std::memory_order_acquire);
        if (cur == SignInState::SignedOut) {
            return SignInState::SignedOut;  // idempotent no-op
        }
        
        // Step 1: enter teardown state. Concurrent requesters must short-circuit.
        sign_in_state_.store(SignInState::SigningOut, std::memory_order_release);
        transitions.emplace_back(SignInState::SigningOut, account_);
        
        LOGF("NetworkStubs: Signing out '%s' — running session teardown\n",
             account_.user_id.c_str());
        
        // Step 2+3: abort in-flight requests and drop the response cache for
        // the torn-down session. New responses would reference a dead token.
        AbortAllPendingRequests(NetworkError::NotConnected);
        completed_responses_.clear();
        
        // Step 4: invalidate the account. Any lingering reference now reads
        // observably-empty fields (token/session == 0) instead of stale data.
        PsnAccount snapshot = account_;   // keep a copy for the callback
        account_.Clear();
        
        // Step 6: drop the transport too — a sign-out is a full disconnect.
        connection_state_.store(ConnectionState::Disconnected, std::memory_order_release);
        
        // Step 7: terminal state.
        sign_in_state_.store(SignInState::SignedOut, std::memory_order_release);
        
        LOGF("NetworkStubs: Signed out '%s' (token=0x%016llx invalidated)\n",
             snapshot.user_id.c_str(),
             static_cast<unsigned long long>(snapshot.auth_token));
        // The terminal callback carries the *cleared* account snapshot so the
        // listener observes the post-teardown state, not the dying one.
        transitions.emplace_back(SignInState::SignedOut, cleared);
    }
    
    for (auto& [st, acct] : transitions) {
        NotifySignInState(st, acct);
    }
    
    return SignInState::SignedOut;
}

uint64_t NetworkStubsManager::CreateRequest(NetworkService service, const std::string& endpoint,
                                             const std::string& method, uint64_t timeout_ms) {
    if (!initialized_.load(std::memory_order_acquire)) {
        return 0;
    }
    
    std::unique_lock lock(mutex_);
    
    call_count_.fetch_add(1, std::memory_order_relaxed);
    
    const uint64_t request_id = next_request_id_.fetch_add(1, std::memory_order_relaxed);
    
    // Gate online requests on the PSN session state. During SigningOut we refuse
    // to enqueue so the sign-out is observable as an immediate rejection rather
    // than a request that silently never completes.
    const SignInState s = sign_in_state_.load(std::memory_order_acquire);
    const bool auth_needed = RequiresSignIn(service);
    if (auth_needed && (s == SignInState::SignedOut || s == SignInState::SigningOut ||
                        s == SignInState::SignInFailed)) {
        NetworkResponse resp;
        resp.request_id     = request_id;
        resp.error          = (s == SignInState::SigningOut)
                                  ? NetworkError::NotConnected
                                  : NetworkError::AuthenticationFailed;
        resp.status_code    = (resp.error == NetworkError::NotConnected) ? 0 : 401;
        resp.is_cached      = false;
        resp.body           = R"({"error":"not_signed_in","message":"PSN session is not authenticated"})";
        completed_responses_[request_id] = resp;
        
        blocked_count_.fetch_add(1, std::memory_order_relaxed);
        LOGF("NetworkStubs: Request %llu - %s %s [%s] - REJECTED (not signed in, state=%u)\n",
             request_id, method.c_str(), endpoint.c_str(),
             GetServiceName(service).c_str(), static_cast<uint32_t>(s));
        return request_id;
    }
    
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
