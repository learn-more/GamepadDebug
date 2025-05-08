// PROJECT:     Gamepad Debug
// LICENSE:     MIT (https://spdx.org/licenses/MIT.html)
// PURPOSE:     Handle & display XInput devices
// COPYRIGHT:   Copyright 2025 Mark Jansen <mark.jansen@reactos.org>


namespace GD::XInput
{
    void RenderFrame();

    void Update(double time);
    void EnumerateDevices();
    void Init();
    void Shutdown();
}
