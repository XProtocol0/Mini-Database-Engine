# One library for the whole engine. Modules are separated by directory
# (include/db/<module>, src/<module>); dependencies must only flow downward,
# see docs/architecture.md.
add_library(minidb
  common/failpoint.cpp
  common/logger.cpp
  common/metrics.cpp
)
target_include_directories(minidb PUBLIC ${PROJECT_SOURCE_DIR}/include)
target_link_libraries(minidb PUBLIC Threads::Threads PRIVATE minidb_options)
