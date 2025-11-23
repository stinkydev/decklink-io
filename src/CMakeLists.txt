
cmake_minimum_required(VERSION 3.28)

add_library(decklink-sdk INTERFACE)
add_library(Decklink::SDK ALIAS decklink-sdk)

target_include_directories(
  decklink-sdk
  INTERFACE
    $<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk>
    $<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/win/decklink-sdk>
)

add_library (decklink STATIC)

target_sources(
  decklink-sdk
  INTERFACE
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPI.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIConfiguration.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIDeckControl.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIDiscovery.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIDispatch.cpp>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIModes.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPITypes.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/DeckLinkAPIVersion.h>"
    "$<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:${CMAKE_CURRENT_SOURCE_DIR}/linux/decklink-sdk/LinuxCOM.h>"
    "$<$<PLATFORM_ID:Windows>:${CMAKE_CURRENT_SOURCE_DIR}/win/decklink-sdk/DeckLinkAPIVersion.h>"
)

target_sources(
  decklink
  PRIVATE
  $<$<PLATFORM_ID:Darwin>:mac/platform.cc>
  $<$<PLATFORM_ID:Linux,FreeBSD,OpenBSD>:linux/platform.cc>
  $<$<PLATFORM_ID:Windows>:win/platform.cc>
  decklink/api.cc
  decklink/decklink-device-mgr.cc
  decklink/decklink-device.cc
  decklink/decklink-input.cc
  decklink/decklink-output.cc
)

target_include_directories(decklink PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(decklink PRIVATE $<$<PLATFORM_ID:Windows>:NOMINMAX>)

target_link_libraries(
  decklink
  PRIVATE
    decklink-sdk
    "$<$<PLATFORM_ID:Darwin>:$<LINK_LIBRARY:FRAMEWORK,CoreFoundation.framework>>"
)

if(WIN32)
  message(STATUS "Building for Windows")
  include(cmake/idlfilehelper.cmake)
  target_add_idl_files(decklink win/decklink-sdk/DeckLinkAPI.idl)
  set_source_files_properties(win/decklink-sdk/DeckLinkAPIVersion.h PROPERTIES GENERATED TRUE)
  set_source_files_properties(win/decklink-sdk/DeckLinkAPIVersion.h PROPERTIES GENERATED TRUE)
  target_sources(decklink-sdk INTERFACE win/decklink-sdk/DeckLinkAPIVersion.h)
 
  target_include_directories(decklink PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()

add_subdirectory(app)
