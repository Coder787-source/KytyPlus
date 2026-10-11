#include "common/abi.h"
#include "libs/errno.h"
#include "loader/symbolDatabase.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace Libs {
void InitPlayGo_1(Loader::SymbolDatabase* symbols);
void InitPlayGoDialog_1(Loader::SymbolDatabase* symbols);
void InitNpCommerce_1(Loader::SymbolDatabase* symbols);
}

namespace {
bool available = false;
uint32_t manifest_chunks = 0;
uint32_t metadata_queries = 0;

void Check(bool condition, const char* message) {
	if (!condition) {
	 std::fprintf(stderr, "PlayGo API failure: %s", message);
	 std::abort();
	}
}

template <typename F>
F Resolve(Loader::SymbolDatabase& symbols, const char* nid, const char* library, int minor = 0) {
	Loader::SymbolResolve query {nid, library, 1, library, 1, minor, Loader::SymbolType::Func};
	const auto* record = symbols.Find(query);
	Check(record != nullptr && record->vaddr != 0, "guest import unresolved");
	return reinterpret_cast<F>(record->vaddr);
}

struct Progress { uint64_t progress_size; uint64_t total_size; };
using Enumerate = int(KYTY_SYSV_ABI*)(int, uint16_t*, uint32_t, uint32_t*);
using Open = int(KYTY_SYSV_ABI*)(int*, const void*);
using Terminate = int(KYTY_SYSV_ABI*)();
using Locus = int(KYTY_SYSV_ABI*)(int, const uint16_t*, uint32_t, int8_t*);
using GetProgress = int(KYTY_SYSV_ABI*)(int, const uint16_t*, uint32_t, Progress*);
using Eta = int(KYTY_SYSV_ABI*)(int, const uint16_t*, uint32_t, int64_t*);
using Prefetch = int(KYTY_SYSV_ABI*)(int, const uint16_t*, uint32_t, int8_t);

void TestEnumeration(Enumerate enumerate, int handle, uint32_t expected) {
	uint32_t count = 0xffffffffu;
	Check(enumerate(handle, nullptr, 0, &count) == OK && count == expected,
	      "count-only pass did not return total chunks");
	Check(enumerate(handle, nullptr, 1, &count) == OK && count == expected,
	      "null-buffer count query depends on capacity");
	std::vector<uint16_t> ids(expected + 1, 0xffff);
	Check(enumerate(handle, ids.data(), expected + 1, &count) == OK && count == expected,
	      "second enumeration pass failed");
	for (uint32_t i = 0; i < expected; i++) Check(ids[i] == i, "wrong chunk ID");
	Check(ids[expected] == 0xffff, "wrote beyond available chunks");
	uint16_t small[] {0xffff, 0xffff, 0xbeef};
	Check(enumerate(handle, small, 2, &count) == OK && count == 2,
	      "capacity-limited enumeration failed");
	Check(small[0] == 0 && small[1] == 1 && small[2] == 0xbeef, "buffer overflow");
	Check(enumerate(handle, small, 0, &count) == Libs::PlayGo::PLAYGO_ERROR_BAD_SIZE,
	      "accepted nonnull zero-capacity buffer");
	Check(enumerate(handle, nullptr, 0, nullptr) == Libs::PlayGo::PLAYGO_ERROR_BAD_POINTER,
	      "accepted null count pointer");
	Check(enumerate(99, nullptr, 0, &count) == Libs::PlayGo::PLAYGO_ERROR_BAD_HANDLE,
	      "accepted invalid handle");
}
}

// Only substitute the external metadata provider; exercise the real registered
// production guest APIs and resolver, not a copy of the implementation.
namespace Loader {
bool SystemContentGetChunksNum(uint32_t* count) {
	++metadata_queries;
	if (!available) return false;
	*count = manifest_chunks;
	return true;
}
}

