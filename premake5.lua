local build_root = "IDE_project"

workspace "SBP"
    architecture "x64"
    configurations { "Debug", "Release" }
    location (build_root)

    -- Common output layout
    targetdir (build_root .. "/bin/%{cfg.buildcfg}")
    objdir (build_root .. "/obj/%{prj.name}/%{cfg.buildcfg}")

-- =========================================================
-- Common settings function (DRY)
-- =========================================================
function common_settings()
    language "C++"
    cppdialect "C++20"
    warnings "Extra"
    staticruntime "off"

    includedirs { "src" }
	files {
		"src/**.h",
		"src/**.hpp"
	}

    -- =====================================================
    -- OpenMP support
    -- =====================================================
    filter "toolset:gcc or toolset:clang"
        buildoptions { "-fopenmp" }
        links { "gomp", "omp" }

    filter "toolset:msc"
        buildoptions { "/openmp" }

    -- =====================================================
    -- Debug configuration
    -- =====================================================
    filter "configurations:Debug"
        symbols "On"

    -- AddressSanitizer for GCC/Clang
    filter { "configurations:Debug", "toolset:gcc or toolset:clang" }
        buildoptions {
            "-fsanitize=address",
            "-fno-omit-frame-pointer"
        }
        linkoptions { "-fsanitize=address" }

    -- Optional: MSVC ASan (VS 2019+)
    filter { "configurations:Debug", "toolset:msc" }
        buildoptions { "/fsanitize=address" }

    -- =====================================================
    -- Release configuration
    -- =====================================================
    filter "configurations:Release"
        optimize "Full"
        defines { "NDEBUG" }

    filter {}
end


-- =========================================================
-- sbp_experiment executable
-- =========================================================
project "sbp_experiment"
    kind "ConsoleApp"
    common_settings()

    files {
        "src/top_down_sbp.cpp",
        "src/bottom_up_sbp.cpp",
        "src/main_sbp.cpp"
    }

-- =========================================================
-- sbp_benchmark executable
-- =========================================================
project "sbp_benchmark"
    kind "ConsoleApp"
    common_settings()

    files {
        "src/top_down_sbp.cpp",
        "src/bottom_up_sbp.cpp",
        "src/benchmark_sbp.cpp"
    }
