
include "scripts/actions.lua"

workspace "GamepadDebug"
    configurations { "Release", "Debug" }
    platforms { "Win32", "Win64" }
    location "build"
    language "C++"
    cppdialect "C++17"

filter { "platforms:Win32" }
    system "Windows"
    architecture "x86"

filter { "platforms:Win64" }
    system "Windows"
    architecture "x86_64"

filter "configurations:Debug"
    defines { "DEBUG" }
    symbols "On"

filter "configurations:Release"
    defines { "NDEBUG", "IMGUI_DISABLE_DEMO_WINDOWS" }
    optimize "On"

-- End filters
filter {}
    -- , "_SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING"
    defines { "NOMINMAX", "WIN32_LEAN_AND_MEAN" }

include "vendor"

project "GamepadDebug"
    kind "WindowedApp"
    files { "src/**.cpp", "src/**.h" }
    includedirs { "src/include" }
    links { "d3d9", "Cfgmgr32" }
    add_imgui {}
