
#ifndef _SEAL_PATH_H_
#define _SEAL_PATH_H_

#include "iters.h"
#include "typetraits.h"

#include <algorithm>
#include <cassert>
#include <string>
#include <unistd.h>


namespace sea {

namespace path {

static bool is_abs(const std::string &p) {
	return !p.empty() && p.front() == '/';
}

static std::string simplify(std::string path) {
	auto next = [&path] (std::string::iterator b) {
		if ( *b == '/' ) ++b;
		return ipair(b, find(b, path.end(), '/'));
	};
	bool abs = is_abs(path);

	auto b = path.begin(), w = path.begin();
	while ( b != path.end() ) {
		auto p = next(b);
		b = p.end();
		if ( p.empty() ) {
		} else if ( p.size() == 1 && p.front() == '.' ) {
		} else if ( p.size() == 2 && p.front() == '.' && p.back() == '.' ) {
			std::string::reverse_iterator r(w);
			r = find(r, path.rend(), '/');
			if ( r != path.rend() ) w = (++r).base();
			else if ( !abs && w == path.begin() ) w = copy(p.begin(), p.end(), w);
			else w = path.begin();
		} else {
			if ( abs || w != path.begin() ) *w++ = '/';
			w = copy(p.begin(), p.end(), w);
		}
	}
	if ( !path.empty() && path.back() == '/' ) *w++ = '/';
	path.erase(w, path.end());
	if ( abs && path.empty() ) path.push_back('/');
	return move(path);
}


template <typename P1, typename P2> static
typename enable_if_decay_same<std::string, P1, P2>::type
join_impl(P1 &&p1, P2 &&p2) {
	if ( p1.empty() || is_abs(p2) ) {
		return std::forward<P2>(p2);
	} else if ( p1.back() != '/' ) {
		return std::forward<P1>(p1) + '/' + std::forward<P2>(p2);
	}
	return std::forward<P1>(p1) + std::forward<P2>(p2);
}


template <typename T, typename U>
constexpr typename std::enable_if<is_decay_same<T, U>::value, T &&>::type
forward_as(typename std::remove_reference<T>::type& t) {
	return static_cast<T&&>(t);
}

template <typename T, typename U>
constexpr typename std::enable_if<!is_decay_same<T, U>::value, U>::type
forward_as(typename std::remove_reference<T>::type& t) {
	return U(t);
}

template <typename P1, typename P2> static
std::string join(P1 &&p1, P2 &&p2) {
	return join_impl(forward_as<P1, std::string>(p1), forward_as<P2, std::string>(p2));
}

template <typename P, typename ... Ps>
static std::string join(P &&p, Ps &&... ps) {
	return join(std::forward<P>(p), join(std::forward<Ps>(ps)...));
}

static std::string current() {
	char buf[1024];
	char *r = getcwd(buf, 1024);
	assert(r != nullptr);
	return r;
}

static std::string make_abs(const std::string &p) {
	return join(current(), p);
}

static std::string name_of(std::string p) {
	auto i = find(p.rbegin(), p.rend(), '/');
	if ( i == p.rend() ) return std::move(p);
	p.erase(p.begin(), i.base());
	return std::move(p);
}

static std::string path_of(std::string p) {
	auto i = find(p.rbegin(), p.rend(), '/');
	if ( i == p.rend() ) return move(p.assign("./"));
	p.erase(i.base(), p.end());
	return std::move(p);
}


}
}

#endif

