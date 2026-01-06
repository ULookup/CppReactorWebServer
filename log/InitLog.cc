#include "InitLog.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace webserver::log {

// 唯一一次定义
std::shared_ptr<spdlog::logger> g_default_logger;

void init_logger(bool mode,
                 const std::string &filename,
                 int32_t level)
{
    if (!mode) {
        spdlog::drop("console-logger");
        g_default_logger = spdlog::stdout_color_mt("console-logger");
        g_default_logger->set_level(spdlog::level::trace);
        g_default_logger->flush_on(spdlog::level::trace);
    } else {
        spdlog::drop("file-logger");
        g_default_logger = spdlog::basic_logger_mt("file-logger", filename);
        g_default_logger->set_level(
            static_cast<spdlog::level::level_enum>(level)
        );
        g_default_logger->flush_on(
            static_cast<spdlog::level::level_enum>(level)
        );
    }

    g_default_logger->set_pattern("[%n][%H:%M:%S][%t]%^[%l]%$%v");
}

} // namespace webserver::log