int main() {
	Loader::SymbolDatabase symbols;
	Libs::InitPlayGo_1(&symbols);
	Libs::InitPlayGoDialog_1(&symbols);
	Libs::InitNpCommerce_1(&symbols);
	auto enumerate = Resolve<Enumerate>(symbols, "73fF1MFU8hA", "PlayGo");
	auto installed = Resolve<Enumerate>(symbols, "8-e7E989rCU", "PlayGo");
	auto open = Resolve<Open>(symbols, "M1Gma1ocrGE", "PlayGo");
	auto terminate = Resolve<Terminate>(symbols, "MPe0EeBGM-E", "PlayGo");
	auto locus = Resolve<Locus>(symbols, "uWIYLFkkwqk", "PlayGo");
	auto progress = Resolve<GetProgress>(symbols, "-RJWNMK3fC8", "PlayGo");
	auto eta = Resolve<Eta>(symbols, "v6EZ-YWRdMs", "PlayGo");
	auto prefetch = Resolve<Prefetch>(symbols, "-Q1-u1a7p0g", "PlayGo");
	const char* commerce[] {"0aR2aWmQal4", "DfSCDRA3EjY", "m-I92Ab50W8", "LR5cwFMMCVE",
	                        "r42bWcQbtZY", "uKTDW8hk-ts", "DHmwsa6S8Tc", "dsqCVsNM0Zg"};
	for (const auto* nid : commerce) Resolve<Terminate>(symbols, nid, "NpCommerce", 1);
	int handle = 0;
	Check(open(&handle, nullptr) == OK && handle == 1, "open failed");
	TestEnumeration(enumerate, handle, 1000);
	TestEnumeration(installed, handle, 1000);
	Check(metadata_queries == 1, "repeated missing metadata lookups");
	const uint16_t ids[] {0, 6, 999, 1000, 65535};
	int8_t loci[5] {};
	Check(locus(handle, ids, 5, loci) == OK, "fallback rejected requested chunk");
	Check(std::all_of(loci, loci + 5, [](int8_t l) { return l == 3; }), "not LOCAL_FAST");
	Progress value {};
	Check(progress(handle, ids, 5, &value) == OK && value.total_size != 0 &&
	      value.progress_size == value.total_size, "incomplete installed progress");
	int64_t remaining = -1;
	Check(eta(handle, ids, 5, &remaining) == OK && remaining == 0, "ETA not zero");
	Check(prefetch(handle, ids, 5, 3) == OK, "local prefetch failed");
	Check(locus(handle, ids, 0, loci) == Libs::PlayGo::PLAYGO_ERROR_BAD_SIZE, "accepted empty locus");
	Check(locus(handle, nullptr, 5, loci) == Libs::PlayGo::PLAYGO_ERROR_BAD_POINTER, "accepted null IDs");
	Check(prefetch(handle, ids, 5, 1) == Libs::PlayGo::PLAYGO_ERROR_BAD_LOCUS, "accepted invalid locus");
	Check(progress(handle, ids, 5, nullptr) == Libs::PlayGo::PLAYGO_ERROR_BAD_POINTER,
	      "accepted null progress output");
	Check(terminate() == OK, "termination failed");
	available = true;
	manifest_chunks = 7;
	Check(open(&handle, nullptr) == OK, "manifest-backed open failed");
	TestEnumeration(enumerate, handle, 7);
	TestEnumeration(installed, handle, 7);
	Check(locus(handle, ids, 2, loci) == OK && loci[0] == 3 && loci[1] == 3,
	      "valid manifest chunks not installed");
	const uint16_t invalid = 7;
	Check(locus(handle, &invalid, 1, loci) == Libs::PlayGo::PLAYGO_ERROR_BAD_CHUNK_ID,
	      "ignored manifest bounds");
	Check(progress(handle, &invalid, 1, &value) == Libs::PlayGo::PLAYGO_ERROR_BAD_CHUNK_ID,
	      "accepted nonexistent chunk progress");
	Check(metadata_queries == 2, "termination did not reset metadata");
	Check(terminate() == OK, "second termination failed");
	manifest_chunks = 0;
	Check(open(&handle, nullptr) == OK, "empty manifest open failed");
	uint32_t count = 1;
	Check(enumerate(handle, nullptr, 0, &count) == OK && count == 0,
	      "empty manifest used invented fallback");
	Check(locus(handle, ids, 1, loci) == Libs::PlayGo::PLAYGO_ERROR_BAD_CHUNK_ID,
	      "empty manifest accepted nonexistent chunk");
	std::puts("PlayGo API regression tests passed");
	return 0;
}
