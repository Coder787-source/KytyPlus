#ifndef KYTY_COMMON_ASSERT_H_
#define KYTY_COMMON_ASSERT_H_

#include "common/common.h"
#include "common/logging/log.h"

#include <cstdlib>
#include <string_view>

namespace Common {

#ifdef __clang__
int DbgExitHandler(char const* file, int line, std::string_view text)
    __attribute__((analyzer_noreturn));
int DbgExitHandler(char const* file, int line, fmt::text_style style, std::string_view text)
    __attribute__((analyzer_noreturn));
int DbgExitIfHandler(char const* expr, char const* file, int line)
    __attribute__((analyzer_noreturn));
int DbgNotImplementedHandler(char const* expr, char const* file, int line)
    __attribute__((analyzer_noreturn));
#else
int  DbgExitHandler(char const* file, int line, std::string_view text);
int  DbgExitHandler(char const* file, int line, fmt::text_style style, std::string_view text);
int  DbgExitIfHandler(char const* expr, char const* file, int line);
int  DbgNotImplementedHandler(char const* expr, char const* file, int line);
#endif

[[noreturn]] void DbgExit(int status);

} // namespace Common

[[noreturn]] inline void KytyExitHalt() noexcept {
#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS || KYTY_PLATFORM == KYTY_PLATFORM_LINUX
	Common::DbgExit(321);
#else
	std::_Exit(321);
#endif
}

#define EXIT_HALT() (KytyExitHalt(), 1)

#ifndef KYTY_FINAL
#define EXIT_IF(x)                                                                                 \
	do {                                                                                           \
		if (x) {                                                                                   \
			(void)Common::DbgExitIfHandler(#x, __FILE__, __LINE__);                                \
			KytyExitHalt();                                                                        \
		}                                                                                          \
	} while (0)
#else
#define EXIT_IF(x)                                                                                 \
	do {                                                                                           \
		constexpr bool kyty_exit_if_disabled = false && (x);                                       \
		(void)kyty_exit_if_disabled;                                                               \
	} while (0)
#endif

#define EXIT(...)                                                                                  \
	do {                                                                                           \
		(void)Common::DbgExitHandler(__FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__));             \
		KytyExitHalt();                                                                            \
	} while (0)

#define EXIT_COLOR(style, ...)                                                                     \
	do {                                                                                           \
		(void)Common::DbgExitHandler(__FILE__, __LINE__, (style), ::fmt::sprintf(__VA_ARGS__));    \
		KytyExitHalt();                                                                            \
	} while (0)

#define EXIT_NOT_IMPLEMENTED(x)                                                                    \
	do {                                                                                           \
		if (x) {                                                                                   \
			(void)Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__);                        \
			KytyExitHalt();                                                                        \
		}                                                                                          \
	} while (0)
#define KYTY_NOT_IMPLEMENTED EXIT_NOT_IMPLEMENTED(true)

#endif /* KYTY_COMMON_ASSERT_H_ */
