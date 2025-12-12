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

target_compile_features(webcpp PRIVATE cxx_std_20)

target_compile_definitions(webcpp PRIVATE
  _CONSOLE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

# IMPORTANT: allow includes like "json/json.hpp" and "bigdec/bigdec.hpp"
# This is the equivalent of VS $(ProjectDir) include behavior.
target_include_directories(webcpp PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}"    # = web-cpp/
)

# =========================
# MSVC settings (Windows)
# =========================
if(MSVC)
  target_compile_options(webcpp PRIVATE /W3 /permissive-)

  # Match your old vcxproj: Release used /MT (MultiThreaded)
  set_property(TARGET webcpp PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

# =========================
# Dependencies: libpqxx
# =========================
option(USE_BUNDLED_LIBPQXX "Use ../deps/libpqxx (Windows-style .lib + include) instead of find_package" OFF)

if(USE_BUNDLED_LIBPQXX AND WIN32)
  # ---- Windows: your existing local deps layout (repo_root/deps/libpqxx/{include,libs}) ----
  set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
  set(LIBPQXX_INCLUDE "${REPO_ROOT}/deps/libpqxx/include")
  set(LIBPQXX_LIBDIR  "${REPO_ROOT}/deps/libpqxx/libs")

  target_include_directories(webcpp PRIVATE
    "${LIBPQXX_INCLUDE}"
  )

  target_link_directories(webcpp PRIVATE
    "${LIBPQXX_LIBDIR}"
  )

  target_link_libraries(webcpp PRIVATE
    pqxx.lib
    libecpg_compat.lib
    libecpg.lib
    libpgtypes.lib
    libpq.lib
    libpgcommon.lib
    libpgport.lib
    libcrypto.lib
    libssl.lib
    lz4.lib
    ws2_32.lib secur32.lib crypt32.lib wldap32.lib advapi32.lib
  )

else()
  # ---- Preferred: CMake config package (great for vcpkg on Windows, or source install on Linux) ----
  find_package(libpqxx CONFIG QUIET)

  if(libpqxx_FOUND)
    target_link_libraries(webcpp PRIVATE libpqxx::pqxx)
  else()
    # ---- Fallback: pkg-config (common on Linux distros) ----
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
    target_link_libraries(webcpp PRIVATE PkgConfig::PQXX)
  endif()
endif()
