#pragma once

#include <functional>
#include <vector>
#include <queue>
#include <condition_variable>
#include <future>
#include <atomic>

namespace ts {

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

	// add task to the thread pool
	template <typename F, typename... Args>
	auto job(F&& f, Args&&... args);

	unsigned int get_total_cores() const;
	unsigned int get_num_extra_cores() const;

private:
	std::atomic<unsigned int> m_num_extra_cores;
	std::atomic<bool> m_shutdown_requested;

	std::condition_variable m_worker_condvar;
	std::mutex m_worker_cond_mutex;
	std::vector<std::thread> m_worker_threads;

	std::queue<std::function<void()>> m_queue;
};

template<typename F, typename... Args>
auto thread_pool::job(F&& f, Args&&... args) {
	using return_type = std::result_of_t<F(Args...)>;

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
