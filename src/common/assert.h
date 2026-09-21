#ifndef KYTY_COMMON_ASSERT_H_
#define KYTY_COMMON_ASSERT_H_

#include "common/common.h"
#include "common/emulatorConfig.h"
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
int DbgNotImplementedHandler(char const* expr, char const* file, int line, std::string_view msg)
    __attribute__((analyzer_noreturn));
int DbgSoftNotImplementedHandler(char const* expr, char const* file, int line, std::string_view msg);
int DbgSoftExitHandler(char const* file, int line, std::string_view text);
void DbgExit(int status) __attribute__((analyzer_noreturn));
#else
int  DbgExitHandler(char const* file, int line, std::string_view text);
int  DbgExitHandler(char const* file, int line, fmt::text_style style, std::string_view text);
int  DbgExitIfHandler(char const* expr, char const* file, int line);
int  DbgNotImplementedHandler(char const* expr, char const* file, int line);
int  DbgNotImplementedHandler(char const* expr, char const* file, int line, std::string_view msg);
int  DbgSoftNotImplementedHandler(char const* expr, char const* file, int line, std::string_view msg);
int  DbgSoftExitHandler(char const* file, int line, std::string_view text);
void DbgExit(int status);
#endif

} // namespace Common

#if KYTY_PLATFORM == KYTY_PLATFORM_WINDOWS || KYTY_PLATFORM == KYTY_PLATFORM_LINUX
#define EXIT_HALT() (Common::DbgExit(321), 1)
#else
#define EXIT_HALT() (std::_Exit(321), 1)
#endif

#ifndef KYTY_FINAL
#define EXIT_IF(x)                                                                                 \
	((void)((x) && Common::DbgExitIfHandler(#x, __FILE__, __LINE__) != 0 && (EXIT_HALT(), 1) != 0))
#else
#define EXIT_IF(x)                                                                                 \
	do {                                                                                           \
		constexpr bool kyty_exit_if_disabled = false && (x);                                       \
		(void)kyty_exit_if_disabled;                                                               \
	} while (0)
#endif

#define EXIT(...)                                                                                  \
	do {                                                                                           \
		((void)(Common::DbgExitHandler(__FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__)) &&         \
		        (EXIT_HALT(), 1)));                                                                \
	} while (0)

#define EXIT_COLOR(style, ...)                                                                     \
	do {                                                                                           \
		((void)(Common::DbgExitHandler(__FILE__, __LINE__, (style),                                \
		                               ::fmt::sprintf(__VA_ARGS__)) &&                             \
		        (EXIT_HALT(), 1)));                                                                \
	} while (0)

#define EXIT_NOT_IMPLEMENTED(x)                                                                    \
	((void)((x) && Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__) != 0 &&                \
	        (EXIT_HALT(), 1) != 0))
#define KYTY_NOT_IMPLEMENTED EXIT_NOT_IMPLEMENTED(true)

// Like EXIT_NOT_IMPLEMENTED, but with a human-readable description of what is missing,
// so tester logs name the unimplemented path (opcode / register / syscall) instead of
// dumping the raw guard condition. The original condition is still printed for context.
#define EXIT_NOT_IMPLEMENTED_MSG(x, ...)                                                          \
	((void)((x) &&                                                                                \
	        Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__)) != 0 && \
	        (EXIT_HALT(), 1) != 0))
#define KYTY_NOT_IMPLEMENTED_MSG(...) EXIT_NOT_IMPLEMENTED_MSG(true, __VA_ARGS__)

// --- Soft (non-fatal) unimplemented-path guards -------------------------------
// Unlike EXIT_NOT_IMPLEMENTED*/EXIT, these do NOT terminate the process in the
// default build. They log a one-line diagnostic and return control to the caller
// so rendering can continue past a graphics gap with (possibly incorrect) output.
// Enabled strict mode (--strict-unimplemented) restores fail-fast behaviour by
// routing the soft handler through the fatal EXIT_HALT path.
//
// A soft guard used as a statement must be written as SOFT_NOT_IMPLEMENTED(cond);
// a soft guard used where the caller must bail early can use the boolean form
// (returns true when it fired), e.g. `if (SOFT_NOT_IMPLEMENTED(cond)) { return ...; }`.
#define SOFT_NOT_IMPLEMENTED(x)                                                                    \
	((void)((x) && (Config::UnimplementedStrictMode()                                              \
	                        ? (Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__) != 0 &&   \
	                           (EXIT_HALT(), 1) != 0)                                              \
	                        : (Common::DbgSoftNotImplementedHandler(#x, __FILE__, __LINE__, "") != 0))))
#define SOFT_NOT_IMPLEMENTED_MSG(x, ...)                                                           \
	((void)((x) && (Config::UnimplementedStrictMode()                                              \
	                        ? (Common::DbgNotImplementedHandler(#x, __FILE__, __LINE__,            \
	                                                            ::fmt::sprintf(__VA_ARGS__)) != 0  \
	                           && (EXIT_HALT(), 1) != 0)                                           \
	                        : (Common::DbgSoftNotImplementedHandler(#x, __FILE__, __LINE__,        \
	                                                                 ::fmt::sprintf(__VA_ARGS__)) != 0))))
#define KYTY_SOFT_NOT_IMPLEMENTED      SOFT_NOT_IMPLEMENTED(true)
#define KYTY_SOFT_NOT_IMPLEMENTED_MSG(...) SOFT_NOT_IMPLEMENTED_MSG(true, __VA_ARGS__)

// Soft hard-EXIT equivalent: logs the formatted message and continues instead of
// aborting. Strict mode restores the abort.
#define SOFT_EXIT(...)                                                                             \
	do {                                                                                           \
		if (Config::UnimplementedStrictMode()) {                                                    \
			((void)(Common::DbgExitHandler(__FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__)) &&      \
			        (EXIT_HALT(), 1)));                                                               \
		} else {                                                                                   \
			Common::DbgSoftExitHandler(__FILE__, __LINE__, ::fmt::sprintf(__VA_ARGS__));            \
		}                                                                                          \
	} while (0)

#endif /* KYTY_COMMON_ASSERT_H_ */
