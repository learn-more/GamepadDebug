// PROJECT:     Gamepad Debug
// LICENSE:     MIT (https://spdx.org/licenses/MIT.html)
// PURPOSE:     Handle & display DirectInput controllers
// COPYRIGHT:   Copyright 2025 Mark Jansen <mark.jansen@reactos.org>


namespace GD::DInput
{
    void RenderFrame();

    void Update();
    void EnumerateDevices();
    void Init();
    void Shutdown();
}
