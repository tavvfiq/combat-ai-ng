-- set minimum xmake version
set_xmakever("2.8.2")

-- includes
includes("lib/CommonLibSSE-NG")

-- set project
set_project("CombatAI-NG")
set_version("0.0.1")
set_license("GPL-3.0")

-- set defaults
set_languages("c++23")
set_warnings("allextra")

-- set policies
set_policy("package.requires_lock", true)

-- add rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- require packages
add_requires("simpleini")

-- targets
target("CombatAI-NG")
    -- add dependencies to target
    add_deps("commonlibsse-ng")	
    add_packages("simpleini")

    -- add commonlibsse-ng plugin
    add_rules("commonlibsse-ng.plugin", {
        name = "CombatAI-NG",
        author = "tavvfiq",
        description = "SKSE64 plugin template using CommonLibSSE-NG"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
