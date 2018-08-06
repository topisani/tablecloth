#pragma once

#include <cassert>
#include <iostream>
#include <fmt/format.h>
#include <fmt/ostream.h>

#define LOGD(...) std::cout << "[DEBUG]: " << ::fmt::format(__VA_ARGS__) << '\n'
#define LOGI(...) std::cout << " [INFO]: " << ::fmt::format(__VA_ARGS__) << '\n'
#define LOGE(...) std::cerr << "  [ERR]: " << ::fmt::format(__VA_ARGS__) << '\n'
