project "3D"
  kind "StaticLib"
  files { "*.cxx", "*.h", "../../include/*.h" }
  vpaths {
    ["Header Files/include"] = "../../include",
    ["Header Files"] = "**.h",
    ["Source Files"] = "**.cxx"
  }
