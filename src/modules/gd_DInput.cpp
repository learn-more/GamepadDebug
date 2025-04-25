#define INITGUID
#define DIRECTINPUT_VERSION 0x0800
#include "gd_win32.h"
#include "gd_log.h"
#include "modules/gd_DInput.h"
#include "imgui.h"
#include "objbase.h"
#include <dinput.h>
#include <strsafe.h>
#include <string>
#include <vector>

struct DIDevice
{
    std::string TypeName;
    std::string TypeDesc;

    std::string InstanceGuid;
    std::string ProductGuid;
};

static bool s_CoInitialized = false;
static IDirectInput8A* s_DirectInput = nullptr;
static std::vector<DIDevice> s_Devices;

std::string devTypeToStr(DWORD dwDevType)
{
    std::string typeStr;
    BYTE bType = LOBYTE(dwDevType);
    BYTE bSubType = HIBYTE(LOWORD(dwDevType));
    switch (bType)
    {
    case DI8DEVTYPE_JOYSTICK:
        typeStr = "Joystick";
        switch (bSubType)
        {
        case DI8DEVTYPEJOYSTICK_LIMITED:
            typeStr += " (Limited)";
            break;
        case DI8DEVTYPEJOYSTICK_STANDARD:
            typeStr += " (Standard)";
            break;
        default:
            typeStr += " (Unknown):" + std::to_string(bSubType);
            break;
        }
        break;
    case DI8DEVTYPE_GAMEPAD:
        typeStr = "Gamepad";
        switch (bSubType)
        {
        case DI8DEVTYPEGAMEPAD_LIMITED:
            typeStr += " (Limited)";
            break;
        case DI8DEVTYPEGAMEPAD_STANDARD:
            typeStr += " (Standard)";
            break;
        case DI8DEVTYPEGAMEPAD_TILT:
            typeStr += " (Tilt)";
            break;
        default:
            typeStr += " (Unknown):" + std::to_string(bSubType);
            break;
        }
        break;
    case DI8DEVTYPE_1STPERSON:
        typeStr = "1st Person";

        switch (bSubType)
        {
        case DI8DEVTYPE1STPERSON_LIMITED:
            typeStr += " (Limited)";
            break;
        case DI8DEVTYPE1STPERSON_UNKNOWN:
            typeStr += " (Unknown)";
            break;
        case DI8DEVTYPE1STPERSON_SIXDOF:
            typeStr += " (SixDOF)";
            break;
        case DI8DEVTYPE1STPERSON_SHOOTER:
            typeStr += " (Shooter)";
            break;
        default:
            typeStr += " (Unknown):" + std::to_string(bSubType);
            break;
        }
        break;
    default:
        typeStr = "Unknown:" + std::to_string(bType);
        break;
    }
    return typeStr;
}


void GD::DInput::RenderFrame()
{
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImGui::GetStyleColorVec4(ImGuiCol_TitleBgActive));
    ImGui::Begin("DInput devices", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::PopStyleColor();

    for (const auto& device : s_Devices)
    {
        ImGui::Text("Device: %s", device.TypeName.c_str());
        ImGui::Text("Type: %s", device.TypeDesc.c_str());

        ImGui::Text("Instance: %s", device.InstanceGuid.c_str());
        ImGui::Text("Product: %s", device.ProductGuid.c_str());

        ImGui::Separator();
    }


    ImGui::End();
}

void GD::DInput::Update()
{
}


static std::string FormatGuid(const GUID& guid)
{
    char buf[64];
    StringCchPrintfA(buf, _countof(buf), "{%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
    return buf;
}


static BOOL CALLBACK EnumDeviceCallback(const DIDEVICEINSTANCEA* pDeviceInstance, LPVOID pContext)
{
    if (!(pDeviceInstance->dwDevType & DIDEVTYPE_HID))
    {
        return DIENUM_CONTINUE;
    }

    s_Devices.push_back({});
    auto& dev = s_Devices.back();
    dev.TypeName = pDeviceInstance->tszInstanceName;
    dev.TypeDesc = devTypeToStr(pDeviceInstance->dwDevType);

    dev.InstanceGuid = FormatGuid(pDeviceInstance->guidInstance);
    dev.ProductGuid = FormatGuid(pDeviceInstance->guidProduct);

    return DIENUM_CONTINUE;
}

void GD::DInput::EnumerateDevices()
{
    s_Devices.clear();
    s_DirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumDeviceCallback, NULL, DIEDFL_ATTACHEDONLY);
}

void GD::DInput::Init()
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr))
    {
        GD_Log("CoInitializeEx failed: %08X\n", hr);
        return;
    }
    else
    {
        s_CoInitialized = true;
    }

    hr = CoCreateInstance(CLSID_DirectInput8, NULL, CLSCTX_INPROC_SERVER, IID_IDirectInput8A, (LPVOID*)&s_DirectInput);
    if (FAILED(hr))
    {
        GD_Log("CoCreateInstance failed: %08X\n", hr);
        return GD::DInput::Shutdown();
    }

    hr = s_DirectInput->Initialize(GetModuleHandle(NULL), DIRECTINPUT_VERSION);
    if (FAILED(hr))
    {
        GD_Log("DirectInput Initialize failed: %08X\n", hr);
        return GD::DInput::Shutdown();
    }
    GD_Log("DirectInput initialized\n");
}

void GD::DInput::Shutdown()
{
    if (s_DirectInput)
    {
        s_DirectInput->Release();
        s_DirectInput = nullptr;
    }
    if (s_CoInitialized)
    {
        CoUninitialize();
        s_CoInitialized = false;
    }
}
