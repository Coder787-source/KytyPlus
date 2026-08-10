#pragma once

// C++20 polyfill for std::expected / std::unexpected (C++23).
// Scaffolding-only. Prefer real <expected> when the toolchain provides it.

#include <version>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#else

#include <type_traits>
#include <utility>
#include <variant>

namespace std {

template <class E>
class unexpected {
public:
	constexpr explicit unexpected(const E& e) : error_(e) {}
	constexpr explicit unexpected(E&& e) : error_(std::move(e)) {}

	constexpr const E& error() const& noexcept { return error_; }
	constexpr E& error() & noexcept { return error_; }
	constexpr const E&& error() const&& noexcept { return std::move(error_); }
	constexpr E&& error() && noexcept { return std::move(error_); }

private:
	E error_;
};

template <class E>
unexpected(E) -> unexpected<E>;

template <class T, class E>
class expected {
public:
	using value_type = T;
	using error_type = E;

	constexpr expected(const T& v) : storage_(v) {}
	constexpr expected(T&& v) : storage_(std::move(v)) {}
	constexpr expected(const unexpected<E>& u) : storage_(u.error()) {}
	constexpr expected(unexpected<E>&& u) : storage_(std::move(u).error()) {}

	template <class U, class = std::enable_if_t<std::is_constructible_v<E, const U&>>>
	constexpr expected(const unexpected<U>& u) : storage_(E(u.error())) {}
	template <class U, class = std::enable_if_t<std::is_constructible_v<E, U>>>
	constexpr expected(unexpected<U>&& u) : storage_(E(std::move(u).error())) {}

	constexpr bool has_value() const noexcept { return std::holds_alternative<T>(storage_); }
	constexpr explicit operator bool() const noexcept { return has_value(); }

	constexpr T& value() & { return std::get<T>(storage_); }
	constexpr const T& value() const& { return std::get<T>(storage_); }
	constexpr T&& value() && { return std::get<T>(std::move(storage_)); }

	constexpr E& error() & { return std::get<E>(storage_); }
	constexpr const E& error() const& { return std::get<E>(storage_); }

	constexpr T* operator->() { return &value(); }
	constexpr const T* operator->() const { return &value(); }
	constexpr T& operator*() & { return value(); }
	constexpr const T& operator*() const& { return value(); }

private:
	std::variant<T, E> storage_;
};

template <class E>
class expected<void, E> {
public:
	using value_type = void;
	using error_type = E;

	constexpr expected() : ok_(true), error_() {}
	constexpr expected(const unexpected<E>& u) : ok_(false), error_(u.error()) {}
	constexpr expected(unexpected<E>&& u) : ok_(false), error_(std::move(u).error()) {}

	template <class U, class = std::enable_if_t<std::is_constructible_v<E, const U&>>>
	constexpr expected(const unexpected<U>& u) : ok_(false), error_(E(u.error())) {}
	template <class U, class = std::enable_if_t<std::is_constructible_v<E, U>>>
	constexpr expected(unexpected<U>&& u) : ok_(false), error_(E(std::move(u).error())) {}

	constexpr bool has_value() const noexcept { return ok_; }
	constexpr explicit operator bool() const noexcept { return ok_; }
	constexpr void value() const {}
	constexpr E& error() & { return error_; }
	constexpr const E& error() const& { return error_; }

private:
	bool ok_;
	E error_;
};

} // namespace std

#endif
