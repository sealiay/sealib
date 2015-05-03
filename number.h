
#ifndef __SEAL_NUMBER_H__
#define __SEAL_NUMBER_H__

#include "dimension.h"
#include "typetraits.h"
#include "macro.h"

#include <cmath>
#include <cstdlib>


namespace sea {

class writer;

template <typename __T, typename __D = dimensions::unit, typename __R = std::integral_constant<int, 0>, typename __G = empty> class number;


template <typename>
struct is_number : public std::false_type {};
template <typename T, typename D, typename R, typename G>
struct is_number<number<T, D, R, G>> : public std::true_type {
	static constexpr int enable = 0;
};


namespace number_ops {

struct level1 {};
struct level2 : level1 {};
struct level3 : level2 {};

template <typename G> struct tag_result {
	typedef G type;
	static constexpr int enable = 0;
};
template <> struct tag_result<void> {};


#define macro_def_tag_operate(xx)						\
	template <typename G1, typename G2>					\
	tag_result<void> xx##_impl(G1, G2, level1);			\
	template <typename G>								\
	tag_result<G>    xx##_impl(G, G, level2);			\
	template <typename G1, typename G2>					\
	decltype(tag_##xx(G1(), G2()))						\
	xx##_impl(G1, G2, level3);							\
	template <typename G1, typename G2>					\
	using xx = decltype(xx##_impl(G1(), G2(), level3()));

	macro_def_tag_operate(add)
	macro_def_tag_operate(sub)
	macro_def_tag_operate(mul)
	macro_def_tag_operate(div)
	macro_def_tag_operate(mod)
	macro_def_tag_operate(iadd)
	macro_def_tag_operate(isub)
	macro_def_tag_operate(imul)
	macro_def_tag_operate(idiv)
	macro_def_tag_operate(imod)
	macro_def_tag_operate(assign)
	macro_def_tag_operate(compare)

#undef macro_def_tag_operate


template <typename G>
static constexpr double do_mod_impl(double u, double v, level2) {
	return fmod(u, v);
}

#define macro_def_do_operate(xx, o, r)						\
	template <typename G, typename T>						\
	static constexpr r do_##xx##_impl(T u, T v, level1) {	\
		return u o v;										\
	}														\
	template <typename G, typename T>						\
	static constexpr auto do_##xx##_impl(T u, T v, level3)	\
	-> decltype(G().do_##xx(u, v)) {						\
		return G().do_##xx(u, v);							\
	}														\
	template <typename G, typename T>						\
	static constexpr r do_##xx(T u, T v) {					\
		return do_##xx##_impl<G>(u, v, level3());			\
	}

macro_def_do_operate(lt, < , bool)
macro_def_do_operate(eq, ==, bool)
macro_def_do_operate(le, <=, bool)

macro_def_do_operate(add, +, T)
macro_def_do_operate(sub, -, T)
macro_def_do_operate(mul, *, T)
macro_def_do_operate(div, /, T)
macro_def_do_operate(mod, %, T)

#undef macro_def_do_operate


template <typename T, typename U>
using common = typename std::conditional<std::is_integral<typename std::common_type<T, U>::type>::value, long long, double>::type;


template <typename T, typename F>
static constexpr T cast(F v) {
	return std::is_floating_point<F>::value && !std::is_floating_point<T>::value ? (T)round(v) : (T)v;
}


template <typename T, int R>
struct mul10 {
	static constexpr T value = std::conditional<(R < 0), mul10<T, -R>, typename std::conditional<(R > 0), mul10<T, R-1>, std::integral_constant<int, 1>>::type>::type::value * (R > 0 ? 10 : 1);
	static constexpr T calc(T v) {
		return R > 0 ? v * value : v / value;
	}
};

template <typename T, typename N>
struct aser {
	constexpr T operator()(N n) const { return cast<T>(n.val()); }
};

template <typename T, typename D, typename R, typename G, typename N>
struct aser<number<T, D, R, G>, N> {
	constexpr number<T, D, R, G> operator()(N n) const {
		typedef common<typename N::value_type, T> ct;
		typedef std::integral_constant<int, N::ratio_type::value - R::value> rt;
		return number<T, D, R, G>(cast<T>(mul10<ct, rt::value>::calc((ct)n.val())));
	}
};

template <typename T, typename D, typename G, typename N>
struct aser<number<T, D, typename N::ratio_type, G>, N> {
	constexpr number<T, D, typename N::ratio_type, G> operator()(N n) const {
		return number<T, D, typename N::ratio_type, G>(cast<T>(n.val()));
	}
};

}


template <typename __T, typename __D, typename __R, typename __G>
class number {
public:
	typedef number type;
	typedef __T value_type;
	typedef __D dimension_type;
	typedef __R ratio_type;
	typedef __G tag_type;

private:
	value_type _v;

