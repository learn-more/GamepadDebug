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

constexpr auto XINPUT_GAMEPAD_GUIDE = 0x0400;

typedef DWORD (WINAPI *tXInputGetStateEx)(_In_ DWORD dwUserIndex, XINPUT_STATE_EX* pState);
typedef DWORD(WINAPI* tXInputPowerOffController)(DWORD XUser);


static HINSTANCE s_XInputInstance = nullptr;
static tXInputGetStateEx s_XInputGetStateEx = nullptr;
static decltype(XInputGetCapabilities)* s_XInputGetCapabilities = nullptr;
static decltype(XInputGetBatteryInformation)* s_XInputGetBatteryInformation = nullptr;
static double s_LastBatteryUpdate = 0.0;


static GD::XInput::GamePadState s_XInputDevices[4]{};

static void XInput_Poweroff(DWORD XUser);

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

static bool s_DontConfirmPoweroff = false;
static bool ConfirmPoweroffModal()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    bool confirmed = false;
    if (ImGui::BeginPopupModal("Power off?", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("The controller will be powered off.\n\nNote: This does not work for every controller!");
        ImGui::Separator();

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
        ImGui::Checkbox("Don't ask me next time", &s_DontConfirmPoweroff);
        ImGui::PopStyleVar();

        if (ImGui::Button("OK", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
            confirmed = true;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    return confirmed;
}

void GD::XInput::RenderFrame()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
    ImGui::Begin("XInput devices", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();

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

                auto& style = ImGui::GetStyle();
                float pad_r = style.FramePadding.x;
                float button_sz = ImGui::GetFontSize();

                ImRect xr = ImGui::TableGetCellBgRect(ImGui::GetCurrentTable(), 1);

                auto close_button_pos = ImVec2(xr.Max.x - pad_r - button_sz, xr.Min.y + style.FramePadding.y);
                if (ImGui::CloseButton(ImGui::GetID("#DISCONNECT"), close_button_pos))
                {
                    if (s_DontConfirmPoweroff)
                        XInput_Poweroff(i);
                    else
                        ImGui::OpenPopup("Power off?");
                }

                if (ConfirmPoweroffModal())
                    XInput_Poweroff(i);

                ImGui::TableNextColumn();

                ImGui::Text("Buttons");
                ImGui::TableNextColumn();
                {
                    ImGuiIO& io = ImGui::GetIO();
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
                ImGui::Text("LTrigger");
                ImGui::TableNextColumn();
                char buf[32];
                sprintf_s(buf, "%d", device.buttons.LeftTrigger);
                ImGui::ProgressBar(device.buttons.LeftTrigger / 255.0f, ImVec2(-FLT_MIN, 0.f), buf);

                ImGui::TableNextColumn();
                ImGui::Text("RTrigger");
                ImGui::TableNextColumn();
                sprintf_s(buf, "%d", device.buttons.RightTrigger);
                ImGui::ProgressBar(device.buttons.RightTrigger / 255.0f, ImVec2(-FLT_MIN, 0.f), buf);

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

static void XInput_Poweroff(DWORD XUser)
{
    tXInputPowerOffController XInputPowerOffController = (tXInputPowerOffController)GetProcAddress(s_XInputInstance, (LPCSTR)103);
    if (!XInputPowerOffController)
    {
        GD_Log("Failed to get XInput PowerOff function\n");
        return;
    }

    GD_Log("Powering off controller %d\n", XUser);
    DWORD res = XInputPowerOffController(XUser);
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
    s_XInputGetBatteryInformation = (decltype(XInputGetBatteryInformation)*)GetProcAddress(s_XInputInstance, "XInputGetBatteryInformation");

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
