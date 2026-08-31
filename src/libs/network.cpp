#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#if defined(_WIN32)
#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#else
#include <poll.h>
#endif
#ifdef s_addr
#undef s_addr
#endif
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "common/assert.h"
#include "common/byteBuffer.h"
#include "common/common.h"
#include "common/logging/log.h"
#include "common/stringUtils.h"
#include "common/threads.h"
#include "kernel/pthread.h"
#include "libs/errno.h"
#include "libs/libs.h"
#include "libs/network.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <fmt/format.h>
#include <memory>
#if defined(KYTY_HAS_MBEDTLS)
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>
#if defined(_WIN32)
#include <wincrypt.h>
#endif
#endif
#include <mutex>
#include <string>
#include <vector>

namespace Libs::LibKernel {
// Defined in libKernel.cpp; dispatches pending signals for the current guest
// thread so a blocked HttpWaitRequest still delivers APCs.
void KernelDispatchPendingSignalForCurrentThread();
} // namespace Libs::LibKernel

namespace Libs::Network {

namespace Http {
struct HttpEpoll;
} // namespace Http

// Raw-TLS session wrapper (defined in the TLS support block below). Declared
// at Libs::Network scope so Network's connection slots can hold an incomplete
// type here and destroy the complete type in the out-of-line destructor.
class TlsConnection;

class Network {
public:
	class Id {
	public:
		static constexpr int MAX_ID = 65536;

		enum class Type : uint32_t {
			Invalid    = 0,
			Http       = 1,
			Ssl        = 2,
			Template   = 3,
			Connection = 4,
			Request    = 5,
		};

		explicit Id(int id)
		    : m_id(static_cast<uint32_t>(id) & 0xffffu), m_type(static_cast<uint32_t>(id) >> 16u) {}
		[[nodiscard]] int ToInt() const {
			return static_cast<int>(m_id + (static_cast<uint32_t>(m_type) << 16u));
		}
		[[nodiscard]] bool IsValid() const { return GetType() != Type::Invalid; }
		[[nodiscard]] Type GetType() const {
			switch (m_type) {
				case static_cast<uint32_t>(Type::Http): return Type::Http;
				case static_cast<uint32_t>(Type::Ssl): return Type::Ssl;
				case static_cast<uint32_t>(Type::Template): return Type::Template;
				case static_cast<uint32_t>(Type::Connection): return Type::Connection;
				case static_cast<uint32_t>(Type::Request): return Type::Request;
				default: return Type::Invalid;
			}
		}

		[[nodiscard]] bool operator==(const Id& other) const {
			return m_id == other.m_id && m_type == other.m_type;
		}

		friend class Network;

	private:
		Id() = default;
		static Id Invalid() { return {}; }
		static Id Create(int net_id, Type type) {
			Id r;
			r.m_id   = net_id;
			r.m_type = static_cast<uint32_t>(type);
			return r;
		}
		[[nodiscard]] int GetId() const { return static_cast<int>(m_id); }

		uint32_t m_id   = 0;
		uint32_t m_type = static_cast<uint32_t>(Type::Invalid);
	};

	using HttpsCallback = KYTY_SYSV_ABI int (*)(int, unsigned int, void* const*, int, void*);

	Network();
	~Network();

	KYTY_CLASS_NO_COPY(Network);

	int  PoolCreate(const char* name, int size);
	bool PoolDestroy(int memid);
	int  ResolverCreate(const char* name, int memid);
	bool ResolverDestroy(int rid);
	bool ResolverValid(int rid);

	Id   SslInit(uint64_t pool_size);
	bool SslTerm(Id ssl_ctx_id);
	bool SslValid(Id ssl_ctx_id);

	Id   HttpInit(int memid, Id ssl_ctx_id, uint64_t pool_size);
	bool HttpTerm(Id http_ctx_id);
	Id   HttpCreateTemplate(Id http_ctx_id, const char* user_agent, int http_ver,
	                        bool is_auto_proxy_conf);
	bool HttpDeleteTemplate(Id tmpl_id);
	bool HttpSetNonblock(Id id, bool enable);
	bool HttpsSetSslCallback(Id id, HttpsCallback cbfunc, void* user_arg);
	bool HttpsSetMinSslVersion(Id id, uint32_t ssl_version);
	bool HttpsDisableOption(Id id, uint32_t ssl_flags);
	bool HttpAddRequestHeader(Id id, const char* name, const char* value, bool add);
	bool HttpValid(Id http_ctx_id);
	bool HttpValidTemplate(Id tmpl_id);
	bool HttpValidConnection(Id conn_id);
	bool HttpValidRequest(Id req_id);
	Id HttpCreateConnection(Id tmpl_id, const char* server_name, const char* scheme, uint16_t port,
	                        bool enable_keep_alive);
	Id HttpCreateConnectionWithURL(Id tmpl_id, const char* url, bool enable_keep_alive);
	bool HttpDeleteConnection(Id conn_id);
	Id   HttpCreateRequestWithURL2(Id conn_id, const char* method, const char* url,
	                               uint64_t content_length);
	bool HttpSetRequestContentLength(Id req_id, uint64_t content_length);
	bool HttpDeleteRequest(Id req_id);
	bool HttpSetResolveTimeOut(Id id, uint32_t usec);
	bool HttpSetResolveRetry(Id id, int32_t retry);
	bool HttpSetConnectTimeOut(Id id, uint32_t usec);
	bool HttpSetSendTimeOut(Id id, uint32_t usec);
	bool HttpSetRecvTimeOut(Id id, uint32_t usec);
	bool HttpSetAutoRedirect(Id id, int enable);
	bool HttpSetAuthEnabled(Id id, int enable);
	bool HttpMarkRequestSent(Id req_id, int result);
	bool HttpGetRequestResponse(Id req_id, int* send_result, int* status_code, const char** headers,
	                            size_t* headers_size, uint64_t* content_length);
	bool HttpStoreRequestResponse(Id req_id, int send_result, int status_code, std::string headers,
	                             std::string body, uint64_t content_length);

	// Plain-data snapshot of a request for the ABI layer (which cannot name the
	// private HttpRequest type). Empty optional-like semantics: has_value=false
	// when the id is invalid.
	struct HttpRequestView {
		std::string method;
		std::string url;
		std::string user_agent;
		bool        valid = false;
	};
	[[nodiscard]] HttpRequestView HttpGetRequestView(Id req_id) const;

	// Http epoll registry: HttpSendRequest needs to find every epoll bound to
	// a request so it can queue a completion event on each. Handles are owned by
	// guest code (HttpCreateEpoll/HttpDestroyEpoll); the registry only tracks
	// live pointers, so entries are removed on destroy.
	void RegisterHttpEpoll(Libs::Network::Http::HttpEpoll* epoll);
	void UnregisterHttpEpoll(Libs::Network::Http::HttpEpoll* epoll);
	std::vector<Libs::Network::Http::HttpEpoll*> GetEpollsForRequest(Id req_id);
	void                                     RemoveQueuedEpollEvents(Id req_id);

private:
	struct Pool {
		bool        used = false;
		std::string name;
		int         size = 0;
	};

	struct Ssl {
		bool     used = false;
		uint64_t size = 0;
	};

	struct Resolver {
		bool        used = false;
		std::string name;
		int         memid = 0;
	};

	struct Http {
		bool     used       = false;
		uint64_t size       = 0;
		int      memid      = 0;
		int      ssl_ctx_id = 0;
	};

	struct HttpHeader {
		std::string name;
		std::string value;
	};

	struct HttpBase {
		std::vector<HttpHeader> headers;
		bool                    used            = false;
		bool                    nonblock        = false;
		bool                    auto_redirect   = true;
		bool                    auth_enabled    = true;
		HttpsCallback           ssl_cbfunc      = nullptr;
		void*                   ssl_user_arg    = nullptr;
		uint32_t                ssl_flags       = 0xA7;
		uint32_t                min_ssl_version = 0;
		int                     http_ctx_id     = 0;
		uint32_t                resolve_timeout = 1'000000;
		int32_t                 resolve_retry   = 4;
		uint32_t                connect_timeout = 30'000000;
		uint32_t                send_timeout    = 120'000000;
		uint32_t                recv_timeout    = 120'000000;
	};

	struct HttpTemplate: public HttpBase {
		std::string user_agent;
		int         http_ver           = 0;
		bool        is_auto_proxy_conf = true;
	};

	struct HttpConnection: public HttpTemplate {
		explicit HttpConnection(const HttpTemplate& tmpl): HttpTemplate(tmpl) {}
		// int    tmpl_id = 0;
		std::string url;
		bool        enable_keep_alive = false;
	};

	struct HttpRequest: public HttpConnection {
		explicit HttpRequest(HttpConnection& conn): HttpConnection(conn) {}
		// int      conn_id = 0;
		std::string method;
		std::string url;
		uint64_t    content_length = 0;
		bool        send_attempted = false;
		int         send_result    = HTTP_ERROR_BEFORE_SEND;
		int         status_code    = 0;
		std::string response_headers;

		// Populated by the real HTTP client on success (plain http:// only).
		std::string response_body;
		uint64_t    response_content_length = 0;
	};

	HttpBase* FindHttpBase(Id id, bool include_request);

	static constexpr int POOLS_MAX     = 32;
	static constexpr int RESOLVERS_MAX = 32;
	static constexpr int SSL_MAX       = 32;
	static constexpr int HTTP_MAX      = 32;

	Common::Mutex               m_mutex;
	Pool                        m_pools[POOLS_MAX];
	Resolver                    m_resolvers[RESOLVERS_MAX];
	Ssl                         m_ssl[SSL_MAX];
	Http                        m_http[HTTP_MAX];

public:
	static constexpr int SSL_CONNECTION_MAX = 32;

	// libSsl raw-TLS connections: each slot owns the TLS session and either
	// the guest dial fd (SslCreateConnection over an existing socket) or an
	// internally dialed one. m_ssl_connections_mutex guards the slots; the
	// TlsConnection itself is owned exclusively by its slot.
	struct SslConnectionSlot {
		bool                                used = false;
		std::shared_ptr<TlsConnection> tls;
		std::string                         hostname;
		int                                 guest_socket = -1;
		int                                 last_error   = 0;
		bool                                connected    = false;
	};
	Common::Mutex              m_ssl_connections_mutex;
	SslConnectionSlot          m_ssl_connections[SSL_CONNECTION_MAX];

private:
	std::vector<HttpTemplate>   m_templates;
	std::vector<HttpConnection> m_connections;
	std::vector<HttpRequest>    m_requests;

	Common::Mutex                              m_epoll_registry_mutex;
	std::vector<Libs::Network::Http::HttpEpoll*> m_http_epolls;
};

static Network* g_net = nullptr;

Network::Network() = default;

void Initialize() {
	EXIT_IF(g_net != nullptr);

	g_net = new Network;
}

void Shutdown() {
	delete g_net;
	g_net = nullptr;
}

int Network::PoolCreate(const char* name, int size) {
	Common::LockGuard lock(m_mutex);

	for (int id = 0; id < POOLS_MAX; id++) {
		if (!m_pools[id].used) {
			m_pools[id].used = true;
			m_pools[id].size = size;
			m_pools[id].name = std::string(name);

			return id;
		}
	}

	return -1;
}

bool Network::PoolDestroy(int memid) {
	Common::LockGuard lock(m_mutex);

	if (memid >= 0 && memid < POOLS_MAX && m_pools[memid].used) {
		m_pools[memid].used = false;

		return true;
	}

	return false;
}

int Network::ResolverCreate(const char* name, int memid) {
	Common::LockGuard lock(m_mutex);

	if (memid < 0 || memid >= POOLS_MAX || !m_pools[memid].used) {
		return -1;
	}

	for (int id = 0; id < RESOLVERS_MAX; id++) {
		if (!m_resolvers[id].used) {
			m_resolvers[id].used  = true;
			m_resolvers[id].name  = std::string(name);
			m_resolvers[id].memid = memid;

			return id;
		}
	}

	return -1;
}

bool Network::ResolverDestroy(int rid) {
	Common::LockGuard lock(m_mutex);

	if (rid >= 0 && rid < RESOLVERS_MAX && m_resolvers[rid].used) {
		m_resolvers[rid] = {};
		return true;
	}

	return false;
}

bool Network::ResolverValid(int rid) {
	Common::LockGuard lock(m_mutex);

	return rid >= 0 && rid < RESOLVERS_MAX && m_resolvers[rid].used;
}

Network::Id Network::SslInit(uint64_t pool_size) {
	Common::LockGuard lock(m_mutex);

	for (int id = 0; id < SSL_MAX; id++) {
		if (!m_ssl[id].used) {
			m_ssl[id].used = true;
			m_ssl[id].size = pool_size;

			return Id::Create(id, Id::Type::Ssl);
		}
	}

	return Id::Invalid();
}

bool Network::SslTerm(Id ssl_ctx_id) {
	Common::LockGuard lock(m_mutex);

	if (ssl_ctx_id.GetType() == Id::Type::Ssl && ssl_ctx_id.GetId() >= 0 &&
	    ssl_ctx_id.GetId() < SSL_MAX && m_ssl[ssl_ctx_id.GetId()].used) {
		m_ssl[ssl_ctx_id.GetId()].used = false;

		return true;
	}

	return false;
}

bool Network::SslValid(Id ssl_ctx_id) {
	Common::LockGuard lock(m_mutex);

	return ssl_ctx_id.GetType() == Id::Type::Ssl && ssl_ctx_id.GetId() >= 0 &&
	       ssl_ctx_id.GetId() < SSL_MAX && m_ssl[ssl_ctx_id.GetId()].used;
}

Network::Id Network::HttpInit(int memid, Id ssl_ctx_id, uint64_t pool_size) {
	Common::LockGuard lock(m_mutex);

	if (ssl_ctx_id.GetType() == Id::Type::Ssl && ssl_ctx_id.GetId() >= 0 &&
	    ssl_ctx_id.GetId() < SSL_MAX && m_ssl[ssl_ctx_id.GetId()].used && memid >= 0 &&
	    memid < POOLS_MAX && m_pools[memid].used) {
		for (int id = 0; id < HTTP_MAX; id++) {
			if (!m_http[id].used) {
				m_http[id].used       = true;
				m_http[id].size       = pool_size;
				m_http[id].ssl_ctx_id = ssl_ctx_id.GetId();
				m_http[id].memid      = memid;

				return Id::Create(id, Id::Type::Http);
			}
		}
	}

	return Id::Invalid();
}

bool Network::HttpValid(Id http_ctx_id) {
	Common::LockGuard lock(m_mutex);

	return (http_ctx_id.GetType() == Id::Type::Http && http_ctx_id.GetId() >= 0 &&
	        http_ctx_id.GetId() < HTTP_MAX && m_http[http_ctx_id.GetId()].used);
}

bool Network::HttpValidTemplate(Id tmpl_id) {
	Common::LockGuard lock(m_mutex);

	return (tmpl_id.GetType() == Id::Type::Template && tmpl_id.GetId() >= 0 &&
	        static_cast<size_t>(tmpl_id.GetId()) < m_templates.size() &&
	        m_templates[tmpl_id.GetId()].used);
}

bool Network::HttpValidConnection(Id conn_id) {
	Common::LockGuard lock(m_mutex);

	return (conn_id.GetType() == Id::Type::Connection && conn_id.GetId() >= 0 &&
	        static_cast<size_t>(conn_id.GetId()) < m_connections.size() &&
	        m_connections[conn_id.GetId()].used);
}

bool Network::HttpValidRequest(Id req_id) {
	Common::LockGuard lock(m_mutex);

	return (req_id.GetType() == Id::Type::Request && req_id.GetId() >= 0 &&
	        static_cast<size_t>(req_id.GetId()) < m_requests.size() &&
	        m_requests[req_id.GetId()].used);
}

Network::HttpBase* Network::FindHttpBase(Id id, bool include_request) {
	const auto index = id.GetId();

	switch (id.GetType()) {
		case Id::Type::Template:
			if (index >= 0 && static_cast<size_t>(index) < m_templates.size() &&
			    m_templates[index].used) {
				return &m_templates[index];
			}
			break;
		case Id::Type::Connection:
			if (index >= 0 && static_cast<size_t>(index) < m_connections.size() &&
			    m_connections[index].used) {
				return &m_connections[index];
			}
			break;
		case Id::Type::Request:
			if (include_request && index >= 0 && static_cast<size_t>(index) < m_requests.size() &&
			    m_requests[index].used) {
				return &m_requests[index];
			}
			break;
		default: break;
	}

	return nullptr;
}

bool Network::HttpTerm(Id http_ctx_id) {
	Common::LockGuard lock(m_mutex);

	if (HttpValid(http_ctx_id)) {
		m_http[http_ctx_id.GetId()].used = false;

		return true;
	}

	return false;
}

Network::Id Network::HttpCreateTemplate(Id http_ctx_id, const char* user_agent, int http_ver,
                                        bool is_auto_proxy_conf) {
	Common::LockGuard lock(m_mutex);

	if (HttpValid(http_ctx_id)) {
		HttpTemplate tn {};
		tn.used               = true;
		tn.http_ver           = http_ver;
		tn.user_agent         = std::string(user_agent);
		tn.is_auto_proxy_conf = is_auto_proxy_conf;
		tn.http_ctx_id        = http_ctx_id.GetId();
		tn.nonblock           = false;

		int index = 0;
		for (auto& t: m_templates) {
			if (!t.used) {
				t = tn;
				return Id::Create(index, Id::Type::Template);
			}
			index++;
		}

		if (index < Id::MAX_ID) {
			m_templates.push_back(tn);
			return Id::Create(index, Id::Type::Template);
		}
	}

	return Id::Invalid();
}

Network::Id Network::HttpCreateConnectionWithURL(Id tmpl_id, const char* url,
                                                 bool enable_keep_alive) {
	Common::LockGuard lock(m_mutex);

	if (url != nullptr && url[0] != '\0' && HttpValidTemplate(tmpl_id)) {
		HttpConnection cn(m_templates[tmpl_id.GetId()]);
		cn.used              = true;
		cn.enable_keep_alive = enable_keep_alive;
		cn.url               = std::string(url);
		// cn.tmpl_id           = tmpl_id.ToInt();

		int index = 0;
		for (auto& t: m_connections) {
			if (!t.used) {
				t = cn;
				return Id::Create(index, Id::Type::Connection);
			}
			index++;
		}

		if (index < Id::MAX_ID) {
			m_connections.push_back(cn);
			return Id::Create(index, Id::Type::Connection);
		}
	}

	return Id::Invalid();
}

Network::Id Network::HttpCreateConnection(Id tmpl_id, const char* server_name, const char* scheme,
                                          uint16_t port, bool enable_keep_alive) {
	Common::LockGuard lock(m_mutex);

	if (server_name != nullptr && scheme != nullptr && HttpValidTemplate(tmpl_id)) {
		HttpConnection cn(m_templates[tmpl_id.GetId()]);
		cn.used              = true;
		cn.enable_keep_alive = enable_keep_alive;
		cn.url               = std::string(scheme) + "://" + std::string(server_name);
		if (port != 0) {
			cn.url = cn.url + fmt::format(":{}", static_cast<unsigned>(port));
		}

		int index = 0;
		for (auto& t: m_connections) {
			if (!t.used) {
				t = cn;
				return Id::Create(index, Id::Type::Connection);
			}
			index++;
		}

		if (index < Id::MAX_ID) {
			m_connections.push_back(cn);
			return Id::Create(index, Id::Type::Connection);
		}
	}

	return Id::Invalid();
}

bool Network::HttpDeleteConnection(Id conn_id) {
	Common::LockGuard lock(m_mutex);

	if (HttpValidConnection(conn_id)) {
		m_connections[conn_id.GetId()].used = false;

		return true;
	}

	return false;
}

Network::Id Network::HttpCreateRequestWithURL2(Id conn_id, const char* method, const char* url,
                                               uint64_t content_length) {
	Common::LockGuard lock(m_mutex);

	if (method != nullptr && url != nullptr && url[0] != '\0' && HttpValidConnection(conn_id)) {
		HttpRequest cn(m_connections[conn_id.GetId()]);
		cn.used   = true;
		cn.method = std::string(method);
		cn.url    = std::string(url);
		// cn.conn_id        = conn_id.ToInt();
		cn.content_length = content_length;

		int index = 0;
		for (auto& t: m_requests) {
			if (!t.used) {
				t = cn;
				return Id::Create(index, Id::Type::Request);
			}
			index++;
		}

		if (index < Id::MAX_ID) {
			m_requests.push_back(cn);
			return Id::Create(index, Id::Type::Request);
		}
	}

	return Id::Invalid();
}

bool Network::HttpDeleteRequest(Id req_id) {
	Common::LockGuard lock(m_mutex);

	if (HttpValidRequest(req_id)) {
		m_requests[req_id.GetId()].used = false;

		return true;
	}

	return false;
}

bool Network::HttpSetRequestContentLength(Id req_id, uint64_t content_length) {
	Common::LockGuard lock(m_mutex);

	if (HttpValidRequest(req_id)) {
		m_requests[req_id.GetId()].content_length = content_length;
		return true;
	}

	return false;
}

bool Network::HttpMarkRequestSent(Id req_id, int result) {
	Common::LockGuard lock(m_mutex);

	if (req_id.GetType() == Id::Type::Request && req_id.GetId() >= 0 &&
	    static_cast<size_t>(req_id.GetId()) < m_requests.size() &&
	    m_requests[req_id.GetId()].used) {
		auto& request          = m_requests[req_id.GetId()];
		request.send_attempted = true;
		request.send_result    = result;
		request.status_code    = (result == OK ? 200 : 0);
		request.response_headers.clear();
		return true;
	}

	return false;
}

bool Network::HttpStoreRequestResponse(Id req_id, int send_result, int status_code,
	                                std::string headers, std::string body,
	                                uint64_t content_length) {
	Common::LockGuard lock(m_mutex);

	if (req_id.GetType() != Id::Type::Request || req_id.GetId() < 0 ||
	    static_cast<size_t>(req_id.GetId()) >= m_requests.size() ||
	    !m_requests[req_id.GetId()].used) {
		return false;
	}

	auto& request                      = m_requests[req_id.GetId()];
	request.send_result               = send_result;
	request.status_code               = status_code;
	request.response_headers           = std::move(headers);
	request.response_body              = std::move(body);
	request.response_content_length    = content_length;
	return true;
}

Network::HttpRequestView Network::HttpGetRequestView(Id req_id) const {
	HttpRequestView view;
	if (req_id.GetType() != Id::Type::Request || req_id.GetId() < 0 ||
	    static_cast<size_t>(req_id.GetId()) >= m_requests.size() ||
	    !m_requests[req_id.GetId()].used) {
		return view;
	}

	const auto& request = m_requests[req_id.GetId()];
	view.method     = request.method;
	view.url       = request.url;
	view.user_agent = request.user_agent;
	view.valid     = true;
	return view;
}

bool Network::HttpGetRequestResponse(Id req_id, int* send_result, int* status_code,
                                     const char** headers, size_t* headers_size,
                                     uint64_t* content_length) {
	Common::LockGuard lock(m_mutex);

	if (req_id.GetType() != Id::Type::Request || req_id.GetId() < 0 ||
	    static_cast<size_t>(req_id.GetId()) >= m_requests.size() ||
	    !m_requests[req_id.GetId()].used) {
		return false;
	}

	const auto& request = m_requests[req_id.GetId()];

	if (send_result != nullptr) {
		*send_result = (request.send_attempted ? request.send_result : HTTP_ERROR_BEFORE_SEND);
	}
	if (status_code != nullptr) {
		*status_code = request.status_code;
	}
	if (headers != nullptr) {
		*headers = request.response_headers.c_str();
	}
	if (headers_size != nullptr) {
		*headers_size = request.response_headers.size();
	}
	if (content_length != nullptr) {
		*content_length = request.response_content_length;
	}

	return true;
}

bool Network::HttpDeleteTemplate(Id tmpl_id) {
	Common::LockGuard lock(m_mutex);

	if (HttpValidTemplate(tmpl_id)) {
		m_templates[tmpl_id.GetId()].used = false;

		return true;
	}

	return false;
}

bool Network::HttpSetNonblock(Id id, bool enable) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->nonblock = enable;
		return true;
	}

	return false;
}

