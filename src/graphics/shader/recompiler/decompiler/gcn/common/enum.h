// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
//
// Compatibility shim: faithful port of shadPS4's common/enum.h, providing
// DECLARE_ENUM_FLAG_OPERATORS and Common::Flags<>. Isolated to the gcn/ include
// root so it does not collide with Kyty's own headers.
#pragma once

#include <type_traits>
#include "types.h"

#define DECLARE_ENUM_FLAG_OPERATORS(type)                                                          \
	[[nodiscard]] constexpr type operator|(type a, type b) noexcept {                              \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(static_cast<T>(a) | static_cast<T>(b));                            \
	}                                                                                              \
	[[nodiscard]] constexpr type operator&(type a, type b) noexcept {                              \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(static_cast<T>(a) & static_cast<T>(b));                            \
	}                                                                                              \
	[[nodiscard]] constexpr type operator^(type a, type b) noexcept {                              \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(static_cast<T>(a) ^ static_cast<T>(b));                            \
	}                                                                                              \
	[[nodiscard]] constexpr type operator<<(type a, type b) noexcept {                             \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(static_cast<T>(a) << static_cast<T>(b));                            \
	}                                                                                              \
	[[nodiscard]] constexpr type operator>>(type a, type b) noexcept {                             \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(static_cast<T>(a) >> static_cast<T>(b));                            \
	}                                                                                              \
	constexpr type& operator|=(type& a, type b) noexcept {                                         \
		a = a | b;                                                                                  \
		return a;                                                                                  \
	}                                                                                              \
	constexpr type& operator&=(type& a, type b) noexcept {                                        \
		a = a & b;                                                                                 \
		return a;                                                                                  \
	}                                                                                              \
	constexpr type& operator^=(type& a, type b) noexcept {                                        \
		a = a ^ b;                                                                                 \
		return a;                                                                                  \
	}                                                                                              \
	constexpr type& operator<<=(type& a, type b) noexcept {                                       \
		a = a << b;                                                                                \
		return a;                                                                                  \
	}                                                                                              \
	constexpr type& operator>>=(type& a, type b) noexcept {                                       \
		a = a >> b;                                                                                \
		return a;                                                                                  \
	}                                                                                              \
	[[nodiscard]] constexpr type operator~(type key) noexcept {                                    \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<type>(~static_cast<T>(key));                                             \
	}                                                                                              \
	[[nodiscard]] constexpr bool True(type key) noexcept {                                         \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<T>(key) != 0;                                                            \
	}                                                                                              \
	[[nodiscard]] constexpr bool False(type key) noexcept {                                       \
		using T = std::underlying_type_t<type>;                                                    \
		return static_cast<T>(key) == 0;                                                            \
	}

namespace Common {

template <typename Enum>
class Flags {
public:
	using IntType = std::underlying_type_t<Enum>;

	Flags() {}
	Flags(IntType t) : m_bits(t) {}

	template <typename T, typename... Tx>
	Flags(T f, Tx... fx) {
		this->set(f, fx...);
	}

	template <typename T, typename... Tx>
	void set(T f, Tx... fx) {
		m_bits |= bits(f, fx...);
	}
	void set(Flags flags) {
		m_bits |= flags.m_bits;
	}
	template <typename T, typename... Tx>
	void clr(T f, Tx... fx) {
		m_bits &= ~bits(f, fx...);
	}
	void clr(Flags flags) {
		m_bits &= ~flags.m_bits;
	}
	template <typename T, typename... Tx>
	bool any(T f, Tx... fx) const {
		return (m_bits & bits(f, fx...)) != 0;
	}
	template <typename T, typename... Tx>
	bool all(T f, Tx... fx) const {
		const IntType mask = bits(f, fx...);
		return (m_bits & mask) == mask;
	}
	bool test(Enum f) const {
		return this->any(f);
	}
	bool isClear() const {
		return m_bits == 0;
	}
	void clrAll() {
		m_bits = 0;
	}
	u32 raw() const {
		return m_bits;
	}
	Flags operator&(const Flags& other) const {
		return Flags(m_bits & other.m_bits);
	}
	Flags operator|(const Flags& other) const {
		return Flags(m_bits | other.m_bits);
	}
	Flags operator^(const Flags& other) const {
		return Flags(m_bits ^ other.m_bits);
	}
	bool operator==(const Flags& other) const {
		return m_bits == other.m_bits;
	}
	bool operator!=(const Flags& other) const {
		return m_bits != other.m_bits;
	}

private:
	IntType m_bits = 0;

	static IntType bit(Enum f) {
		return IntType(1) << static_cast<IntType>(f);
	}
	template <typename T, typename... Tx>
	static IntType bits(T f, Tx... fx) {
		return bit(f) | bits(fx...);
	}
	static IntType bits() {
		return 0;
	}
};

} // namespace Common