#pragma once
#include <string>

namespace rune::core {

void log_open(const std::string& path);
void log_close();

void log_info(const std::string& msg);
void log_warn(const std::string& msg);
void log_error(const std::string& msg);

} // namespace rune::core
