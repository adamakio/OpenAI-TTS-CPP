cmake_minimum_required (VERSION 3.8)
set(CMAKE_PREFIX_PATH "C:/Users/16478/libsndfile_install")
set (CMAKE_CXX_STANDARD 17)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_TOOLCHAIN_FILE "C:/Users/16478/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "toolchain file")

project ("NetworkingCPP")

# Find packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(SndFile CONFIG REQUIRED)
find_package(FLAC CONFIG REQUIRED)
find_path(MINIMP3_INCLUDE_DIRS
    NAMES "minimp3/minimp3.h" "minimp3/minimp3_ex.h"
    PATHS "C:/Users/16478/source/repos/NetworkingCPP/vcpkg_installed/x64-windows/include"
)

file(GLOB HEADER_FILES "*.h")
file(GLOB CPP_HEADER_FILES "*.hpp")
file(GLOB CPP_FILES "*.cpp")

# Add source to this project's executable.
add_executable (NetworkingCPP ${CPP_FILES} ${HEADER_FILES} ${CPP_HEADER_FILES})

target_include_directories(NetworkingCPP PRIVATE ${MINIMP3_INCLUDE_DIRS})

# Link libraries to your executable
target_link_libraries(NetworkingCPP PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(NetworkingCPP PRIVATE CURL::libcurl)
target_link_libraries(NetworkingCPP PRIVATE portaudio)
target_link_libraries(NetworkingCPP PRIVATE SndFile::sndfile)
target_link_libraries(NetworkingCPP PRIVATE FLAC::FLAC)