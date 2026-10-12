include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
  URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(minidb_tests
  common/failpoint_test.cpp
  common/ids_test.cpp
  common/logger_test.cpp
  common/metrics_test.cpp
)
target_link_libraries(minidb_tests PRIVATE minidb minidb_options GTest::gtest_main)

include(GoogleTest)
# PRE_TEST discovers tests when ctest runs rather than at build time, so a
# sanitizer report during discovery shows up as a test failure, not a build
# failure.
gtest_discover_tests(minidb_tests DISCOVERY_MODE PRE_TEST)
