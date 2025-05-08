// PROJECT:     Gamepad Debug
// LICENSE:     MIT (https://spdx.org/licenses/MIT.html)
// PURPOSE:     Handle & display XInput devices
// COPYRIGHT:   Copyright 2025 Mark Jansen <mark.jansen@reactos.org>

#include "gd_win32.h"
#include "gd_log.h"
#include "fonts/cf_xbox_one.h"
#include "modules/gd_XInput.h"
#include "modules/gd_XInput_types.h"
#include <Xinput.h>
#include "imgui.h"
#include "imgui_internal.h"

// Private (semi-) undocumented XInput functions
// We want these to read the state of the Guide button (Xbox button) on the controller
typedef struct _XINPUT_GAMEPAD_EX {
    WORD wButtons;
    BYTE bLeftTrigger;
    BYTE bRightTrigger;
    SHORT sThumbLX;
    SHORT sThumbLY;
    SHORT sThumbRX;
    SHORT sThumbRY;
    DWORD dwPaddingReserved;
} XINPUT_GAMEPAD_EX, * PXINPUT_GAMEPAD_EX;

typedef struct _XINPUT_STATE_EX {
    DWORD dwPacketNumber;
    XINPUT_GAMEPAD_EX Gamepad;
} XINPUT_STATE_EX, * PXINPUT_STATE_EX;

struct XINPUT_CAPABILITIES_EX
{
    XINPUT_CAPABILITIES Capabilities;
    WORD vendorId;
    WORD productId;
    WORD productVersion;
    WORD unk1;
    DWORD unk2;
};

constexpr auto XINPUT_GAMEPAD_GUIDE = 0x0400;

typedef DWORD(WINAPI* tXInputGetStateEx)(_In_ DWORD dwUserIndex, XINPUT_STATE_EX* pState);
typedef DWORD(WINAPI* tXInputPowerOffController)(DWORD XUser);
typedef void (WINAPI* tXInputEnable)(_In_ BOOL enable);
typedef DWORD(WINAPI* tXInputGetCapabilitiesEx)(DWORD unk, DWORD dwUserIndex, DWORD dwFlags, XINPUT_CAPABILITIES_EX* pCapabilities);


static HINSTANCE s_XInputInstance = nullptr;
static tXInputGetStateEx s_XInputGetStateEx = nullptr;
static decltype(XInputGetCapabilities)* s_XInputGetCapabilities = nullptr;
static tXInputGetCapabilitiesEx s_XInputGetCapabilitiesEx = nullptr;

static decltype(XInputGetBatteryInformation)* s_XInputGetBatteryInformation = nullptr;
static tXInputPowerOffController s_XInputPowerOffController = nullptr;
static decltype(XInputSetState)* s_XInputSetState = nullptr;
static tXInputEnable s_XInputEnable = nullptr;
static bool s_fXInputIsEnabled = true;

static double s_LastBatteryUpdate = 0.0;


static GD::XInput::GamePadState s_XInputDevices[4]{};

static void XInput_Poweroff(DWORD XUser);
static void XInput_EnableDisable(BOOL fEnable);
static void XInput_SetRumble(DWORD XUser, WORD left, WORD right);

static const string SubTypeToString(BYTE subtype)
{
    switch (subtype)
    {
    case XINPUT_DEVSUBTYPE_GAMEPAD: return "Gamepad";
    case XINPUT_DEVSUBTYPE_WHEEL: return "Wheel";
    case XINPUT_DEVSUBTYPE_ARCADE_PAD: return "Arcade Pad";
    case XINPUT_DEVSUBTYPE_FLIGHT_STICK: return "Flight Stick";
    case XINPUT_DEVSUBTYPE_DANCE_PAD: return "Dance Pad";
    case XINPUT_DEVSUBTYPE_GUITAR: return "Guitar";
    case XINPUT_DEVSUBTYPE_DRUM_KIT: return "Drum Kit";
    case XINPUT_DEVSUBTYPE_ARCADE_STICK: return "Arcade Stick";
    default: return std::to_string(subtype);
    }
}

static const string BatteryTypeToString(BYTE type)
{
    switch (type)
    {
    case BATTERY_TYPE_DISCONNECTED: return "Disconnected";
    case BATTERY_TYPE_WIRED: return "Wired";
    case BATTERY_TYPE_NIMH: return "NiMH";
    case BATTERY_TYPE_ALKALINE: return "Alkaline";
    case BATTERY_TYPE_UNKNOWN: return "Unknown";
    default: return std::to_string(type);
    }
}

struct usbid_t
{
    uint16_t vendorId;
    uint16_t productId;
    const char* desc;
    uint16_t xtype;
};

enum xtype_t
{
    XTYPE_UNKNOWN = 0,
    XTYPE_XBOX = 1,
    XTYPE_XBOX360 = 2,
    XTYPE_XBOX360W = 3,
    XTYPE_XBOXONE = 4,
    PTYPE_PS3 = 5,
    PTYPE_PS4 = 6,
    PTYPE_BT = 7
};

