#include "imgui.h"
#include "imgui_internal.h"
#include "gd_main.h"
#include "gd_log.h"
#include "gd_types.h"
#include "modules/Notifications.h"
#include "modules/gd_XInput.h"
#include "fonts/sourcecodepro.h"
#include "fonts/cf_xbox_one.h"


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

static void append_dpad(std::string& text, DPad dpad)
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
    //io.Fonts->AddFontDefault();
    SC_AddFont(io.Fonts);
    CF_AddFont(io.Fonts);
    //DInput_Init();
}

void GD_Shutdown()
{
    //DInput_Shutdown();
    XInput_Shutdown();
    Notifications_Shutdown();
}
