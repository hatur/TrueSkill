#pragma once

#include <iostream>
#include <functional>
#include <vector>
#include <deque>
#include <queue>
#include <condition_variable>
#include <future>
#include <atomic>
#include <memory>

namespace ts {

class worker_thread {
public:
	worker_thread() = delete;
	
	template<typename T>
	explicit worker_thread(T&& t);

	~worker_thread() = default;

	void join();

private:
	std::thread m_thread {nullptr};
	unsigned int m_threadnumber;
};

template<typename T>
inline worker_thread::worker_thread(T&& t)
	: m_thread{std::forward<T>(t)} {

	static int threadnumber_count = 1;
	m_threadnumber = threadnumber_count;
	++threadnumber_count;
}

inline void worker_thread::join() {
	m_thread.join();
}

/**
* A standard thread pool for n-cores
*/
class thread_pool
{
public:
	thread_pool();
	~thread_pool();

	// non-copy, non-movable
	thread_pool(const thread_pool& other) = delete;
	thread_pool& operator= (const thread_pool& other) = delete;
	thread_pool(thread_pool&& other) = delete;
	thread_pool& operator= (thread_pool&& other) = delete;

	// add task to the thread pool, thread safe
	template <typename F, typename... Args>
	auto job(F&& f, Args&&... args);

	// thread safe
	unsigned int get_total_cores() const;

	// thread safe
	unsigned int get_num_extra_cores() const;

private:
	std::atomic<unsigned int> m_num_extra_cores;
	std::atomic<bool> m_shutdown_requested;

	std::condition_variable m_worker_condvar;
	mutable std::mutex m_threadpool_mutex;
	std::mutex m_worker_cond_mutex;
	std::vector<std::unique_ptr<worker_thread>> m_worker_threads;

	std::queue<std::function<void()>> m_queue;
};

template<typename F, typename... Args>
auto thread_pool::job(F&& f, Args&&... args) {
	using return_type = std::result_of_t<F(Args...)>;

	std::lock_guard<std::mutex> lock{m_threadpool_mutex};

	auto task = std::make_shared<std::packaged_task<return_type()>>(std::bind(std::forward<F>(f), std::forward<Args>(args)...));

	std::future<return_type> res = task->get_future();
	{
		if (m_num_extra_cores == 0) {
			(*task)();
		}
		else {
			std::unique_lock<std::mutex> lock(m_worker_cond_mutex);
			m_queue.emplace([task] () { (*task)(); });
		}
	}

	m_worker_condvar.notify_one();

	return res;
}

inline unsigned int thread_pool::get_total_cores() const {
	return m_num_extra_cores + 1;
}

inline unsigned int thread_pool::get_num_extra_cores() const {
	return m_num_extra_cores;
}

// Helper functions

/**
* @brief is_future_ready - Helper function to determine if a task via queue (or a future in general) is completed yet
*/
template<typename R>
bool is_future_ready(const std::future<R>& f) {
	return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
}

} // namespace ts
