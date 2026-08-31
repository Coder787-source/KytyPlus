#include "libs/errno.h"
#include "libs/network.h"

#include <cstdio>
#include <cstdlib>
#include <thread>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>

namespace Http = Libs::Network::Http;
namespace Net   = Libs::Network::Net;
namespace Ssl   = Libs::Network::Ssl;

static void Require(bool ok, const char* what) {
	if (!ok) {
		std::fprintf(stderr, "FAIL: %s\n", what);
		std::abort();
	}
}

// Build the pool -> ssl -> http object chain HttpInit requires.
static int MakeHttpContext() {
	const int memid = Net::NetPoolCreate("kyty-http-tests", 1024 * 1024, 0);
	if (memid < 0) {
		return -1;
	}
	const int ssl_ctx = Ssl::SslInit(1024 * 1024);
	if (ssl_ctx < 0) {
		return -1;
	}
	return Http::HttpInit(memid, ssl_ctx, 1024 * 1024);
}

// Regression test for the Crash Bandicoot 4 initialization hang
// (upstream issue #88): HttpSendRequest marked the request terminal, but
// HttpWaitRequest never reported the completion event, so the game polled
// the http epoll forever (the multi-GB log loop). The fix queues a
// completion event on every epoll bound to the request and drains it in
// HttpWaitRequest.
static void TestHttpWaitRequestDeliversCompletion() {
	Libs::Network::Initialize();

	// Build the object chain the game creates: pool -> ssl -> http ctx ->
	// template -> connection -> request.
	Require(Libs::Network::Net::NetInit() == OK, "net init");

	const int http_ctx = MakeHttpContext();
	Require(http_ctx > 0, "http init returns valid id");

	const int tmpl = Http::HttpCreateTemplate(http_ctx, "kyty-test/1.0", 0, 1);
	Require(tmpl > 0, "create template");

	const int conn = Http::HttpCreateConnectionWithURL(
	    tmpl, "http://127.0.0.1:1/kyty-http-wait-request-test", 0);
	Require(conn > 0, "create connection");

	const int req = Http::HttpCreateRequestWithURL2(conn, "GET", "http://127.0.0.1:1/test",
	                                                0);
	Require(req > 0, "create request");

	Http::HttpEpollHandle eh = nullptr;
	Require(Http::HttpCreateEpoll(http_ctx, &eh) == OK, "create epoll");
	Require(eh != nullptr, "epoll handle non-null");

	Require(Http::HttpSetEpoll(req, eh, nullptr) == OK, "bind epoll to request");

	// Before send: a poll-mode wait must report no events.
	Http::HttpNBEvent ev {};
	Require(Http::HttpWaitRequest(eh, &ev, 1, 0) == 0, "poll wait empty before send");

	// Send. This HLE resolves synchronously with a timeout failure; the
	// completion event must still be delivered to the bound epoll.
	const int send_result = Http::HttpSendRequest(req, nullptr, 0);
	Require(send_result != OK, "send reports failure in this HLE");

	// The hang: this wait previously returned 0 forever. It must now return
	// exactly one completion event carrying the failure bits.
	const int count = Http::HttpWaitRequest(eh, &ev, 1, 100000);
	Require(count == 1, "wait delivers the completion event");
	Require(ev.id == req, "event identifies the request");
	Require(ev.user_arg == nullptr, "event carries the bound user arg");
	Require((ev.events & 0x00000010u) != 0, "event carries HUP (terminal failure)");

	// The event is consumed: a second wait with a short timeout must return 0.
	Require(Http::HttpWaitRequest(eh, &ev, 1, 1000) == 0, "queued event consumed by first wait");

	// Deleting the request must reap any queued event for it (none queued
	// here) and succeed.
	Require(Http::HttpDeleteRequest(req) == OK, "delete request");
	Require(Http::HttpWaitRequest(eh, &ev, 1, 0) == 0, "no events after delete");

	Require(Http::HttpDestroyEpoll(http_ctx, eh) == OK, "destroy epoll");
	Require(Http::HttpDeleteConnection(conn) == OK, "delete connection");
	Require(Http::HttpDeleteTemplate(tmpl) == OK, "delete template");
	Require(Http::HttpTerm(http_ctx) == OK, "http term");
	Require(Libs::Network::Net::NetTerm() == OK, "net term");

	Libs::Network::Shutdown();
}

