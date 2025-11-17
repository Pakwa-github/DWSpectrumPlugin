// Implementation of the async, non-blocking, atomic-line logger declared in log.h

#include "log.h"

#include <deque>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace AIBox {

struct LogItem
{
	bool isOutput;
	std::string text;
	std::string file;
};
struct Log::Impl
{
	Impl(): running(false), maxSize(1000), enableOutput(true) {}

	std::deque<LogItem> queue;
	std::mutex mutex;
	std::condition_variable cv;
	std::thread worker;
	std::atomic<bool> running;
	size_t maxSize;
	bool enableOutput;
};
// Singleton access
Log& Log::instance()
{
	static Log s;
	return s;
}
Log::Log(): pimpl_(new Impl()) {}

Log::~Log() { shutdown(); }

void Log::init(size_t maxQueueSize, bool enableOutput)
{
	Impl* m = pimpl_.get();
	m->maxSize = maxQueueSize;
	m->enableOutput = enableOutput;
	bool expected = false;
	if (!m->running && m->running.compare_exchange_strong(expected, true))
	{
			m->worker = std::thread([m]()
			{
				while (m->running || !m->queue.empty())
				{
					LogItem item;

					{
						std::unique_lock<std::mutex> lk(m->mutex);
						if (m->queue.empty())
							m->cv.wait(lk, [&](){ return !m->running || !m->queue.empty(); });
						if (m->queue.empty())
							continue;
						item = std::move(m->queue.front());
						m->queue.pop_front();
					}

					std::string outText;
					// if (!item.file.empty())
					// 	outText = item.file + ": " + item.text;
					// else
						outText = item.text;

					if (item.isOutput)
					{
						if (m->enableOutput)
							NX_PRINT << outText; // use NX_PRINT but gate by enableOutput to avoid depending on ini().
					}
					else
					{
						NX_PRINT << outText;
					}
				}
			});
	}
}
void Log::shutdown()
{
	Impl* m = pimpl_.get();
	if (!m->running)
		return;
	m->running = false;
	m->cv.notify_all();
	if (m->worker.joinable())
		m->worker.join();
}
void Log::print(const std::string& msg, const char* callerFile)
{
	Impl* m = pimpl_.get();
	{
		std::lock_guard<std::mutex> lk(m->mutex);
		if (m->queue.size() >= m->maxSize)
			m->queue.pop_front(); // drop oldest
		LogItem it; it.isOutput = false; it.text = msg; it.file = callerFile ? callerFile : std::string();
		m->queue.push_back(std::move(it));
	}
	m->cv.notify_one();
}
void Log::output(const std::string& msg, const char* callerFile)
{
	Impl* m = pimpl_.get();
	{
		std::lock_guard<std::mutex> lk(m->mutex);
		if (m->queue.size() >= m->maxSize)
			m->queue.pop_front();
		LogItem it; it.isOutput = true; it.text = msg; it.file = callerFile ? callerFile : std::string();
		m->queue.push_back(std::move(it));
	}
	m->cv.notify_one();
}

} // namespace AIBox
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx
