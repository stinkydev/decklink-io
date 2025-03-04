add_executable(
  decklink-io-tests
  main.test.cc
)

target_link_libraries(
  decklink-io-tests
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(decklink-io-tests)