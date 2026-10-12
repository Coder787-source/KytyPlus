#ifndef LAUNCHER_INCLUDE_COMMON_H_
#define LAUNCHER_INCLUDE_COMMON_H_

// IWYU pragma: begin_exports
#include <QArgument>
#include <QObject>
#include <QString>

#include <cinttypes>
#include <cstdint>
#include <cstdio>
// IWYU pragma: end_exports

inline bool IsGameImagePath(const QString& path) {
	return path.endsWith(QStringLiteral(".ffpfsc"), Qt::CaseInsensitive) ||
	       path.endsWith(QStringLiteral(".exfat"), Qt::CaseInsensitive);
}

#define KYTY_QT_CLASS_NO_COPY(name)                                                                \
public:                                                                                            \
	name(const name&)                = delete; /* NOLINT(bugprone-macro-parentheses) */            \
	name& operator=(const name&)     = delete; /* NOLINT(bugprone-macro-parentheses) */            \
	name(name&&) noexcept            = delete; /* NOLINT(bugprone-macro-parentheses) */            \
	name& operator=(name&&) noexcept = delete; /* NOLINT(bugprone-macro-parentheses) */

#endif /* LAUNCHER_INCLUDE_COMMON_H_ */