bool Network::HttpsSetSslCallback(Id id, HttpsCallback cbfunc, void* user_arg) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->ssl_cbfunc   = cbfunc;
		base->ssl_user_arg = user_arg;
		return true;
	}

	return false;
}

bool Network::HttpsSetMinSslVersion(Id id, uint32_t ssl_version) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->min_ssl_version = ssl_version;
		return true;
	}

	return false;
}

bool Network::HttpsDisableOption(Id id, uint32_t ssl_flags) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->ssl_flags &= ~ssl_flags;
		return true;
	}

	return false;
}

bool Network::HttpAddRequestHeader(Id id, const char* name, const char* value, bool add) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		HttpHeader nh({std::string(name), std::string(value)});
		if (add) {
			base->headers.push_back(nh);
		} else {
			for (auto& h: base->headers) {
				if (h.name == nh.name) {
					h.value = nh.value;
				}
			}
		}
		return true;
	}

	return false;
}

bool Network::HttpSetResolveTimeOut(Id id, uint32_t usec) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, false);

	if (base != nullptr) {
		base->resolve_timeout = usec;
		return true;
	}

	return false;
}

bool Network::HttpSetResolveRetry(Id id, int32_t retry) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, false);

	if (base != nullptr) {
		base->resolve_retry = retry;
		return true;
	}

	return false;
}

bool Network::HttpSetConnectTimeOut(Id id, uint32_t usec) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->connect_timeout = usec;
		return true;
	}

	return false;
}

bool Network::HttpSetSendTimeOut(Id id, uint32_t usec) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->send_timeout = usec;
		return true;
	}

	return false;
}

bool Network::HttpSetRecvTimeOut(Id id, uint32_t usec) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->recv_timeout = usec;
		return true;
	}

	return false;
}

bool Network::HttpSetAutoRedirect(Id id, int enable) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->auto_redirect = (enable != 0);
		return true;
	}

	return false;
}

bool Network::HttpSetAuthEnabled(Id id, int enable) {
	Common::LockGuard lock(m_mutex);

	HttpBase* base = FindHttpBase(id, true);

	if (base != nullptr) {
		base->auth_enabled = (enable != 0);
		return true;
	}

	return false;
}

namespace Net {

LIB_NAME("Net", "Net");

// Diagnostics intentionally exercise the host backend directly; guest ABI
// handles require an initialized emulator process and are tested separately.

struct NetEtherAddr {
	uint8_t data[6] = {0};
};

#if defined(_WIN32)
using NativeSocket                                  = SOCKET;
static constexpr NativeSocket INVALID_NATIVE_SOCKET = INVALID_SOCKET;
using SocketLength                                  = int;
#else
using NativeSocket                                  = int;
static constexpr NativeSocket INVALID_NATIVE_SOCKET = -1;
using SocketLength                                  = socklen_t;
static constexpr int           SOCKET_ERROR         = -1;
#endif

struct SocketSlot {
	bool         used   = false;
	NativeSocket socket = INVALID_NATIVE_SOCKET;
};

struct NetTimeval {
	int64_t tv_sec;
	int64_t tv_usec;
};

static constexpr int                         SOCKET_FD_MIN = 128;
static constexpr int                         SOCKET_FD_MAX = 1024;
static Common::Mutex                         g_socket_mutex;
static std::array<SocketSlot, SOCKET_FD_MAX> g_sockets;
static std::atomic_bool                      g_winsock_initialized = false;

constexpr int      EPOLL_ID_BASE = SOCKET_FD_MAX;
constexpr uint32_t EPOLL_IN      = 0x00000001;
constexpr uint32_t EPOLL_OUT     = 0x00000002;
constexpr uint32_t EPOLL_ERR     = 0x00000008;

struct EpollRegistration {
	int           id = -1;
	NetEpollEvent event {};
};

struct EpollSlot {
	bool                           used = false;
	std::string                    name;
	std::vector<EpollRegistration> registrations;
};

static std::mutex                g_epoll_mutex;
static std::condition_variable   g_epoll_changed;
static std::array<EpollSlot, 32> g_epolls;

static EpollSlot* GetEpollSlot(int eid) {
	const auto index = eid - EPOLL_ID_BASE;
	if (index < 0 || index >= static_cast<int>(g_epolls.size())) {
		return nullptr;
	}
	auto& slot = g_epolls[static_cast<size_t>(index)];
	return slot.used ? &slot : nullptr;
}

static void RemoveSocketFromEpolls(int id) {
	std::lock_guard lock(g_epoll_mutex);
	for (auto& epoll: g_epolls) {
		if (!epoll.used) {
			continue;
		}
		std::erase_if(epoll.registrations,
		              [id](const auto& registration) { return registration.id == id; });
	}
	g_epoll_changed.notify_all();
}

static bool EnsureSocketBackend() {
#if defined(_WIN32)
	if (!g_winsock_initialized.load()) {
		WSADATA data {};
		if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
			return false;
		}
		g_winsock_initialized = true;
	}
#endif
	return true;
}

static int SetGuestSocketError(int error) {
	*Posix::GetErrorAddr() = error;
	return -1;
}

static int SetHostSocketError(int error) {
	int posix_error = Posix::POSIX_EIO;
#if defined(_WIN32)
	switch (error) {
		case WSAEACCES: posix_error = Posix::POSIX_EACCES; break;
		case WSAEADDRINUSE: posix_error = Posix::POSIX_EADDRINUSE; break;
		case WSAEADDRNOTAVAIL: posix_error = Posix::POSIX_EADDRNOTAVAIL; break;
		case WSAEAFNOSUPPORT: posix_error = Posix::POSIX_EAFNOSUPPORT; break;
		case WSAEFAULT: posix_error = Posix::POSIX_EFAULT; break;
		case WSAEINTR: posix_error = Posix::POSIX_EINTR; break;
		case WSAEINVAL: posix_error = Posix::POSIX_EINVAL; break;
		case WSAEISCONN: posix_error = Posix::POSIX_EISCONN; break;
		case WSAEMFILE: posix_error = Posix::POSIX_EMFILE; break;
		case WSAEMSGSIZE: posix_error = Posix::POSIX_EMSGSIZE; break;
		case WSAENOBUFS: posix_error = Posix::POSIX_ENOBUFS; break;
		case WSAENETDOWN: posix_error = Posix::POSIX_ENETDOWN; break;
		case WSAENETRESET: posix_error = Posix::POSIX_ENETRESET; break;
		case WSAENETUNREACH: posix_error = Posix::POSIX_ENETUNREACH; break;
		case WSAENOTCONN: posix_error = Posix::POSIX_ENOTCONN; break;
		case WSAENOTSOCK: posix_error = Posix::POSIX_ENOTSOCK; break;
		case WSAEOPNOTSUPP: posix_error = Posix::POSIX_EOPNOTSUPP; break;
		case WSAEPROTONOSUPPORT: posix_error = Posix::POSIX_EPROTONOSUPPORT; break;
		case WSAESHUTDOWN: posix_error = Posix::POSIX_ESHUTDOWN; break;
		case WSAETIMEDOUT: posix_error = Posix::POSIX_ETIMEDOUT; break;
		case WSAEWOULDBLOCK: posix_error = Posix::POSIX_EWOULDBLOCK; break;
		case WSAECONNABORTED: posix_error = Posix::POSIX_ECONNABORTED; break;
		case WSAECONNREFUSED: posix_error = Posix::POSIX_ECONNREFUSED; break;
		case WSAECONNRESET: posix_error = Posix::POSIX_ECONNRESET; break;
		case WSAEDESTADDRREQ: posix_error = Posix::POSIX_EDESTADDRREQ; break;
		case WSAEHOSTUNREACH: posix_error = Posix::POSIX_EHOSTUNREACH; break;
		case WSAEINPROGRESS: posix_error = Posix::POSIX_EINPROGRESS; break;
		case WSAEALREADY: posix_error = Posix::POSIX_EALREADY; break;
		default: break;
	}
#else
	switch (error) {
		case EACCES: posix_error = Posix::POSIX_EACCES; break;
		case EADDRINUSE: posix_error = Posix::POSIX_EADDRINUSE; break;
		case EADDRNOTAVAIL: posix_error = Posix::POSIX_EADDRNOTAVAIL; break;
		case EAFNOSUPPORT: posix_error = Posix::POSIX_EAFNOSUPPORT; break;
		case EBADF: posix_error = Posix::POSIX_EBADF; break;
		case EFAULT: posix_error = Posix::POSIX_EFAULT; break;
		case EINVAL: posix_error = Posix::POSIX_EINVAL; break;
		case EMFILE: posix_error = Posix::POSIX_EMFILE; break;
		case ENFILE: posix_error = Posix::POSIX_ENFILE; break;
		case ENOBUFS: posix_error = Posix::POSIX_ENOBUFS; break;
		case ENOMEM: posix_error = Posix::POSIX_ENOMEM; break;
		case ENOTSOCK: posix_error = Posix::POSIX_ENOTSOCK; break;
		case EPROTONOSUPPORT: posix_error = Posix::POSIX_EPROTONOSUPPORT; break;
		default: break;
	}
#endif
	return SetGuestSocketError(posix_error);
}

static int SetHostSocketError() {
#if defined(_WIN32)
	return SetHostSocketError(WSAGetLastError());
#else
	return SetHostSocketError(errno);
#endif
}

static int ConvertFamily(int family) {
	switch (family) {
		case 2: return AF_INET;
		case 28: return AF_INET6;
		default: return -1;
	}
}

static int ConvertSocketOptionLevel(int level) {
	return (level == 0xffff ? SOL_SOCKET : level);
}

static int ConvertMessageFlags(int flags) {
#if defined(_WIN32)
	constexpr int guest_msg_peek      = 0x00000002;
	constexpr int guest_msg_dontroute = 0x00000004;
	constexpr int guest_msg_waitall   = 0x00000040;
	constexpr int guest_msg_dontwait  = 0x00000080;
	constexpr int guest_msg_nosignal  = 0x00020000;

	int host_flags = 0;
	if ((flags & guest_msg_peek) != 0) {
		host_flags |= MSG_PEEK;
	}
	if ((flags & guest_msg_dontroute) != 0) {
		host_flags |= MSG_DONTROUTE;
	}
	if ((flags & guest_msg_waitall) != 0) {
		host_flags |= MSG_WAITALL;
	}

	flags &= ~(guest_msg_peek | guest_msg_dontroute | guest_msg_waitall | guest_msg_dontwait |
	           guest_msg_nosignal);
	if (flags != 0) {
		*Posix::GetErrorAddr() = Posix::POSIX_EOPNOTSUPP;
		return -1;
	}

	return host_flags;
#else
	return flags;
#endif
}

static int ConvertGuestSockaddr(const void* addr, uint32_t addrlen, sockaddr_storage* out,
                                SocketLength* out_len) {
	EXIT_IF(out == nullptr);
	EXIT_IF(out_len == nullptr);

	if (addr == nullptr) {
		*Posix::GetErrorAddr() = Posix::POSIX_EFAULT;
		return -1;
	}

	const auto* bytes = static_cast<const uint8_t*>(addr);
	if (addrlen < 2) {
		*Posix::GetErrorAddr() = Posix::POSIX_EINVAL;
		return -1;
	}

	const int family = ConvertFamily(bytes[1]);
	if (family != AF_INET) {
		*Posix::GetErrorAddr() = (family < 0 ? Posix::POSIX_EAFNOSUPPORT : Posix::POSIX_EOPNOTSUPP);
		return -1;
	}

	if (addrlen < 8) {
		*Posix::GetErrorAddr() = Posix::POSIX_EINVAL;
		return -1;
	}

	std::memset(out, 0, sizeof(*out));
	auto* in       = reinterpret_cast<sockaddr_in*>(out);
	in->sin_family = AF_INET;
	std::memcpy(&in->sin_port, bytes + 2, sizeof(in->sin_port));
	std::memcpy(&in->sin_addr, bytes + 4, sizeof(in->sin_addr));
	*out_len = sizeof(sockaddr_in);
	return 0;
}

#if defined(_WIN32)
static int ConvertHostSockaddr(const sockaddr_storage* addr, int addrlen, void* out,
                               uint32_t* out_len) {
	EXIT_IF(addr == nullptr);
	EXIT_IF(out_len == nullptr);

	if (out == nullptr) {
		*Posix::GetErrorAddr() = Posix::POSIX_EFAULT;
		return -1;
	}

	if (addr->ss_family != AF_INET || addrlen < static_cast<int>(sizeof(sockaddr_in))) {
		*Posix::GetErrorAddr() = Posix::POSIX_EOPNOTSUPP;
		return -1;
	}

	static constexpr uint32_t guest_addrlen = 16;
	if (*out_len < guest_addrlen) {
		*out_len               = guest_addrlen;
		*Posix::GetErrorAddr() = Posix::POSIX_EINVAL;
		return -1;
	}

	auto*       bytes = static_cast<uint8_t*>(out);
	const auto* in    = reinterpret_cast<const sockaddr_in*>(addr);
	std::memset(bytes, 0, guest_addrlen);
	bytes[0] = static_cast<uint8_t>(guest_addrlen);
	bytes[1] = 2;
	std::memcpy(bytes + 2, &in->sin_port, sizeof(in->sin_port));
	std::memcpy(bytes + 4, &in->sin_addr, sizeof(in->sin_addr));
	*out_len = guest_addrlen;
	return 0;
}
#endif

static bool GetSocketBackend(int guest_fd, NativeSocket* out) {
	EXIT_IF(out == nullptr);

	if (guest_fd < 0 || guest_fd >= SOCKET_FD_MAX) {
		return false;
	}

	Common::LockGuard lock(g_socket_mutex);
	if (!g_sockets[static_cast<size_t>(guest_fd)].used) {
		return false;
	}

	*out = g_sockets[static_cast<size_t>(guest_fd)].socket;
	return true;
}

bool KYTY_SYSV_ABI IsSocket(int s) {
	if (s < 0 || s >= SOCKET_FD_MAX) {
		return false;
	}

	Common::LockGuard lock(g_socket_mutex);
	return g_sockets[static_cast<size_t>(s)].used;
}

static bool TakeSocketBackend(int guest_fd, NativeSocket* out) {
	EXIT_IF(out == nullptr);

	if (guest_fd < 0 || guest_fd >= SOCKET_FD_MAX) {
		return false;
	}

	Common::LockGuard lock(g_socket_mutex);
	auto&             slot = g_sockets[static_cast<size_t>(guest_fd)];
	if (!slot.used) {
		return false;
	}

	*out = slot.socket;
	slot = {};
	return true;
}

static int AllocSocketFd(NativeSocket socket) {
	Common::LockGuard lock(g_socket_mutex);
	for (int fd = SOCKET_FD_MIN; fd < SOCKET_FD_MAX; fd++) {
		auto& slot = g_sockets[static_cast<size_t>(fd)];
		if (!slot.used) {
			slot.used   = true;
			slot.socket = socket;
			return fd;
		}
	}

	return -1;
}

static bool GuestFdIsSet(const void* fds, int fd) {
	if (fds == nullptr || fd < 0 || fd >= SOCKET_FD_MAX) {
		return false;
	}

	const auto* words = static_cast<const uint64_t*>(fds);
	return (words[fd / 64] & (uint64_t {1} << (fd % 64))) != 0;
}

static void GuestFdZero(void* fds, int nfds) {
	if (fds == nullptr || nfds <= 0) {
		return;
	}

	std::memset(fds, 0, static_cast<size_t>((nfds + 63) / 64) * sizeof(uint64_t));
}

static void GuestFdSet(void* fds, int fd) {
	if (fds == nullptr || fd < 0 || fd >= SOCKET_FD_MAX) {
		return;
	}

	auto* words = static_cast<uint64_t*>(fds);
	words[fd / 64] |= (uint64_t {1} << (fd % 64));
}

int KYTY_SYSV_ABI NetInit() {
	PRINT_NAME();

	EnsureSocketBackend();

	return OK;
}

