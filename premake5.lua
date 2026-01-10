local build_root = "IDE_project"
local BIN_DIR = "bin"

-- =========================================================
-- Workspace Configuration
-- =========================================================
workspace "SBP"
    architecture "x64"
    configurations { "Debug", "Release" }
    location (build_root)

    -- Common output layout
    targetdir (BIN_DIR)
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
        linkoptions { "-fopenmp" }

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


-- =========================================================
-- Custom Actions for Running and Benchmarking
-- =========================================================

-- Helper function to check if a file exists
local function file_exists(name)
    local f = io.open(name, "r")
    if f ~= nil then
        io.close(f)
        return true
    else
        return false
    end
end

-- Helper function to get number of CPU cores
local function get_num_cores()
    local handle
    if os.host() == "windows" then
        handle = io.popen("echo %NUMBER_OF_PROCESSORS%")
    else
        handle = io.popen("nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4")
    end
    local result = handle:read("*a")
    handle:close()
    return tonumber(result:match("%d+")) or 4
end

-- Action: run - Run the experiment with default settings
newaction {
    trigger = "run",
    description = "Build and run the SBP experiment",
    execute = function()
        print("Building project...")
        os.execute("./premake5 gmake2")
        os.execute("cd " .. build_root .. " && make config=release -j" .. get_num_cores())
        
        print("\n" .. string.rep("=", 60))
        print("Running SBP Experiment")
        print(string.rep("=", 60) .. "\n")
        
        if file_exists(BIN_DIR .. "/sbp_experiment") then
            os.execute(BIN_DIR .. "/sbp_experiment")
        else
            print("ERROR: sbp_experiment not found. Build may have failed.")
            os.exit(1)
        end
    end
}

-- Action: benchmark - Run the full benchmark suite
newaction {
    trigger = "benchmark",
    description = "Build and run the full benchmark suite",
    execute = function()
        print("Building project...")
        os.execute("./premake5 gmake2")
        os.execute("cd " .. build_root .. " && make config=release -j" .. get_num_cores())
        
        print("\n" .. string.rep("=", 60))
        print("Running Full Benchmark Suite")
        print(string.rep("=", 60) .. "\n")
        
        -- Create results directory if it doesn't exist
        os.execute("mkdir -p results")
        
        if file_exists(BIN_DIR .. "/sbp_benchmark") then
            os.execute(BIN_DIR .. "/sbp_benchmark")
            
            print("\n" .. string.rep("=", 60))
            print("Benchmark complete! Results saved to results/benchmark_results.csv")
            print(string.rep("=", 60) .. "\n")
            
            -- Show quick summary if analyze script exists
            if file_exists("scripts/analyze_results.sh") then
                print("Running analysis...\n")
                os.execute("./scripts/analyze_results.sh")
            end
        else
            print("ERROR: sbp_benchmark not found. Build may have failed.")
            os.exit(1)
        end
    end
}

-- Action: compare - Compare algorithms with different thread counts
newaction {
    trigger = "compare",
    description = "Compare Top-Down vs Bottom-Up with thread scaling",
    execute = function()
        print("Building project...")
        os.execute("./premake5 gmake2")
        os.execute("cd " .. build_root .. " && make config=release -j" .. get_num_cores())
        
        if not file_exists(BIN_DIR .. "/sbp_experiment") then
            print("ERROR: sbp_experiment not found. Build may have failed.")
            os.exit(1)
        end
        
        local num_cores = get_num_cores()
        local thread_counts = {1, 4, 8, 16, math.min(20, num_cores), num_cores}
        
        print("\n" .. string.rep("=", 70))
        print("Algorithm Comparison - Thread Scaling Analysis")
        print("System: " .. num_cores .. " CPU cores")
        print("Graph: N=200, K=4 (from sbp_experiment)")
        print(string.rep("=", 70) .. "\n")
        
        -- Header
        print(string.format("%-12s | %-15s | %-15s | %-10s", 
            "Threads", "Top-Down", "Bottom-Up", "Speedup"))
        print(string.rep("-", 70))
        
        local baseline_bu = nil
        
        for _, threads in ipairs(thread_counts) do
            -- Set environment variable and run
            local cmd = "OMP_NUM_THREADS=" .. threads .. " " .. BIN_DIR .. "/sbp_experiment 2>&1"
            local handle = io.popen(cmd)
            local output = handle:read("*a")
            handle:close()
            
            -- Parse output for Top-Down timing
            local td_time = output:match("Top%-Down.-Finished in ([%d%.]+)s")
            
            -- Parse output for Bottom-Up timing
            local bu_time = output:match("Bottom%-Up.-Finished in ([%d%.]+)s")
            
            if td_time and bu_time then
                td_time = tonumber(td_time)
                bu_time = tonumber(bu_time)
                
                if baseline_bu == nil then
                    baseline_bu = bu_time
                end
                
                local speedup = baseline_bu / bu_time
                
                print(string.format("%-12s | %-15s | %-15s | %-10s", 
                    threads .. " threads",
                    string.format("%.6fs", td_time),
                    string.format("%.6fs", bu_time),
                    string.format("%.2fx", speedup)))
            else
                print(string.format("%-12s | ERROR: Could not parse output", threads .. " threads"))
            end
        end
        
        print(string.rep("=", 70))
        print("\nRecommendation: Use 16-20 threads for optimal performance")
        print(string.rep("=", 70) .. "\n")
    end
}

-- Action: test-threads - Test specific thread count
newaction {
    trigger = "test-threads",
    description = "Test with specific thread count (use: premake5 test-threads --threads=N)",
    execute = function()
        local threads = _OPTIONS["threads"] or "16"
        
        print("Building project...")
        os.execute("./premake5 gmake2")
        os.execute("cd " .. build_root .. " && make config=release -j" .. get_num_cores())
        
        if file_exists(BIN_DIR .. "/sbp_experiment") then
            print("\n" .. string.rep("=", 60))
            print("Running with " .. threads .. " threads")
            print(string.rep("=", 60) .. "\n")
            
            os.execute("OMP_NUM_THREADS=" .. threads .. " " .. BIN_DIR .. "/sbp_experiment")
        else
            print("ERROR: sbp_experiment not found. Build may have failed.")
            os.exit(1)
        end
    end
}

-- Add option for thread count
newoption {
    trigger = "threads",
    value = "NUMBER",
    description = "Number of threads to use (for test-threads action)",
    default = "16"
}

-- Action: clean - Clean all build artifacts
newaction {
    trigger = "clean",
    description = "Clean all build artifacts",
    execute = function()
        print("Cleaning build artifacts...")
        os.execute("rm -rf " .. build_root)
        os.execute("rm -rf " .. BIN_DIR)
        os.execute("rm -f Makefile *.make")
        print("Clean complete!")
    end
}
