#pragma once
#define fatalErr(msg) fatalErrInternal(msg, __FILE__, __LINE__)

namespace worlds
{
    void fatalErrInternal(const char *msg, const char *file, int line);
    void assertInternal(const char* condition, const char* file, int line);
}

#define releaseAssert(condition) if (!(condition)) worlds::assertInternal(#condition, __FILE__, __LINE__)