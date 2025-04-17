#include "imgui.h"
#include <tchar.h>

static void LogOutputFrame()
{
    ImGui::Begin("Log output", nullptr, ImGuiWindowFlags_NoDecoration);
    ImGui::End();
}


void GamepadDebugFrame()
{
    // Our state
    static bool show_demo_window = true;

    if (show_demo_window)
    {
        static bool once = true;
        if (once)
        {
            ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
            once = false;
        }
        ImGui::ShowDemoWindow(&show_demo_window);
    }

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    auto pos = viewport->WorkPos;
    auto size = viewport->WorkSize;
    size.x /= 2;

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
    LogOutputFrame();

    pos.x += size.x;

}
