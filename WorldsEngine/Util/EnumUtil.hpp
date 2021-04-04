#pragma once

namespace worlds {
    template <typename T>
    bool enumHasFlag(T in, T flag) {
        return (static_cast<unsigned int>(in) & static_cast<unsigned int>(flag)) == static_cast<unsigned int>(flag);
    }
}