static usbid_t usb_devs[] = {
    { 0x045e, 0x0202, "Microsoft XBox pad v1 (US)", XTYPE_XBOX },
    { 0x045e, 0x0285, "Microsoft XBox pad (Japan)", XTYPE_XBOX },
    { 0x045e, 0x0287, "Microsoft Xbox Controller S", XTYPE_XBOX },
    { 0x045e, 0x0289, "Microsoft XBox pad v2 (US)", XTYPE_XBOX },
    { 0x045e, 0x028e, "Microsoft XBox 360 pad", XTYPE_XBOX360 },
    { 0x045e, 0x0000, "Microsoft XBox 360 pad (compat)", XTYPE_XBOX360 },  // actually 0x02a1

    { 0x045e, 0x02d1, "Microsoft XBox One pad", XTYPE_XBOXONE },
    { 0x045e, 0x02dd, "Microsoft XBox One pad (Firmware 2015)", XTYPE_XBOXONE },
    { 0x045e, 0x02e3, "Microsoft XBox One Elite pad", XTYPE_XBOXONE },
    { 0x045e, 0x02ea, "Microsoft XBox One S pad", XTYPE_XBOXONE },
    { 0x045e, 0x0291, "Xbox 360 Wireless Receiver (XBOX)", XTYPE_XBOX360W },
    { 0x045e, 0x0719, "Xbox 360 Wireless Receiver", XTYPE_XBOX360W },
    { 0x044f, 0x0f07, "Thrustmaster Inc. Controller", XTYPE_XBOX },
    { 0x044f, 0xb326, "Thrustmaster Gamepad GP XID", XTYPE_XBOX360 },
    { 0x046d, 0xc21d, "Logitech Gamepad F310", XTYPE_XBOX360 },
    { 0x046d, 0xc21e, "Logitech Gamepad F510", XTYPE_XBOX360 },
    { 0x046d, 0xc21f, "Logitech Gamepad F710", XTYPE_XBOX360 },
    { 0x046d, 0xc242, "Logitech Chillstream Controller", XTYPE_XBOX360 },
    { 0x046d, 0xca84, "Logitech Xbox Cordless Controller", XTYPE_XBOX },
    { 0x046d, 0xca88, "Logitech Compact Controller for Xbox", XTYPE_XBOX },
    { 0x05fd, 0x1007, "Mad Catz Controller (unverified)", XTYPE_XBOX },
    { 0x05fd, 0x107a, "InterAct 'PowerPad Pro' XBox pad (Germany)", XTYPE_XBOX },
    { 0x0738, 0x4516, "Mad Catz Control Pad", XTYPE_XBOX },
    { 0x0738, 0x4522, "Mad Catz LumiCON", XTYPE_XBOX },
    { 0x0738, 0x4526, "Mad Catz Control Pad Pro", XTYPE_XBOX },
    { 0x0738, 0x4536, "Mad Catz MicroCON", XTYPE_XBOX },
    { 0x0738, 0x4540, "Mad Catz Beat Pad", XTYPE_XBOX },
    { 0x0738, 0x4556, "Mad Catz Lynx Wireless Controller", XTYPE_XBOX },
    { 0x0738, 0x4716, "Mad Catz Wired Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x0738, 0x4718, "Mad Catz Street Fighter IV FightStick SE", XTYPE_XBOX360 },
    { 0x0738, 0x4726, "Mad Catz Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x0738, 0x4728, "Mad Catz Street Fighter IV FightPad", XTYPE_XBOX360 },
    { 0x0738, 0x4738, "Mad Catz Wired Xbox 360 Controller (SFIV)", XTYPE_XBOX360 },
    { 0x0738, 0x4740, "Mad Catz Beat Pad", XTYPE_XBOX360 },
    { 0x0738, 0x4a01, "Mad Catz FightStick TE 2", XTYPE_XBOXONE },
    { 0x0738, 0x6040, "Mad Catz Beat Pad Pro", XTYPE_XBOX },
    { 0x0738, 0xb726, "Mad Catz Xbox controller - MW2", XTYPE_XBOX360 },
    { 0x0738, 0xbeef, "Mad Catz JOYTECH NEO SE Advanced GamePad", XTYPE_XBOX360 },
    { 0x0738, 0xcb02, "Saitek Cyborg Rumble Pad - PC/Xbox 360", XTYPE_XBOX360 },
    { 0x0738, 0xcb03, "Saitek P3200 Rumble Pad - PC/Xbox 360", XTYPE_XBOX360 },
    { 0x0738, 0xf738, "Super SFIV FightStick TE S", XTYPE_XBOX360 },
    { 0x0c12, 0x8802, "Zeroplus Xbox Controller", XTYPE_XBOX },
    { 0x0c12, 0x8809, "RedOctane Xbox Dance Pad", XTYPE_XBOX },
    { 0x0c12, 0x880a, "Pelican Eclipse PL-2023", XTYPE_XBOX },
    { 0x0c12, 0x8810, "Zeroplus Xbox Controller", XTYPE_XBOX },
    { 0x0c12, 0x9902, "HAMA VibraX - *FAULTY HARDWARE*", XTYPE_XBOX },
    { 0x0d2f, 0x0002, "Andamiro Pump It Up pad", XTYPE_XBOX },
    { 0x0e4c, 0x1097, "Radica Gamester Controller", XTYPE_XBOX },
    { 0x0e4c, 0x2390, "Radica Games Jtech Controller", XTYPE_XBOX },
    { 0x0e6f, 0x0003, "Logic3 Freebird wireless Controller", XTYPE_XBOX },
    { 0x0e6f, 0x0005, "Eclipse wireless Controller", XTYPE_XBOX },
    { 0x0e6f, 0x0006, "Edge wireless Controller", XTYPE_XBOX },
    { 0x0e6f, 0x0105, "HSM3 Xbox360 dancepad", XTYPE_XBOX360 },
    { 0x0e6f, 0x0113, "Afterglow AX.1 Gamepad for Xbox 360", XTYPE_XBOX360 },
    { 0x0e6f, 0x0139, "Afterglow Prismatic Wired Controller", XTYPE_XBOXONE },
    { 0x0e6f, 0x0201, "Pelican PL-3601 'TSZ' Wired Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x0e6f, 0x0213, "Afterglow Gamepad for Xbox 360", XTYPE_XBOX360 },
    { 0x0e6f, 0x021f, "Rock Candy Gamepad for Xbox 360", XTYPE_XBOX360 },
    { 0x0e6f, 0x0146, "Rock Candy Wired Controller for Xbox One", XTYPE_XBOXONE },
    { 0x0e6f, 0x0301, "Logic3 Controller", XTYPE_XBOX360 },
    { 0x0e6f, 0x0401, "Logic3 Controller", XTYPE_XBOX360 },
    { 0x0e8f, 0x0201, "SmartJoy Frag Xpad/PS2 adaptor", XTYPE_XBOX },
    { 0x0e8f, 0x3008, "Generic xbox control (dealextreme)", XTYPE_XBOX },
    { 0x0f0d, 0x000a, "Hori Co. DOA4 FightStick", XTYPE_XBOX360 },
    { 0x0f0d, 0x000d, "Hori Fighting Stick EX2", XTYPE_XBOX360 },
    { 0x0f0d, 0x0016, "Hori Real Arcade Pro.EX", XTYPE_XBOX360 },
    { 0x0f0d, 0x0067, "HORIPAD ONE", XTYPE_XBOXONE },
    { 0x0f30, 0x0202, "Joytech Advanced Controller", XTYPE_XBOX },
    { 0x0f30, 0x8888, "BigBen XBMiniPad Controller", XTYPE_XBOX },
    { 0x102c, 0xff0c, "Joytech Wireless Advanced Controller", XTYPE_XBOX },
    { 0x12ab, 0x0004, "Honey Bee Xbox360 dancepad", XTYPE_XBOX360 },
    { 0x12ab, 0x0301, "PDP AFTERGLOW AX.1", XTYPE_XBOX360 },
    { 0x12ab, 0x8809, "Xbox DDR dancepad", XTYPE_XBOX },
    { 0x1430, 0x4748, "RedOctane Guitar Hero X-plorer", XTYPE_XBOX360 },
    { 0x1430, 0x8888, "TX6500+ Dance Pad (first generation)", XTYPE_XBOX },
    { 0x146b, 0x0601, "BigBen Interactive XBOX 360 Controller", XTYPE_XBOX360 },
    { 0x1532, 0x0037, "Razer Sabertooth", XTYPE_XBOX360 },
    { 0x15e4, 0x3f00, "Power A Mini Pro Elite", XTYPE_XBOX360 },
    { 0x15e4, 0x3f0a, "Xbox Airflo wired controller", XTYPE_XBOX360 },
    { 0x15e4, 0x3f10, "Batarang Xbox 360 controller", XTYPE_XBOX360 },
    { 0x162e, 0xbeef, "Joytech Neo-Se Take2", XTYPE_XBOX360 },
    { 0x1689, 0xfd00, "Razer Onza Tournament Edition", XTYPE_XBOX360 },
    { 0x1689, 0xfd01, "Razer Onza Classic Edition", XTYPE_XBOX360 },
    { 0x24c6, 0x542a, "Xbox ONE spectra", XTYPE_XBOXONE },
    { 0x24c6, 0x5d04, "Razer Sabertooth", XTYPE_XBOX360 },
    { 0x1bad, 0x0002, "Harmonix Rock Band Guitar", XTYPE_XBOX360 },
    { 0x1bad, 0x0003, "Harmonix Rock Band Drumkit", XTYPE_XBOX360 },
    { 0x1bad, 0xf016, "Mad Catz Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x1bad, 0xf023, "MLG Pro Circuit Controller (Xbox)", XTYPE_XBOX360 },
    { 0x1bad, 0xf028, "Street Fighter IV FightPad", XTYPE_XBOX360 },
    { 0x1bad, 0xf038, "Street Fighter IV FightStick TE", XTYPE_XBOX360 },
    { 0x1bad, 0xf900, "Harmonix Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x1bad, 0xf901, "Gamestop Xbox 360 Controller", XTYPE_XBOX360 },
    { 0x1bad, 0xf903, "Tron Xbox 360 controller", XTYPE_XBOX360 },
    { 0x24c6, 0x5000, "Razer Atrox Arcade Stick", XTYPE_XBOX360 },
    { 0x24c6, 0x5300, "PowerA MINI PROEX Controller", XTYPE_XBOX360 },
    { 0x24c6, 0x5303, "Xbox Airflo wired controller", XTYPE_XBOX360 },
    { 0x24c6, 0x541a, "PowerA Xbox One Mini Wired Controller", XTYPE_XBOXONE },
    { 0x24c6, 0x543a, "PowerA Xbox One wired controller", XTYPE_XBOXONE },
    { 0x24c6, 0x5500, "Hori XBOX 360 EX 2 with Turbo", XTYPE_XBOX360 },
    { 0x24c6, 0x5501, "Hori Real Arcade Pro VX-SA", XTYPE_XBOX360 },
    { 0x24c6, 0x5506, "Hori SOULCALIBUR V Stick", XTYPE_XBOX360 },
    { 0x24c6, 0x5b02, "Thrustmaster, Inc. GPX Controller", XTYPE_XBOX360 },
    { 0x24c6, 0x5b03, "Thrustmaster Ferrari 458 Racing Wheel", XTYPE_XBOX360 },
    { 0xffff, 0xffff, "Chinese-made Xbox Controller", XTYPE_XBOX },
    { 0x0000, 0x0000, "Generic XBox pad", XTYPE_UNKNOWN },
    { 0x054c, 0x0268, "Sony Playstation DualShock 3", PTYPE_PS3 },
    { 0x054c, 0x05c4, "Sony Playstation DualShock 4", PTYPE_PS4 },
    { 0x10D7, 0xB012, "QGOO Bluetooth Dongle 5.3", PTYPE_BT },
    { 0x0a5c, 0x2148, "IOGEAR GBU421", PTYPE_BT },

};

std::string GetDeviceType(uint16_t vendorId, uint16_t productId)
{
    for (const auto& dev : usb_devs)
    {
        if (dev.vendorId == vendorId && dev.productId == productId)
        {
            return dev.desc;
        }
    }
    return "Unknown Device";
}

static void append_text_comma_if(bool show, std::string& output, const char* text)
{
    if (show)
    {
        if (!output.empty())
            output += ", ";
        output += text;
    }
}

static void append_text_if(bool show, std::string& output, const char* text)
{
    if (show)
    {
        output += text;
    }
}

static void append_dpad(std::string& text, GD::XInput::DPad dpad)
{
    // First check for combinations that we can handle
    if (dpad.Left && dpad.Up)
    {
        text += CF_XBOX_DPAD_UP_LEFT;
        dpad.Left = 0;
        dpad.Up = 0;
    }
    if (dpad.Left && dpad.Down)
    {
        text += CF_XBOX_DPAD_DOWN_LEFT;
        dpad.Left = 0;
        dpad.Down = 0;
    }
    if (dpad.Right && dpad.Up)
    {
        text += CF_XBOX_DPAD_UP_RIGHT;
        dpad.Right = 0;
        dpad.Up = 0;
    }
    if (dpad.Right && dpad.Down)
    {
        text += CF_XBOX_DPAD_DOWN_RIGHT;
        dpad.Right = 0;
        dpad.Down = 0;
    }
    // Now handle each direction individually
    append_text_if(dpad.Up, text, CF_XBOX_DPAD_UP);
    append_text_if(dpad.Down, text, CF_XBOX_DPAD_DOWN);
    append_text_if(dpad.Left, text, CF_XBOX_DPAD_LEFT);
    append_text_if(dpad.Right, text, CF_XBOX_DPAD_RIGHT);
}

static void ProgressBarEx(float fraction, const ImVec2& size_arg, const char* overlay)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 size = ImGui::CalcItemSize(size_arg, ImGui::CalcItemWidth(), g.FontSize + style.FramePadding.y * 2.0f);
    ImRect bb(pos, pos + size);
    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, 0))
        return;

    float fill_n0 = fraction < 0.0f ? (fraction + 1.0f) / 2 : 0.5f;
    float fill_n1 = fraction > 0.0f ? (fraction + 1.0f) / 2 : 0.5f;

    // Render
    ImGui::RenderFrame(bb.Min, bb.Max, ImGui::GetColorU32(ImGuiCol_FrameBg), true, style.FrameRounding);
    bb.Expand(ImVec2(-style.FrameBorderSize, -style.FrameBorderSize));
    ImGui::RenderRectFilledRangeH(window->DrawList, bb, ImGui::GetColorU32(ImGuiCol_PlotHistogram), fill_n0, fill_n1, style.FrameRounding);

    if (overlay)
    {
        ImVec2 overlay_size = ImGui::CalcTextSize(overlay, NULL);
        if (overlay_size.x > 0.0f)
        {
            float text_x = ImLerp(bb.Min.x, bb.Max.x, fill_n1) + style.ItemSpacing.x;
            ImGui::RenderTextClipped(ImVec2(ImClamp(text_x, bb.Min.x, bb.Max.x - overlay_size.x - style.ItemInnerSpacing.x), bb.Min.y), bb.Max, overlay, NULL, &overlay_size, ImVec2(0.0f, 0.5f), &bb);
        }
    }
}

const char* analog_glyph(const GD::XInput::Thumb& thumb, float deadzone)
{
#ifndef M_PI
#define M_PI       3.14159265358979323846
#endif

    float magnitude = sqrtf(thumb.X * (float)thumb.X + thumb.Y * (float)thumb.Y);
    int xpos = 1;
    int ypos = 1;
    if (magnitude >= deadzone)
    {
        float direction = atan2f(thumb.Y, thumb.X);
        if (direction < 0.0f)
            direction += 2.0f * (float)M_PI;
        float angle = direction * (180.0f / (float)M_PI);
        if (angle < 22.5f || angle >= 337.5f)
            return "6";
        else if (angle < 67.5f)
            return "9";
        else if (angle < 112.5f)
            return "8";
        else if (angle < 157.5f)
            return "7";
        else if (angle < 202.5f)
            return "4";
        else if (angle < 247.5f)
            return "1";
        else if (angle < 292.5f)
            return "2";
        else
            return "3";
    }
    return "5";
}

void GD::XInput::RenderFrame()
{
    ImGuiIO& io = ImGui::GetIO();
    auto& style = ImGui::GetStyle();

    float pad_r = style.FramePadding.x;
    float button_sz = ImGui::GetFontSize();

    if (!s_fXInputIsEnabled)
        ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.50f, 0.18f, 0.17f, 1.00f));

    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
    ImGui::Begin("XInput devices", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();

    if (!s_fXInputIsEnabled)
        ImGui::PopStyleColor();

    auto title_bar_rect = ImGui::GetCurrentWindow()->TitleBarRect();
    ImVec2 collapse_button_pos = ImVec2(title_bar_rect.Max.x - pad_r - button_sz, title_bar_rect.Min.y + style.FramePadding.y);
    ImGui::PushClipRect(title_bar_rect.Min, title_bar_rect.Max, false);
    if (ImGui::CollapseButton(ImGui::GetID("#XINPUTOPTIONS"), collapse_button_pos))
    {
        ImGui::OpenPopup("XInput_Options");
    }
    ImGui::PopClipRect();

    if (ImGui::BeginPopup("XInput_Options", ImGuiWindowFlags_NoMove))
    {
        if (ImGui::Selectable("Enable XInput"))
            XInput_EnableDisable(TRUE);
        if (ImGui::Selectable("Disable XInput"))
            XInput_EnableDisable(FALSE);
        if (ImGui::Selectable("Enumerate devices"))
            GD::XInput::EnumerateDevices();
        ImGui::EndPopup();
    }

    auto avail = ImGui::GetContentRegionAvail();
    avail /= 2;
    avail -= ImGui::GetStyle().FramePadding;

    for (int i = 0; i < 4; i++)
    {
        ImGui::PushID(i);
        auto& device = s_XInputDevices[i];

        if (i % 2 != 0)
            ImGui::SameLine();

        ImGui::BeginDisabled(!device.connected);
        if (ImGui::BeginChild("controller", avail, ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_NoDecoration))
        {
            if (ImGui::BeginTable("table", 2, ImGuiTableFlags_BordersInner))
            {
                ImGui::TableSetupColumn("desc", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextColumn();
                ImGui::Text("XUser %d", i);
                ImGui::TableNextColumn();
                ImGui::Text(device.connected ? device.type.c_str() : "Disconnected");

                if (device.connected && device.features.any())
                {
                    ImGui::SameLine();
                    ImGui::TextDisabled("(f)");
                    if (ImGui::BeginItemTooltip())
                    {
                        if (device.features.voice)
                            ImGui::BulletText("Device has an integrated voice device.");
                        if (device.features.forceFeedback)
                            ImGui::BulletText("Device supports force feedback functionality.");
                        if (device.features.wireless)
                            ImGui::BulletText("Device is wireless.");
                        if (device.features.noNavigation)
                            ImGui::BulletText("Device lacks menu navigation buttons (START, BACK, DPAD).");
                        if (device.features.plugInModules)
                            ImGui::BulletText("Device supports plug-in modules.");
                        ImGui::EndTooltip();
                    }
                }

                ImRect xr = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 1);

                auto option_button_pos = ImVec2(xr.Max.x - pad_r - button_sz, xr.Min.y + style.FramePadding.y);
                if (ImGui::CollapseButton(ImGui::GetID("#OPTIONS"), option_button_pos))
                {
                    ImGui::OpenPopup("XInput_Controller_Options");
                }
                if (ImGui::BeginPopup("XInput_Controller_Options", ImGuiWindowFlags_NoMove))
                {
                    if (ImGui::Selectable("Power off"))
                        XInput_Poweroff(i);

                    ImGui::Separator();
                    if (ImGui::Selectable("Rumble left"))
                        XInput_SetRumble(i, std::numeric_limits<WORD>::max(), 0);
                    if (ImGui::Selectable("Rumble right"))
                        XInput_SetRumble(i, 0, std::numeric_limits<WORD>::max());
                    if (ImGui::Selectable("Rumble both"))
                        XInput_SetRumble(i, std::numeric_limits<WORD>::max(), std::numeric_limits<WORD>::max());
                    if (ImGui::Selectable("Stop rumble"))
                        XInput_SetRumble(i, 0, 0);

                    ImGui::EndPopup();
                }

                ImGui::TableNextColumn();

                ImGui::Text("Buttons");
                ImGui::TableNextColumn();
                {
                    ImGui::PushFont(io.Fonts->Fonts[1]);
                    std::string text;
                    append_dpad(text, device.buttons.DPad);
                    append_text_if(device.buttons.Start, text, CF_XBOX_MENU);
                    append_text_if(device.buttons.Back, text, CF_XBOX_VIEW);
                    append_text_if(device.buttons.LeftThumb, text, CF_ANALOG_L);
                    append_text_if(device.buttons.RightThumb, text, CF_ANALOG_R);
                    append_text_if(device.buttons.LeftShoulder, text, CF_XBOX_LEFT_SHOULDER);
                    append_text_if(device.buttons.RightShoulder, text, CF_XBOX_RIGHT_SHOULDER);
                    append_text_if(device.buttons.A, text, CF_XBOX_A);
                    append_text_if(device.buttons.B, text, CF_XBOX_B);
                    append_text_if(device.buttons.X, text, CF_XBOX_X);
                    append_text_if(device.buttons.Y, text, CF_XBOX_Y);
                    append_text_if(device.buttons.Guide, text, CF_ICON_XBOX);
                    ImGui::TextWrapped(text.c_str());
                    ImGui::PopFont();
                }


                ImGui::TableNextColumn();
                ImGui::Text("Triggers");
                ImGui::TableNextColumn();
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float height = ImGui::GetTextLineHeight();
                char buf[32];
                sprintf_s(buf, "%d", device.buttons.LeftTrigger);
                ImGui::ProgressBar(device.buttons.LeftTrigger / 255.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);
                ImGui::SameLine();
                sprintf_s(buf, "%d", device.buttons.RightTrigger);
                ImGui::ProgressBar(device.buttons.RightTrigger / 255.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);


                ImGui::TableNextColumn();
                ImGui::PushFont(io.Fonts->Fonts[1]);
                ImGui::Text(CF_ANALOG_L);
                auto gl = analog_glyph(device.buttons.LeftThumbPos, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                ImGui::SameLine();
                ImGui::Text(gl);
                ImGui::PopFont();
                ImGui::TableNextColumn();

                sprintf_s(buf, "X: %d", device.buttons.LeftThumbPos.X);
                ProgressBarEx(device.buttons.LeftThumbPos.X / 32767.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);
                ImGui::SameLine();
                sprintf_s(buf, "Y: %d", device.buttons.LeftThumbPos.Y);
                ProgressBarEx(device.buttons.LeftThumbPos.Y / 32767.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);

                ImGui::TableNextColumn();
                ImGui::PushFont(io.Fonts->Fonts[1]);
                ImGui::Text(CF_ANALOG_R);
                gl = analog_glyph(device.buttons.RightThumbPos, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
                ImGui::SameLine();
                ImGui::Text(gl);
                ImGui::PopFont();
                ImGui::TableNextColumn();
                sprintf_s(buf, "X: %d", device.buttons.RightThumbPos.X);
                ProgressBarEx(device.buttons.RightThumbPos.X / 32767.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);
                ImGui::SameLine();
                sprintf_s(buf, "Y: %d", device.buttons.RightThumbPos.Y);
                ProgressBarEx(device.buttons.RightThumbPos.Y / 32767.0f, ImVec2(avail.x / 2.f - style.FramePadding.x, 0.f), buf);

                ImGui::TableNextColumn();
                ImGui::Text("Battery");
                ImGui::TableNextColumn();
                {
                    std::string text = BatteryTypeToString(device.battery.Type);
                    if (device.battery.Type != BATTERY_TYPE_UNKNOWN)
                    {
                        if (device.battery.Level == BATTERY_LEVEL_EMPTY)
                            text += ", Empty";
                        else if (device.battery.Level == BATTERY_LEVEL_LOW)
                            text += ", Low";
                        else if (device.battery.Level == BATTERY_LEVEL_MEDIUM)
                            text += ", Medium";
                        else if (device.battery.Level == BATTERY_LEVEL_FULL)
                            text += ", Full";
                        else
                            text += ", Unknown: " + std::to_string(device.battery.Level);
                    }

                    ImGui::TextWrapped(text.c_str());
                }

                if (device.devInfo.any())
                {
                    ImGui::TableNextColumn();
                    ImGui::Text("Device");
                    ImGui::TextDisabled("(?)");
                    if (ImGui::BeginItemTooltip())
                    {
                        ImGui::Text("V = Vendor Id, P = Product Id, PV = Product Version");
                        ImGui::EndTooltip();
                    }

                    ImGui::TableNextColumn();
                    std::string text;

                    text += "V:";
                    sprintf_s(buf, "%04X", device.devInfo.vendorId);
                    text += buf;

                    text += ", P:";
                    sprintf_s(buf, "%04X", device.devInfo.productId);
                    text += buf;

                    text += ", PV:";
                    sprintf_s(buf, "%04X", device.devInfo.productVersion);
                    text += buf;

                    text += "\n";
                    text += GetDeviceType(device.devInfo.vendorId, device.devInfo.productId);

                    ImGui::TextWrapped(text.c_str());
                }

                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::EndDisabled();
        ImGui::PopID();
    }

    ImGui::End();
}

void GD::XInput::Update(double time)
{
    if (!s_XInputGetStateEx)
    {
        return;
    }
    bool updateBattery = false;
    if (s_LastBatteryUpdate == 0.0 || time >= s_LastBatteryUpdate)
    {
        s_LastBatteryUpdate = time + 30.0;
        updateBattery = true;
    }


    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        if (s_XInputDevices[i].connected)
        {
            XINPUT_STATE_EX state{};
            DWORD res = s_XInputGetStateEx(i, &state);
            if (res == ERROR_SUCCESS)
            {
                if (state.dwPacketNumber != s_XInputDevices[i].session)
                {
                    s_XInputDevices[i].session = state.dwPacketNumber;
                    auto& btn = s_XInputDevices[i].buttons;
                    btn.DPad.Up = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
                    btn.DPad.Down = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
                    btn.DPad.Left = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
                    btn.DPad.Right = (state.Gamepad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
                    btn.Start = (state.Gamepad.wButtons & XINPUT_GAMEPAD_START) != 0;
                    btn.Back = (state.Gamepad.wButtons & XINPUT_GAMEPAD_BACK) != 0;
                    btn.LeftThumb = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
                    btn.RightThumb = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
                    btn.LeftShoulder = (state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                    btn.RightShoulder = (state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
                    btn.A = (state.Gamepad.wButtons & XINPUT_GAMEPAD_A) != 0;
                    btn.B = (state.Gamepad.wButtons & XINPUT_GAMEPAD_B) != 0;
                    btn.X = (state.Gamepad.wButtons & XINPUT_GAMEPAD_X) != 0;
                    btn.Y = (state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) != 0;
                    btn.Guide = (state.Gamepad.wButtons & XINPUT_GAMEPAD_GUIDE) != 0;
                    btn.LeftTrigger = state.Gamepad.bLeftTrigger;
                    btn.RightTrigger = state.Gamepad.bRightTrigger;
                    btn.LeftThumbPos.X = state.Gamepad.sThumbLX;
                    btn.LeftThumbPos.Y = state.Gamepad.sThumbLY;
                    btn.RightThumbPos.X = state.Gamepad.sThumbRX;
                    btn.RightThumbPos.Y = state.Gamepad.sThumbRY;
                }
                if (updateBattery)
                {
                    XINPUT_BATTERY_INFORMATION batteryInfo{};
                    DWORD res = s_XInputGetBatteryInformation(i, BATTERY_DEVTYPE_GAMEPAD, &batteryInfo);
                    if (res == ERROR_SUCCESS)
                    {
                        s_XInputDevices[i].battery.Level = batteryInfo.BatteryLevel;
                        s_XInputDevices[i].battery.Type = batteryInfo.BatteryType;
                    }
                    else
                    {
                        GD_Log("Failed to get battery information for controller %d\n", i);
                    }
                }
            }
            else
            {
                GD_Log("XInput controller %d is lost\n", i);
                s_XInputDevices[i].clear();
            }
        }
    }
}

void GD::XInput::EnumerateDevices()
{
    if (!s_XInputGetStateEx)
    {
        return;
    }

    static_assert(_countof(s_XInputDevices) == XUSER_MAX_COUNT, "XInput devices array size mismatch");
    for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i)
    {
        XINPUT_CAPABILITIES capabilities{};

        DWORD res = s_XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &capabilities);
        bool isConnected = (res == ERROR_SUCCESS);
        if (isConnected != s_XInputDevices[i].connected)
        {
            s_XInputDevices[i].connected = isConnected;
            if (isConnected)
            {
                GD_Log("XInput controller %d is connected\n", i);
                if (s_XInputGetCapabilitiesEx)
                {
                    XINPUT_CAPABILITIES_EX capabilitiesEx{};
                    res = s_XInputGetCapabilitiesEx(1, i, 0, &capabilitiesEx);
                    if (res == ERROR_SUCCESS)
                    {
                        ///* Fixup for Wireless Xbox 360 Controller */
                        //if (capabilitiesEx.productId == 0 && capabilitiesEx.Capabilities.Flags & XINPUT_CAPS_WIRELESS)
                        //{
                        //    capabilitiesEx.vendorId = USB_VENDOR_MICROSOFT;
                        //    capabilitiesEx.productId = USB_PRODUCT_XBOX360_XUSB_CONTROLLER;
                        //}

                        s_XInputDevices[i].devInfo.vendorId = capabilitiesEx.vendorId;
                        s_XInputDevices[i].devInfo.productId = capabilitiesEx.productId;
                        s_XInputDevices[i].devInfo.productVersion = capabilitiesEx.productVersion;

                    }
                }
                s_XInputDevices[i].type = SubTypeToString(capabilities.SubType);
                s_XInputDevices[i].features.voice = (capabilities.Flags & XINPUT_CAPS_VOICE_SUPPORTED) != 0;
                s_XInputDevices[i].features.forceFeedback = (capabilities.Flags & XINPUT_CAPS_FFB_SUPPORTED) != 0;
                s_XInputDevices[i].features.wireless = (capabilities.Flags & XINPUT_CAPS_WIRELESS) != 0;
                s_XInputDevices[i].features.noNavigation = (capabilities.Flags & XINPUT_CAPS_NO_NAVIGATION) != 0;
                s_XInputDevices[i].features.plugInModules = (capabilities.Flags & XINPUT_CAPS_PMD_SUPPORTED) != 0;
                s_LastBatteryUpdate = 0.0;
            }
            else
            {
                GD_Log("XInput controller %d is disconnected\n", i);
                s_XInputDevices[i].clear();
            }
        }
    }
}

static void XInput_EnableDisable(BOOL fEnable)
{
    if (!s_XInputEnable)
    {
        GD_Log("Failed to get XInput Enable function\n");
        return;
    }

    GD_Log("XInput %s\n", fEnable ? "enabled" : "disabled");
    s_XInputEnable(fEnable);
    s_fXInputIsEnabled = fEnable != FALSE;

    GD::XInput::EnumerateDevices();
}

static void XInput_Poweroff(DWORD XUser)
{
    if (!s_XInputPowerOffController)
    {
        GD_Log("Failed to get XInput PowerOff function\n");
        return;
    }

    GD_Log("Powering off controller %d\n", XUser);
    DWORD res = s_XInputPowerOffController(XUser);
    if (res == ERROR_SUCCESS)
    {
        GD_Log("Powering off controller succeeded\n");
    }
    else
    {
        // Convert the winapi error code to a string
        char errorMsg[256]{};
        FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, res,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errorMsg, sizeof(errorMsg)-1, nullptr);

        char* tmp = errorMsg + strlen(errorMsg) - 1;
        while (tmp > errorMsg && *tmp == '\n')
        {
            *tmp = '\0';
            --tmp;
        }

        GD_Log("Powering off controller failed: ERROR %d (%s)\n", res, errorMsg);
    }
}

static void XInput_SetRumble(DWORD XUser, WORD left, WORD right)
{
    if (!s_XInputSetState)
    {
        GD_Log("Failed to get XInput SetState function\n");
        return;
    }

    GD_Log("Rumbling controller %d: left %d, right %d\n", XUser, left, right);
    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = left;
    vibration.wRightMotorSpeed = right;
    DWORD res = s_XInputSetState(XUser, &vibration);
    if (res == ERROR_SUCCESS)
    {
        GD_Log("Rumble succeeded\n");
    }
    else
    {
        GD_Log("Rumble failed: ERROR %d\n", res);
    }
}

void GD::XInput::Init()
{
    // Load XInput library
    s_XInputInstance = LoadLibraryA("xinput1_4.dll");
    if (s_XInputInstance)
    {
        GD_Log("Loaded xinput1_4.dll\n");
    }
    else if (s_XInputInstance = LoadLibraryA("xinput1_3.dll"))
    {
        GD_Log("Loaded xinput1_3.dll\n");
    }
    else
    {
        GD_Log("Failed to load XInput library\n");
        return;
    }

    // XInput does not export the Guide (Xbox) button from the XInputGetState function, so we try to resolve the private version here
    s_XInputGetStateEx = (tXInputGetStateEx)GetProcAddress(s_XInputInstance, (LPCSTR)100);
    if (!s_XInputGetStateEx)
    {
        // If we cannot resolve this for some reason (that really should not be happening), just use the public version.
        // Since the struct is the same (except for the padding), we can just give the private struct to the public function
        s_XInputGetStateEx = (tXInputGetStateEx)GetProcAddress(s_XInputInstance, "XInputGetState");
        GD_Log("Falling back to public XInputGetState\n");
    }
    s_XInputGetCapabilities = (decltype(XInputGetCapabilities)*)GetProcAddress(s_XInputInstance, "XInputGetCapabilities");
    s_XInputGetCapabilitiesEx = (tXInputGetCapabilitiesEx)GetProcAddress(s_XInputInstance, (LPCSTR)108);
    s_XInputGetBatteryInformation = (decltype(XInputGetBatteryInformation)*)GetProcAddress(s_XInputInstance, "XInputGetBatteryInformation");

    s_XInputPowerOffController = (tXInputPowerOffController)GetProcAddress(s_XInputInstance, (LPCSTR)103);
    s_XInputEnable = (tXInputEnable)GetProcAddress(s_XInputInstance, "XInputEnable");
    s_XInputSetState = (decltype(XInputSetState)*)GetProcAddress(s_XInputInstance, "XInputSetState");

    if (!s_XInputGetStateEx || !s_XInputGetCapabilities || !s_XInputGetBatteryInformation)
    {
        GD_Log("Failed to get XInput function pointers\n");
        GD::XInput::Shutdown();
        return;
    }
}

void GD::XInput::Shutdown()
{
    if (s_XInputInstance)
    {
        FreeLibrary(s_XInputInstance);
        s_XInputInstance = nullptr;
    }
}
