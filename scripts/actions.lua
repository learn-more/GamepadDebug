

local function selected_platforms()
    if _OPTIONS.platform == 'All' then
        return {'Win64', 'Win32'}
    end
    return {_OPTIONS.platform}
end

newaction {
    trigger = "build",
    description = "Build the project using the specified platform and configuration",
    execute = function()
        for _, platform in pairs(selected_platforms()) do
            print('Building', platform, _OPTIONS.configuration)
            local res = os.executef('msbuild /m /p:Configuration=%s /p:Platform=%s build/GamepadDebug.sln', _OPTIONS.configuration, platform)
            if not res then
                os.exit(1)
            end
        end
    end,
}

newoption {
    trigger     = "platform",
    value       = "Win64",
    description = "The platform to use",
    default     = "Win64",
    category    = "Build/Test Options",
    allowed = {
       { "Win64",   "Select the x64 architecture" },
       { "Win32",   "Select the x86 architecture" },
       { "All",     "Select all supported platforms" },
    }
 }

 newoption {
    trigger     = "configuration",
    value       = "Release",
    description = "The configuration to use",
    default     = "Release",
    category    = "Build/Test Options",
    allowed = {
       { "Release",     "The optimized version" },
       { "Debug",       "The debug version" },
       { "Profiling",   "The debug version with profiling" },
    }
 }


