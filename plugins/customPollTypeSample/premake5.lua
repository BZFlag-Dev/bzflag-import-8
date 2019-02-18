project "customPollTypeSample"
  kind "SharedLib"
  targetprefix ""
  files {
    "../../include/bzfsApi.h",
    "../plugin_utils/plugin_utils.h",
    "*.cpp",
    "*.h",
    "*.txt",
    "*.cfg"
  }
  vpaths {
    ["*"] = { "**.txt", "**.cfg" },
    ["Header Files"] = { "**.h", "../plugin_utils/**.h", "../../include/**.h" },
    ["Source Files"] = "**.cpp",
  }
  includedirs "../plugin_utils"
  links "plugin_utils"

  filter "system:windows"
    defines {
      "WIN32",
      "_USE_MATH_DEFINES",
      "_WINDOWS",
      "_USRDLL",
      "customPollTypeSample_EXPORTS"
    }
    libdirs "$(OutDir)"
    links "bzfs.lib" -- ".lib" required to distinguish from the executable
    dependson "bzfs"
    postbuildcommands {
      "if not exist ..\\..\\bin_$(Configuration)_$(Platform) mkdir ..\\..\\bin_$(Configuration)_$(Platform)",
      "if not exist ..\\..\\bin_$(Configuration)_$(Platform)\\plugins mkdir ..\\..\\bin_$(Configuration)_$(Platform)\\plugins",
      "copy $(OutDir)customPollTypeSample.dll ..\\..\\bin_$(Configuration)_$(Platform)\\plugins\\",
      "copy ..\\..\\plugins\\customPollTypeSample\\*.txt ..\\..\\bin_$(Configuration)_$(Platform)\\plugins\\",
      "if exist ..\\..\\plugins\\customPollTypeSample\\*.cfg ( copy ..\\..\\plugins\\customPollTypeSample\\*.cfg ..\\..\\bin_$(configuration)_$(Platform)\\plugins\\ )"
    }

  filter "system:macosx"
    linkoptions "-undefined dynamic_lookup"

  filter "system:linux"
    symbols "On"
  filter { }