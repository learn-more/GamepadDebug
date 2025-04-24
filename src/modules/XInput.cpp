#include "gd_win32.h"
#include "gd_log.h"
#include "gd_types.h"
#include "modules/gd_XInput.h"
#include <Xinput.h>

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


static GamePadState s_XInputDevices[4]{};

GamePadState* XInput_Devices()
{
    return s_XInputDevices;
}


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

void XInput_Update()
{
    if (!s_XInputGetStateEx)
    {
        return;
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


void XInput_EnumerateDevices()
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
            }
            else
            {
                GD_Log("XInput controller %d is disconnected\n", i);
                s_XInputDevices[i].clear();
            }
        }
    }
}

void XInput_Poweroff(uint32_t XUser)
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


void XInput_Init()
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

    if (!s_XInputGetStateEx || !s_XInputGetCapabilities)
    {
        GD_Log("Failed to get XInput function pointers\n");
        XInput_Shutdown();
        return;
    }
}

void XInput_Shutdown()
{
    if (s_XInputInstance)
    {
        FreeLibrary(s_XInputInstance);
        s_XInputInstance = nullptr;
    }
}