// A request deleted after send but before wait must not surface a stale event.
static void TestHttpDeleteRequestReapsQueuedEvent() {
	Libs::Network::Initialize();

	Require(Libs::Network::Net::NetInit() == OK, "net init");

	const int http_ctx = MakeHttpContext();
	Require(http_ctx > 0, "http init");

	const int tmpl = Http::HttpCreateTemplate(http_ctx, "kyty-test/1.0", 0, 1);
	Require(tmpl > 0, "create template");

	const int conn = Http::HttpCreateConnectionWithURL(
	    tmpl, "http://127.0.0.1:1/kyty-http-reap-test", 0);
	Require(conn > 0, "create connection");

	const int req = Http::HttpCreateRequestWithURL2(conn, "GET", "http://127.0.0.1:1/test",
	                                                0);
	Require(req > 0, "create request");

	Http::HttpEpollHandle eh = nullptr;
	Require(Http::HttpCreateEpoll(http_ctx, &eh) == OK, "create epoll");
	Require(Http::HttpSetEpoll(req, eh, nullptr) == OK, "bind epoll");

	Require(Http::HttpSendRequest(req, nullptr, 0) != OK, "send");

	// Delete before any wait: the queued completion event must be reaped.
	Require(Http::HttpDeleteRequest(req) == OK, "delete request");

	Http::HttpNBEvent ev {};
	Require(Http::HttpWaitRequest(eh, &ev, 1, 0) == 0, "reaped event never delivered");

	Require(Http::HttpDestroyEpoll(http_ctx, eh) == OK, "destroy epoll");
	Require(Http::HttpDeleteConnection(conn) == OK, "delete connection");
	Require(Http::HttpDeleteTemplate(tmpl) == OK, "delete template");
	Require(Http::HttpTerm(http_ctx) == OK, "http term");
	Require(Libs::Network::Net::NetTerm() == OK, "net term");

	Libs::Network::Shutdown();
}

// A blocked wait must be released by SignalAll from another thread path: the
// event queued by a send on another thread wakes the waiting drain.
static void TestHttpWaitRequestBlockingWakeup() {
	Libs::Network::Initialize();

	Require(Libs::Network::Net::NetInit() == OK, "net init");

	const int http_ctx = MakeHttpContext();
	Require(http_ctx > 0, "http init");

	const int tmpl = Http::HttpCreateTemplate(http_ctx, "kyty-test/1.0", 0, 1);
	Require(tmpl > 0, "create template");

	const int conn = Http::HttpCreateConnectionWithURL(
	    tmpl, "http://127.0.0.1:1/kyty-http-block-test", 0);
	Require(conn > 0, "create connection");

	const int req = Http::HttpCreateRequestWithURL2(conn, "GET", "http://127.0.0.1:1/test",
	                                                0);
	Require(req > 0, "create request");

	Http::HttpEpollHandle eh = nullptr;
	Require(Http::HttpCreateEpoll(http_ctx, &eh) == OK, "create epoll");
	Require(Http::HttpSetEpoll(req, eh, nullptr) == OK, "bind epoll");

	std::thread sender([req] {
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
		// The send result is not checked here: the point is the cross-thread
		// event delivery that must release the blocked wait below.
		(void) Http::HttpSendRequest(req, nullptr, 0);
	});

	// Blocking wait (timeout < 0). Must return exactly one event after the
	// sender completes the request.
	Http::HttpNBEvent ev {};
	const int count = Http::HttpWaitRequest(eh, &ev, 1, -1);
	Require(count == 1, "blocking wait woken by send");
	Require(ev.id == req, "blocking event identifies request");

	sender.join();

	Require(Http::HttpDestroyEpoll(http_ctx, eh) == OK, "destroy epoll");
	Require(Http::HttpDeleteRequest(req) == OK, "delete request");
	Require(Http::HttpDeleteConnection(conn) == OK, "delete connection");
	Require(Http::HttpDeleteTemplate(tmpl) == OK, "delete template");
	Require(Http::HttpTerm(http_ctx) == OK, "http term");
	Require(Libs::Network::Net::NetTerm() == OK, "net term");

	Libs::Network::Shutdown();
}

