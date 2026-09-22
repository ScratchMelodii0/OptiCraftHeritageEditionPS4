// InputBackend_PS4.cpp — platform/Input.h on the DualShock 4.
//
// The text-input mapping follows the PS2 port's so the shared virtual
// keyboard and D-pad menus behave the same on both consoles, with OPTIONS in
// place of START and the touchpad click in place of SELECT.
#include "platform/Input.h"

#include "ps4/input/Ps4AnalogFilter.h"
#include "ps4/input/Ps4Input.h"
#include "ps4/input/Ps4PadState.h"

namespace
{
std::uint32_t mapTextButtons(std::uint32_t bits)
{
    std::uint32_t value = 0;
    if (bits & PS4_PAD_LEFT) value |= PLATFORM_TEXT_LEFT;
    if (bits & PS4_PAD_RIGHT) value |= PLATFORM_TEXT_RIGHT;
    if (bits & PS4_PAD_UP) value |= PLATFORM_TEXT_UP;
    if (bits & PS4_PAD_DOWN) value |= PLATFORM_TEXT_DOWN;
    if (bits & PS4_PAD_CROSS) value |= PLATFORM_TEXT_TYPE;
    if (bits & PS4_PAD_SQUARE) value |= PLATFORM_TEXT_BACK;
    if (bits & PS4_PAD_TOUCHPAD) value |= PLATFORM_TEXT_SPACE;
    if (bits & PS4_PAD_TRIANGLE) value |= PLATFORM_TEXT_SHIFT;
    if (bits & PS4_PAD_OPTIONS) value |= PLATFORM_TEXT_ENTER;
    if (bits & PS4_PAD_CIRCLE) value |= PLATFORM_TEXT_CLOSE;
    return value;
}
}

PlatformTextInputSnapshot platformTextInputSnapshot(int)
{
    PlatformTextInputSnapshot out;
    const Ps4PadSnapshot& pad = ps4PadGetSnapshot();
    out.connected = pad.connected;
    out.held = mapTextButtons(pad.held);
    out.pressed = mapTextButtons(ps4PadConsumePressed());
    return out;
}

PlatformGamepadSnapshot platformGamepadSnapshot(int)
{
    PlatformGamepadSnapshot out;
    const Ps4PadSnapshot& pad = ps4PadGetSnapshot();
    out.connected = pad.connected;
    out.leftX = Ps4AnalogFilter::apply(pad.leftX);
    out.leftY = Ps4AnalogFilter::apply(pad.leftY);
    out.rightX = Ps4AnalogFilter::apply(pad.rightX);
    out.rightY = Ps4AnalogFilter::apply(pad.rightY);
    return out;
}

PlatformGamepadSnapshot platformRawGamepadSnapshot(int)
{
    PlatformGamepadSnapshot out;
    const Ps4PadSnapshot& pad = ps4PadGetSnapshot();
    out.connected = pad.connected;
    out.leftX = pad.leftX;
    out.leftY = pad.leftY;
    out.rightX = pad.rightX;
    out.rightY = pad.rightY;
    return out;
}

// One local player: the user who launched the application.
int platformMenuPad()
{
    return 0;
}

bool platformMenuPointerActive()
{
    return true;
}

bool platformMenuCursorVisible()
{
    return true;
}

void platformSetMenuCursor(int x, int y)
{
    Ps4Input::setMenuCursor(x, y);
}

const PlatformKeyboardHints& platformKeyboardHints()
{
    static const PlatformKeyboardHints hints = {
        {
            "X:type  Sq:del  Tri:shift  Touchpad:space  Options:ok  O:close",
            "Right stick: move keyboard", nullptr
        }, 2
    };
    return hints;
}

const char* platformInputDebugLine()
{
    return "";
}