	template <typename G>
	using friend_t = number<value_type, dimension_type, ratio_type, G>;

public:
	// ctor and assign
	number() = default;
	explicit constexpr number(value_type v): _v(v) {}
	constexpr number(const number &) = default;

	template <typename G, int = number_ops::assign<G, tag_type>::enable>
	constexpr number(friend_t<G> n): _v(n.val()) {}

	template <typename G, int = number_ops::assign<G, tag_type>::enable>
	number &operator=(friend_t<G> n) {
		_v = n.val();
		return *this;
	}

	// compare
	constexpr bool operator<(number n) const {
		return number_ops::do_lt<tag_type>(val(), n.val());
	}
	constexpr bool operator==(number n) const {
		return number_ops::do_eq<tag_type>(val(), n.val());
	}
	constexpr bool operator<=(number n) const {
		return number_ops::do_le<tag_type>(val(), n.val());
	}

	constexpr bool operator> (number n) const { return n < *this; }
	constexpr bool operator!=(number n) const { return !(n == *this); }
	constexpr bool operator>=(number n) const { return n <= *this; }


	// add
	template <typename G>
	constexpr friend_t<typename number_ops::add<tag_type, G>::type>
	operator+(friend_t<G> n) const {
		typedef typename number_ops::add<tag_type, G>::type gt;
		return friend_t<gt>(number_ops::do_add<gt>(val(), n.val()));
	}

	template <typename G, int = number_ops::iadd<tag_type, G>::enable>
	number &operator+=(friend_t<G> n) {
		_v = number_ops::do_add<tag_type>(val(), n.val());
		return *this;
	}

	number &operator++() { ++_v; return *this; }
	number operator++(int) { return number(_v++); }
	constexpr number operator+() const { return number(+_v); }


	// sub
	template <typename G>
	constexpr friend_t<typename number_ops::sub<tag_type, G>::type>
	operator-(friend_t<G> n) const {
		typedef typename number_ops::sub<tag_type, G>::type gt;
		return friend_t<gt>(number_ops::do_sub<gt>(val(), n.val()));
	}

	template <typename G, int = number_ops::isub<tag_type, G>::enable>
	number &operator-=(friend_t<G> n) {
		_v = number_ops::do_sub<tag_type>(val(), n.val());
		return *this;
	}

	number &operator--() { --_v; return *this; }
	number operator--(int) { return number(_v--); }
	constexpr number operator-() const { return number(-_v); }


	// mul
	template <typename D, typename R, typename G>
	constexpr number<value_type,
			typename dimensions::mul<dimension_type, D>::type,
			std::integral_constant<int, ratio_type::value + R::value>,
			typename number_ops::mul<tag_type, G>::type>
	operator*(number<value_type, D, R, G> n) const {
		typedef typename dimensions::mul<dimension_type, D>::type dt;
		typedef std::integral_constant<int, ratio_type::value + R::value> rt;
		typedef typename number_ops::mul<tag_type, G>::type gt;
		return number<value_type, dt, rt, gt>(number_ops::do_mul<gt>(val(), n.val()));
	}