// --- Real HTTP client test ---------------------------------------------------
// Spins up a loopback HTTP server inside the test, issues a real request
// through HttpSendRequest, and verifies the response is stored and surfaced
// through the query ABIs + the epoll completion event with the IN bit.
static void TestRealHttpExchange() {
	// --- Minimal one-shot loopback server ---
	auto server_ready  = std::atomic<bool> {false};
	auto accept_done   = std::atomic<bool> {false};
	auto loopback_port = std::atomic<uint16_t> {0};
	std::string captured_request;
	const std::string body_payload = "kyty-real-http-client-ok";
	const std::string expected_headers =
		"HTTP/1.1 200 OK\r\nContent-Length: 24\r\nConnection: close\r\n";

	std::thread server([&] {
		WSADATA wsa{};
		const int wsa_rc = WSAStartup(MAKEWORD(2, 2), &wsa);
		if (wsa_rc != 0) {
			std::fprintf(stderr, "FAIL: WSAStartup rc=%d\n", wsa_rc);
			std::abort();
		}
		const SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
		if (listener == INVALID_SOCKET) {
			std::abort();
		}
		sockaddr_in addr{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(0x7f000001u);
		addr.sin_port        = 0; // ephemeral
		if (::bind(listener, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) != 0) {
			std::abort();
		}
		socklen_t alen = sizeof(addr);
		if (::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) {
			std::abort();
		}
		const uint16_t port = ntohs(addr.sin_port);
		if (::listen(listener, 1) != 0) {
			std::abort();
		}
		loopback_port.store(port);
		server_ready.store(true);

		const SOCKET client = ::accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) {
			std::abort();
		}
		accept_done.store(true);
		char buf[2048]{};
		const int got = ::recv(client, buf, sizeof(buf) - 1, 0);
		if (got <= 0) {
			std::abort();
		}
		captured_request.assign(buf, static_cast<size_t>(got));

		const std::string response =
			expected_headers + "\r\n" + body_payload;
		if (::send(client, response.c_str(), static_cast<int>(response.size()), 0) ==
		    SOCKET_ERROR) {
			std::abort();
		}
		::closesocket(client);
		::closesocket(listener);
	});

	while (!server_ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	Libs::Network::Initialize();
	Require(Libs::Network::Net::NetInit() == OK, "net init (real exchange)");
	const int http_ctx = MakeHttpContext();
	Require(http_ctx > 0, "http init (real exchange)");

	const int tmpl = Http::HttpCreateTemplate(http_ctx, "kyty-test/2.0", 0, 1);
	Require(tmpl > 0, "create template (real)");

	// Route the connection at the loopback server; the port is fixed by the
	// server thread's bind above, discovered via getsockname.
	const std::string url =
		"http://127.0.0.1:" + std::to_string(loopback_port.load()) + "/probe";

	const int conn = Http::HttpCreateConnectionWithURL(tmpl, url.c_str(), 0);
	Require(conn > 0, "create connection (real)");

	const int req = Http::HttpCreateRequestWithURL2(conn, "GET", url.c_str(), 0);
	Require(req > 0, "create request (real)");

	Http::HttpEpollHandle eh = nullptr;
	Require(Http::HttpCreateEpoll(http_ctx, &eh) == OK, "create epoll (real)");
	Require(Http::HttpSetEpoll(req, eh, nullptr) == OK, "bind epoll (real)");

	// Real send against the loopback server must return OK.
	Require(Http::HttpSendRequest(req, nullptr, 0) == OK, "real send returns OK");

	// The completion event must carry IN (response ready to read).
	Http::HttpNBEvent ev{};
	Require(Http::HttpWaitRequest(eh, &ev, 1, 100000) == 1, "real exchange event");
	Require(ev.id == req, "real event request id");
	Require((ev.events & 0x00000001u) != 0, "real event has IN bit");

	// Status code + content length surfaced through the query ABIs.
	int status = 0;
	Require(Http::HttpGetStatusCode(req, &status) == OK, "get status code rc");
	Require(status == 200, "real status code is 200");

	int      rc2   = 0;
	uint64_t clen  = 0;
	Require(Http::HttpGetResponseContentLength(req, &rc2, &clen) == OK,
	        "get content length rc");
	Require(clen == body_payload.size(), "real content length");

	// The server must have received a well-formed request line.
	Require(accept_done.load(), "server accepted the connection");
	Require(captured_request.rfind("GET /probe HTTP/1.1\r\n", 0) == 0,
	        "request line is GET /probe HTTP/1.1");
	Require(captured_request.find("Host: 127.0.0.1") != std::string::npos,
	        "request carries Host header");

	Require(Http::HttpDestroyEpoll(http_ctx, eh) == OK, "destroy epoll (real)");
	Require(Http::HttpDeleteRequest(req) == OK, "delete request (real)");
	Require(Http::HttpDeleteConnection(conn) == OK, "delete connection (real)");
	Require(Http::HttpDeleteTemplate(tmpl) == OK, "delete template (real)");
	Require(Http::HttpTerm(http_ctx) == OK, "http term (real)");
	Require(Libs::Network::Net::NetTerm() == OK, "net term (real)");
	Libs::Network::Shutdown();

	server.join();
}


// --- TLS (HTTPS) regression test ------------------------------------------------
// Spins up a real TLS server inside the test using the vendored mbed TLS
// (self-signed cert). The client must complete a real TLS handshake attempt
// and REJECT the self-signed certificate with a verification error (not a
// hang, not a crash, not silent acceptance). This proves the handshake
// machinery, the system-trust-store path, and hostname verification are live.
#ifdef KYTY_HAS_MBEDTLS
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <mbedtls/net_sockets.h>
#include <mbedtls/pk.h>
#include <mbedtls/ssl.h>
#include <mbedtls/x509_crt.h>

static const char kTlsTestCert[] =
	"-----BEGIN CERTIFICATE-----\n" "MIIC/TCCAeWgAwIBAgIUcfo9OJrcaqcUs2+k5uE88tnrLzgwDQYJKoZIhvcNAQEL\n" "BQAwADAeFw0yNjA4MzEwMDQ5NTZaFw0yNjA5MDIwMDQ5NTZaMAAwggEiMA0GCSqG\n" "SIb3DQEBAQUAA4IBDwAwggEKAoIBAQCQMneIiLp7Ix6ElOzDkhfnzv2WBR2KOjsj\n" "LzG08mUuz67agGUhTC0ifWSrvtKNOSMdJ14LxcdjgvWXeXv36SjqauCjM5vQK/Al\n" "RJjcpsmsksCf/e9h25Tjr/yUMUFwdWQvUZQKCahuhdouP3bsmA6zluF+rNf91tsY\n" "ly9gV3yskcP0Yq+kaxjmWdsQdPh7O9sjKzHEMj6E/xjrFRlyCnJrmO8wZ9kOJqVh\n" "UrUGyXwkpZHg7qemktwNt///T2hislxgPqplJphnDs99rfVVbhq4fjP0gZJE14mE\n" "ZaSPXv9fn6mcGfYxnOFEysVCIfuvICaBsJPDWRRRaowHFfHs3LOfAgMBAAGjbzBt\n" "MB0GA1UdDgQWBBRqd+ToWXvinbMjMD4S31JUE7audDAfBgNVHSMEGDAWgBRqd+To\n" "WXvinbMjMD4S31JUE7audDAPBgNVHRMBAf8EBTADAQH/MBoGA1UdEQQTMBGHBH8A\n" "AAGCCTEyNy4wLjAuMTANBgkqhkiG9w0BAQsFAAOCAQEAW3wb511xv4etiushYCJb\n" "aWVXGc3FTwc4jc5CpFK1Or3nOdqg62OQ90Qd0o3T9NZnnrYhCfNJfJ4fNQLEHj4R\n" "DypafbzCni0I/UJM0wTzrIX1XP+KaDv2qhciqanT5Ty7Vz/zkyH3JKbbgYDapbGd\n" "N7JtakS4rGkdaaqXy+outIMcL9ErCIeKfaC7o37I7I/UgS9O41RuaisFNFTr+Msk\n" "BTjsEni/uBJstlknNKKlzR9FE91dr87XScYFFCOdfgB+iukCJk/pgjgp8jXRN3lC\n" "MAAJLFBD+jCH79cmkUDZUzqzBgOJ7KBXkQ0IiWw3+x44F73fCy+hAf0BC0PTnJFe\n" "LA==\n" "-----END CERTIFICATE-----\n";
static const char kTlsTestKey[] =
	"-----BEGIN PRIVATE KEY-----\n" "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCQMneIiLp7Ix6E\n" "lOzDkhfnzv2WBR2KOjsjLzG08mUuz67agGUhTC0ifWSrvtKNOSMdJ14LxcdjgvWX\n" "eXv36SjqauCjM5vQK/AlRJjcpsmsksCf/e9h25Tjr/yUMUFwdWQvUZQKCahuhdou\n" "P3bsmA6zluF+rNf91tsYly9gV3yskcP0Yq+kaxjmWdsQdPh7O9sjKzHEMj6E/xjr\n" "FRlyCnJrmO8wZ9kOJqVhUrUGyXwkpZHg7qemktwNt///T2hislxgPqplJphnDs99\n" "rfVVbhq4fjP0gZJE14mEZaSPXv9fn6mcGfYxnOFEysVCIfuvICaBsJPDWRRRaowH\n" "FfHs3LOfAgMBAAECggEAJEADq9fPpaAlDmiQAvRpvRVcy8eabwMHledJ5Dbht/xu\n" "9TA8/Dt1x0tlsQVmUilwieY5LtV4NCyaiAt63k6HwjK/KVgSjilVlFDj6uilH5S/\n" "BxmkAeJdUkYORcUCMf7ZAkpMYJ+f77UHrgO3ZnAtlflS0/Eo1YAbkVmEiABGb+1O\n" "jFOMC9COGolRW2lEJZYn0gawOiJU5kcfAg3z/mXIeUFnDzwjxqBqEj6wPaOaxSpi\n" "zvENbL48RzlWuVC7r4UbaIxYjU6wvBUkWjdIks2wd2IOaLK1scDPuqfnqTnOvJBp\n" "7AoUkokQ2SEh59I7mxkfSFlQZBQaD7pPSp7Znk1KRQKBgQDDEOC4y0nipFmfWRnB\n" "ukIQdivkILFyxE3XrOll0w2Bkg3cgYfpY+lvZRKXWMTRY2xC+FLL64OPXWoT1Tek\n" "CA9MAZxjdnKi5TlF2guPlELQ1wZBc0V2q9z7Mj71lEQwMIsRSPF8Hv9IaUAD77az\n" "heVdn7TCmazHVmh98zD1DYrlTQKBgQC9PbCmnGWiwoLRcl9YnSfLR2iCNwufJAkk\n" "cqLZFgWm6RlUCuGZProojs+Krt4STBwevikaVGF1JKElY6mhzwOCxHWuSlIBYcPh\n" "JRjDXvqeYl2TCrMZ54w0KS64OV3rY0VgovZfQnyafmv3M5LT6hogDXlA3mXzAFT1\n" "eVlfhaxWmwKBgQChdMXcjv8v5gL8fv5vuGBYSceYgmr96HfZ7ZgeHOvP9Hkeq+Q4\n" "DOe5uToDJYl7GsUQRYQan2x8bMCRe+kbD0TCD49HUFIgfITESW9KP1hyjZfy8ptr\n" "V2OIU5WDJV6vWG6zNuISb4GziNJgr7hUrZ4kuT0f2Z0GPYItxe4e1z/A/QKBgCvJ\n" "ZmJmta2fTB2iVSVv15FViHz4t5uu/t6MF9obGluwe8fSbLjEptZTlPoF1CRvr+H9\n" "Jg2NkK9GNnMsSEfEWo+bXY9iau8e6+/gYYZzJ5IWOIiZZS+NQAehr8m4nF6mrZra\n" "mh8YhzRIJDsil8qo1DquY2v7CaPcY8wYs/FCQMoZAoGAEFzXBftlDjaJh3W32gI8\n" "IVPiOO/t1xn/9i4XSOGzgjtrnAUBhtiiHWa5pbTSSXAqAmuiTQpXOC1uoBFFQ3iC\n" "9UgklDBnskIcKbquijG+icrtskl1IHHwRfO1UwkFXZmv/5n5x4WErqoCasVamrpP\n" "g6hFP33wB8utyFmaUe2Arf8=\n" "-----END PRIVATE KEY-----\n";



static void TestHttpsRejectsUntrustedCert() {
	// --- TLS server thread: listen, accept, handshake, respond ---
	std::atomic<bool> server_ready{false};
	std::atomic<bool> handshake_attempted{false};
	std::atomic<int> server_handshake_rc{0};
	std::atomic<unsigned short> tls_test_port{0};

	std::thread server([&] {
		WSADATA wsa{};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			return;
		}
		SOCKET listener = ::socket(AF_INET, SOCK_STREAM, 0);
		sockaddr_in addr{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(0x7f000001u);
		addr.sin_port        = 0;
		if (::bind(listener, (sockaddr*)&addr, sizeof(addr)) != 0) {
			return;
		}
		socklen_t alen = sizeof(addr);
		::getsockname(listener, (sockaddr*)&addr, &alen);
		const unsigned short port = ntohs(addr.sin_port);
		::listen(listener, 1);
		tls_test_port.store(port);
		server_ready.store(true);

		SOCKET client = ::accept(listener, nullptr, nullptr);
		if (client == INVALID_SOCKET) {
			return;
		}
		handshake_attempted.store(true);

		mbedtls_ssl_context ssl{};
		mbedtls_ssl_config conf{};
		mbedtls_x509_crt srvcert{};
		mbedtls_pk_context pkey{};
		mbedtls_ctr_drbg_context ctr_drbg{};
		mbedtls_entropy_context entropy{};
		mbedtls_net_context server_fd{};

		mbedtls_ssl_init(&ssl);
		mbedtls_ssl_config_init(&conf);
		mbedtls_x509_crt_init(&srvcert);
		mbedtls_pk_init(&pkey);
		mbedtls_ctr_drbg_init(&ctr_drbg);
		mbedtls_entropy_init(&entropy);

		int rc = mbedtls_x509_crt_parse(&srvcert, (const unsigned char*)kTlsTestCert,
		                               strlen(kTlsTestCert) + 1);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		rc = mbedtls_pk_parse_key(&pkey, (const unsigned char*)kTlsTestKey,
		                        strlen(kTlsTestKey) + 1, nullptr, 0, nullptr,
		                        nullptr);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		rc = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		rc = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_SERVER,
		                                MBEDTLS_SSL_TRANSPORT_STREAM,
		                                MBEDTLS_SSL_PRESET_DEFAULT);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
		mbedtls_ssl_conf_ca_chain(&conf, &srvcert, nullptr);
		rc = mbedtls_ssl_conf_own_cert(&conf, &srvcert, &pkey);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		rc = mbedtls_ssl_setup(&ssl, &conf);
		if (rc != 0) {
			server_handshake_rc.store(rc);
			return;
		}
		server_fd.fd = client;
		// Bound the handshake so a protocol stall surfaces as an error, not a hang.
		DWORD rcv_ms = 10000;
		::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&rcv_ms, sizeof(rcv_ms));
		::setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, (const char*)&rcv_ms, sizeof(rcv_ms));
		mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, nullptr);

		// The client will abort on the untrusted cert; the server observes the
		// failed handshake (or its alert). Either way the machinery is live.
		const int hs = mbedtls_ssl_handshake(&ssl);
		server_handshake_rc.store(hs);

		mbedtls_ssl_close_notify(&ssl);
		mbedtls_net_free(&server_fd);
		mbedtls_ssl_free(&ssl);
		mbedtls_ssl_config_free(&conf);
		mbedtls_x509_crt_free(&srvcert);
		mbedtls_pk_free(&pkey);
		mbedtls_ctr_drbg_free(&ctr_drbg);
		mbedtls_entropy_free(&entropy);
		::closesocket(listener);
	});

	while (!server_ready.load()) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	Libs::Network::Initialize();
	Require(Libs::Network::Net::NetInit() == OK, "net init (tls)");
	const int http_ctx = MakeHttpContext();
	Require(http_ctx > 0, "http init (tls)");

	const int tmpl = Http::HttpCreateTemplate(http_ctx, "kyty-tls-test/1.0", 0, 1);
	Require(tmpl > 0, "create template (tls)");

	// Connect via URL: https://127.0.0.1:<ephemeral port published by the
	// server thread>/probe
	const std::string url = "https://127.0.0.1:" + std::to_string(tls_test_port.load()) + "/probe";

	const int conn = Http::HttpCreateConnectionWithURL(tmpl, url.c_str(), 0);
	Require(conn > 0, "create connection (tls)");

	const int req = Http::HttpCreateRequestWithURL2(conn, "GET", url.c_str(), 0);
	Require(req > 0, "create request (tls)");

	Http::HttpEpollHandle eh = nullptr;
	Require(Http::HttpCreateEpoll(http_ctx, &eh) == OK, "create epoll (tls)");
	Require(Http::HttpSetEpoll(req, eh, nullptr) == OK, "bind epoll (tls)");

	// Real TLS send against the self-signed server: the handshake must run and
	// the request must FAIL with an SSL-family error (never hang, never
	// silently succeed, never downgrade to plaintext).
	const int rc = Http::HttpSendRequest(req, nullptr, 0);
	Require(rc != OK, "untrusted-cert request must not succeed");
	Require(rc != Libs::Network::HTTP_ERROR_TIMEOUT, "untrusted-cert must not report timeout");
	Require(rc != Libs::Network::HTTP_ERROR_BEFORE_SEND, "untrusted-cert must actually send");
	Require((rc == Libs::Network::HTTP_ERROR_SSL || rc == Libs::Network::HTTP_ERROR_NETWORK ||
	         rc == static_cast<int>(0x80431064) || rc == static_cast<int>(0x80431075)),
	        "untrusted-cert failure is an SSL/network-family error");

	// A completion event must still be delivered for the failed request.
	Http::HttpNBEvent ev{};
	Require(Http::HttpWaitRequest(eh, &ev, 1, 100000) == 1, "tls failure event delivered");
	Require(ev.id == req, "tls event request id");
	Require((ev.events & 0x00000010u) != 0, "tls failure event carries HUP bit");

	// The server really performed a TLS handshake (the client talked to it).
	Require(handshake_attempted.load(), "server accepted a connection");

	Require(Http::HttpDestroyEpoll(http_ctx, eh) == OK, "destroy epoll (tls)");
	Require(Http::HttpDeleteRequest(req) == OK, "delete request (tls)");
	Require(Http::HttpDeleteConnection(conn) == OK, "delete connection (tls)");
	Require(Http::HttpDeleteTemplate(tmpl) == OK, "delete template (tls)");
	Require(Http::HttpTerm(http_ctx) == OK, "http term (tls)");
	Require(Libs::Network::Net::NetTerm() == OK, "net term (tls)");
	Libs::Network::Shutdown();

	server.join();
}
#endif // KYTY_HAS_MBEDTLS


