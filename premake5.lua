solution "PubClientSolution"
    configurations { "Debug", "Release" }

    project "mqtt_sub_client"
        kind "ConsoleApp"
        language "C"
        files { 
            -- "src/*.h", 
            "subscribe/*.c", 
            "lib/*.h", 
            "lib/*.c", 
            "queue/*.h",
            "queue/*.c",
            "timer/*.h",
            "timer/*.c"}
            includedirs { "include", ".", "/usr/include/postgresql"}
            
        
        defines { "VERSION=\"2.0.13\"", "WITH_THREADING"}
        linkoptions {"-pthread", "-ldl"}
        links{"sqlite3", "pq", "cjson", "z"}

    filter { "configurations:Debug" }
        defines { "DEBUG" }
        symbols "On"

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "On"

    --filter { "system:linux" }
        --libdirs { "bin/linux", "/usr/local/lib"  }
        --buildoptions "-std=c++1"

    --project "gtest_main"
        --language "C++"
        --kind "StaticLib"

        --includedirs { "include", "vendor/gtest" }
        --files { "vendor/gtest/src/*.cc", "/vendor/gtest/src/*.h" }
    