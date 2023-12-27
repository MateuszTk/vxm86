# CMakeList.txt : CMake project for vxm86, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("vxm86")

# Add source to this project's executable.
file (GLOB_RECURSE SOURCES "src/*.cpp")

add_executable (vxm86 ${SOURCES})

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET vxm86 PROPERTY CXX_STANDARD 20)
endif()

# Copy assets to build directory
add_custom_target(copy_assets ALL
  COMMENT "Copying assets to build directory"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/elf
          ${CMAKE_CURRENT_BINARY_DIR}/elf
  DEPENDS ${PROJECT_NAME}
)

