#include "thread_pool.h"

#include <iostream>

#include <thread>

namespace ts {

thread_pool::thread_pool()
	: m_num_extra_cores(0)
	, m_shutdown_requested(false) {

	m_num_extra_cores = [this]() -> unsigned int {
		auto total_cores = std::thread::hardware_concurrency();
		if (total_cores == 0 || total_cores == 1) {
			return 0;
		}
		else {
			return total_cores - 1;
		}
	}();

	const auto created_threads = (m_num_extra_cores + 1) * 6;

	std::cout << "thread_pool(): starting " << created_threads << " threads" << std::endl;

	// Num cores -1 because the main loop is running on one core
	for (unsigned int i = 0; i < created_threads; ++i) {
		m_worker_threads.push_back(std::make_unique<worker_thread>([this] {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(m_worker_cond_mutex);
					m_worker_condvar.wait(lock, [this] {
						return m_shutdown_requested || !m_queue.empty();
					});

					if (m_shutdown_requested && m_queue.empty()) {
						break;
					}

					task = std::move(m_queue.front());
					m_queue.pop();
				}

				task();
				//std::cout << "thread_pool task successfully performed by thread with id: " << std::this_thread::get_id() << std::endl;
			}
		}));
	}
}

thread_pool::~thread_pool() {
	m_shutdown_requested = true;

	// Is this enough?
	m_worker_condvar.notify_all();

	for (auto& thread : m_worker_threads) {
		thread->join();
	}

	//std::cout << "~thread_pool(): successfully joined all threads" << std::endl;
}

} // namespace ts