int KYTY_SYSV_ABI NetTerm() {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NetPoolCreate(const char* name, int size, int flags) {
	PRINT_NAME();

	LOGF("\t name = %s\n"
	     "\t size = %d\n"
	     "\t flags = %d\n",
	     name, size, flags);

	EXIT_IF(g_net == nullptr);

	EXIT_NOT_IMPLEMENTED(flags != 0);
	EXIT_NOT_IMPLEMENTED(size == 0);

	int id = g_net->PoolCreate(name, size);

	if (id < 0) {
		return NET_ERROR_ENFILE;
	}

	return id;
}

int KYTY_SYSV_ABI NetPoolDestroy(int memid) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (!g_net->PoolDestroy(memid)) {
		return NET_ERROR_EBADF;
	}

	return OK;
}

int KYTY_SYSV_ABI NetResolverCreate(const char* name, int memid, int flags) {
	PRINT_NAME();

	LOGF("\t name  = %s\n"
	     "\t memid = %d\n"
	     "\t flags = %d\n",
	     name != nullptr ? name : "<null>", memid, flags);

	if (name == nullptr || flags != 0) {
		return NET_ERROR_EINVAL;
	}

	EXIT_IF(g_net == nullptr);

	const int id = g_net->ResolverCreate(name, memid);
	if (id < 0) {
		return NET_ERROR_RESOLVER_ENOSPACE;
	}

	return id;
}

int KYTY_SYSV_ABI NetResolverDestroy(int rid) {
	PRINT_NAME();

	LOGF("\t rid = %d\n", rid);

	EXIT_IF(g_net == nullptr);
	if (!g_net->ResolverDestroy(rid)) {
		return NET_ERROR_EBADF;
	}

	return OK;
}

int KYTY_SYSV_ABI NetResolverStartNtoa(int rid, const char* hostname, void* addr, int timeout,
                                       int retry, int flags) {
	PRINT_NAME();

	LOGF("\t rid      = %d\n"
	     "\t hostname = %s\n"
	     "\t timeout  = %d\n"
	     "\t retry    = %d\n"
	     "\t flags    = 0x%08x\n",
	     rid, hostname != nullptr ? hostname : "<null>", timeout, retry, flags);

	(void)timeout;
	(void)retry;

	if (hostname == nullptr || addr == nullptr || (flags & ~0x00010000) != 0) {
		return NET_ERROR_EINVAL;
	}

	EXIT_IF(g_net == nullptr);
	if (!g_net->ResolverValid(rid)) {
		return NET_ERROR_EBADF;
	}

	if ((flags & 0x00010000) == 0 && NetInetPton(2, hostname, addr) == 1) {
		return OK;
	}

#if defined(_WIN32)
	if (!EnsureSocketBackend()) {
		return NET_ERROR_ENETDOWN;
	}

	addrinfo hints {};
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	addrinfo* result = nullptr;
	const int ret    = getaddrinfo(hostname, nullptr, &hints, &result);
	if (ret != 0 || result == nullptr) {
		if (result != nullptr) {
			freeaddrinfo(result);
		}
		return ret == EAI_NONAME ? NET_ERROR_RESOLVER_ENOHOST : NET_ERROR_RESOLVER_EINTERNAL;
	}

	for (auto* ai = result; ai != nullptr; ai = ai->ai_next) {
		if (ai->ai_family == AF_INET && ai->ai_addr != nullptr &&
		    ai->ai_addrlen >= sizeof(sockaddr_in)) {
			const auto* in = reinterpret_cast<const sockaddr_in*>(ai->ai_addr);
			std::memcpy(addr, &in->sin_addr, sizeof(in->sin_addr));
			freeaddrinfo(result);
			return OK;
		}
	}

	freeaddrinfo(result);
	return NET_ERROR_RESOLVER_ENORECORD;
#else
	return NET_ERROR_RESOLVER_ENOTIMPLEMENTED;
#endif
}

int KYTY_SYSV_ABI NetInetPton(int af, const char* src, void* dst) {
	PRINT_NAME();

	if (src == nullptr || dst == nullptr) {
		return NET_ERROR_EINVAL;
	}

	const int host_family = ConvertFamily(af);
	if (host_family < 0) {
		return NET_ERROR_EAFNOSUPPORT;
	}

	LOGF("\t src = %.46s\n", src);

#if defined(_WIN32)
	const int result = ::InetPtonA(host_family, src, dst);
#else
	const int result = ::inet_pton(host_family, src, dst);
#endif
	return result < 0 ? NET_ERROR_EINVAL : result;
}

const char* KYTY_SYSV_ABI NetInetNtop(int af, const void* src, char* dst, uint32_t size) {
	PRINT_NAME();

	if (src == nullptr || dst == nullptr || size == 0) {
		return nullptr;
	}

	if (af != 2 && af != 28) {
		return nullptr;
	}

#if defined(_WIN32)
	const int win_af = (af == 28 ? AF_INET6 : AF_INET);
	if (::InetNtopA(win_af, const_cast<void*>(src), dst, size) == nullptr) {
		return nullptr;
	}
#else
	if (::inet_ntop(af, src, dst, size) == nullptr) {
		return nullptr;
	}
#endif

	return dst;
}

int KYTY_SYSV_ABI NetEtherNtostr(const NetEtherAddr* n, char* str, size_t len) {
	PRINT_NAME();

	NetEtherAddr zero {};

	EXIT_NOT_IMPLEMENTED(len != 18);
	EXIT_NOT_IMPLEMENTED(n == nullptr);
	EXIT_NOT_IMPLEMENTED(str == nullptr);
	EXIT_NOT_IMPLEMENTED(memcmp(n->data, zero.data, sizeof(zero.data)) != 0);

	strcpy(str, "00:00:00:00:00:00"); // NOLINT

	return OK;
}

int KYTY_SYSV_ABI NetGetMacAddress(NetEtherAddr* addr, int flags) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(addr == nullptr);
	EXIT_NOT_IMPLEMENTED(flags != 0);

	memset(addr->data, 0, sizeof(addr->data));

	return OK;
}

int KYTY_SYSV_ABI NetGetSockInfo(int s, void* info, int n, int flags) {
	PRINT_NAME();

	LOGF("\t s     = %d\n"
	     "\t info  = 0x%016" PRIx64 "\n"
	     "\t n     = %d\n"
	     "\t flags = %d\n",
	     s, reinterpret_cast<uint64_t>(info), n, flags);

	return OK;
}

int KYTY_SYSV_ABI EpollCreate(const char* name, int flags) {
	if (name == nullptr || flags != 0) {
		return SetGuestSocketError(Posix::POSIX_EINVAL);
	}

	std::lock_guard lock(g_epoll_mutex);
	for (size_t index = 0; index < g_epolls.size(); index++) {
		auto& slot = g_epolls[index];
		if (!slot.used) {
			slot.used = true;
			slot.name = name;
			slot.registrations.clear();
			return EPOLL_ID_BASE + static_cast<int>(index);
		}
	}

	return SetGuestSocketError(Posix::POSIX_EMFILE);
}

int KYTY_SYSV_ABI EpollControl(int eid, int op, int id, const NetEpollEvent* event) {
	constexpr int EPOLL_CTL_ADD = 1;
	constexpr int EPOLL_CTL_MOD = 2;
	constexpr int EPOLL_CTL_DEL = 3;

	if ((op == EPOLL_CTL_ADD || op == EPOLL_CTL_MOD) && event == nullptr) {
		return SetGuestSocketError(Posix::POSIX_EINVAL);
	}
	if (op == EPOLL_CTL_DEL && event != nullptr) {
		return SetGuestSocketError(Posix::POSIX_EINVAL);
	}
	if (op < EPOLL_CTL_ADD || op > EPOLL_CTL_DEL) {
		return SetGuestSocketError(Posix::POSIX_EINVAL);
	}
	if (!IsSocket(id)) {
		return SetGuestSocketError(Posix::POSIX_EBADF);
	}

	std::lock_guard lock(g_epoll_mutex);
	auto*           slot = GetEpollSlot(eid);
	if (slot == nullptr) {
		return SetGuestSocketError(Posix::POSIX_EBADF);
	}

	auto registration = std::find_if(slot->registrations.begin(), slot->registrations.end(),
	                                 [id](const auto& value) { return value.id == id; });
	switch (op) {
		case EPOLL_CTL_ADD:
			if (registration != slot->registrations.end()) {
				return SetGuestSocketError(Posix::POSIX_EEXIST);
			}
			slot->registrations.push_back({id, *event});
			break;
		case EPOLL_CTL_MOD:
			if (registration == slot->registrations.end()) {
				return SetGuestSocketError(Posix::POSIX_ENOENT);
			}
			registration->event = *event;
			break;
		case EPOLL_CTL_DEL:
			if (registration == slot->registrations.end()) {
				return SetGuestSocketError(Posix::POSIX_ENOENT);
			}
			slot->registrations.erase(registration);
			break;
		default: break;
	}

	g_epoll_changed.notify_all();
	return 0;
}

int KYTY_SYSV_ABI EpollWait(int eid, NetEpollEvent* events, int maxevents, int timeout) {
	if (events == nullptr) {
		return SetGuestSocketError(Posix::POSIX_EFAULT);
	}
	if (maxevents <= 0 || timeout < -1) {
		return SetGuestSocketError(Posix::POSIX_EINVAL);
	}

	std::vector<EpollRegistration> registrations;
	{
		std::unique_lock lock(g_epoll_mutex);
		auto*            slot = GetEpollSlot(eid);
		if (slot == nullptr) {
			return SetGuestSocketError(Posix::POSIX_EBADF);
		}

		if (slot->registrations.empty() && timeout != 0) {
			auto ready = [eid] {
				auto* current = GetEpollSlot(eid);
				return current == nullptr || !current->registrations.empty();
			};
			if (timeout < 0) {
				g_epoll_changed.wait(lock, ready);
			} else {
				g_epoll_changed.wait_for(lock, std::chrono::microseconds(timeout), ready);
			}
			slot = GetEpollSlot(eid);
			if (slot == nullptr) {
				return SetGuestSocketError(Posix::POSIX_EBADF);
			}
		}
		registrations = slot->registrations;
	}

	if (registrations.empty()) {
		return 0;
	}

#if defined(_WIN32)
	struct HostRegistration {
		EpollRegistration guest;
		NativeSocket      socket = INVALID_NATIVE_SOCKET;
	};

	fd_set host_read {};
	fd_set host_write {};
	fd_set host_except {};
	FD_ZERO(&host_read);
	FD_ZERO(&host_write);
	FD_ZERO(&host_except);

	std::vector<HostRegistration> host_registrations;
	host_registrations.reserve(registrations.size());
	for (const auto& registration: registrations) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (!GetSocketBackend(registration.id, &socket)) {
			continue;
		}
		if (host_registrations.size() >= FD_SETSIZE) {
			return SetGuestSocketError(Posix::POSIX_EINVAL);
		}
		if ((registration.event.events & EPOLL_IN) != 0) {
			FD_SET(socket, &host_read);
		}
		if ((registration.event.events & EPOLL_OUT) != 0) {
			FD_SET(socket, &host_write);
		}
		FD_SET(socket, &host_except);
		host_registrations.push_back({registration, socket});
	}

	if (host_registrations.empty()) {
		return 0;
	}

	timeval  host_timeout {};
	timeval* host_timeout_ptr = nullptr;
	if (timeout >= 0) {
		host_timeout.tv_sec  = timeout / 1000000;
		host_timeout.tv_usec = timeout % 1000000;
		host_timeout_ptr     = &host_timeout;
	}

	const int result = ::select(0, &host_read, &host_write, &host_except, host_timeout_ptr);
	if (result == SOCKET_ERROR) {
		return SetHostSocketError();
	}
	if (result == 0) {
		return 0;
	}

	int count = 0;
	for (const auto& registration: host_registrations) {
		uint32_t ready = 0;
		if (FD_ISSET(registration.socket, &host_read)) {
			ready |= EPOLL_IN;
		}
		if (FD_ISSET(registration.socket, &host_write)) {
			ready |= EPOLL_OUT;
		}
		if (FD_ISSET(registration.socket, &host_except)) {
			ready |= EPOLL_ERR;
		}
		if (ready == 0) {
			continue;
		}

		auto& output    = events[count++];
		output          = registration.guest.event;
		output.events   = ready;
		output.reserved = 0;
		output.ident    = static_cast<uint64_t>(registration.guest.id);
		if (count == maxevents) {
			break;
		}
	}
	return count;
#else
	// POSIX port: poll(2) instead of select(). Same guest-visible semantics
	// as the Windows branch (readable -> EPOLL_IN, writable -> EPOLL_OUT,
	// error/hangup -> EPOLL_ERR), no FD_SETSIZE cap, and the timeout conversion
	// is identical (guest timeouts are microseconds).
	struct HostPollFd {
		EpollRegistration guest;
		NativeSocket       socket = INVALID_NATIVE_SOCKET;
	};

	std::vector<HostPollFd> host_fds;
	host_fds.reserve(registrations.size());
	for (const auto& registration: registrations) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (!GetSocketBackend(registration.id, &socket)) {
			continue;
		}
		HostPollFd host {};
		host.guest  = registration;
		host.socket = socket;
		host_fds.push_back(host);
	}

	if (host_fds.empty()) {
		return 0;
	}

	std::vector<pollfd> fds;
	fds.reserve(host_fds.size());
	for (const auto& host: host_fds) {
		pollfd pfd {};
		pfd.fd = static_cast<int>(host.socket);
		// Observe everything the guest did not ask for too: POLLERR/POLLHUP are
		// always reported by the Windows branch (FD_SET into host_except), so
		// the POSIX port requests read/write interest plus the implicit
		// error/hangup set poll always monitors.
		pfd.events = short(0);
		if ((host.guest.event.events & EPOLL_IN) != 0) {
			pfd.events |= POLLIN;
		}
		if ((host.guest.event.events & EPOLL_OUT) != 0) {
			pfd.events |= POLLOUT;
		}
		if (pfd.events == 0) {
			pfd.events = POLLIN; // error-only watches: still detect failure via revents
		}
		fds.push_back(pfd);
	}

	int poll_timeout = -1; // guest < 0 = block
	if (timeout >= 0) {
		// microseconds -> milliseconds, rounding up so a 1us guest timeout
		// still observes at least one poll tick.
		poll_timeout = static_cast<int>((timeout + 999) / 1000);
	}

	const int result = ::poll(fds.data(), static_cast<nfds_t>(fds.size()), poll_timeout);
	if (result < 0) {
		return SetHostSocketError();
	}
	if (result == 0) {
		return 0; // timed out
	}

	int count = 0;
	for (size_t i = 0; i < fds.size(); i++) {
		uint32_t ready = 0;
		if ((fds[i].revents & POLLIN) != 0) {
			ready |= EPOLL_IN;
		}
		if ((fds[i].revents & POLLOUT) != 0) {
			ready |= EPOLL_OUT;
		}
		if ((fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			ready |= EPOLL_ERR;
		}
		if (ready == 0) {
			continue;
		}

		auto& output  = events[count++];
		output        = host_fds[i].guest.event;
		output.events = ready;
		output.reserved = 0;
		output.ident    = static_cast<uint64_t>(host_fds[i].guest.id);
		if (count == maxevents) {
			break;
		}
	}
	return count;
#endif
}

int KYTY_SYSV_ABI EpollDestroy(int eid) {
	std::lock_guard lock(g_epoll_mutex);
	auto*           slot = GetEpollSlot(eid);
	if (slot == nullptr) {
		return SetGuestSocketError(Posix::POSIX_EBADF);
	}

	slot->used = false;
	slot->name.clear();
	slot->registrations.clear();
	g_epoll_changed.notify_all();
	return 0;
}

int KYTY_SYSV_ABI SocketClose(int s) {
	PRINT_NAME();

	LOGF("\t s = %d\n", s);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (!TakeSocketBackend(s, &socket)) {
		return NET_ERROR_EBADF;
	}
	RemoveSocketFromEpolls(s);

#if defined(_WIN32)
	const int result = closesocket(socket);
#else
	const int result = ::close(socket);
#endif
	if (result != 0) {
		return NET_ERROR_EBADF;
	}

	return OK;
}

int KYTY_SYSV_ABI Socket(int family, int type, int protocol) {
	PRINT_NAME();

	LOGF("\t family   = %d\n"
	     "\t type     = %d\n"
	     "\t protocol = %d\n",
	     family, type, protocol);

	if (!EnsureSocketBackend()) {
		return SetGuestSocketError(Posix::POSIX_ENETDOWN);
	}

	const int host_family = ConvertFamily(family);
	if (host_family < 0) {
		*Posix::GetErrorAddr() = Posix::POSIX_EAFNOSUPPORT;
		return -1;
	}

	NativeSocket socket = ::socket(host_family, type, protocol);
	if (socket == INVALID_NATIVE_SOCKET) {
		return SetHostSocketError();
	}

	const int fd = AllocSocketFd(socket);
	if (fd < 0) {
#if defined(_WIN32)
		closesocket(socket);
#else
		::close(socket);
#endif
		*Posix::GetErrorAddr() = Posix::POSIX_EMFILE;
		return -1;
	}

	LOGF("\t fd = %d\n", fd);
	return fd;
}

int KYTY_SYSV_ABI Bind(int s, const void* addr, uint32_t addrlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t addr    = 0x%016" PRIx64 "\n"
	     "\t addrlen = %" PRIu32 "\n",
	     s, reinterpret_cast<uint64_t>(addr), addrlen);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (addr == nullptr || !GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = (addr == nullptr ? Posix::POSIX_EFAULT : Posix::POSIX_EBADF);
		return -1;
	}

	sockaddr_storage host_addr {};
	SocketLength     host_addrlen = 0;
	if (ConvertGuestSockaddr(addr, addrlen, &host_addr, &host_addrlen) != 0) {
		return -1;
	}

	if (::bind(socket, reinterpret_cast<const sockaddr*>(&host_addr), host_addrlen) != 0) {
		return SetHostSocketError();
	}

	return 0;
}

int KYTY_SYSV_ABI Connect(int s, const void* addr, uint32_t addrlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t addr    = 0x%016" PRIx64 "\n"
	     "\t addrlen = %" PRIu32 "\n",
	     s, reinterpret_cast<uint64_t>(addr), addrlen);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (addr == nullptr || !GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = (addr == nullptr ? Posix::POSIX_EFAULT : Posix::POSIX_EBADF);
		return -1;
	}

	sockaddr_storage host_addr {};
	SocketLength     host_addrlen = 0;
	if (ConvertGuestSockaddr(addr, addrlen, &host_addr, &host_addrlen) != 0) {
		return -1;
	}

	if (::connect(socket, reinterpret_cast<const sockaddr*>(&host_addr), host_addrlen) ==
	    SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return 0;
}

int KYTY_SYSV_ABI Listen(int s, int backlog) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t backlog = %d\n",
	     s, backlog);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (!GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = Posix::POSIX_EBADF;
		return -1;
	}

	if (::listen(socket, backlog) == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return 0;
}

int KYTY_SYSV_ABI Accept(int s, void* addr, uint32_t* addrlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t addr    = 0x%016" PRIx64 "\n"
	     "\t addrlen = 0x%016" PRIx64 "\n",
	     s, reinterpret_cast<uint64_t>(addr), reinterpret_cast<uint64_t>(addrlen));

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (!GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = Posix::POSIX_EBADF;
		return -1;
	}

	sockaddr_storage host_addr {};
	SocketLength     host_addrlen = sizeof(host_addr);
	NativeSocket     accepted =
	    ::accept(socket, reinterpret_cast<sockaddr*>(&host_addr), &host_addrlen);
	if (accepted == INVALID_NATIVE_SOCKET) {
		return SetHostSocketError();
	}

	const int fd = AllocSocketFd(accepted);
	if (fd < 0) {
#if defined(_WIN32)
		closesocket(accepted);
#else
		::close(accepted);
#endif
		*Posix::GetErrorAddr() = Posix::POSIX_EMFILE;
		return -1;
	}

	if (addr != nullptr || addrlen != nullptr) {
		if (addr == nullptr || addrlen == nullptr) {
			SocketClose(fd);
			*Posix::GetErrorAddr() = Posix::POSIX_EFAULT;
			return -1;
		}
		if (ConvertHostSockaddr(&host_addr, host_addrlen, addr, addrlen) != 0) {
			SocketClose(fd);
			return -1;
		}
	}

	return fd;
}

