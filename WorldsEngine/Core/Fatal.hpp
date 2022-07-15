#pragma once
#define fatalErr(msg) fatalErrInternal(msg, __FILE__, __LINE__)

void fatalErrInternal(const char *msg, const char *file, int line);