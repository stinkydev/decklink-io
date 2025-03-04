set(APP_SRCS
	main.cc
)
	
#include the subdirectory containing our libs
include_directories()

#indicate the entry point for the library
add_executable(decklink-io-app ${APP_SRCS})
target_link_libraries(decklink-io-app decklink)
