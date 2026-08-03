# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Release")
  file(REMOVE_RECURSE
  [[launcher\CMakeFiles\launcher_autogen.dir\AutogenUsed.txt]]
  [[launcher\CMakeFiles\launcher_autogen.dir\ParseCache.txt]]
  [[launcher\launcher_autogen]]
  )
endif()
