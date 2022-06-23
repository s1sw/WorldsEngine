#include <R2/R2.hpp>
#include <string.h>

namespace R2
{
	RenderInitException::RenderInitException(const char* message)
	{
		strncpy(msg, message, sizeof(msg) / sizeof(msg[0]));
	}
}