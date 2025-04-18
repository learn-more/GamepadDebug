#include "imgui.h"
#include <mutex>

using std::mutex;
using std::unique_lock;


static mutex s_Mutex;
static ImGuiTextBuffer s_Buf;
static ImVector<int> s_LineOffsets;

void GD_Log(const char* fmt, ...)
{
    int old_size = s_Buf.size();

    unique_lock<mutex> lock(s_Mutex);

    s_Buf.appendf("%7.1f | ", ImGui::GetTime());

    va_list args;
    va_start(args, fmt);
    s_Buf.appendfv(fmt, args);
    va_end(args);

    if (s_LineOffsets.Size == 0)
        s_LineOffsets.push_back(0);

    for (int new_size = s_Buf.size(); old_size < new_size; old_size++)
    {
        if (s_Buf[old_size] == '\n')
        {
            s_LineOffsets.push_back(old_size + 1);
        }
    }
}

static void Clear()
{
    unique_lock<mutex> lock(s_Mutex);
    s_Buf.clear();
    s_LineOffsets.clear();
    s_LineOffsets.push_back(0);
}

void GD_FrameLogger()
{
    ImGui::Begin("Log output", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
    bool clear = ImGui::Button("Clear");
    ImGui::SameLine();
    bool copy = ImGui::Button("Copy");

    ImGui::Separator();

    if (ImGui::BeginChild("scrolling", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
    {
        if (clear)
            Clear();
        if (copy)
            ImGui::LogToClipboard();

        {
            unique_lock<mutex> lock(s_Mutex);
            const char* buf = s_Buf.begin();
            const char* buf_end = s_Buf.end();
            ImGui::TextWrapped(buf, buf_end);
        }

        // Keep up at the bottom of the scroll region if we were already at the bottom at the beginning of the frame.
        // Using a scrollbar or mouse-wheel will take away from the bottom edge.
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}
