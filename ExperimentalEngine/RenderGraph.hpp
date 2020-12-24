#pragma once
#include <unordered_set>
#include <functional>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace vk {
	enum class ImageAspectFlagBits : uint32_t;
}

namespace worlds {
	struct TextureUsage;
	struct RenderCtx;
	struct PassSetupCtx;
	struct ImageBarrier;
}