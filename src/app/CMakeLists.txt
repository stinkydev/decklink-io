set(APP_SRCS
	main.cc
)
	
#include the subdirectory containing our libs
include_directories()

#indicate the entry point for the library
add_executable(decklink-io-app ${APP_SRCS})
target_link_libraries(decklink-io-app decklink)

add_executable(decklink-cli decklink-cli.cc)
target_link_libraries(decklink-cli decklink)

# When built inside a parent project that collects binaries in a per-config
# output dir (like sesame-server), land decklink-cli next to the main binary
# so installers pick it up.
if(CMAKE_RUNTIME_OUTPUT_DIRECTORY)
  set_target_properties(decklink-cli PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>/)
endif()
