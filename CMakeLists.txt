cmake_minimum_required(VERSION 3.15)

# Only generate Debug and Release configuration types.
set(CMAKE_CONFIGURATION_TYPES Debug Release)
cmake_policy(SET CMP0048 NEW)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)
FetchContent_Declare(
  googletest
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

project(decklink-io)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# we only bother with UNICODE builds on Windows
if(MSVC)
	add_definitions(-DUNICODE -D_UNICODE)
endif()

add_compile_definitions(DECKLINK_IO_EXPORTS)

add_subdirectory(src)

enable_testing()
add_subdirectory(test)
