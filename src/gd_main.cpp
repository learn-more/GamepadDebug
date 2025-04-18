#include "imgui.h"
#include "gd_main.h"
#include "gd_log.h"
#include "gd_types.h"
#include "modules/Notifications.h"
#include "modules/gd_XInput.h"
#include "fonts/promptfont.h"


void append_text_comma_if(bool show, std::string& output, const char* text)
{
    if (show)
    {
        if (!output.empty())
            output += ", ";
        output += text;
    }
}

void append_text_if(bool show, std::string& output, const char* text)
{
    if (show)
    {
        output += text;
    }
}

static void XInput_RenderFrame()
{
    ImGui::Begin("XInput", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::Text("XInput devices");
    ImGui::Separator();

    auto avail = ImGui::GetContentRegionAvail();
    avail /= 2;

    for (int i = 0; i < 4; i++)
    {
        ImGui::PushID(i);
        auto& device = XInput_Devices()[i];
        ImGui::BeginDisabled(!device.connected);
        if (ImGui::BeginChild("controller", avail, ImGuiChildFlags_Borders, ImGuiWindowFlags_NoDecoration))
        {
            if (ImGui::BeginTable("table", 2, ImGuiTableFlags_BordersInner))
            {
                ImGui::TableSetupColumn("desc", ImGuiTableColumnFlags_WidthFixed);
                ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch);

                ImGui::TableNextColumn();
                ImGui::Text("XUser %d", i);
                ImGui::TableNextColumn();
                ImGui::Text(device.connected ? device.type.c_str() : "Disconnected");
                ImGui::TableNextColumn();

                ImGui::Text("Features");
                ImGui::TableNextColumn();
                {
                    std::string text;
                    append_text_comma_if(device.features.voice, text, "Voice");
                    append_text_comma_if(device.features.forceFeedback, text, "Force Feedback");
                    append_text_comma_if(device.features.wireless, text, "Wireless");
                    append_text_comma_if(device.features.noNavigation, text, "No Navigation");
                    ImGui::TextWrapped(text.c_str());
                }
                ImGui::TableNextColumn();

                ImGui::Text("Buttons");
                ImGui::TableNextColumn();
                {
                    ImGuiIO& io = ImGui::GetIO();
                    ImGui::PushFont(io.Fonts->Fonts[1]);
                    std::string text;
                    append_text_if(device.buttons.DPadUp, text, PF_XBOX_DPAD_UP);
                    append_text_if(device.buttons.DPadDown, text, PF_XBOX_DPAD_DOWN);
                    append_text_if(device.buttons.DPadLeft, text, PF_XBOX_DPAD_LEFT);
                    append_text_if(device.buttons.DPadRight, text, PF_XBOX_DPAD_RIGHT);
                    append_text_if(device.buttons.Start, text, PF_XBOX_MENU);
                    append_text_if(device.buttons.Back, text, PF_XBOX_VIEW);
                    append_text_if(device.buttons.LeftThumb, text, PF_ANALOG_L);
                    append_text_if(device.buttons.RightThumb, text, PF_ANALOG_R);
                    append_text_if(device.buttons.LeftShoulder, text, PF_XBOX_LEFT_SHOULDER);
                    append_text_if(device.buttons.RightShoulder, text, PF_XBOX_RIGHT_SHOULDER);
                    append_text_if(device.buttons.A, text, PF_XBOX_A);
                    append_text_if(device.buttons.B, text, PF_XBOX_B);
                    append_text_if(device.buttons.X, text, PF_XBOX_X);
                    append_text_if(device.buttons.Y, text, PF_XBOX_Y);
                    append_text_if(device.buttons.Guide, text, PF_ICON_XBOX);
                    ImGui::TextWrapped(text.c_str());
                    ImGui::PopFont();
                }
                
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::EndDisabled();
        if (i % 2 == 0)
            ImGui::SameLine();
        ImGui::PopID();
    }

    ImGui::End();
}


void GD_Frame()
{
    if (Notifications_DevicesChanged())
    {
        XInput_EnumerateDevices();
        //DInput_EnumerateDevices();
    }

    XInput_Update();


#if !defined(IMGUI_DISABLE_DEMO_WINDOWS)
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
#endif

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    auto pos = viewport->WorkPos;
    auto split_height = viewport->WorkSize;
    split_height.y *= .6f;

    {
        // Do something!
        auto quarter_pos = pos;
        auto quarter_size = split_height;
        quarter_size.x /= 2;

        ImGui::SetNextWindowPos(quarter_pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(quarter_size, ImGuiCond_Always);
        XInput_RenderFrame();
    }


    pos.y += split_height.y;
    split_height.y = viewport->WorkSize.y - split_height.y;
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(split_height, ImGuiCond_Always);
    GD_FrameLogger();
}

void GD_Init()
{
    Notifications_Init();
    XInput_Init();
    ImGuiIO& io = ImGui::GetIO();
    PF_BuildAtlas(io.Fonts, false);
    //DInput_Init();
}

void GD_Shutdown()
{
    //DInput_Shutdown();
    XInput_Shutdown();
    Notifications_Shutdown();
}
