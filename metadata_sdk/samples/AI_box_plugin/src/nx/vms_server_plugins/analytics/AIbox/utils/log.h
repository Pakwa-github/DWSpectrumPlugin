#pragma once

#include <string>
#include <sstream>
#include <memory>

#include <nx/kit/debug.h>

namespace nx {
namespace vms_server_plugins {
namespace analytics {
namespace stub {
namespace AIbox {

class Log
{
public:
	static Log& instance();

	void init(size_t maxQueueSize = 1000, bool enableOutput = true);

	void shutdown();

	void print(const std::string& msg, const char* callerFile = nullptr);
	
    void output(const std::string& msg, const char* callerFile = nullptr);

private:
	Log();
	~Log();

	Log(const Log&) = delete;
	Log& operator=(const Log&) = delete;

	struct Impl;
	std::unique_ptr<Impl> pimpl_;
};

} // namespace AIbox
} // namespace stub
} // namespace analytics
} // namespace vms_server_plugins
} // namespace nx

#define LOG_PRINT(MSG) do { std::ostringstream oss_; oss_ << MSG; nx::vms_server_plugins::analytics::stub::AIbox::Log::instance().print(oss_.str(), __FILE__); } while(0)
#define LOG_OUTPUT(MSG) do { std::ostringstream oss_; oss_ << MSG; nx::vms_server_plugins::analytics::stub::AIbox::Log::instance().output(oss_.str(), __FILE__); } while(0)