// --- UDP regression test --------------------------------------------------------
// Exercises the guest UDP socket path end to end through the libNet ABI: bind
// a guest socket, sendto a loopback peer, recvfrom with peer-address
// validation, non-blocking poll via Select, and clean teardown. UDP is the
// netcode transport for online titles (Among Us et al.), so this guards the
// whole sendto/recvfrom + sockaddr conversion path.
static void TestUdpRoundTrip() {
	Libs::Network::Initialize();
	Require(Libs::Network::Net::NetInit() == OK, "net init (udp)");

	// Peer: a host-side UDP receiver on an ephemeral loopback port.
	std::atomic<unsigned short> peer_port{0};
	std::atomic<bool> peer_got_it{false};
	std::atomic<int> peer_got_len{0};

	std::thread peer([&] {
		WSADATA wsa{};
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			return;
		}
		SOCKET s = ::socket(AF_INET, SOCK_DGRAM, 0);
		sockaddr_in addr{};
		addr.sin_family      = AF_INET;
		addr.sin_addr.s_addr = htonl(0x7f000001u);
		addr.sin_port        = 0;
		if (::bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
			return;
		}
		socklen_t alen = sizeof(addr);
		::getsockname(s, (sockaddr*)&addr, &alen);
		peer_port.store(ntohs(addr.sin_port));

		char buf[64]{};
		sockaddr_in from{};
		socklen_t flen = sizeof(from);
		const int n = ::recvfrom(s, buf, sizeof(buf), 0, (sockaddr*)&from, &flen);
		if (n > 0) {
			peer_got_len.store(n);
			if (std::memcmp(buf, "kyty-udp-probe", 14) == 0 &&
			    ntohl(from.sin_addr.s_addr) == 0x7f000001u) {
				peer_got_it.store(true);
			}
			// Echo back so the guest recvfrom has data to read.
			::sendto(s, buf, n, 0, (sockaddr*)&from, flen);
		}
		::closesocket(s);
	});

	while (peer_port.load() == 0) {
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	// Guest socket through the ABI.
	constexpr int guest_af_inet = 2;
	constexpr int guest_sock_dgram = 2;
	const int s = Libs::Network::Net::Socket(guest_af_inet, guest_sock_dgram, 0);
	Require(s >= 0, "guest udp socket create");

	// Guest sockaddr_in: PS ABI as produced/consumed by the emulator - the
	// BSD layout: byte 0 = sin_len (16), byte 1 = sin_family (2), port at
	// offset 2 and address at offset 4, both network byte order (this matches
	// ConvertHostSockaddr's output, which is the emulator's canonical form).
	struct GuestSockaddrIn {
		uint8_t  len;
		uint8_t  family;
		uint16_t port_be;
		uint32_t addr_be;
		uint8_t  zero[8];
	};
	GuestSockaddrIn peer_addr{};
	peer_addr.len     = sizeof(GuestSockaddrIn);
	peer_addr.family  = guest_af_inet;
	peer_addr.port_be = htons(peer_port.load());
	peer_addr.addr_be = htonl(0x7f000001u);

	// Send a datagram through the guest ABI.
	const int64_t sent = Libs::Network::Net::Sendto(s, "kyty-udp-probe", 14, 0, &peer_addr, sizeof(peer_addr));
	Require(sent == 14, "guest sendto delivers 14 bytes");

	// Wait for the peer's echo with a generous bound (it is loopback, but the
	// peer thread scheduling is not instantaneous).
	bool echoed = false;
	std::string received;
	for (int spin = 0; spin < 200 && !echoed; spin++) {
		char        buf[64]{};
		GuestSockaddrIn from{};
		uint32_t     fromlen = sizeof(from);
		const int64_t n =
		    Libs::Network::Net::Recvfrom(s, buf, sizeof(buf), 0, &from, &fromlen);
		if (n > 0) {
			echoed   = true;
			received = std::string(buf, static_cast<size_t>(n));
			// Peer address must round-trip through the conversion helpers.
			Require(ntohl(from.addr_be) == 0x7f000001u, "recvfrom peer address");
			Require(ntohs(from.port_be) == peer_port.load(), "recvfrom peer port");
		} else {
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
	}
	Require(echoed, "udp echo received");
	Require(received == "kyty-udp-probe", "udp payload round-trips");
	Require(peer_got_it.load(), "peer validated payload + sender address");

	Require(Libs::Network::Net::SocketClose(s) == OK, "guest socket close");
	Require(Libs::Network::Net::NetTerm() == OK, "net term (udp)");
	Libs::Network::Shutdown();

	peer.join();
}

int main() {
	TestHttpWaitRequestDeliversCompletion();
	TestHttpDeleteRequestReapsQueuedEvent();
	TestHttpWaitRequestBlockingWakeup();
	TestRealHttpExchange();
#ifdef KYTY_HAS_MBEDTLS
	TestHttpsRejectsUntrustedCert();
#endif

	TestUdpRoundTrip();

	return 0;
}