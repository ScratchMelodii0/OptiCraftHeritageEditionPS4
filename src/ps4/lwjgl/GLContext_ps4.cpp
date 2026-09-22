#ifdef PS4_PLATFORM

#include "lwjgl/GLContext.h"

#include "ps4/render/Ps4Gl.h"

#include <sstream>

namespace lwjgl
{
namespace GLContext
{
namespace detail
{
static GLCapabilities &ps4Caps()
{
	static GLCapabilities caps;
	return caps;
}
} // namespace detail

// MSAA is not offered: the EGL config is chosen by Ps4Piglet without samples.
void setRequestedSamples(int) {}
int getRequestedSamples() { return 0; }

void instantiate()
{
	// Publish the real Piglet extension string. The desktop paths keyed off
	// it (GL_ARB_occlusion_query, GL_ARB_vertex_buffer_object) use ARB names
	// GLES never reports, so they stay off without special-casing here.
	const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
	if (extensions == nullptr)
		return;
	std::istringstream stream(extensions);
	std::string name;
	while (stream >> name)
		detail::ps4Caps().add(name);
}

const detail::GLCapabilities &getCapabilities()
{
	return detail::ps4Caps();
}

} // namespace GLContext
} // namespace lwjgl

#endif // PS4_PLATFORM
