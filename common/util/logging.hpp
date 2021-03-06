#pragma once

#include <cassert>
#include <fmt/format.h>
#include <fmt/ostream.h>
extern "C"
{
#include "wlr/util/log.h"
}

#pragma clang diagnostic ignored "-Wformat-security"
#define cloth_debug(message, ...) \
  _wlr_log(WLR_DEBUG, fmt::format((std::string("[{}:{}] ") + message), _WLR_FILENAME, __LINE__, ##__VA_ARGS__).c_str())
#define cloth_info(message, ...) \
  _wlr_log(WLR_INFO, fmt::format((std::string("[{}:{}] ") + message), _WLR_FILENAME, __LINE__, ##__VA_ARGS__).c_str())
#define cloth_error(message, ...) \
  _wlr_log(WLR_ERROR, fmt::format((std::string("[{}:{}] ") + message), _WLR_FILENAME, __LINE__, ##__VA_ARGS__).c_str())
