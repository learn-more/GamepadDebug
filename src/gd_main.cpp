#include "imgui.h"
#include "gd_main.h"
#include "gd_log.h"
#include "modules/Notifications.h"
#include "modules/gd_XInput.h"
#include "modules/gd_DInput.h"
#include "fonts/sourcecodepro.h"
#include "fonts/cf_xbox_one.h"

void GD_Frame()
{
    if (Notifications_DevicesChanged())
    {
        GD::XInput::EnumerateDevices();
        GD::DInput::EnumerateDevices();
    }

    GD::XInput::Update(ImGui::GetTime());
    GD::DInput::Update();

#if !defined(IMGUI_DISABLE_DEMO_WINDOWS)
    static bool show_demo_window = false;

    if (ImGui::IsKeyPressed(ImGuiKey_F1))
        show_demo_window = true;

    if (show_demo_window)
    {
        ImGui::ShowDemoWindow(&show_demo_window);
    }
#endif

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    auto pos = viewport->WorkPos;
    auto split_height = viewport->WorkSize;
    split_height.y *= .6f;

    {
        auto quarter_pos = pos;
        auto quarter_size = split_height;
        quarter_size.x /= 2;

        ImGui::SetNextWindowPos(quarter_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(quarter_size, ImGuiCond_Always);
        GD::XInput::RenderFrame();

        quarter_pos.x += quarter_size.x;
        quarter_size.x = viewport->WorkSize.x - quarter_size.x;
        ImGui::SetNextWindowPos(quarter_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(quarter_size, ImGuiCond_Always);
        GD::DInput::RenderFrame();
    }


    pos.y += split_height.y;
    split_height.y = viewport->WorkSize.y - split_height.y;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(split_height, ImGuiCond_Always);
    GD_FrameLogger();
}

void GD_Init()
{
    ImGuiIO& io = ImGui::GetIO();
    SC_AddFont(io.Fonts);

    Notifications_Init();
    GD::XInput::Init();
    GD::DInput::Init();
    CF_AddFont(io.Fonts);
}

void GD_Shutdown()
{
    GD::DInput::Shutdown();
    GD::XInput::Shutdown();
    Notifications_Shutdown();
}
