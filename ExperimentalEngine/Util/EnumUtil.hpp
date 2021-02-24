#pragma once

namespace worlds {
    template <typename T>
    bool enumHasFlag(T in, T flag) {
        return (static_cast<int>(in) & static_cast<int>(flag)) == static_cast<int>(flag);
    }
}
