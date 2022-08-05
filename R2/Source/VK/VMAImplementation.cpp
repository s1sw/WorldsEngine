#include <R2/VKCore.hpp>
#include <volk.h>
R2::VK::IDebugOutputReceiver* vmaDebugOutputRecv;

//#define VMA_DEBUG_LOG(format, ...) do { \
//    char buf[512]; \
//    snprintf(buf, 512, format, __VA_ARGS__); \
//    vmaDebugOutputRecv->DebugMessage(buf); \
//} while(false)
#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>