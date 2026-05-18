#pragma once

#include <chrono>
#include <cstdint>

namespace imtool {

[[nodiscard]] inline uint64_t getTimestamp() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch()
        ).count()
    );
}

}
