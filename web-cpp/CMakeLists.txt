cmake_minimum_required(VERSION 3.20)

project(webcpp LANGUAGES CXX)

# =========================
# Target
# =========================
add_executable(webcpp
  bigdec/bigdec_tests.cpp
  json/json.cpp
  json/json_tests.cpp

  lab/db_user_repo.cpp
  lab/db_user_crud.cpp
  lab/user_service.cpp

  web-cpp.cpp
  web/http_server/http_server.cpp
  web/http_server/http_server_tests.cpp
  web/tcp_server/tcp_server.cpp
)

# =========================
# Language standard
# =========================
target_compile_features(webcpp PRIVATE cxx_std_20)

# =========================
# Compile definitions
# =========================
# IMPORTANT: must match your #ifdefs in code
target_compile_definitions(webcpp PRIVATE
  _CONSOLE
  NO_PQXX=1
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

# =========================
# Include paths
# =========================
# Allows includes like "json/json.hpp", "lab/user_service.hpp"
target_include_directories(webcpp PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"
)

# =========================
# Platform-specific
# =========================
if(MSVC)
  target_compile_options(webcpp PRIVATE /W3 /permissive-)

  set_property(TARGET webcpp PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

# =========================
# NO external dependencies
# =========================
# (libpqxx intentionally removed)
