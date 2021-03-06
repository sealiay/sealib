
#ifndef _SEAL_THREADS_H_
#define _SEAL_THREADS_H_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>


namespace sea {


class spin_lock {
private:
	std::atomic<bool> _a = {false};

public:
	void lock() { while ( _a.exchange(true, std::memory_order_acq_rel) ); }
	void unlock() { _a.store(false, std::memory_order_release); }
	bool try_lock() { return !_a.exchange(true, std::memory_order_acq_rel); }
	bool locked() const { return _a.load(std::memory_order_acquire); }
};


class thread_pool {
private:
	std::vector<std::thread> _threads;
	enum class command {wait, run, stop};
	std::atomic<command> _cmd;
	std::atomic<int> _busy;

	std::mutex _mutex;
	std::condition_variable _cvar;

	const std::function<void (int)> *_func;
	std::atomic<int> _currj;
	int _totalj;

public:
	thread_pool(int n) { extend_by(n > 0 ? n - 1 : 0); }

	~thread_pool() noexcept { stop(); }

	void extend_by(int n) {
		_busy = n + (int)_threads.size();
		_cmd = command::wait;
		_threads.reserve(n + _threads.size());
		while ( n-- > 0 ) {
			_threads.emplace_back(loop_wrapper, this);
		}
		wait_free();
	}

	void run_njob(int n, const std::function<void (int)> &f) {
		_func = &f;
		_currj = 0;
		_totalj = n;

		if ( !_threads.empty() ) {
			_cmd = command::run;
			notify();
		}
		do_run();

		wait_free();
	}

	void run_njob(int n, const std::function<void (int)> &&f) {
		run_njob(n, f);
	}
	void run(const std::function<void (int)> &f) {
		run_njob((int)_threads.size(), f);
	}
	void run(const std::function<void (int)> &&f) {
		run(f);
	}

	void stop() {
		_cmd = command::stop;
		notify();
		for (std::thread &t : _threads) {
			t.join();
		}
		_threads.clear();
		_busy = 0;
		_cmd = command::wait;
	}

	thread_pool(const thread_pool &) = delete;
	thread_pool &operator=(const thread_pool &) = delete;

	static void temporary_run(size_t nth, std::function<void (int)> &f) {
		if ( nth <= 1 ) {
			f(0);
			return;
		}
		std::vector<std::thread> ts;
		ts.reserve(nth);
		int i = 0;
		while ( nth-- > 0 ) {
			ts.emplace_back([&f] (int i) { f(i); }, i++);
		}
		for (std::thread &t : ts) {
			t.join();
		}
	}
	static void temporary_run(size_t nth, std::function<void (int)> &&f) {
		temporary_run(nth, f);
	}

private:
	static void loop_wrapper(thread_pool *p) { p->loop(); }

	void loop() {
		while ( _cmd != command::stop ) {
			if ( _cmd == command::wait ) {
				do_wait();
			} else if ( _cmd == command::run ) {
				do_run();
			}
		}
	}

	void do_wait() {
		if ( --_busy == 0 ) {
			notify();
		}
		std::unique_lock<std::mutex> l{_mutex};
		while ( _cmd == command::wait ) {
			_cvar.wait(l);
		}
		++_busy;
	}

	void do_run() {
		int j;
		while ( (j = _currj++) < _totalj ) {
			(*_func)(j);
		}
		_cmd = command::wait;
	}

	void wait_free() {
		if ( _busy > 0 ) {
			std::unique_lock<std::mutex> l{_mutex};
			while ( _busy > 0 ) {
				_cvar.wait(l);
			}
		}
	}

	void notify() {
		_mutex.lock();
		_mutex.unlock();
		_cvar.notify_all();
	}
};

}

#endif // _SEAL_THREADS_H_


