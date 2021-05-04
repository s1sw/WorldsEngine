#pragma once

namespace worlds {
    template <typename T>
    bool enumHasFlag(T in, T flag) {
        return (static_cast<unsigned int>(in) & static_cast<unsigned int>(flag)) == static_cast<unsigned int>(flag);
    }

    template <typename B, typename F>
    bool enumHasFlag(B in, F flag) {
        return (static_cast<unsigned int>(in) & static_cast<unsigned int>(flag)) == static_cast<unsigned int>(flag);
    }
}
