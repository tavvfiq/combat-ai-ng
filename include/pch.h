#pragma once

#if defined(_WIN32) || defined(_WIN64)
    #define DLLEXPORT __declspec(dllexport)
#else
    #define DLLEXPORT
#endif

// Standard Library
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

// CommonLibSSE-NG
#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

// Plugin-specific
#include "Logger.h"

// Third-party
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
