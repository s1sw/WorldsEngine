#pragma once

namespace R2
{
	struct RenderInitException
	{
	public:
		RenderInitException(const char* message);
	private:
		char msg[512];
	};

	namespace VK
	{
		class Core;
	}
}