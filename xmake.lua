add_rules("mode.debug", "mode.release")

-- NOTE: BedrockTools pins dependencies with `package.requires_lock` plus a
-- committed xmake-requires.lock. Seed-Taker omits the lock policy until the
-- first successful CI build generates a lockfile worth pinning.

package("preloader")
    set_homepage("https://github.com/LiteLDev/preloader-android")
    set_description("Preloader Android")
    add_urls("https://github.com/LiteLDev/preloader-android.git")
    add_versions("main", "main")
    add_deps("cmake")
    on_install("android", function (package)
        import("package.tools.cmake").install(package)
    end)
package_end()

add_requires("preloader")
add_requires("nlohmann_json v3.11.3")

target("SeedTaker")
    set_kind("shared")
    set_languages("c++20")
    set_strip("all")
    add_files("src/main.cpp", "src/core/**.cpp", "src/config/*.cpp", "src/launcher/*.cpp", "src/modules/**.cpp")
    add_includedirs("include", {public = true})
    add_includedirs("src")
    add_packages("preloader", "nlohmann_json")

    if is_plat("android") then
        add_cxflags("-fPIC", "-O2", "-ffunction-sections", "-fdata-sections", "-fexceptions", "-w")
        add_cxxflags("-fno-rtti")
        add_shflags("-Wl,--gc-sections", "-Wl,--hash-style=gnu", "-Wl,-z,max-page-size=16384")
        add_links("android", "log", "EGL")
    end

    after_build(function (target)
        if not target:is_plat("android") then return end
        import("lib.detect.find_tool")
        local python = find_tool("python3") or find_tool("python")
        local args = {}
        if not python then
            python = find_tool("py")
            if python then table.insert(args, "-3") end
        end
        assert(python, "Python 3 is required to package SeedTaker.levipack")
        table.insert(args, path.join(os.projectdir(), "scripts", "package_levipack.py"))
        table.insert(args, "--library")
        table.insert(args, target:targetfile())
        table.insert(args, "--version-header")
        table.insert(args, path.join(os.projectdir(), "include", "seedtaker", "Version.hpp"))
        local icon = path.join(os.projectdir(), "assets", "seedtaker.png")
        if os.isfile(icon) then
            table.insert(args, "--icon")
            table.insert(args, icon)
        end
        table.insert(args, "--output")
        table.insert(args, path.join(target:targetdir(), "SeedTaker.levipack"))
        os.vrunv(python.program, args)
    end)
