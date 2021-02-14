#include "IOUtil.hpp"
#include "Result.hpp"
#include <SDL_log.h>
#include <physfs.h>

namespace worlds {
    Result<void*, IOError> LoadFileToBuffer(std::string path, int64_t* fileLength) {
        PHYSFS_File* file = PHYSFS_openRead(path.c_str());

        if (file == nullptr) {
            PHYSFS_ErrorCode errCode = PHYSFS_getLastErrorCode();
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "Failed to open file %s due to following error: %s",
                path.c_str(),
                PHYSFS_getErrorByCode(errCode));

            switch (errCode) {
            case PHYSFS_ErrorCode::PHYSFS_ERR_NOT_FOUND:
                return IOError::FileNotFound;
            default:
                return IOError::Unknown;
            }
        }

        PHYSFS_sint64 diskFileLength = PHYSFS_fileLength(file);

        if (diskFileLength == -1) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "Failed to obtain the length of file %s.",
                path.c_str());

            return IOError::CouldntStat;
        }

        void* buf = std::malloc(diskFileLength);

        PHYSFS_sint64 readBytes = PHYSFS_readBytes(file, buf, diskFileLength);

        if (readBytes != diskFileLength) {
            SDL_LogError(
                SDL_LOG_CATEGORY_APPLICATION,
                "Only read %lld bytes of %lld in file %s",
                readBytes,
                diskFileLength,
                path.c_str());

            std::free(buf);

            return IOError::IncompleteRead;
        }

        PHYSFS_close(file);

        (*fileLength) = diskFileLength;

        return buf;
    }

    Result<std::string, IOError> LoadFileToString(std::string path) {
        int64_t length;
        auto res = LoadFileToBuffer(path, &length);
        if (res.error != IOError::None) {
            return res.error;
        }
        std::string r(static_cast<char*>(res.value), length);
        std::free(res.value);
        return r;
    }

    bool canOpenFile(std::string path) {
        PHYSFS_File* f = PHYSFS_openRead(path.c_str());
        if (f == nullptr)
            return false;

        PHYSFS_getLastErrorCode();

        PHYSFS_close(f);

        return true;
    }
}