int KYTY_SYSV_ABI Shutdown(int s, int how) {
	PRINT_NAME();

	LOGF("\t s   = %d\n"
	     "\t how = %d\n",
	     s, how);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (!GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = Posix::POSIX_EBADF;
		return -1;
	}

	if (how < 0 || how > 2) {
		*Posix::GetErrorAddr() = Posix::POSIX_EINVAL;
		return -1;
	}

	if (::shutdown(socket, how) == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return 0;
}

int KYTY_SYSV_ABI Getsockname(int s, void* addr, uint32_t* addrlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t addr    = 0x%016" PRIx64 "\n"
	     "\t addrlen = 0x%016" PRIx64 "\n",
	     s, reinterpret_cast<uint64_t>(addr), reinterpret_cast<uint64_t>(addrlen));

	NativeSocket socket    = INVALID_NATIVE_SOCKET;
	const bool   socket_ok = GetSocketBackend(s, &socket);
	if (addr == nullptr || addrlen == nullptr || !socket_ok) {
		*Posix::GetErrorAddr() = (!socket_ok ? Posix::POSIX_EBADF : Posix::POSIX_EFAULT);
		return -1;
	}

	sockaddr_storage host_addr {};
	SocketLength     host_addrlen = sizeof(host_addr);
	if (::getsockname(socket, reinterpret_cast<sockaddr*>(&host_addr), &host_addrlen) ==
	    SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return ConvertHostSockaddr(&host_addr, host_addrlen, addr, addrlen);
}

int KYTY_SYSV_ABI Getsockopt(int s, int level, int optname, void* optval, uint32_t* optlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t level   = 0x%08" PRIx32 "\n"
	     "\t optname = 0x%08" PRIx32 "\n",
	     s, static_cast<uint32_t>(level), static_cast<uint32_t>(optname));

	NativeSocket socket    = INVALID_NATIVE_SOCKET;
	const bool   socket_ok = GetSocketBackend(s, &socket);
	if (optval == nullptr || optlen == nullptr || !socket_ok) {
		*Posix::GetErrorAddr() = (!socket_ok ? Posix::POSIX_EBADF : Posix::POSIX_EFAULT);
		return -1;
	}

	int len = static_cast<int>(*optlen);
	if (::getsockopt(socket, ConvertSocketOptionLevel(level), optname, static_cast<char*>(optval),
	                 &len) == SOCKET_ERROR) {
		return SetHostSocketError();
	}
	*optlen = static_cast<uint32_t>(len);
	return 0;
}

int KYTY_SYSV_ABI Setsockopt(int s, int level, int optname, const void* optval, uint32_t optlen) {
	PRINT_NAME();

	LOGF("\t s       = %d\n"
	     "\t level   = 0x%08" PRIx32 "\n"
	     "\t optname = 0x%08" PRIx32 "\n"
	     "\t optlen  = %" PRIu32 "\n",
	     s, static_cast<uint32_t>(level), static_cast<uint32_t>(optname), optlen);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (optval == nullptr || !GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = (optval == nullptr ? Posix::POSIX_EFAULT : Posix::POSIX_EBADF);
		return -1;
	}

	constexpr int ORBIS_SO_NBIO = 0x1200;
	if (ConvertSocketOptionLevel(level) == SOL_SOCKET && optname == ORBIS_SO_NBIO &&
	    optlen >= sizeof(int)) {
		const bool enabled = (*static_cast<const int*>(optval) != 0);
#if defined(_WIN32)
		u_long arg = (enabled ? 1 : 0);
		if (ioctlsocket(socket, FIONBIO, &arg) == SOCKET_ERROR) {
			return SetHostSocketError();
		}
#else
		const int flags = ::fcntl(socket, F_GETFL, 0);
		if (flags < 0) {
			return SetHostSocketError();
		}
		if (::fcntl(socket, F_SETFL, (enabled ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK))) <
		    0) {
			return SetHostSocketError();
		}
#endif
		return 0;
	}

	if (::setsockopt(socket, ConvertSocketOptionLevel(level), optname,
	                 static_cast<const char*>(optval), static_cast<int>(optlen)) == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return 0;
}

int64_t KYTY_SYSV_ABI Send(int s, const void* buf, uint64_t len, int flags) {
	return Sendto(s, buf, len, flags, nullptr, 0);
}

int64_t KYTY_SYSV_ABI Sendto(int s, const void* buf, uint64_t len, int flags, const void* addr,
                             uint32_t addrlen) {
	PRINT_NAME();

	LOGF("\t s     = %d\n"
	     "\t buf   = 0x%016" PRIx64 "\n"
	     "\t len   = %" PRIu64 "\n"
	     "\t flags = 0x%08" PRIx32 "\n"
	     "\t addr  = 0x%016" PRIx64 "\n"
	     "\t addrlen = %" PRIu32 "\n",
	     s, reinterpret_cast<uint64_t>(buf), len, static_cast<uint32_t>(flags),
	     reinterpret_cast<uint64_t>(addr), addrlen);

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (buf == nullptr || !GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = (buf == nullptr ? Posix::POSIX_EFAULT : Posix::POSIX_EBADF);
		return -1;
	}

	const int host_flags = ConvertMessageFlags(flags);
	if (host_flags < 0) {
		return -1;
	}

	const int host_len = static_cast<int>(len > 0x7fffffffu ? 0x7fffffffu : len);
	int       result   = 0;
	if (addr == nullptr) {
		result = ::send(socket, static_cast<const char*>(buf), host_len, host_flags);
	} else {
		sockaddr_storage host_addr {};
		SocketLength     host_addrlen = 0;
		if (ConvertGuestSockaddr(addr, addrlen, &host_addr, &host_addrlen) != 0) {
			return -1;
		}
		result = ::sendto(socket, static_cast<const char*>(buf), host_len, host_flags,
		                  reinterpret_cast<const sockaddr*>(&host_addr), host_addrlen);
	}
	if (result == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return result;
}

int64_t KYTY_SYSV_ABI Recv(int s, void* buf, uint64_t len, int flags) {
	return Recvfrom(s, buf, len, flags, nullptr, nullptr);
}

bool RunUdpLoopbackDiagnostic() {
	if (!EnsureSocketBackend()) {
		return false;
	}

	const NativeSocket receiver = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (receiver == INVALID_NATIVE_SOCKET) {
		return false;
	}
	const NativeSocket sender = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sender == INVALID_NATIVE_SOCKET) {
#if defined(_WIN32)
		closesocket(receiver);
#else
		::close(receiver);
#endif
		return false;
	}

	sockaddr_in address {};
	address.sin_family      = AF_INET;
	const uint32_t loopback_address = htonl(INADDR_LOOPBACK);
	std::memcpy(&address.sin_addr, &loopback_address, sizeof(loopback_address));
	address.sin_port        = 0;
	bool ok = ::bind(receiver, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == 0;
	SocketLength address_length = sizeof(address);
	if (ok) {
		ok = ::getsockname(receiver, reinterpret_cast<sockaddr*>(&address), &address_length) == 0;
	}

	constexpr char message[] = "kyty-udp-loopback";
	if (ok) {
		const auto sent = ::sendto(sender, message, sizeof(message), 0,
		                           reinterpret_cast<const sockaddr*>(&address), sizeof(address));
		ok = sent == static_cast<decltype(sent)>(sizeof(message));
	}

	char received[sizeof(message)] {};
	if (ok) {
		sockaddr_in source {};
		SocketLength source_length = sizeof(source);
		const auto received_size = ::recvfrom(receiver, received, sizeof(received), 0,
		                                      reinterpret_cast<sockaddr*>(&source), &source_length);
		ok = received_size == static_cast<decltype(received_size)>(sizeof(message)) &&
		     std::memcmp(received, message, sizeof(message)) == 0;
	}

#if defined(_WIN32)
	closesocket(sender);
	closesocket(receiver);
#else
	::close(sender);
	::close(receiver);
#endif
	return ok;
}

int64_t KYTY_SYSV_ABI Recvfrom(int s, void* buf, uint64_t len, int flags, void* addr,
                               uint32_t* addrlen) {
	PRINT_NAME();

	LOGF("\t s     = %d\n"
	     "\t buf   = 0x%016" PRIx64 "\n"
	     "\t len   = %" PRIu64 "\n"
	     "\t flags = 0x%08" PRIx32 "\n"
	     "\t addr  = 0x%016" PRIx64 "\n"
	     "\t addrlen = 0x%016" PRIx64 "\n",
	     s, reinterpret_cast<uint64_t>(buf), len, static_cast<uint32_t>(flags),
	     reinterpret_cast<uint64_t>(addr), reinterpret_cast<uint64_t>(addrlen));

	if (buf == nullptr || (addr != nullptr && addrlen == nullptr)) {
		*Posix::GetErrorAddr() = Posix::POSIX_EFAULT;
		return -1;
	}

	NativeSocket socket = INVALID_NATIVE_SOCKET;
	if (!GetSocketBackend(s, &socket)) {
		*Posix::GetErrorAddr() = Posix::POSIX_EBADF;
		return -1;
	}

	const int host_flags = ConvertMessageFlags(flags);
	if (host_flags < 0) {
		return -1;
	}

	const int host_len = static_cast<int>(len > 0x7fffffffu ? 0x7fffffffu : len);
	int       result   = 0;
	if (addr == nullptr) {
		result = ::recv(socket, static_cast<char*>(buf), host_len, host_flags);
	} else {
		sockaddr_storage host_addr {};
		SocketLength     host_addrlen = sizeof(host_addr);
		result = ::recvfrom(socket, static_cast<char*>(buf), host_len, host_flags,
		                    reinterpret_cast<sockaddr*>(&host_addr), &host_addrlen);
		if (result != SOCKET_ERROR &&
		    ConvertHostSockaddr(&host_addr, host_addrlen, addr, addrlen) != 0) {
			return -1;
		}
	}
	if (result == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	return result;
}

int KYTY_SYSV_ABI Select(int nfds, void* readfds, void* writefds, void* exceptfds,
                         const void* timeout) {
	PRINT_NAME();

	if (nfds < 0 || nfds > SOCKET_FD_MAX) {
		*Posix::GetErrorAddr() = Posix::POSIX_EINVAL;
		return -1;
	}

	static std::atomic_uint32_t select_log_count = 0;
	const bool log_select = select_log_count.fetch_add(1, std::memory_order_relaxed) < 64;

	if (log_select) {
		const auto  read0   = (readfds != nullptr ? *static_cast<const uint64_t*>(readfds) : 0);
		const auto  write0  = (writefds != nullptr ? *static_cast<const uint64_t*>(writefds) : 0);
		const auto  except0 = (exceptfds != nullptr ? *static_cast<const uint64_t*>(exceptfds) : 0);
		const auto* guest_timeout = static_cast<const NetTimeval*>(timeout);
		LOGF("\t nfds       = %d\n"
		     "\t read       = 0x%016" PRIx64 " bits0=0x%016" PRIx64 "\n"
		     "\t write      = 0x%016" PRIx64 " bits0=0x%016" PRIx64 "\n"
		     "\t except     = 0x%016" PRIx64 " bits0=0x%016" PRIx64 "\n"
		     "\t timeout    = 0x%016" PRIx64,
		     nfds, reinterpret_cast<uint64_t>(readfds), read0, reinterpret_cast<uint64_t>(writefds),
		     write0, reinterpret_cast<uint64_t>(exceptfds), except0,
		     reinterpret_cast<uint64_t>(timeout));
		if (guest_timeout != nullptr) {
			LOGF(" sec=%" PRId64 " usec=%" PRId64, guest_timeout->tv_sec, guest_timeout->tv_usec);
		}
		LOGF("\n");
	}

	fd_set host_read {};
	fd_set host_write {};
	fd_set host_except {};
	FD_ZERO(&host_read);
	FD_ZERO(&host_write);
	FD_ZERO(&host_except);

	std::array<int, FD_SETSIZE> read_map {};
	std::array<int, FD_SETSIZE> write_map {};
	std::array<int, FD_SETSIZE> except_map {};
	int                         read_count   = 0;
	int                         write_count  = 0;
	int                         except_count = 0;

	for (int fd = 0; fd < nfds; fd++) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (!GetSocketBackend(fd, &socket)) {
			continue;
		}
		if (GuestFdIsSet(readfds, fd) && read_count < FD_SETSIZE) {
			FD_SET(socket, &host_read);
			read_map[static_cast<size_t>(read_count++)] = fd;
		}
		if (GuestFdIsSet(writefds, fd) && write_count < FD_SETSIZE) {
			FD_SET(socket, &host_write);
			write_map[static_cast<size_t>(write_count++)] = fd;
		}
		if (GuestFdIsSet(exceptfds, fd) && except_count < FD_SETSIZE) {
			FD_SET(socket, &host_except);
			except_map[static_cast<size_t>(except_count++)] = fd;
		}
	}

	timeval  host_timeout {};
	timeval* host_timeout_ptr = nullptr;
	if (timeout != nullptr) {
		const auto* guest_timeout = static_cast<const NetTimeval*>(timeout);
		host_timeout.tv_sec       = static_cast<long>(guest_timeout->tv_sec);
		host_timeout.tv_usec      = static_cast<long>(guest_timeout->tv_usec);
		host_timeout_ptr          = &host_timeout;
	}

	fd_set* read_ptr   = (read_count != 0 ? &host_read : nullptr);
	fd_set* write_ptr  = (write_count != 0 ? &host_write : nullptr);
	fd_set* except_ptr = (except_count != 0 ? &host_except : nullptr);
	if (read_ptr == nullptr && write_ptr == nullptr && except_ptr == nullptr) {
		if (host_timeout_ptr != nullptr) {
			const int64_t sleep_us =
			    static_cast<int64_t>(host_timeout.tv_sec) * 1000000 + host_timeout.tv_usec;
#if defined(_WIN32)
			::Sleep(static_cast<DWORD>(sleep_us / 1000));
#else
			timespec ts {};
			ts.tv_sec  = static_cast<time_t>(sleep_us / 1000000);
			ts.tv_nsec = static_cast<long>((sleep_us % 1000000) * 1000);
			::nanosleep(&ts, nullptr);
#endif
		}
		GuestFdZero(readfds, nfds);
		GuestFdZero(writefds, nfds);
		GuestFdZero(exceptfds, nfds);
		return 0;
	}

	// POSIX select() requires nfds = highest descriptor + 1; Windows ignores
	// it (passing the guest-view nfds is valid on both).
	const int result = ::select(nfds, read_ptr, write_ptr, except_ptr, host_timeout_ptr);
	if (result == SOCKET_ERROR) {
		return SetHostSocketError();
	}

	GuestFdZero(readfds, nfds);
	GuestFdZero(writefds, nfds);
	GuestFdZero(exceptfds, nfds);
	for (int i = 0; i < read_count; i++) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (GetSocketBackend(read_map[static_cast<size_t>(i)], &socket) &&
		    FD_ISSET(socket, &host_read)) {
			GuestFdSet(readfds, read_map[static_cast<size_t>(i)]);
		}
	}
	for (int i = 0; i < write_count; i++) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (GetSocketBackend(write_map[static_cast<size_t>(i)], &socket) &&
		    FD_ISSET(socket, &host_write)) {
			GuestFdSet(writefds, write_map[static_cast<size_t>(i)]);
		}
	}
	for (int i = 0; i < except_count; i++) {
		NativeSocket socket = INVALID_NATIVE_SOCKET;
		if (GetSocketBackend(except_map[static_cast<size_t>(i)], &socket) &&
		    FD_ISSET(socket, &host_except)) {
			GuestFdSet(exceptfds, except_map[static_cast<size_t>(i)]);
		}
	}

	return result;
}

} // namespace Net

