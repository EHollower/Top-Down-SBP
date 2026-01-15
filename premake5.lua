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
        "src/algorithms/top_down_sbp.cpp",
        "src/algorithms/bottom_up_sbp.cpp",
        "src/main_sbp.cpp"
    }

-- =========================================================
-- sbp_benchmark executable
-- =========================================================
project "sbp_benchmark"
    kind "ConsoleApp"
    common_settings()

    files {
        "src/algorithms/top_down_sbp.cpp",
        "src/algorithms/bottom_up_sbp.cpp",
        "src/benchmark_sbp.cpp"
    }


-- =========================================================
-- Custom Actions for Running and Benchmarking
-- =========================================================

-- Helper function to check if a file exists
local function file_exists(name)
	if os.host() == "windows" then
		name = string.gsub(name, "/", "\\")
	end
	
    local f = io.open(name, "r")
    if f ~= nil then
        io.close(f)
        return true
    else
        return false
    end
end

local function executable_exists(name)
	if os.host() == "windows" then
		name = name .. ".exe"
	end
	return file_exists(name)
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

local function build_project_and_compile()
	local build_cmd = ""
	if os.host() == "windows" then
		-- Generate Visual Studio project files
		os.execute("premake5 vs2022")

		-- Build using MSBuild
		build_cmd = string.format(
			'msbuild "%s\\SBP.sln" /p:Configuration=Release /p:Platform=x64 /m',
			build_root
		)
	else
		-- Linux / Unix-like systems
		os.execute("./premake5 gmake2")

		build_cmd = string.format(
			"cd %s && make config=release -j%d",
			build_root,
			get_num_cores()
		)
	end
	
	os.execute(build_cmd)
end

local function run_executable(exe_name)
	local file = exe_name
	if os.host() == "windows" then
		file = file .. ".exe"
		file = string.gsub(file, "/", "\\")
	end
	
	if file_exists(file) then
		os.execute(file)
	else
		return false
	end
	
	return true
end

function mkdir_p(dir)
    if os.host() == "windows" then
        -- Windows
		dir = string.gsub(dir, "/", "\\")
        os.execute('mkdir "' .. dir .. '" >nul 2>nul')
    else
        -- Linux / macOS
        os.execute('mkdir -p "' .. dir .. '"')
    end
end

-- Action: run - Run the experiment with default settings
newaction {
    trigger = "run",
    description = "Build and run the SBP experiment",
    execute = function()
        print("Building project...")
        build_project_and_compile()
		
        print("\n" .. string.rep("=", 60))
        print("Running SBP Experiment")
        print(string.rep("=", 60) .. "\n")
		
		if executable_exists(BIN_DIR .. "/sbp_experiment") then
            run_executable(BIN_DIR .. "/sbp_experiment")
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
		build_project_and_compile()
        
        print("\n" .. string.rep("=", 60))
        print("Running Full Benchmark Suite")
        print(string.rep("=", 60) .. "\n")
        
        -- Create results directory if it doesn't exist
        mkdir_p("results")
		
        if executable_exists(BIN_DIR .. "/sbp_benchmark") then
			run_executable(BIN_DIR .. "/sbp_benchmark")
            
            print("\n" .. string.rep("=", 60))
            print("Benchmark complete! Results saved to results/benchmark_results.csv")
            print(string.rep("=", 60) .. "\n")
            
            -- Show quick summary if analyze script exists
            if file_exists("scripts/analyze_results.sh") then
                print("Running analysis...\n")
				if os.host() == "windows" then
					os.execute("bash.exe --login -i " .. "\"scripts\\analyze_results.sh\"")
				else
					os.execute("./scripts/analyze_results.sh")
				end
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
        build_project_and_compile()
        
        if not executable_exists(BIN_DIR .. "/sbp_experiment") then
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
			local cmd = ""
			if os.host() == "windows" then -- Windows cmd.exe syntax
				cmd = string.format('set OMP_NUM_THREADS=%d && "%s" 2>&1', threads, BIN_DIR .. "\\sbp_experiment.exe")
			else -- Linux / macOS
				cmd = "OMP_NUM_THREADS=" .. threads .. " " .. BIN_DIR .. "/sbp_experiment 2>&1"
			end
			
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
        build_project_and_compile()
        
        if executable_exists(BIN_DIR .. "/sbp_experiment") then
            print("\n" .. string.rep("=", 60))
            print("Running with " .. threads .. " threads")
            print(string.rep("=", 60) .. "\n")
            
			local cmd
			if os.host() == "windows" then -- Windows (cmd.exe)
				cmd = string.format('set OMP_NUM_THREADS=%d && "%s"', threads, BIN_DIR .. "\\sbp_experiment.exe")
			else -- Linux / macOS
				cmd = "OMP_NUM_THREADS=" .. threads .. " " .. BIN_DIR .. "/sbp_experiment"
			end

			local result = os.execute(cmd)
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
		
		if os.host() == "windows" then
			os.rmdir(build_root)
			os.rmdir(BIN_DIR)
			os.remove("Makefile")
			os.remove("*.make")
		else
			os.execute("rm -rf " .. build_root)
			os.execute("rm -rf " .. BIN_DIR)
			os.execute("rm -f Makefile *.make")
		end
        print("Clean complete!")
    end
}
