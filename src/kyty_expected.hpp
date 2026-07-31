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
#include <stdexcept>

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

	constexpr T& value() & {
		if (!has_value()) throw bad_expected_access<E>(error());
		return std::get<T>(storage_);
	}
	constexpr const T& value() const& {
		if (!has_value()) throw bad_expected_access<E>(error());
		return std::get<T>(storage_);
	}
	constexpr T&& value() && {
		if (!has_value()) throw bad_expected_access<E>(std::move(error()));
		return std::get<T>(std::move(storage_));
	}

	constexpr E& error() & {
		if (has_value()) throw bad_expected_access<E>(E{});
		return std::get<E>(storage_);
	}
	constexpr const E& error() const& {
		if (has_value()) throw bad_expected_access<E>(E{});
		return std::get<E>(storage_);
	}

	template <class U>
	constexpr T value_or(U&& default_value) const& {
		return has_value() ? value() : static_cast<T>(std::forward<U>(default_value));
	}

	template <class U>
	constexpr T value_or(U&& default_value) && {
		return has_value() ? std::move(value()) : static_cast<T>(std::forward<U>(default_value));
	}

	template <class F>
	constexpr auto and_then(F&& f) & -> std::invoke_result_t<F, T&> {
		using Result = std::invoke_result_t<F, T&>;
		return has_value() ? std::forward<F>(f)(value()) : Result(unexpected<E>(error()));
	}

	template <class F>
	constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
		using Result = std::invoke_result_t<F, const T&>;
		return has_value() ? std::forward<F>(f)(value()) : Result(unexpected<E>(error()));
	}

	template <class F>
	constexpr auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
		using Result = std::invoke_result_t<F, T&&>;
		return has_value() ? std::forward<F>(f)(std::move(value())) : Result(unexpected<E>(std::move(error())));
	}

	template <class F>
	constexpr auto transform(F&& f) & -> expected<std::invoke_result_t<F, T&>, E> {
		using Result = std::invoke_result_t<F, T&>;
		return has_value() ? expected<Result, E>(std::forward<F>(f)(value())) : expected<Result, E>(unexpected<E>(error()));
	}

	template <class F>
	constexpr auto transform(F&& f) const& -> expected<std::invoke_result_t<F, const T&>, E> {
		using Result = std::invoke_result_t<F, const T&>;
		return has_value() ? expected<Result, E>(std::forward<F>(f)(value())) : expected<Result, E>(unexpected<E>(error()));
	}

	template <class F>
	constexpr auto transform(F&& f) && -> expected<std::invoke_result_t<F, T&&>, E> {
		using Result = std::invoke_result_t<F, T&&>;
		return has_value() ? expected<Result, E>(std::forward<F>(f)(std::move(value()))) : expected<Result, E>(unexpected<E>(std::move(error())));
	}

	template <class F>
	constexpr auto or_else(F&& f) & -> expected {
		return has_value() ? *this : std::forward<F>(f)(error());
	}

	template <class F>
	constexpr auto or_else(F&& f) const& -> expected {
		return has_value() ? *this : std::forward<F>(f)(error());
	}

	template <class F>
	constexpr auto or_else(F&& f) && -> expected {
		return has_value() ? std::move(*this) : std::forward<F>(f)(std::move(error()));
	}

	constexpr T* operator->() { return &value(); }
	constexpr const T* operator->() const { return &value(); }
	constexpr T& operator*() & { return value(); }
	constexpr const T& operator*() const& { return value(); }
	constexpr T&& operator*() && { return std::move(value()); }

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
	constexpr void value() const {
		if (!ok_) throw bad_expected_access<E>(error_);
	}
	constexpr E& error() & {
		if (ok_) throw bad_expected_access<E>(E{});
		return error_;
	}
	constexpr const E& error() const& {
		if (ok_) throw bad_expected_access<E>(E{});
		return error_;
	}

	template <class F>
	constexpr auto and_then(F&& f) & -> std::invoke_result_t<F> {
		using Result = std::invoke_result_t<F>;
		return ok_ ? std::forward<F>(f)() : Result(unexpected<E>(error_));
	}

	template <class F>
	constexpr auto and_then(F&& f) const& -> std::invoke_result_t<F> {
		using Result = std::invoke_result_t<F>;
		return ok_ ? std::forward<F>(f)() : Result(unexpected<E>(error_));
	}

	template <class F>
	constexpr auto or_else(F&& f) & -> expected {
		return ok_ ? *this : std::forward<F>(f)(error_);
	}

	template <class F>
	constexpr auto or_else(F&& f) const& -> expected {
		return ok_ ? *this : std::forward<F>(f)(error_);
	}

private:
	bool ok_;
	E error_;
};

} // namespace std

#endif