namespace Ssl {

LIB_NAME("Ssl", "Ssl");

struct SslData {
	char*  ptr  = nullptr;
	size_t size = 0;
};

struct SslCaCerts {
	SslData* cert_data     = nullptr;
	size_t   cert_data_num = 0;
	void*    pool          = nullptr;
};

int KYTY_SYSV_ABI SslInit(uint64_t pool_size) {
	PRINT_NAME();

	LOGF("\t size = %" PRIu64 "\n", pool_size);

	EXIT_IF(g_net == nullptr);

	EXIT_NOT_IMPLEMENTED(pool_size == 0);

	auto id = g_net->SslInit(pool_size);

	if (!id.IsValid()) {
		return SSL_ERROR_OUT_OF_SIZE;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI SslTerm(int ssl_ctx_id) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (!g_net->SslTerm(Network::Id(ssl_ctx_id))) {
		return SSL_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI SslGetCaCerts(int ssl_ctx_id, void* ca_certs) {
	PRINT_NAME();

	LOGF("\t ssl_ctx_id = %d\n", ssl_ctx_id);

	if (ca_certs == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

	EXIT_IF(g_net == nullptr);

	if (!g_net->SslValid(Network::Id(ssl_ctx_id))) {
		return SSL_ERROR_INVALID_ID;
	}

	auto* certs          = static_cast<SslCaCerts*>(ca_certs);
	certs->cert_data     = nullptr;
	certs->cert_data_num = 0;
	certs->pool          = nullptr;

	return SSL_ERROR_NOT_FOUND;
}

int KYTY_SYSV_ABI SslFreeCaCerts(int ssl_ctx_id, void* ca_certs) {
	PRINT_NAME();

	LOGF("\t ssl_ctx_id = %d\n", ssl_ctx_id);

	if (ca_certs == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

	EXIT_IF(g_net == nullptr);

	if (!g_net->SslValid(Network::Id(ssl_ctx_id))) {
		return SSL_ERROR_INVALID_ID;
	}

	auto* certs          = static_cast<SslCaCerts*>(ca_certs);
	certs->cert_data     = nullptr;
	certs->cert_data_num = 0;
	certs->pool          = nullptr;

	return OK;
}

} // namespace Ssl

namespace Http {

// SceHttpNBEvent bits (libSceHttp). IN means a response is ready to read;
// HUP/RESOLVER_ERR report a terminal request failure. Values match the PS4/PS5
// SDK (verified against the shadPS4 reference implementation of the same ABI).

// APC poll interval while a guest thread is blocked in HttpWaitRequest.
constexpr uint32_t SIGNAL_APC_POLL_MICROS = 10000;
static constexpr uint32_t HTTP_NB_EVENT_IN           = 0x00000001u;
static constexpr uint32_t HTTP_NB_EVENT_HUP          = 0x00000010u;
static constexpr uint32_t HTTP_NB_EVENT_RESOLVED     = 0x00010000u;
static constexpr uint32_t HTTP_NB_EVENT_RESOLVER_ERR = 0x00020000u;

struct HttpEpoll {
	Network::Id http_ctx_id = Network::Id(0);
	Network::Id request_id  = Network::Id(0);
	void*       user_arg    = nullptr;

	// Completion events queued by HttpSendRequest and drained by
	// HttpWaitRequest. Completion is synchronous in this HLE (requests are
	// resolved at send time), so no worker thread is required.
	Common::Mutex            events_mutex;
	Common::CondVar          events_cv;
	std::vector<Network::Id> completed_requests;
};

} // namespace Http

void Network::RegisterHttpEpoll(Libs::Network::Http::HttpEpoll* epoll) {
	EXIT_IF(epoll == nullptr);

	Common::LockGuard lock(m_epoll_registry_mutex);

	if (std::find(m_http_epolls.begin(), m_http_epolls.end(), epoll) == m_http_epolls.end()) {
		m_http_epolls.push_back(epoll);
	}
}

void Network::UnregisterHttpEpoll(Libs::Network::Http::HttpEpoll* epoll) {
	EXIT_IF(epoll == nullptr);

	Common::LockGuard lock(m_epoll_registry_mutex);

	auto it = std::find(m_http_epolls.begin(), m_http_epolls.end(), epoll);
	if (it != m_http_epolls.end()) {
		m_http_epolls.erase(it);
	}
}

std::vector<Libs::Network::Http::HttpEpoll*> Network::GetEpollsForRequest(Id req_id) {
	Common::LockGuard lock(m_epoll_registry_mutex);

	std::vector<Libs::Network::Http::HttpEpoll*> result;
	result.reserve(m_http_epolls.size());
	for (Libs::Network::Http::HttpEpoll* epoll: m_http_epolls) {
		if (epoll == nullptr) {
			continue;
		}

		Common::LockGuard epoll_lock(epoll->events_mutex);
		if (epoll->request_id == req_id) {
			result.push_back(epoll);
		}
	}
	return result;
}

void Network::RemoveQueuedEpollEvents(Id req_id) {
	Common::LockGuard lock(m_epoll_registry_mutex);

	for (Libs::Network::Http::HttpEpoll* epoll: m_http_epolls) {
		if (epoll == nullptr) {
			continue;
		}

		Common::LockGuard epoll_lock(epoll->events_mutex);
		auto&      queued = epoll->completed_requests;
		queued.erase(std::remove(queued.begin(), queued.end(), req_id), queued.end());
		if (epoll->request_id == req_id) {
			epoll->request_id = Id(0);
		}
	}
}

namespace Http {

LIB_NAME("Http", "Http");

static const char* HttpMethodToString(int method) {
	switch (method) {
		case 0: return "GET";
		case 1: return "POST";
		case 2: return "HEAD";
		case 3: return "OPTIONS";
		case 4: return "PUT";
		case 5: return "DELETE";
		case 6: return "TRACE";
		case 7: return "CONNECT";
		default: return nullptr;
	}
}

int KYTY_SYSV_ABI HttpInit(int memid, int ssl_ctx_id, uint64_t pool_size) {
	PRINT_NAME();

	LOGF("\t memid      = %d\n"
	     "\t ssl_ctx_id = %d\n"
	     "\t size       = %" PRIu64 "\n",
	     memid, ssl_ctx_id, pool_size);

	EXIT_IF(g_net == nullptr);

	EXIT_NOT_IMPLEMENTED(pool_size == 0);

	auto id = g_net->HttpInit(memid, Network::Id(ssl_ctx_id), pool_size);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpTerm(int http_ctx_id) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpTerm(Network::Id(http_ctx_id))) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpCreateTemplate(int http_ctx_id, const char* user_agent, int http_ver,
                                     int is_auto_proxy_conf) {
	PRINT_NAME();

	LOGF("\t http_ctx_id        = %d\n"
	     "\t user_agent         = %s\n"
	     "\t http_ver           = %d\n"
	     "\t is_auto_proxy_conf = %d\n",
	     http_ctx_id, user_agent, http_ver, is_auto_proxy_conf);

	EXIT_IF(g_net == nullptr);

	auto id = g_net->HttpCreateTemplate(Network::Id(http_ctx_id), user_agent, http_ver,
	                                    is_auto_proxy_conf != 0);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpDeleteTemplate(int tmpl_id) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpDeleteTemplate(Network::Id(tmpl_id))) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetNonblock(int id, int enable) {
	PRINT_NAME();

	LOGF("\t id     = %d\n"
	     "\t enable = %d\n",
	     id, enable);

	if (!g_net->HttpSetNonblock(Network::Id(id), enable != 0)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpsSetSslCallback(int id, HttpsCallback cbfunc, void* user_arg) {
	PRINT_NAME();

	LOGF("\t id     = %d\n", id);

	if (!g_net->HttpsSetSslCallback(Network::Id(id), cbfunc, user_arg)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpsSetMinSslVersion(int id, uint32_t ssl_version) {
	PRINT_NAME();

	LOGF("\t id          = %d\n"
	     "\t ssl_version = %u\n",
	     id, ssl_version);

	if (!g_net->HttpsSetMinSslVersion(Network::Id(id), ssl_version)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpsDisableOption(int id, uint32_t ssl_flags) {
	PRINT_NAME();

	LOGF("\t id        = %d\n"
	     "\t ssl_flags = %u\n",
	     id, ssl_flags);

	if (!g_net->HttpsDisableOption(Network::Id(id), ssl_flags)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetResolveTimeOut(int id, uint32_t usec) {
	PRINT_NAME();

	LOGF("\t id   = %d\n"
	     "\t usec = %u\n",
	     id, usec);

	if (!g_net->HttpSetResolveTimeOut(Network::Id(id), usec)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetResolveRetry(int id, int32_t retry) {
	PRINT_NAME();

	LOGF("\t id    = %d\n"
	     "\t retry = %d\n",
	     id, retry);

	if (!g_net->HttpSetResolveRetry(Network::Id(id), retry)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetConnectTimeOut(int id, uint32_t usec) {
	PRINT_NAME();

	LOGF("\t id   = %d\n"
	     "\t usec = %u\n",
	     id, usec);

	if (!g_net->HttpSetConnectTimeOut(Network::Id(id), usec)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetSendTimeOut(int id, uint32_t usec) {
	PRINT_NAME();

	LOGF("\t id   = %d\n"
	     "\t usec = %u\n",
	     id, usec);

	if (!g_net->HttpSetSendTimeOut(Network::Id(id), usec)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetRecvTimeOut(int id, uint32_t usec) {
	PRINT_NAME();

	LOGF("\t id   = %d\n"
	     "\t usec = %u\n",
	     id, usec);

	if (!g_net->HttpSetRecvTimeOut(Network::Id(id), usec)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetAutoRedirect(int id, int enable) {
	PRINT_NAME();

	LOGF("\t id     = %d\n"
	     "\t enable = %d\n",
	     id, enable);

	if (!g_net->HttpSetAutoRedirect(Network::Id(id), enable)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpSetAuthEnabled(int id, int enable) {
	PRINT_NAME();

	LOGF("\t id     = %d\n"
	     "\t enable = %d\n",
	     id, enable);

	if (!g_net->HttpSetAuthEnabled(Network::Id(id), enable)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpAddRequestHeader(int id, const char* name, const char* value, uint32_t mode) {
	PRINT_NAME();

	LOGF("\t id    = %d\n"
	     "\t name  = %s\n"
	     "\t value = %s\n"
	     "\t mode  = %u\n",
	     id, name, value, mode);

	EXIT_NOT_IMPLEMENTED(mode != 0 && mode != 1);

	if (!g_net->HttpAddRequestHeader(Network::Id(id), name, value, mode == 1)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpCreateEpoll(int http_ctx_id, HttpEpollHandle* eh) {
	PRINT_NAME();

	LOGF("\t http_ctx_id = %d\n", http_ctx_id);

	EXIT_IF(g_net == nullptr);

	EXIT_NOT_IMPLEMENTED(eh == nullptr);

	EXIT_NOT_IMPLEMENTED(!g_net->HttpValid(Network::Id(http_ctx_id)));

	*eh = new HttpEpoll;

	(*eh)->http_ctx_id = Network::Id(http_ctx_id);

	g_net->RegisterHttpEpoll(*eh);

	return OK;
}

int KYTY_SYSV_ABI HttpDestroyEpoll(int http_ctx_id, HttpEpollHandle eh) {
	PRINT_NAME();

	LOGF("\t http_ctx_id = %d\n", http_ctx_id);

	EXIT_IF(g_net == nullptr);

	EXIT_NOT_IMPLEMENTED(eh == nullptr);

	EXIT_NOT_IMPLEMENTED(!g_net->HttpValid(Network::Id(http_ctx_id)));

	g_net->UnregisterHttpEpoll(eh);

	delete eh;

	return OK;
}

int KYTY_SYSV_ABI HttpSetEpoll(int id, HttpEpollHandle eh, void* user_arg) {
	PRINT_NAME();

	LOGF("\t id = %d\n", id);

	EXIT_NOT_IMPLEMENTED(eh == nullptr);

	EXIT_NOT_IMPLEMENTED(!g_net->HttpValidRequest(Network::Id(id)));

	{
		Common::LockGuard lock(eh->events_mutex);
		eh->request_id = Network::Id(id);
		eh->user_arg   = user_arg;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpUnsetEpoll(int id) {
	PRINT_NAME();

	LOGF("\t id = %d\n", id);

	EXIT_NOT_IMPLEMENTED(!g_net->HttpValidRequest(Network::Id(id)));

	return OK;
}

} // namespace Http

// --- Real plain-HTTP client -------------------------------------------------
// Performs an actual HTTP/1.1 request over the host socket layer for
// http:// and https:// URLs (TLS via vendored mbed TLS when enabled).

#if defined(KYTY_HAS_MBEDTLS)
// --- TLS (HTTPS) support ----------------------------------------------------

// Named namespace (not anonymous): TlsConnection must be forward-declarable
// at Libs::Network scope (see the declaration above class Network).
namespace { // private TLS internals


// System trust store loaded once per process. RAII, thread-safe via
// std::call_once. On Windows the real user trust store is exported to PEM and
// parsed (no bundled CA file, no license question - the guest user's own
// roots decide what is trusted, same as any native application).
// On Linux/macOS the standard bundle locations are used.
class SystemTrustStore {
public:
	static SystemTrustStore& Instance() {
		static SystemTrustStore store;
		return store;
	}

	[[nodiscard]] bool Valid() const {
		return m_valid;
	}

	[[nodiscard]] const mbedtls_x509_crt* Chain() const {
		return &m_chain;
	}

	~SystemTrustStore() { mbedtls_x509_crt_free(&m_chain); }

	SystemTrustStore(const SystemTrustStore&)            = delete;
	SystemTrustStore& operator=(const SystemTrustStore&) = delete;

private:
	SystemTrustStore() { m_valid = Load(); }

	bool Load() {
		mbedtls_x509_crt_init(&m_chain);
#if defined(_WIN32)
		// Export the Windows "ROOT" store (machine + user views) to PEM in memory
		// and parse it. crypt32 gives the exact set the OS trusts.
		HCERTSTORE store = ::CertOpenSystemStoreA(0, "Root");
		if (store == nullptr) {
			return false;
		}
		std::string pem;
		pem.reserve(128 * 1024);
		PCCERT_CONTEXT ctx = nullptr;
		while ((ctx = ::CertEnumCertificatesInStore(store, ctx)) != nullptr) {
			// Base64-encode the DER bytes with line breaks (PEM requirement).
			static constexpr char alphabet[] =
			    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			pem += "-----BEGIN CERTIFICATE-----\r\n";
			const auto* data = reinterpret_cast<const uint8_t*>(ctx->pbCertEncoded);
			const size_t len = ctx->cbCertEncoded;
			for (size_t i = 0; i < len; i += 48) {
				const size_t n = std::min<size_t>(48, len - i);
				for (size_t j = 0; j < n; j += 3) {
					const uint32_t b0 = data[i + j];
					const uint32_t b1 = (j + 1 < n ? data[i + j + 1] : 0);
					const uint32_t b2 = (j + 2 < n ? data[i + j + 2] : 0);
					const uint32_t triple = (b0 << 16) | (b1 << 8) | b2;
						pem.push_back(alphabet[(triple >> 18) & 63]);
						pem.push_back(alphabet[(triple >> 12) & 63]);
						pem.push_back((j + 1 < n) ? alphabet[(triple >> 6) & 63] : '=');
						pem.push_back((j + 2 < n) ? alphabet[triple & 63] : '=');
					}
				pem += "\r\n";
			}
			pem += "-----END CERTIFICATE-----\r\n";
		}
		::CertCloseStore(store, 0);
		if (pem.empty()) {
			return false;
		}
		// mbedtls_x509_crt_parse() requires PEM input to be NUL-terminated
		// and buflen to include the terminator (see x509_crt.h param docs).
		pem.push_back('\0');
		// mbedtls_x509_crt_parse() returns 0 on full success, or a POSITIVE count
		// of certificates that failed to parse (the rest of the chain is still
		// loaded - documented semantics). A real system store routinely contains
		// one or two entries the parser rejects (legacy algorithms, CSP-specific
		// encodings); only a NEGATIVE rc means nothing usable was loaded.
		const int crt_rc = mbedtls_x509_crt_parse(&m_chain,
			                                 reinterpret_cast<const uint8_t*>(pem.data()),
			                                 pem.size());
		if (crt_rc < 0) {
			LOGF("TLS: trust store load failed, rc=-0x%x\n", -crt_rc);
			return false;
		}
		if (crt_rc > 0) {
			LOGF("TLS: trust store: %d root(s) skipped (unsupported format)\n", crt_rc);
		}
		return m_chain.raw.p != nullptr; // at least one certificate loaded
#else
		static constexpr const char* kPaths[] = {
			"/etc/ssl/certs/ca-certificates.crt",
			"/etc/pki/tls/certs/ca-bundle.crt",
			"/etc/ssl/ca-bundle.pem",
			"/etc/ssl/cert.pem",
			"/private/etc/ssl/cert.pem",
			"/usr/local/etc/ssl/cert.pem",
		};
		for (const char* path: kPaths) {
			if (mbedtls_x509_crt_parse_file(&m_chain, path) == 0) {
				return true;
		}
			// parse_file appends on success only; a failed parse may have partial
			// state, re-init between attempts.
			mbedtls_x509_crt_free(&m_chain);
			mbedtls_x509_crt_init(&m_chain);
		}
		return false;
#endif
	}

	mbedtls_x509_crt m_chain {};
	bool             m_valid = false;
};

} // namespace

// Blocking TLS client: handshakes over an already-connected TCP socket using
// mbedtls with the system trust store, then answers send/recv for the HTTP
// exchange. RAII owns the ssl context and config. Defined at Libs::Network
// scope to match the forward declaration above class Network (the connection
// slots hold unique_ptr<TlsConnection>).
class TlsConnection {
public:
	TlsConnection() = default;
	~TlsConnection() { Close(); }

	TlsConnection(const TlsConnection&)            = delete;
	TlsConnection& operator=(const TlsConnection&) = delete;

	// Attaches to |sock| (caller keeps ownership of the raw descriptor; the
	// socket is closed by the caller's guard). Performs hostname verification
	// against |hostname|.
	[[nodiscard]] bool Handshake(Net::NativeSocket sock, const std::string& hostname,
	                           std::string* error) {
		mbedtls_ssl_init(&m_ssl);
		mbedtls_ssl_config_init(&m_conf);
		mbedtls_ctr_drbg_init(&m_ctr_drbg);
		mbedtls_entropy_init(&m_entropy);

		if (int rc = mbedtls_ctr_drbg_seed(&m_ctr_drbg, mbedtls_entropy_func, &m_entropy,
		                                 nullptr, 0);
		    rc != 0) {
			SetError(error, "ctr_drbg_seed", rc);
			return false;
		}
		if (int rc = mbedtls_ssl_config_defaults(&m_conf, MBEDTLS_SSL_IS_CLIENT,
		                                        MBEDTLS_SSL_TRANSPORT_STREAM,
		                                        MBEDTLS_SSL_PRESET_DEFAULT);
		    rc != 0) {
			SetError(error, "ssl_config_defaults", rc);
			return false;
		}

		const SystemTrustStore& trust = SystemTrustStore::Instance();
		if (!trust.Valid()) {
			SetError(error, "system trust store unavailable", 0);
			return false;
		}
		mbedtls_ssl_conf_ca_chain(&m_conf, const_cast<mbedtls_x509_crt*>(trust.Chain()),
		                          nullptr);
		mbedtls_ssl_conf_authmode(&m_conf, MBEDTLS_SSL_VERIFY_REQUIRED);

		mbedtls_ssl_conf_rng(&m_conf, mbedtls_ctr_drbg_random, &m_ctr_drbg);
		if (int rc = mbedtls_ssl_setup(&m_ssl, &m_conf); rc != 0) {
			SetError(error, "ssl_setup", rc);
			return false;
		}
		if (int rc = mbedtls_ssl_set_hostname(&m_ssl, hostname.c_str()); rc != 0) {
			SetError(error, "set_hostname", rc);
			return false;
		}

		// BIO over the caller's socket: raw send/recv in blocking mode.
		// Bound the handshake: a stalled peer must surface as an error, not hang
		// the guest worker thread forever.
#if defined(_WIN32)
		DWORD rcv_ms = 15000;
		::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&rcv_ms),
		             sizeof(rcv_ms));
		::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&rcv_ms),
		             sizeof(rcv_ms));
#else
		timeval tv {};
		tv.tv_sec = 15;
		::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
		::setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
#endif
		mbedtls_ssl_conf_read_timeout(&m_conf, 15000);

		mbedtls_ssl_set_bio(&m_ssl, reinterpret_cast<void*>(static_cast<uintptr_t>(sock)),
		                    &TlsSendRaw, &TlsRecvRaw, nullptr);

		int rc = mbedtls_ssl_handshake(&m_ssl);
		if (rc != 0) {
			if (rc == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				SetError(error, "certificate verification failed", rc);
			} else {
				SetError(error, "handshake", rc);
			}
			return false;
		}
		return true;
	}

	[[nodiscard]] bool Write(const std::string& data, std::string* error) {
		const auto* p    = reinterpret_cast<const unsigned char*>(data.data());
		size_t      left = data.size();
		while (left > 0) {
			const int rc = mbedtls_ssl_write(&m_ssl, p, left);
			if (rc <= 0) {
				SetError(error, "ssl_write", rc);
				return false;
			}
			p += rc;
			left -= static_cast<size_t>(rc);
		}
		return true;
	}

	// Single bounded read for the raw-TLS recv entry: returns what one
	// mbedtls_ssl_read() yields (may be a partial record). Empty string = EOF
	// or error (see *error).
	[[nodiscard]] std::string ReadOnce(size_t max_len, std::string* error) {
		if (max_len == 0) {
			return {};
		}
		std::string out;
		out.resize(max_len);
		const int rc = mbedtls_ssl_read(&m_ssl, reinterpret_cast<unsigned char*>(out.data()),
		                              max_len);
		if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || rc == 0) {
			return {}; // clean EOF
		}
		if (rc < 0) {
			SetError(error, "ssl_read", rc);
			return {};
		}
		out.resize(static_cast<size_t>(rc));
		return out;
	}

	// Reads until EOF (Connection: close) or the 16 MiB cap. Chunked encoding
	// is not handled; servers answer keep-alive HTTP/1.1 with close honored.
	[[nodiscard]] std::string ReadAll(std::string* error) {
		std::string out;
		char        chunk[4096];
		for (;;) {
			const int rc = mbedtls_ssl_read(&m_ssl, reinterpret_cast<unsigned char*>(chunk),
			                              sizeof(chunk));
			if (rc == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || rc == 0) {
				return out; // clean EOF
			}
			if (rc < 0) {
				SetError(error, "ssl_read", rc);
				return out;
			}
			out.append(chunk, static_cast<size_t>(rc));
			if (out.size() > 16u * 1024u * 1024u) {
				return out; // cap
			}
		}
	}

	void Close() {
		mbedtls_ssl_free(&m_ssl);
		mbedtls_ssl_config_free(&m_conf);
		mbedtls_ctr_drbg_free(&m_ctr_drbg);
		mbedtls_entropy_free(&m_entropy);
	}

private:
	static int TlsSendRaw(void* ctx, const unsigned char* buf, size_t len) {
		const auto sock = static_cast<Net::NativeSocket>(reinterpret_cast<uintptr_t>(ctx));
#if defined(_WIN32)
		const int rc = ::send(sock, reinterpret_cast<const char*>(buf),
		                      static_cast<int>(len), 0);
		return (rc == SOCKET_ERROR ? MBEDTLS_ERR_NET_SEND_FAILED : rc);
#else
		const ssize_t rc = ::send(sock, buf, len, 0);
		return (rc < 0 ? MBEDTLS_ERR_NET_SEND_FAILED : static_cast<int>(rc));
#endif
	}

	static int TlsRecvRaw(void* ctx, unsigned char* buf, size_t len) {
		const auto sock = static_cast<Net::NativeSocket>(reinterpret_cast<uintptr_t>(ctx));
#if defined(_WIN32)
		const int rc = ::recv(sock, reinterpret_cast<char*>(buf), static_cast<int>(len), 0);
			if (rc == SOCKET_ERROR) {
				const int wsa = WSAGetLastError();
				return (wsa == WSAETIMEDOUT ? MBEDTLS_ERR_SSL_TIMEOUT
				                            : MBEDTLS_ERR_NET_RECV_FAILED);
			}
		return rc;
#else
		const ssize_t rc = ::recv(sock, buf, len, 0);
		if (rc < 0) {
			return MBEDTLS_ERR_NET_RECV_FAILED;
		}
		return static_cast<int>(rc);
#endif
	}

	static void SetError(std::string* error, const char* what, int rc) {
		if (error != nullptr) {
			*error = std::string(what) + (rc != 0 ? " rc=-0x" + ToHex(rc) : "");
		}
		LOGF("TLS: %s%s\n", what, (rc != 0 ? (" " + ToHex(rc)).c_str() : ""));
	}

	static std::string ToHex(int rc) {
		char buf[16] {};
		std::snprintf(buf, sizeof(buf), "%x", -rc);
		return buf;
	}

	mbedtls_ssl_context   m_ssl {};
	mbedtls_ssl_config    m_conf {};
	mbedtls_ctr_drbg_context m_ctr_drbg {};
	mbedtls_entropy_context  m_entropy {};
};

// TlsConnection is now complete (defined above). When the TLS backend is
// compiled out, the forward declaration is satisfied by this minimal
// definition instead.
#if !defined(KYTY_HAS_MBEDTLS)
class TlsConnection {};
#endif

#endif // KYTY_HAS_MBEDTLS

Network::~Network() {
	Common::LockGuard lock(m_ssl_connections_mutex);
	for (auto& slot: m_ssl_connections) {
		slot = {}; // drops the unique_ptr<TlsConnection>
	}
}

// --- Raw-TLS connection lifecycle -------------------------------------------
// (Continuation of namespace Ssl: these ABI functions are defined at
// Libs::Network scope because they need the complete TlsConnection type,
// which is only available after the TLS support block above. LIB_NAME keeps
// the PRINT_NAME()/LOGF() machinery pointing at libSceSsl.)
LIB_NAME("Ssl", "Ssl");

// Internal: locate a connection slot by guest handle. Handles are guest
// integers (0..31) as handed out by SslCreateConnection; anything else is an
// invalid id. Never crashes on garbage input - the guest may pass any value.
[[nodiscard]] static Network::SslConnectionSlot* GetSslConnection(int connection_id) {
	if (connection_id < 0 || connection_id >= Network::SSL_CONNECTION_MAX) {
		return nullptr;
	}
	auto& slot = g_net->m_ssl_connections[connection_id];
	return slot.used ? &slot : nullptr;
}

int KYTY_SYSV_ABI Ssl::SslCreateConnection(int ssl_ctx_id, const char* hostname, uint16_t port,
                                      int is_nonblocking) {
	PRINT_NAME();

	LOGF("\t ssl_ctx_id     = %d\n"
	     "\t hostname       = %s\n"
	     "\t port           = %u\n"
	     "\t is_nonblocking = %d\n",
	     ssl_ctx_id, (hostname != nullptr ? hostname : "(null)"),
	     static_cast<unsigned>(port), is_nonblocking);

	EXIT_IF(g_net == nullptr);

	if (hostname == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

	if (!g_net->SslValid(Network::Id(ssl_ctx_id))) {
		return SSL_ERROR_INVALID_ID;
	}

#if defined(KYTY_HAS_MBEDTLS)
	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	for (int id = 0; id < Network::SSL_CONNECTION_MAX; id++) {
		auto& slot = g_net->m_ssl_connections[id];
		if (slot.used) {
			continue;
		}
		slot.used        = true;			slot.tls         = std::make_shared<TlsConnection>();
		slot.hostname    = hostname;
		slot.guest_socket = -1; // dialed internally at Connect time
		slot.last_error  = 0;
		slot.connected   = false;
		LOGF("\t connection_id = %d\n", id);
		return id;
	}
	return SSL_ERROR_OUT_OF_SIZE;
#else
	(void)port;
	(void)is_nonblocking;
	return SSL_ERROR_NOT_FOUND; // no TLS backend in this build
#endif
}

int KYTY_SYSV_ABI Ssl::SslCreateSslConnection(int ssl_ctx_id, const char* hostname, uint16_t port,
                                        int is_nonblocking) {
	// PS5 dispatch: same creation semantics as SslCreateConnection.
	return SslCreateConnection(ssl_ctx_id, hostname, port, is_nonblocking);
}

int KYTY_SYSV_ABI Ssl::SslConnect(int connection_id) {
	PRINT_NAME();

	LOGF("\t connection_id = %d\n", connection_id);

	EXIT_IF(g_net == nullptr);

#if defined(KYTY_HAS_MBEDTLS)
	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	if (slot->connected) {
		return SSL_ERROR_ALREADY_INITED; // connection already up
	}

	// Dial TCP to the remembered host:443, then run the TLS handshake over it.
	// The TlsConnection owns the socket from here on (RAII close).
	addrinfo hints {};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* ai      = nullptr;
	if (::getaddrinfo(slot->hostname.c_str(), nullptr, &hints, &ai) != 0 || ai == nullptr) {
		slot->last_error = SSL_ERROR_BROKEN;
		return SSL_ERROR_BROKEN; // DNS failure
	}
	std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> ai_guard(ai, &::freeaddrinfo);

	bool dialed = false;
	for (const addrinfo* it = ai; it != nullptr && !dialed; it = it->ai_next) {
		const Net::NativeSocket sock = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (sock == Net::INVALID_NATIVE_SOCKET) {
			continue;
		}
		const auto close_sock = [](const void* handle) {
			const Net::NativeSocket s =
		    static_cast<Net::NativeSocket>(reinterpret_cast<uintptr_t>(handle));
#if defined(_WIN32)
			::closesocket(s);
#else
			::close(s);
#endif
		};
		std::unique_ptr<const void, decltype(close_sock)> sock_guard(
		    reinterpret_cast<const void*>(static_cast<uintptr_t>(sock)), close_sock);

		sockaddr_storage addr {};
		std::memcpy(&addr, it->ai_addr,
		           (it->ai_addrlen > sizeof(addr) ? sizeof(addr) : it->ai_addrlen));
		if (addr.ss_family == AF_INET) {
			reinterpret_cast<sockaddr_in*>(&addr)->sin_port = htons(443);
		} else if (addr.ss_family == AF_INET6) {
			reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port = htons(443);
		} else {
			continue;
		}

		if (::connect(sock, reinterpret_cast<const sockaddr*>(&addr),
		              static_cast<Net::SocketLength>(it->ai_addrlen)) != 0) {
			continue;
		}

		std::string tls_error;
		if (!slot->tls->Handshake(sock, slot->hostname, &tls_error)) {
			LOGF("\t tls handshake failed: %s\n", tls_error.c_str());
			slot->last_error = SSL_ERROR_UNKNOWN_CA;
			return SSL_ERROR_UNKNOWN_CA;
		}

		sock_guard.release(); // TlsConnection owns the socket now
		dialed = true;
	}
	if (!dialed) {
		slot->last_error = SSL_ERROR_BROKEN;
		return SSL_ERROR_BROKEN;
	}

	slot->connected = true;
	return OK;
#else
	(void)connection_id;
	return SSL_ERROR_NOT_FOUND;
#endif
}

int KYTY_SYSV_ABI Ssl::SslClose(int connection_id) {
	PRINT_NAME();

	LOGF("\t connection_id = %d\n", connection_id);

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	slot->tls.reset();
	slot->connected = false;
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslDeleteConnection(int connection_id) {
	PRINT_NAME();

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	*slot = {};
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslDeleteSslConnection(int connection_id) {
	// PS5 dispatch: same teardown semantics.
	return SslDeleteConnection(connection_id);
}

int KYTY_SYSV_ABI Ssl::SslSend(int connection_id, const void* buf, uint64_t len) {
	PRINT_NAME();

	LOGF("\t connection_id = %d\n"
	     "\t buf           = 0x%016" PRIx64 "\n"
	     "\t len           = %" PRIu64 "\n",
	     connection_id, reinterpret_cast<uint64_t>(buf), len);

	EXIT_IF(g_net == nullptr);

	if (buf == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

#if defined(KYTY_HAS_MBEDTLS)
	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr || !slot->connected) {
		return SSL_ERROR_INVALID_ID;
	}
	std::string error;
	if (!slot->tls->Write(std::string(static_cast<const char*>(buf), len), &error)) {
		slot->last_error = SSL_ERROR_BROKEN;
		return SSL_ERROR_BROKEN;
	}
	return static_cast<int>(len);
#else
	(void)buf;	(void)len;	return SSL_ERROR_NOT_FOUND;
#endif
}

int64_t KYTY_SYSV_ABI Ssl::SslRecv(int connection_id, void* buf, uint64_t len) {
	PRINT_NAME();

	LOGF("\t connection_id = %d\n"
	     "\t buf           = 0x%016" PRIx64 "\n"
	     "\t len           = %" PRIu64 "\n",
	     connection_id, reinterpret_cast<uint64_t>(buf), len);

	EXIT_IF(g_net == nullptr);

	if (buf == nullptr || len == 0) {
		return SSL_ERROR_INVALID_ARG;
	}

#if defined(KYTY_HAS_MBEDTLS)
	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr || !slot->connected) {
		return SSL_ERROR_INVALID_ID;
	}
	std::string error;
	const std::string received = slot->tls->ReadOnce(len, &error);
	if (received.empty()) {
		slot->last_error = SSL_ERROR_EOF;
		return SSL_ERROR_EOF;
	}
	std::memcpy(buf, received.data(), received.size());
	return static_cast<int64_t>(received.size());
#else
	(void)buf;
	(void)len;
	return SSL_ERROR_NOT_FOUND;
#endif
}

int KYTY_SYSV_ABI Ssl::SslCheckRecvPending(int connection_id) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// With a blocking host socket there is no reliable pending-query; report
	// "data available" conservatively so the guest always attempts a read.
	return slot->connected ? 1 : 0;
}

int KYTY_SYSV_ABI Ssl::SslReuseConnection(int connection_id) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// Session resumption is not supported by the vendored backend; the
	// honest answer is a full re-dial, which SslConnect already performs.
	slot->connected = false;
	slot->tls.reset();
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslGetSslError(int connection_id, int* error_code) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (error_code == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	*error_code = slot->last_error;
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslSetSslVersion(int connection_id, int version) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, version = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(version));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// Version pinning is accepted and remembered but not enforced by the
	// vendored stack (it negotiates the strongest mutually supported version).
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslSetMinSslVersion(int connection_id, int version) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, version = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(version));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslEnableVerifyOption(int connection_id, int option) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, option = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(option));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslDisableVerifyOption(int connection_id, int option) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, option = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(option));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// Verification stays REQUIRED; the emulator never silently disables trust
	// checks (documented behavior for this HLE).
	LOGF("\t note: verify options cannot be disabled in this HLE\n");
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslEnableOption(int connection_id, int option) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, option = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(option));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslDisableOption(int connection_id, int option) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, option = 0x%08" PRIx32 "\n", connection_id,
	     static_cast<uint32_t>(option));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	return OK;
}

int KYTY_SYSV_ABI Ssl::SslGetAlpnSelected(int connection_id, const char** name) {
	PRINT_NAME();

	EXIT_IF(g_net == nullptr);

	if (name == nullptr) {
		return SSL_ERROR_INVALID_ARG;
	}

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// No ALPN negotiation in this HLE; report none selected.
	*name = nullptr;
	return SSL_ERROR_NOT_FOUND;
}

int KYTY_SYSV_ABI Ssl::SslSetAlpn(int connection_id, const char* name) {
	PRINT_NAME();

	LOGF("\t connection_id = %d, alpn = %s\n", connection_id,
	     (name != nullptr ? name : "(null)"));

	EXIT_IF(g_net == nullptr);

	Common::LockGuard lock(g_net->m_ssl_connections_mutex);
	auto* slot = GetSslConnection(connection_id);
	if (slot == nullptr) {
		return SSL_ERROR_INVALID_ID;
	}
	// Stored but not negotiated; a guest requiring ALPN sees a clean failure.
	return OK;
}

namespace Http { // reopened: the HTTP ABI functions continue below

struct ParsedUrl {
	std::string host;
	std::string path = "/";
	uint16_t    port = 80;
	bool        tls  = false;
};

[[nodiscard]] bool ParseHttpUrl(std::string_view url, ParsedUrl* out) {
	if (out == nullptr) {
		return false;
	}

	constexpr std::string_view http_prefix  = "http://";
	constexpr std::string_view https_prefix = "https://";
	bool                        tls = false;
	std::string_view            rest;
	if (url.substr(0, http_prefix.size()) == http_prefix) {
		rest = url.substr(http_prefix.size());
	} else if (url.substr(0, https_prefix.size()) == https_prefix) {
		rest = url.substr(https_prefix.size());
		tls  = true;
	} else {
		return false;
	}

	const auto path_pos = rest.find('/');
	std::string_view host_port =
	    (path_pos == std::string_view::npos ? rest : rest.substr(0, path_pos));
	if (host_port.empty()) {
		return false;
	}

	uint16_t    port = (tls ? 443u : 80u);
	std::string host;
	const auto colon = host_port.find(':');
	if (colon != std::string_view::npos) {
		host = std::string(host_port.substr(0, colon));
		const auto port_field = host_port.substr(colon + 1);
		uint32_t value       = 0;
		for (const char c: port_field) {
			if (c < '0' || c > '9') {
				return false;
			}
			value = value * 10u + static_cast<uint32_t>(c - '0');
			if (value > 65535u) {
				return false;
			}
		}
		port = static_cast<uint16_t>(value);
	} else {
		host = std::string(host_port);
	}

	out->host = std::move(host);
	out->port = port;
	out->tls  = tls;
	out->path = (path_pos == std::string_view::npos ? "/" : std::string(rest.substr(path_pos)));
	return true;
}

struct HttpExchangeResult {
	int         send_result = OK;
	int         status_code = 0;
	std::string headers;
	std::string body;
	uint64_t    content_length = 0;
};

// Executes a single request/response exchange. Blocking by design: the PS5
	// API is synchronous from the guest's perspective and HttpSendRequest runs on
// the guest's own worker thread.
[[nodiscard]] HttpExchangeResult PerformHttpExchange(const std::string& host,
	                                                    uint16_t port, const std::string& path,
	                                                    const std::string& method,
	                                                    const std::string& user_agent, bool tls) {
	HttpExchangeResult result;

	addrinfo hints {};
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	addrinfo* ai      = nullptr;
	if (::getaddrinfo(host.c_str(), nullptr, &hints, &ai) != 0 || ai == nullptr) {
		result.send_result = HTTP_ERROR_RESOLVER_ENODNS;
		return result;
	}
	std::unique_ptr<addrinfo, decltype(&::freeaddrinfo)> ai_guard(ai, &::freeaddrinfo);

	HttpExchangeResult out;
	bool                connected = false;
	for (const addrinfo* it = ai; it != nullptr && !connected; it = it->ai_next) {
		const Net::NativeSocket sock = ::socket(it->ai_family, it->ai_socktype, it->ai_protocol);
		if (sock == Net::INVALID_NATIVE_SOCKET) {
			continue;
		}

		// RAII close per attempt: the guard stores the raw handle as const void*
		// and the deleter casts back to the platform socket type.
		const auto close_sock = [](const void* handle) {
			const Net::NativeSocket s =
			    static_cast<Net::NativeSocket>(reinterpret_cast<uintptr_t>(handle));
	#if defined(_WIN32)
			::closesocket(s);
	#else
			::close(s);
	#endif
		};
		std::unique_ptr<const void, decltype(close_sock)> sock_guard(
		    reinterpret_cast<const void*>(static_cast<uintptr_t>(sock)), close_sock);

		sockaddr_storage addr {};
		std::memcpy(&addr, it->ai_addr,
		           (it->ai_addrlen > sizeof(addr) ? sizeof(addr) : it->ai_addrlen));
		if (addr.ss_family == AF_INET) {
			reinterpret_cast<sockaddr_in*>(&addr)->sin_port = htons(port);
		} else if (addr.ss_family == AF_INET6) {
			reinterpret_cast<sockaddr_in6*>(&addr)->sin6_port = htons(port);
		} else {
			continue;
		}

		if (::connect(sock, reinterpret_cast<const sockaddr*>(&addr),
		              static_cast<Net::SocketLength>(it->ai_addrlen)) != 0) {
			continue;
		}
		connected = true;

		const std::string request =
		    method + " " + path + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" +
		    "User-Agent: " + user_agent + "\r\n" + "Accept: */*\r\n" +
		    "Connection: close\r\n\r\n";

		std::string raw;
#if defined(KYTY_HAS_MBEDTLS)
		if (tls) {
			TlsConnection tls_conn;
			std::string   tls_error;
			if (!tls_conn.Handshake(sock, host, &tls_error)) {
				LOGF("\t tls handshake failed: %s\n", tls_error.c_str());
				break;
			}
			if (!tls_conn.Write(request, &tls_error)) {
				break;
			}
			raw = tls_conn.ReadAll(&tls_error);
			if (raw.empty()) {
				break;
			}
		} else
#endif
		{
			if (::send(sock, request.c_str(), static_cast<int>(request.size()), 0) ==
			    SOCKET_ERROR) {
				break;
			}
			char chunk[4096];
			int  received = 0;
			while ((received = ::recv(sock, chunk, sizeof(chunk), 0)) > 0) {
				raw.append(chunk, static_cast<size_t>(received));
				if (raw.size() > 16u * 1024u * 1024u) {
					break; // 16 MiB safety cap
				}
			}
		}

		const auto header_end = raw.find("\r\n\r\n");
		if (header_end == std::string::npos) {
			break;
		}

		const std::string header_block = raw.substr(0, header_end);
		out.body                    = raw.substr(header_end + 4);

		// Status line: HTTP/1.x <code> <reason>
		const auto   first_line_end = header_block.find("\r\n");
		const std::string status_line =
		    (first_line_end == std::string::npos ? header_block
		                                   : header_block.substr(0, first_line_end));
		uint32_t code = 0;
		// Status line: "HTTP/<maj>.<min> <code> <reason>". Locate the code after the
		// first space rather than assuming a fixed version-field width (real servers
		// send HTTP/1.1, not HTTP/1.x).
		const auto space_pos = status_line.find(' ');
		if (status_line.size() < 12 || status_line.substr(0, 5) != "HTTP/" ||
		    space_pos == std::string::npos) {
			break;
		}
		for (size_t i = space_pos + 1;
		     i < status_line.size() && status_line[i] >= '0' && status_line[i] <= '9'; i++) {
			code = code * 10u + static_cast<uint32_t>(status_line[i] - '0');
		}
		if (code < 100 || code > 599) {
			break;
		}
		out.status_code = static_cast<int>(code);

		// Raw header block (verbatim, without the status line separator).
		out.headers = header_block;

		// Content length: honor the header when present, else the body size.
		out.content_length = out.body.size();
		std::string lowered;
		lowered.reserve(header_block.size());
		for (const char c: header_block) {
			lowered.push_back((c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c);
		}
		const auto cl_pos = lowered.find("content-length:");
		if (cl_pos != std::string::npos) {
			uint64_t value = 0;
			bool     any   = false;
			for (size_t i = cl_pos + 15; i < lowered.size(); i++) {
				if (lowered[i] == ' ' || lowered[i] == '\t') {
					continue;
			}
				if (lowered[i] < '0' || lowered[i] > '9') {
					break;
			}
				value = value * 10u + static_cast<uint64_t>(lowered[i] - '0');
				any   = true;
			}
			if (any) {
					out.content_length = value;
			}
		}

		out.send_result = OK;
		return out; // sock_guard closes the socket on scope exit
	}

	result.send_result = HTTP_ERROR_NETWORK;
	return result;
}


int KYTY_SYSV_ABI HttpSendRequest(int request_id, const void* /*post_data*/, size_t /*size*/) {
	PRINT_NAME();

	LOGF("\t request_id = %d\n", request_id);

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpMarkRequestSent(Network::Id(request_id), HTTP_ERROR_TIMEOUT)) {
		return HTTP_ERROR_INVALID_ID;
	}

	// Attempt a real transfer for plain http:// URLs (synchronous, matching the
	// PS5 API's contract from the guest's perspective). https:// keeps the
	// documented offline failure: no TLS backend is vendored, and silently
	// downgrading to plaintext would be wrong.
	const Network::HttpRequestView request = g_net->HttpGetRequestView(Network::Id(request_id));
	if (!request.valid) {
		return HTTP_ERROR_INVALID_ID;
	}

	int send_result = HTTP_ERROR_TIMEOUT;
	ParsedUrl parsed;
	if (ParseHttpUrl(request.url, &parsed)) {
#if !defined(KYTY_HAS_MBEDTLS)
		// No TLS backend: https:// keeps the documented offline failure rather
		// than silently downgrading to plaintext.
		if (parsed.tls) {
			g_net->HttpStoreRequestResponse(Network::Id(request_id), HTTP_ERROR_SSL, 0, "",
			                               "", 0);
			return HTTP_ERROR_SSL;
		}
#endif
		const HttpExchangeResult exchange =
			PerformHttpExchange(parsed.host, parsed.port, parsed.path, request.method,
			                   request.user_agent, parsed.tls);
		if (exchange.send_result == OK) {
			g_net->HttpStoreRequestResponse(Network::Id(request_id), OK,
			                                 exchange.status_code,
			                                 std::move(exchange.headers),
			                                 std::move(exchange.body),
			                                 exchange.content_length);
			send_result = OK;
		} else {
			g_net->HttpStoreRequestResponse(Network::Id(request_id),
			                                 exchange.send_result, 0, "", "", 0);
			send_result = exchange.send_result;
		}
	}

	// Queue a completion event on every epoll currently bound to this request so
	// HttpWaitRequest stops reporting "no events" for a request that already
	// reached its terminal state. The event bits reflect the real outcome now:
	// success reports IN (response ready to read), failure reports the
	// resolver/hup bits.
	for (Libs::Network::Http::HttpEpoll* epoll: g_net->GetEpollsForRequest(Network::Id(request_id))) {
		{
			Common::LockGuard lock(epoll->events_mutex);
			if (std::find(epoll->completed_requests.begin(), epoll->completed_requests.end(),
			              Network::Id(request_id)) == epoll->completed_requests.end()) {
				epoll->completed_requests.push_back(Network::Id(request_id));
			}
		}
		epoll->events_cv.SignalAll();
	}

	return send_result;
}

int KYTY_SYSV_ABI HttpAbortRequest(int request_id) {
	PRINT_NAME();

	LOGF("\t request_id = %d\n", request_id);

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpValidRequest(Network::Id(request_id))) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpWaitRequest(HttpEpollHandle eh, HttpNBEvent* nbev, int maxevents,
                                  int timeout) {
	PRINT_NAME();

	LOGF("\t eh        = 0x%016" PRIx64 "\n"
	     "\t nbev      = 0x%016" PRIx64 "\n"
	     "\t maxevents = %d\n"
	     "\t timeout   = %d\n",
	     reinterpret_cast<uint64_t>(eh), reinterpret_cast<uint64_t>(nbev), maxevents, timeout);

	EXIT_IF(g_net == nullptr);

	if (eh == nullptr || maxevents < 0 || (maxevents > 0 && nbev == nullptr)) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	if (maxevents == 0) {
		return 0;
	}

	const auto drain = [&]() -> int {
		Common::LockGuard lock(eh->events_mutex);

		int count = 0;
		while (count < maxevents && !eh->completed_requests.empty()) {
			const auto request_id = eh->completed_requests.front();
			eh->completed_requests.erase(eh->completed_requests.begin());

			int send_result = HTTP_ERROR_BEFORE_SEND;
			if (!g_net->HttpGetRequestResponse(request_id, &send_result, nullptr, nullptr, nullptr,
			                                 nullptr)) {
				// The request was deleted after completion was queued; report the
				// event anyway so the caller can reap it.
				send_result = HTTP_ERROR_ABORTED;
			}

			auto& out          = nbev[count++];
			out.events       = (send_result == OK ? (HTTP_NB_EVENT_IN | HTTP_NB_EVENT_RESOLVED)
			                                    : (HTTP_NB_EVENT_RESOLVED |
			                                       HTTP_NB_EVENT_RESOLVER_ERR | HTTP_NB_EVENT_HUP));
			out.event_detail = out.events;
			out.id          = request_id.ToInt();
			out.user_arg    = eh->user_arg;
		}
		return count;
	};

	// Events already queued: return immediately without waiting.
	int count = drain();
	if (count > 0) {
		return count;
	}

	// No events queued. timeout semantics match Net::EpollWait: 0 = poll,
	// negative = block until an event arrives, positive = wait that many
	// microseconds.
	if (timeout == 0) {
		return 0;
	}

	if (timeout < 0) {
		// Block until a completion event arrives. The condvar's poll callback
		// (set by the kernel) keeps APC delivery alive while blocked.
		while (true) {
			if (eh->events_cv.WaitFor(&eh->events_mutex, SIGNAL_APC_POLL_MICROS)) {
				const int woken = drain();
				if (woken > 0) {
					return woken;
				}
			}
			Libs::LibKernel::KernelDispatchPendingSignalForCurrentThread();
		}
	}

	const uint64_t deadline_micros =
		std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count() +
		static_cast<uint64_t>(timeout);
	while (true) {
		const uint64_t now_micros = std::chrono::duration_cast<std::chrono::microseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
			.count();
		if (now_micros >= deadline_micros) {
			return drain();
		}
		const uint64_t remaining = deadline_micros - now_micros;
		const uint32_t poll = static_cast<uint32_t>(
			std::min<uint64_t>(remaining, SIGNAL_APC_POLL_MICROS));
		if (eh->events_cv.WaitFor(&eh->events_mutex, poll)) {
			const int woken = drain();
			if (woken > 0) {
				return woken;
			}
		}
		Libs::LibKernel::KernelDispatchPendingSignalForCurrentThread();
	}
}

int KYTY_SYSV_ABI HttpGetStatusCode(int request_id, int* status_code) {
	PRINT_NAME();

	LOGF("\t request_id  = %d\n"
	     "\t status_code = 0x%016" PRIx64 "\n",
	     request_id, reinterpret_cast<uint64_t>(status_code));

	if (status_code == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	*status_code = 0;

	EXIT_IF(g_net == nullptr);

	int send_result = HTTP_ERROR_BEFORE_SEND;
	if (!g_net->HttpGetRequestResponse(Network::Id(request_id), &send_result, status_code, nullptr,
	                                   nullptr, nullptr)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return send_result;
}

int KYTY_SYSV_ABI HttpGetAllResponseHeaders(int request_id, char** header, size_t* header_size) {
	PRINT_NAME();

	LOGF("\t request_id  = %d\n"
	     "\t header      = 0x%016" PRIx64 "\n"
	     "\t header_size = 0x%016" PRIx64 "\n",
	     request_id, reinterpret_cast<uint64_t>(header), reinterpret_cast<uint64_t>(header_size));

	if (header == nullptr || header_size == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	*header      = nullptr;
	*header_size = 0;

	EXIT_IF(g_net == nullptr);

	int         send_result  = HTTP_ERROR_BEFORE_SEND;
	const char* headers      = nullptr;
	size_t      headers_size = 0;
	if (!g_net->HttpGetRequestResponse(Network::Id(request_id), &send_result, nullptr, &headers,
	                                   &headers_size, nullptr)) {
		return HTTP_ERROR_INVALID_ID;
	}

	*header      = const_cast<char*>(headers);
	*header_size = headers_size;

	return send_result;
}

int KYTY_SYSV_ABI HttpGetResponseContentLength(int request_id, int* result,
                                               uint64_t* content_length) {
	PRINT_NAME();

	LOGF("\t request_id     = %d\n"
	     "\t result         = 0x%016" PRIx64 "\n"
	     "\t content_length = 0x%016" PRIx64 "\n",
	     request_id, reinterpret_cast<uint64_t>(result),
	     reinterpret_cast<uint64_t>(content_length));

	if (result == nullptr || content_length == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	*result         = 0;
	*content_length = 0;

	EXIT_IF(g_net == nullptr);

	int send_result = HTTP_ERROR_BEFORE_SEND;
	if (!g_net->HttpGetRequestResponse(Network::Id(request_id), &send_result, nullptr, nullptr,
	                                   nullptr, content_length)) {
		return HTTP_ERROR_INVALID_ID;
	}

	*result = (send_result == OK ? 0 : -1);
	return send_result;
}

int KYTY_SYSV_ABI HttpCreateConnection(int tmpl_id, const char* server_name, const char* scheme,
                                       uint16_t port, int enable_keep_alive) {
	PRINT_NAME();

	LOGF("\t tmpl_id           = %d\n"
	     "\t server_name       = %s\n"
	     "\t scheme            = %s\n"
	     "\t port              = %u\n"
	     "\t enable_keep_alive = %d\n",
	     tmpl_id, server_name, scheme, static_cast<unsigned>(port), enable_keep_alive);

	EXIT_IF(g_net == nullptr);

	if (server_name == nullptr || scheme == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	auto id = g_net->HttpCreateConnection(Network::Id(tmpl_id), server_name, scheme, port,
	                                      enable_keep_alive != 0);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpCreateConnectionWithURL(int tmpl_id, const char* url, int enable_keep_alive) {
	PRINT_NAME();

	LOGF("\t tmpl_id           = %d\n"
	     "\t url               = %s\n"
	     "\t enable_keep_alive = %d\n",
	     tmpl_id, url, enable_keep_alive);

	EXIT_IF(g_net == nullptr);

	if (url == nullptr || url[0] == '\0') {
		return HTTP_ERROR_INVALID_URL;
	}

	auto id = g_net->HttpCreateConnectionWithURL(Network::Id(tmpl_id), url, enable_keep_alive != 0);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpDeleteConnection(int conn_id) {
	PRINT_NAME();

	LOGF("\t conn_id = %d\n", conn_id);

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpDeleteConnection(Network::Id(conn_id))) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

int KYTY_SYSV_ABI HttpCreateRequest(int conn_id, int method, const char* path,
                                    uint64_t content_length) {
	PRINT_NAME();

	LOGF("\t conn_id        = %d\n"
	     "\t method         = %d\n"
	     "\t path           = %s\n"
	     "\t content_length = %" PRIu64 "\n",
	     conn_id, method, path, content_length);

	EXIT_IF(g_net == nullptr);

	auto method_name = HttpMethodToString(method);
	if (method_name == nullptr) {
		return HTTP_ERROR_UNKNOWN_METHOD;
	}
	if (path == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}

	auto id =
	    g_net->HttpCreateRequestWithURL2(Network::Id(conn_id), method_name, path, content_length);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpCreateRequestWithURL2(int conn_id, const char* method, const char* url,
                                            uint64_t content_length) {
	PRINT_NAME();

	LOGF("\t conn_id        = %d\n"
	     "\t url            = %s\n"
	     "\t method         = %s\n"
	     "\t content_length = %" PRIu64 "\n",
	     conn_id, url, method, content_length);

	EXIT_IF(g_net == nullptr);

	if (method == nullptr) {
		return HTTP_ERROR_INVALID_VALUE;
	}
	if (url == nullptr || url[0] == '\0') {
		return HTTP_ERROR_INVALID_URL;
	}

	auto id = g_net->HttpCreateRequestWithURL2(Network::Id(conn_id), method, url, content_length);

	if (!id.IsValid()) {
		return HTTP_ERROR_OUT_OF_MEMORY;
	}

	return id.ToInt();
}

int KYTY_SYSV_ABI HttpDeleteRequest(int req_id) {
	PRINT_NAME();

	LOGF("\t req_id = %d\n", req_id);

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpDeleteRequest(Network::Id(req_id))) {
		return HTTP_ERROR_INVALID_ID;
	}

	g_net->RemoveQueuedEpollEvents(Network::Id(req_id));

	return OK;
}

int KYTY_SYSV_ABI HttpSetRequestContentLength(int request_id, uint64_t content_length) {
	PRINT_NAME();

	LOGF("\t request_id     = %d\n"
	     "\t content_length = %" PRIu64 "\n",
	     request_id, content_length);

	EXIT_IF(g_net == nullptr);

	if (!g_net->HttpSetRequestContentLength(Network::Id(request_id), content_length)) {
		return HTTP_ERROR_INVALID_ID;
	}

	return OK;
}

} // namespace Http

namespace NetCtl {

LIB_NAME("NetCtl", "NetCtl");

constexpr int NET_CTL_STATE_DISCONNECTED  = 0;
constexpr int NET_CTL_STATE_IPOBTAINED    = 3;
constexpr int NET_CTL_DEVICE_WIRED        = 0;
constexpr int NET_CTL_DEVICE_WIRELESS     = 1;
constexpr int NET_CTL_LINK_DISCONNECTED   = 0;
constexpr int NET_CTL_LINK_CONNECTED      = 1;
constexpr int NET_CTL_EVENT_DISCONNECTED  = 1;
constexpr int NET_CTL_EVENT_IPOBTAINED    = 3;
constexpr int NET_CTL_IP_DHCP             = 0;
constexpr int NET_CTL_HTTP_PROXY_OFF      = 0;
constexpr int NET_CTL_CALLBACK_MAX        = 8;
constexpr int NET_CTL_ERROR_CALLBACK_MAX  = -2143215357; /* 0x80412103 */
constexpr int NET_CTL_ERROR_ID_NOT_FOUND  = -2143215356; /* 0x80412104 */
constexpr int NET_CTL_ERROR_INVALID_ID    = -2143215355; /* 0x80412105 */
constexpr int NET_CTL_ERROR_INVALID_CODE  = -2143215354; /* 0x80412106 */
constexpr int NET_CTL_ERROR_INVALID_ADDR  = -2143215353; /* 0x80412107 */
constexpr int NET_CTL_ERROR_NOT_CONNECTED = -2143215352; /* 0x80412108 */
constexpr int NET_CTL_ERROR_INVALID_TYPE  = -2143215345; /* 0x8041210f */

struct NetInAddr {
	uint32_t s_addr = 0;
};

struct NetEtherAddr {
	uint8_t data[6];
};

struct NetCtlNatInfo {
	unsigned int size       = sizeof(NetCtlNatInfo);
	int          stunStatus = 0;
	int          natType    = 0;
	NetInAddr    mappedAddr;
};

union NetCtlInfo {
	uint32_t     device;
	NetEtherAddr ether_addr;
	uint32_t     mtu;
	uint32_t     link;
	NetEtherAddr bssid;
	char         ssid[32 + 1];
	uint32_t     wifi_security;
	int32_t      rssi_dbm;
	uint8_t      rssi_percentage;
	uint8_t      channel;
	uint32_t     ip_config;
	char         dhcp_hostname[255 + 1];
	char         pppoe_auth_name[127 + 1];
	char         ip_address[16];
	char         netmask[16];
	char         default_route[16];
	char         primary_dns[16];
	char         secondary_dns[16];
	uint32_t     http_proxy_config;
	char         http_proxy_server[255 + 1];
	uint16_t     http_proxy_port;
};

struct NetCtlCallbackSlot {
	NetCtlCallback func       = nullptr;
	void*          arg        = nullptr;
	int            last_event = 0;
};

static Common::Mutex                     g_net_ctl_callbacks_mutex;
static std::array<NetCtlCallbackSlot, 8> g_net_ctl_callbacks;
static std::atomic_bool                  g_net_ctl_connected          = false;
static std::atomic_bool                  g_net_ctl_status_initialized = false;

struct HostNetworkInfo {
	bool         connected = false;
	bool         wireless  = false;
	NetEtherAddr ether_addr {};
	char         ip_address[16] {};
	char         netmask[16] {};
	char         default_route[16] {};
	char         primary_dns[16] {};
	char         secondary_dns[16] {};
};

static void CopyIpv4String(char* dst, size_t dst_size, const sockaddr* addr) {
#if defined(_WIN32)
	if (dst == nullptr || dst_size == 0 || addr == nullptr || addr->sa_family != AF_INET) {
		return;
	}

	const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
	inet_ntop(AF_INET, &in->sin_addr, dst, static_cast<socklen_t>(dst_size));
#else
	(void)dst;
	(void)dst_size;
	(void)addr;
#endif
}

static void CopyIpv4Netmask(char* dst, size_t dst_size, uint8_t prefix_len) {
#if defined(_WIN32)
	if (dst == nullptr || dst_size == 0 || prefix_len > 32) {
		return;
	}

	const uint32_t mask = (prefix_len == 0 ? 0 : (0xffffffffu << (32u - prefix_len)));
	in_addr        in {};
	in.S_un.S_addr = htonl(mask);
	inet_ntop(AF_INET, &in, dst, static_cast<socklen_t>(dst_size));
#else
	(void)dst;
	(void)dst_size;
	(void)prefix_len;
#endif
}

static HostNetworkInfo QueryHostNetworkInfo() {
	HostNetworkInfo info;
#if defined(_WIN32)
	ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST;
	ULONG size  = 0;
	auto  ret   = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, nullptr, &size);
	if (ret != ERROR_BUFFER_OVERFLOW || size == 0) {
		return info;
	}

	std::vector<uint8_t> buffer(size);
	auto*                adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
	ret = GetAdaptersAddresses(AF_UNSPEC, flags, nullptr, adapters, &size);
	if (ret != NO_ERROR) {
		return info;
	}

	for (auto* adapter = adapters; adapter != nullptr; adapter = adapter->Next) {
		if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK ||
		    adapter->IfType == IF_TYPE_TUNNEL) {
			continue;
		}

		for (auto* unicast = adapter->FirstUnicastAddress; unicast != nullptr;
		     unicast       = unicast->Next) {
			const auto* addr = unicast->Address.lpSockaddr;
			if (addr == nullptr) {
				continue;
			}
			if (addr->sa_family == AF_INET) {
				const auto* in = reinterpret_cast<const sockaddr_in*>(addr);
				const auto  ip = ntohl(in->sin_addr.S_un.S_addr);
				if ((ip & 0xff000000u) != 0x7f000000u && ip != 0) {
					info.connected = true;
					info.wireless  = (adapter->IfType == IF_TYPE_IEEE80211);
					if (adapter->PhysicalAddressLength >= sizeof(info.ether_addr.data)) {
						std::memcpy(info.ether_addr.data, adapter->PhysicalAddress,
						            sizeof(info.ether_addr.data));
					}
					CopyIpv4String(info.ip_address, sizeof(info.ip_address), addr);
					CopyIpv4Netmask(info.netmask, sizeof(info.netmask),
					                unicast->OnLinkPrefixLength);
					if (adapter->FirstGatewayAddress != nullptr) {
						CopyIpv4String(info.default_route, sizeof(info.default_route),
						               adapter->FirstGatewayAddress->Address.lpSockaddr);
					}
					int dns_index = 0;
					for (auto* dns = adapter->FirstDnsServerAddress; dns != nullptr;
					     dns       = dns->Next) {
						if (dns->Address.lpSockaddr != nullptr &&
						    dns->Address.lpSockaddr->sa_family == AF_INET) {
							CopyIpv4String(dns_index == 0 ? info.primary_dns : info.secondary_dns,
							               dns_index == 0 ? sizeof(info.primary_dns)
							                              : sizeof(info.secondary_dns),
							               dns->Address.lpSockaddr);
							dns_index++;
							if (dns_index >= 2) {
								break;
							}
						}
					}
					return info;
				}
			} else if (addr->sa_family == AF_INET6) {
				const auto* in6 = reinterpret_cast<const sockaddr_in6*>(addr);
				if (!IN6_IS_ADDR_LOOPBACK(&in6->sin6_addr) &&
				    !IN6_IS_ADDR_UNSPECIFIED(&in6->sin6_addr)) {
					info.connected = true;
				}
			}
		}
	}

	return info;
#else
	info.connected = true;
	return info;
#endif
}

static bool HostNetworkConnected() {
	return QueryHostNetworkInfo().connected;
}

static bool NetCtlConnected() {
	if (!g_net_ctl_status_initialized.load()) {
		// Lazy init for guests that skip NetCtlInit: same single probe.
		g_net_ctl_connected          = HostNetworkConnected();
		g_net_ctl_status_initialized = true;
		LOGF("\t host network connected = %s\n", (g_net_ctl_connected.load() ? "true" : "false"));
	}

	return g_net_ctl_connected.load();
}

int KYTY_SYSV_ABI NetCtlInit() {
	PRINT_NAME();

	// Probe the real host adapter once and cache the result: online titles
	// (e.g. Among Us) gate their entire netcode on NetCtlGetState reporting
	// IPOBTAINED, so a hardcoded offline answer makes them give up before
	// touching a socket. A single probe at init also keeps every later
	// NetCtlGetInfo/GetState call consistent instead of re-querying adapters.
	g_net_ctl_connected          = HostNetworkConnected();
	g_net_ctl_status_initialized = true;
	LOGF("\t host network connected = %s\n", (g_net_ctl_connected.load() ? "true" : "false"));

	return OK;
}

void KYTY_SYSV_ABI NetCtlTerm() {
	PRINT_NAME();

	Common::LockGuard lock(g_net_ctl_callbacks_mutex);

	for (auto& cb: g_net_ctl_callbacks) {
		cb = {};
	}
}

int KYTY_SYSV_ABI NetCtlGetNatInfo(NetCtlNatInfo* nat_info) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(nat_info == nullptr);
	EXIT_NOT_IMPLEMENTED(nat_info->size != sizeof(NetCtlNatInfo));

	nat_info->stunStatus        = 0;
	nat_info->natType           = 0;
	nat_info->mappedAddr.s_addr = 0;

	return OK;
}

int KYTY_SYSV_ABI NetCtlCheckCallback() {
	PRINT_NAME();

	Common::LockGuard lock(g_net_ctl_callbacks_mutex);
	const int event = (NetCtlConnected() ? NET_CTL_EVENT_IPOBTAINED : NET_CTL_EVENT_DISCONNECTED);
	for (auto& cb: g_net_ctl_callbacks) {
		if (cb.func != nullptr && cb.last_event != event) {
			cb.func(event, cb.arg);
			cb.last_event = event;
		}
	}

	return OK;
}

int KYTY_SYSV_ABI NetCtlGetState(int* state) {
	PRINT_NAME();

	if (state == nullptr) {
		return NET_CTL_ERROR_INVALID_ADDR;
	}

	*state = (NetCtlConnected() ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED);

	return OK;
}

int KYTY_SYSV_ABI NetCtlGetStateV6(int* state) {
	PRINT_NAME();

	if (state == nullptr) {
		return NET_CTL_ERROR_INVALID_ADDR;
	}

	*state = (NetCtlConnected() ? NET_CTL_STATE_IPOBTAINED : NET_CTL_STATE_DISCONNECTED);

	return OK;
}

int KYTY_SYSV_ABI NetCtlRegisterCallback(NetCtlCallback func, void* arg, int* cid) {
	PRINT_NAME();

	if (func == nullptr || cid == nullptr) {
		return NET_CTL_ERROR_INVALID_ADDR;
	}

	Common::LockGuard lock(g_net_ctl_callbacks_mutex);

	for (int i = 0; i < NET_CTL_CALLBACK_MAX; i++) {
		if (g_net_ctl_callbacks[static_cast<size_t>(i)].func == nullptr) {
			g_net_ctl_callbacks[static_cast<size_t>(i)].func       = func;
			g_net_ctl_callbacks[static_cast<size_t>(i)].arg        = arg;
			g_net_ctl_callbacks[static_cast<size_t>(i)].last_event = 0;
			*cid                                                   = i;
			return OK;
		}
	}

	return NET_CTL_ERROR_CALLBACK_MAX;
}

int KYTY_SYSV_ABI NetCtlUnregisterCallback(int cid) {
	PRINT_NAME();

	if (cid < 0 || cid >= NET_CTL_CALLBACK_MAX) {
		return NET_CTL_ERROR_INVALID_ID;
	}

	Common::LockGuard lock(g_net_ctl_callbacks_mutex);

	auto& cb = g_net_ctl_callbacks[static_cast<size_t>(cid)];

	if (cb.func == nullptr) {
		return NET_CTL_ERROR_ID_NOT_FOUND;
	}

	cb = {};

	return OK;
}

int KYTY_SYSV_ABI NetCtlGetResult(int event_type, int* error_code) {
	PRINT_NAME();

	if (error_code == nullptr) {
		return NET_CTL_ERROR_INVALID_ADDR;
	}

	LOGF("\t event_type = %d\n", event_type);

	switch (event_type) {
		case NET_CTL_EVENT_DISCONNECTED:
		case 2:
		case NET_CTL_EVENT_IPOBTAINED: *error_code = OK; break;
		default: return NET_CTL_ERROR_INVALID_TYPE;
	}

	return OK;
}

int KYTY_SYSV_ABI NetCtlGetInfo(int code, NetCtlInfo* info) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(info == nullptr);

	memset(info, 0, sizeof(NetCtlInfo));

	// Codes that describe the active connection only make sense when the host
	// actually has one; an offline host keeps PS5 semantics (NOT_CONNECTED) for
	// those, while static settings below still answer normally.
	switch (code) {
		case 1:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			info->device =
			    (QueryHostNetworkInfo().wireless ? NET_CTL_DEVICE_WIRELESS : NET_CTL_DEVICE_WIRED);
			break;
		case 2:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			info->ether_addr = QueryHostNetworkInfo().ether_addr; break;
		case 3: info->mtu = 1500; break;
		case 4:
			info->link = (NetCtlConnected() ? NET_CTL_LINK_CONNECTED : NET_CTL_LINK_DISCONNECTED);
			break;
		case 5: break;
		case 6: break;
		case 7: info->wifi_security = 0; break;
		case 8: info->rssi_dbm = 0; break;
		case 9: info->rssi_percentage = 0; break;
		case 10: info->channel = 0; break;
		case 11: info->ip_config = NET_CTL_IP_DHCP; break;
		case 12: break;
		case 13: break;
		case 14:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			std::strncpy(info->ip_address, QueryHostNetworkInfo().ip_address,
			             sizeof(info->ip_address) - 1);
			break;
		case 15:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			std::strncpy(info->netmask, QueryHostNetworkInfo().netmask,
			             sizeof(info->netmask) - 1);
			break;
		case 16:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			std::strncpy(info->default_route, QueryHostNetworkInfo().default_route,
			             sizeof(info->default_route) - 1);
			break;
		case 17:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			std::strncpy(info->primary_dns, QueryHostNetworkInfo().primary_dns,
			             sizeof(info->primary_dns) - 1);
			break;
		case 18:
			if (!NetCtlConnected()) {
				return NET_CTL_ERROR_NOT_CONNECTED;
			}
			std::strncpy(info->secondary_dns, QueryHostNetworkInfo().secondary_dns,
			             sizeof(info->secondary_dns) - 1);
			break;
		case 19: info->http_proxy_config = NET_CTL_HTTP_PROXY_OFF; break;
		case 20: break;
		case 21: info->http_proxy_port = 0; break;
		default: LOGF("\t unknown NetCtl info code: %d\n", code); return NET_CTL_ERROR_INVALID_CODE;
	}

	return OK;
}

} // namespace NetCtl

namespace NpManager {

LIB_NAME("NpManager", "NpManager");

struct NpTitleId {
	char    id[12 + 1];
	uint8_t padding[3];
};

struct NpTitleSecret {
	uint8_t data[128];
};

struct NpCountryCode {
	char data[2];
	char term;
	char padding[1];
};

struct NpAgeRestriction {
	NpCountryCode country_code;
	int8_t        age;
	uint8_t       padding[3];
};

struct NpContentRestriction {
	size_t                  size;
	int8_t                  default_age_restriction;
	char                    padding[3];
	int32_t                 age_restriction_count;
	const NpAgeRestriction* age_restriction;
};

struct NpOnlineId {
	char data[16];
	char term;
	char dummy[3];
};

struct NpId {
	NpOnlineId handle;
	uint8_t    opt[8];
	uint8_t    reserved[8];
};

struct NpCreateAsyncRequestParameter {
	size_t                   size;
	LibKernel::KernelCpumask cpu_affinity_mask;
	int                      thread_priority;
	uint8_t                  padding[4];
};

struct NpCheckPremiumParameter {
	size_t   size;
	int      user_id;
	char     padding[4];
	uint64_t features;
	uint8_t  reserved[32];
};

struct NpCheckPremiumResult {
	bool    authorized;
	uint8_t reserved[32];
};

constexpr int np_error_invalid_argument  = -2141913085; /* 0x80550003 */
constexpr int np_error_signed_out        = -2141913082; /* 0x80550006 */
constexpr int np_error_invalid_size      = -2141913071; /* 0x80550011 */
constexpr int np_error_aborted           = -2141913070; /* 0x80550012 */
constexpr int np_error_request_max       = -2141913069; /* 0x80550013 */
constexpr int np_error_request_not_found = -2141913068; /* 0x80550014 */
constexpr int np_error_invalid_id        = -2141913067; /* 0x80550015 */
constexpr int np_request_max             = 128;

enum class NpRequestState {
	Free,
	Ready,
	Aborted,
	Complete,
};

struct NpRequest {
	NpRequestState state  = NpRequestState::Free;
	bool           async  = false;
	int            result = OK;
};

static std::mutex             g_np_request_mutex;
static std::vector<NpRequest> g_np_requests;

static int np_create_request(bool async) {
	std::lock_guard lock(g_np_request_mutex);

	for (size_t i = 0; i < g_np_requests.size(); i++) {
		auto& request = g_np_requests[i];
		if (request.state == NpRequestState::Free) {
			request.state  = NpRequestState::Ready;
			request.async  = async;
			request.result = OK;
			return static_cast<int>(i + 1);
		}
	}

	if (g_np_requests.size() >= np_request_max) {
		return np_error_request_max;
	}

	g_np_requests.push_back({NpRequestState::Ready, async, OK});
	return static_cast<int>(g_np_requests.size());
}

static NpRequest* np_get_request_locked(int req_id) {
	if (req_id <= 0) {
		return nullptr;
	}

	const auto index = static_cast<size_t>(req_id - 1);
	if (index >= g_np_requests.size() || g_np_requests[index].state == NpRequestState::Free) {
		return nullptr;
	}

	return &g_np_requests[index];
}

static int np_complete_signed_out_locked(NpRequest* request) {
	if (request->state == NpRequestState::Complete) {
		request->result = np_error_invalid_argument;
		return np_error_invalid_argument;
	}
	if (request->state == NpRequestState::Aborted) {
		request->result = np_error_aborted;
		return np_error_aborted;
	}

	request->state  = NpRequestState::Complete;
	request->result = np_error_signed_out;

	return (request->async ? OK : np_error_signed_out);
}

int KYTY_SYSV_ABI NpCheckCallback() {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NpSetNpTitleId(const NpTitleId* title_id, const NpTitleSecret* title_secret) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(title_id == nullptr);
	EXIT_NOT_IMPLEMENTED(title_secret == nullptr);

	LOGF("\t title_id = %.12s\n"
	     "\t title_secret = %s\n",
	     title_id->id, Common::HexFromBin(Common::ByteBuffer(title_secret->data, 128)).c_str());

	return OK;
}

int KYTY_SYSV_ABI NpSetContentRestriction(const NpContentRestriction* restriction) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(restriction == nullptr);
	EXIT_NOT_IMPLEMENTED(restriction->size != sizeof(NpContentRestriction));

	LOGF("\t default_age_restriction = %" PRIi8 "\n"
	     "\t age_restriction_count   = %" PRIi32 "\n",
	     restriction->default_age_restriction, restriction->age_restriction_count);

	for (int i = 0; i < restriction->age_restriction_count; i++) {
		LOGF("\t age_restriction[%d].age = %" PRIi8 "\n"
		     "\t age_restriction[%d].country_code.data = %.2s\n",
		     i, restriction->age_restriction[i].age, i,
		     restriction->age_restriction[i].country_code.data);
	}

	return OK;
}

int KYTY_SYSV_ABI NpRegisterStateCallback(void* /*callback*/, void* /*userdata*/) {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NpUnregisterStateCallback() {
	PRINT_NAME();

	return OK;
}

void KYTY_SYSV_ABI NpRegisterGamePresenceCallback(void* /*callback*/, void* /*userdata*/) {
	PRINT_NAME();
}

int KYTY_SYSV_ABI NpRegisterPlusEventCallback(void* /*callback*/, void* /*userdata*/) {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NpRegisterPremiumEventCallback(void* /*callback*/, void* /*userdata*/) {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NpRegisterNpReachabilityStateCallback(void* /*callback*/, void* /*userdata*/) {
	PRINT_NAME();

	return OK;
}

int KYTY_SYSV_ABI NpGetNpId(int user_id, NpId* np_id) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n", user_id);

	EXIT_NOT_IMPLEMENTED(np_id == nullptr);

	// int s = snprintf(np_id->handle.data, 16, "Kyty");
	// EXIT_NOT_IMPLEMENTED(s >= 16);
	// np_id->handle.term = 0;
	std::memset(np_id, 0, sizeof(*np_id));

	// return OK;
	return np_error_signed_out;
}

int KYTY_SYSV_ABI NpGetOnlineId(int user_id, NpOnlineId* online_id) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n", user_id);

	EXIT_NOT_IMPLEMENTED(online_id == nullptr);

	// int s = snprintf(online_id->data, 16, "Kyty");
	// EXIT_NOT_IMPLEMENTED(s >= 16);
	// online_id->term = 0;
	std::memset(online_id, 0, sizeof(*online_id));

	// return OK;
	return np_error_signed_out;
}

int KYTY_SYSV_ABI NpGetAccountIdA(int user_id, uint64_t* account_id) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n", user_id);

	EXIT_NOT_IMPLEMENTED(account_id == nullptr);

	// *account_id = 0x00000000feedfaceull;
	*account_id = 0;

	// return OK;
	return np_error_signed_out;
}

int KYTY_SYSV_ABI NpGetAccountCountryA(int user_id, void* country_code) {
	PRINT_NAME();

	LOGF("\t user_id = %d\n", user_id);

	EXIT_NOT_IMPLEMENTED(country_code == nullptr);

	auto* code = static_cast<NpCountryCode*>(country_code);
	std::memset(code, 0, sizeof(*code));
	// code->data[0] = '';
	// code->data[1] = 'S';

	// return OK;
	return np_error_signed_out;
}

int KYTY_SYSV_ABI NpGetAccountAge(int req_id, int user_id, uint8_t* age) {
	PRINT_NAME();

	LOGF("\t req_id  = %d\n", req_id);
	LOGF("\t user_id = %d\n", user_id);
	LOGF("\t age     = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(age));

	if (req_id <= 0 || age == nullptr) {
		return np_error_invalid_argument;
	}

	// *age = 21;
	*age = 0;

	// return OK;
	return np_error_signed_out;
}

int KYTY_SYSV_ABI NpCreateRequest() {
	PRINT_NAME();

	const auto req_id = np_create_request(false);

	LOGF("\t req_id = %d\n", req_id);

	return req_id;
}

int KYTY_SYSV_ABI NpCreateAsyncRequest(const NpCreateAsyncRequestParameter* param) {
	PRINT_NAME();

	if (param == nullptr) {
		return np_error_invalid_argument;
	}

	if (param->size < sizeof(NpCreateAsyncRequestParameter)) {
		return np_error_invalid_size;
	}

	LOGF("\t size              = %" PRIu64 "\n"
	     "\t cpu_affinity_mask = %" PRIu64 "\n"
	     "\t thread_priority   = %d\n",
	     param->size, param->cpu_affinity_mask, param->thread_priority);

	const auto req_id = np_create_request(true);

	LOGF("\t req_id            = %d\n", req_id);

	return req_id;
}

int KYTY_SYSV_ABI NpDeleteRequest(int req_id) {
	PRINT_NAME();

	LOGF("\t req_id = %d\n", req_id);

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	request->state  = NpRequestState::Free;
	request->async  = false;
	request->result = OK;

	return OK;
}

int KYTY_SYSV_ABI NpAbortRequest(int req_id) {
	PRINT_NAME();

	LOGF("\t req_id = %d\n", req_id);

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	request->state  = NpRequestState::Aborted;
	request->result = np_error_aborted;

	return OK;
}

int KYTY_SYSV_ABI NpCheckNpAvailability(int req_id, const char* user, void* result) {
	PRINT_NAME();

	// EXIT_NOT_IMPLEMENTED(req_id <= 0);
	// EXIT_NOT_IMPLEMENTED(user == nullptr);
	// EXIT_NOT_IMPLEMENTED(result != nullptr);

	LOGF("\t req_id = %d\n"
	     "\t user   = %s\n",
	     req_id, user != nullptr ? user : "(null)");

	if (req_id <= 0 || user == nullptr) {
		return np_error_invalid_argument;
	}

	if (result != nullptr) {
		std::memset(result, 0, sizeof(int));
	}

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	// return OK;
	return np_complete_signed_out_locked(request);
}

int KYTY_SYSV_ABI NpCheckNpReachability(int req_id, int user_id) {
	PRINT_NAME();

	// EXIT_NOT_IMPLEMENTED(req_id <= 0);

	LOGF("\t req_id  = %d\n", req_id);
	LOGF("\t user_id = %d\n", user_id);

	if (req_id <= 0) {
		return np_error_invalid_argument;
	}

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	// return OK;
	return np_complete_signed_out_locked(request);
}

int KYTY_SYSV_ABI NpPollAsync(int req_id, int* result) {
	PRINT_NAME();

	if (result == nullptr) {
		return np_error_invalid_argument;
	}

	LOGF("\t req_id = %d\n", req_id);

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	if (!request->async || request->state == NpRequestState::Ready) {
		return np_error_invalid_id;
	}

	*result = request->result;

	// return request->state == NpRequestState::Aborted ? np_error_aborted : OK;
	return OK;
}

int KYTY_SYSV_ABI NpCheckPremium(int req_id, const NpCheckPremiumParameter* param,
                                 NpCheckPremiumResult* result) {
	PRINT_NAME();

	LOGF("\t req_id = %d\n", req_id);
	LOGF("\t param  = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(param));
	LOGF("\t result = 0x%016" PRIx64 "\n", reinterpret_cast<uint64_t>(result));

	if (param == nullptr || result == nullptr) {
		return np_error_invalid_argument;
	}

	if (param->size < sizeof(NpCheckPremiumParameter)) {
		return np_error_invalid_size;
	}

	LOGF("\t size    = %" PRIu64 "\n"
	     "\t user_id = %d\n"
	     "\t features = 0x%016" PRIx64 "\n",
	     param->size, param->user_id, param->features);

	std::lock_guard lock(g_np_request_mutex);

	auto* request = np_get_request_locked(req_id);
	if (request == nullptr) {
		return np_error_request_not_found;
	}

	if (request->state == NpRequestState::Complete) {
		request->result = np_error_invalid_argument;
		return np_error_invalid_argument;
	}

	if (request->state == NpRequestState::Aborted) {
		return np_error_aborted;
	}

	std::memset(result, 0, sizeof(*result));
	// result->authorized = true;

	// request->state  = NpRequestState::Complete;
	// request->result = OK;

	// return OK;
	return np_complete_signed_out_locked(request);
}

int KYTY_SYSV_ABI NpGetState(int user_id, uint32_t* state) {
	PRINT_NAME();

	EXIT_NOT_IMPLEMENTED(state == nullptr);

	LOGF("\t user_id = %d\n", user_id);

	*state = 1; // Signed out

	return OK;
}

int KYTY_SYSV_ABI NpGetNpReachabilityState(int user_id, uint32_t* state) {
	PRINT_NAME();

	if (state == nullptr) {
		return np_error_invalid_argument;
	}

	LOGF("\t user_id = %d\n", user_id);

	// *state = 2; // SCE_NP_REACHABILITY_STATE_REACHABLE
	*state = 0; // SCE_NP_REACHABILITY_STATE_UNAVAILABLE

	return OK;
}

int KYTY_SYSV_ABI NpHasSignedUp(int user_id, bool* has_signed_up) {
	PRINT_NAME();

	if (has_signed_up == nullptr) {
		return np_error_invalid_argument;
	}

	LOGF("\t user_id = %d\n", user_id);

	*has_signed_up = false;

	return OK;
}

} // namespace NpManager

} // namespace Libs::Network
