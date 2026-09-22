#ifdef PS4_PLATFORM

#include "ps4/input/Ps4Pointer.h"
#include "platform/time.h"
#include "lwjgl/Mouse.h"

namespace
{
int s_width = 1920;
int s_height = 1080;
float s_x = 960.0f;
float s_y = 540.0f;
float s_lastTime = 0.0f;
int s_publishedX = -1;
int s_publishedY = -1;

void clamp()
{
	const float maxX = static_cast<float>(s_width - 1);
	const float maxY = static_cast<float>(s_height - 1);
	if (s_x < 0.0f) s_x = 0.0f;
	if (s_y < 0.0f) s_y = 0.0f;
	if (s_x > maxX) s_x = maxX;
	if (s_y > maxY) s_y = maxY;
}
}

namespace Ps4Pointer
{
void setBounds(int width, int height)
{
	s_width = width > 0 ? width : 1;
	s_height = height > 0 ? height : 1;
	clamp();
}

void enterMenu()
{
	s_x = static_cast<float>(s_width) * 0.5f;
	s_y = static_cast<float>(s_height) * 0.5f;
	s_lastTime = getTimeS();
	s_publishedX = -1;
	s_publishedY = -1;
}

void leaveMenu()
{
	s_lastTime = 0.0f;
	s_publishedX = -1;
	s_publishedY = -1;
}

float beginMenuFrame()
{
	const float now = getTimeS();
	float dt = s_lastTime > 0.0f ? now - s_lastTime : 0.0f;
	s_lastTime = now;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > 0.10f) dt = 0.10f;
	return dt;
}

void move(float dx, float dy)
{
	s_x += dx;
	s_y += dy;
	clamp();
}

void publish()
{
	const int px = static_cast<int>(s_x);
	const int py = static_cast<int>(s_y);
	if (px == s_publishedX && py == s_publishedY)
		return;
	const int dx = s_publishedX < 0 ? 0 : px - s_publishedX;
	const int dy = s_publishedY < 0 ? 0 : py - s_publishedY;
	s_publishedX = px;
	s_publishedY = py;
	lwjgl::Mouse::detail::pushMotion(px, py, dx, dy);
}

void setPosition(int px, int py)
{
	s_x = static_cast<float>(px);
	s_y = static_cast<float>(py);
	clamp();
	publish();
}

int x() { return static_cast<int>(s_x); }
int y() { return static_cast<int>(s_y); }
}

#endif // PS4_PLATFORM
