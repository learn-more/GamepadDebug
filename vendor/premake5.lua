

vendorBase = _SCRIPT_DIR

function fixPath(relative_path)
    return path.rebase(relative_path, vendorBase, _SCRIPT_DIR)
end

function add_imgui()
    includedirs { fixPath("imgui") }
    links {"imgui"}
end

group "vendor"
    project "imgui"
        kind "StaticLib"
        files { "imgui/**" }

group ""
