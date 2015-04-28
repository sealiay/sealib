
#ifndef __SEAL_IO_WRITER_H__
#define __SEAL_IO_WRITER_H__

#include "macro.h"

#include <algorithm>
#include <functional>
#include <ostream>
#include <string>
#include <vector>

#include <cstdarg>
#include <cstdio>
#include <cstring>


namespace sea {

class writer;

seal_macro_def_has_elem(write_to);


class writer {
public:
	writer() = default;
	virtual ~writer() = default;

	writer &write(bool b) { return write(b ? "true" : "false"); }

	writer &write(short i) { return format("%d", i); }
	writer &write(int i) { return format("%d", i); }
	writer &write(long i) { return format("%ld", i); }
	writer &write(long long i) { return format("%lld", i); }

	writer &write(unsigned short i) { return format("%u", i); }
	writer &write(unsigned int i) { return format("%u", i); }
	writer &write(unsigned long i) { return format("%lu", i); }
	writer &write(unsigned long long i) { return format("%llu", i); }

	writer &write(float f) { return format("%g", f); }
	writer &write(double f) { return format("%g", f); }
	writer &write(long double f) { return format("%Lg", f); }

	writer &write(const void *p) { return format("%p", p); }

	writer &write(const char *s) { return write(s, strlen(s)); }
	writer &write(const std::string &s) { return write(s.data(), s.size()); }

	template <typename T, typename = typename enable_if_has_write_to<T, void (T::*)(writer &) const>::type>
	writer &write(const T &o) {
		o.write_to(*this);
		return *this;
	}

	writer &b() { return write(' '); }
	writer &c() { return write(',').b(); }

	void nl() {
		write('\n');
		flush();
	}


	template <typename T>
	writer &operator()(const T &o) { return write(o); }

	writer &operator()(const char *f, ...)
		__attribute__ ((format(printf, 2, 3))) {
		va_list p;
		va_start(p, f);
		vformat(f, p);
		va_end(p);
		return *this;
	}
	writer &format(const char *f, ...)
		__attribute__ ((format(printf, 2, 3))) {
		va_list p;
		va_start(p, f);
		vformat(f, p);
		va_end(p);
		return *this;
	}


	virtual writer &write(char c) = 0;
	virtual writer &write(const void *, size_t) = 0;
	virtual writer &vformat(const char *, va_list) = 0;
	virtual writer &flush() = 0;

	size_t vformat_size(const char *f, va_list p) const {
		va_list q;
		va_copy(q, p);
		int n = vsnprintf(nullptr, 0, f, q);
		va_end(q);
		return (size_t)std::max(n, 0);
	}

	writer &vformat_base_impl(const char *f, va_list p) {
		size_t l = vformat_size(f, p);
		if ( l != 0 ) {
			static constexpr size_t BUF = 1024;
			if ( l < BUF ) {
				char buf[BUF];
				vsprintf(buf, f, p);
				write(buf, l);
			} else {
				char *buf = new char [l+1];
				vsprintf(buf, f, p);
				write(buf, l);
				delete [] buf;
			}
		}
		return *this;
	}

	seal_macro_only_move(writer)
};


class file_writer : public writer {
private:
	FILE *_file;
public:
	file_writer(FILE *f): _file(f) {}
	~file_writer() noexcept { file_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		fputc(c, _file);
		return *this;
	}
	writer &write(const void *p, size_t n) override {
		fwrite(p, 1, n, _file);
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		vfprintf(_file, f, p);
		return *this;
	}
	writer &flush() override {
		fflush(_file);
		return *this;
	}

	FILE *file() const { return _file; }
	file_writer &set_file(FILE *f) { _file = f; return *this; }
};


class stream_writer : public writer {
private:
	std::ostream &_os;
public:
	stream_writer(std::ostream &s): _os(s) {}
	~stream_writer() noexcept { stream_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		_os.put(c);
		return *this;
	}
	writer &write(const void *p, size_t n) override {
		_os.write((const char *)p, n);
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		return vformat_base_impl(f, p);
	}
	writer &flush() override {
		_os.flush();
		return *this;
	}
};


class string_writer : public writer {
private:
	std::string &_s;
public:
	string_writer(std::string &s): _s(s) {}
	~string_writer() noexcept { string_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		_s.push_back(c);
		return *this;
	}
	writer &write(const void *p, size_t n) override {
		_s.append((const char *)p, n);
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		size_t l = vformat_size(f, p) + 1;
		if ( l != 0 ) {
			_s.append(l, '#');
			char *buf = &_s.front() + _s.size() - l;
			vsprintf(buf, f, p);
			_s.pop_back();
		}
		return *this;
	}
	writer &flush() override { return *this; }
};


class array_writer : public writer {
private:
	char *_array;
public:
	array_writer(char *a): _array(a) {}
	~array_writer() noexcept { array_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		*_array++ = c;
		*_array = '\0';
		return *this;
	}
	writer &write(const void *p, size_t n) override {
		memcpy(_array, p, n);
		_array += n;
		*_array = '\0';
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		int n = vsprintf(_array, f, p);
		_array += std::max(n, 0);
		return *this;
	}
	writer &flush() override { return *this; }
};


template <typename __Insertor>
class insertor_writer : public writer {
private:
	__Insertor _bi;
public:
	insertor_writer(__Insertor bi): _bi(bi) {}
	~insertor_writer() noexcept { insertor_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		*_bi++ = c;
		return *this;
	}
	writer &write(const void *p, size_t n) override {
		const char *s = (const char *)p;
		_bi = std::copy(s, s + n, _bi);
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		return vformat_base_impl(f, p);
	}
	writer &flush() override { return *this; }
};


class multi_writer : public writer {
private:
	std::vector<std::reference_wrapper<writer>> _writers;

public:
	template <typename ... Ws>
	multi_writer(Ws &...ws): _writers{ws...} {}
	~multi_writer() noexcept { multi_writer::flush(); }
	using writer::write;
	writer &write(char c) override {
		for (writer &w : _writers) {
			w.write(c);
		}
		return *this;
	}
	writer &write(const void *s, size_t n) override {
		for (writer &w : _writers) {
			w.write(s, n);
		}
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		for (writer &w : _writers) {
			va_list c;
			va_copy(c, p);
			w.vformat(f, c);
			va_end(c);
		}
		return *this;
	}
	writer &flush() override {
		for (writer &w : _writers) {
			w.flush();
		}
		return *this;
	}

	const std::vector<std::reference_wrapper<writer>> &writers() const { return _writers; }
	std::vector<std::reference_wrapper<writer>> &writers() { return _writers; }
};


class empty_writer : public writer {
public:
	using writer::write;
	writer &write(char) override { return *this; }
	writer &write(const void *, size_t) override { return *this; }
	writer &vformat(const char *, va_list) override { return *this; }
	writer &flush() override { return *this; }
};


class count_writer : public writer {
private:
	size_t _cnt = 0;

public:
	using writer::write;
	writer &write(char) override {
		++_cnt;
		return *this;
	}
	writer &write(const void *, size_t n) override {
		_cnt += n;
		return *this;
	}
	writer &vformat(const char *f, va_list p) override {
		_cnt += vformat_size(f, p);
		return *this;
	}
	writer &flush() override { return *this; }

	size_t count() const { return _cnt; }
	void clear() { _cnt = 0; }
};

}

#endif