	template <typename R, typename G, int = number_ops::imul<tag_type, G>::enable>
	number &operator*=(number<value_type, dimensions::unit, R, G> n) {
		typedef std::integral_constant<int, ratio_type::value + R::value> rt;
		typedef number<value_type, dimension_type, rt, tag_type> nt;
		_v = ((number)nt(number_ops::do_mul<tag_type>(val(), n.val()))).val();
		return *this;
	}

	template <typename N, typename = typename std::enable_if<std::is_arithmetic<N>::value>::type>
	constexpr number operator*(N n) const {
		typedef number_ops::common<value_type, N> ct;
		return number(number_ops::cast<value_type>(number_ops::do_mul<tag_type>((ct)val(), (ct)n)));
	}

	template <typename N, typename = typename std::enable_if<std::is_arithmetic<N>::value>::type>
	number &operator*=(N n) { return *this = *this * n; }


	// div
	template <typename D, typename R, typename G>
	constexpr number<value_type,
			typename dimensions::div<dimension_type, D>::type,
			std::integral_constant<int, ratio_type::value - R::value>,
			typename number_ops::div<tag_type, G>::type>
	operator/(number<value_type, D, R, G> n) const {
		typedef typename dimensions::div<dimension_type, D>::type dt;
		typedef std::integral_constant<int, ratio_type::value - R::value> rt;
		typedef typename number_ops::div<tag_type, G>::type gt;
		return number<value_type, dt, rt, gt>(number_ops::do_div<gt>(val(), n.val()));
	}


	template <typename R, typename G, int = number_ops::idiv<tag_type, G>::enable>
	number &operator/=(number<value_type, dimensions::unit, R, G> n) {
		typedef std::integral_constant<int, ratio_type::value - R::value> rt;
		typedef number<value_type, dimension_type, rt, tag_type> nt;
		_v = ((number)nt(number_ops::do_div<tag_type>(val(), n.val()))).val();
		return *this;
	}

	template <typename N, typename = typename std::enable_if<std::is_arithmetic<N>::value>::type>
	constexpr number operator/(N n) const {
		typedef number_ops::common<value_type, N> ct;
		return number(number_ops::cast<value_type>(number_ops::do_div<tag_type>((ct)val(), (ct)n)));
	}

	template <typename N, typename = typename std::enable_if<std::is_arithmetic<N>::value>::type>
	number &operator/=(N n) { return *this = *this / n; }


	//mod
	template <typename G>
	constexpr friend_t<typename number_ops::mod<tag_type, G>::type>
	operator%(friend_t<G> n) const {
		typedef typename number_ops::mod<tag_type, G>::type gt;
		return friend_t<gt>(number_ops::do_mod<gt>(val(), n.val()));
	}

	template <typename G, int = number_ops::imod<tag_type, G>::enable>
	number &operator%=(friend_t<G> n) {
		_v = number_ops::do_mod<tag_type>(val(), n.val());
		return *this;
	}


	// convert
	template <typename N>
	constexpr N as() const { return number_ops::aser<N, number>()(*this); }
	template <typename N>
	constexpr explicit operator N() const { return as<N>(); }

	constexpr value_type val() const { return _v; }

	void write_to(writer &) const;
};

template <typename N, int = is_number<N>::enable>
static constexpr int sign(N n) {
	return n.val() > typename N::value_type(0) ? +1 : n.val() < typename N::value_type(0) ? -1 : 0;
}

template <typename N, int = is_number<N>::enable>
static constexpr N abs(N n) {
	using std::abs;
	return N(abs(n.val()));
}

namespace number_unit_test {

typedef number<int, dimensions::unit, std::integral_constant<int, 2>> n2;
typedef number<int, dimensions::unit, std::integral_constant<int, 0>> n0;

static_assert(n2(2) + n2(3) == n2(5), "");
static_assert(n2(3) - n2(2) == n2(1), "");
static_assert(n2(2) * n0(3) == n2(6), "");
static_assert(n2(6) / n0(3) == n2(2), "");
static_assert(n0(200).as<n2>() == n2(2), "");

}


}

#endif

