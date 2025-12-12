cmake_minimum_required(VERSION 3.20)
project(webcpp LANGUAGES CXX)

# -------------------------
# Build target
# -------------------------
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

# -------------------------
# MSVC niceties (Windows)
# -------------------------
if(MSVC)
  target_compile_options(webcpp PRIVATE /W3 /permissive-)

  # Match your old vcxproj: Release used /MT (MultiThreaded)
  set_property(TARGET webcpp PROPERTY
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
  )
endif()

# -------------------------
# Dependencies: libpqxx (preferred), fallback options
# -------------------------
option(USE_BUNDLED_LIBPQXX "Use ../deps/libpqxx (Windows-style .lib + include) instead of find_package" OFF)

if(USE_BUNDLED_LIBPQXX)
  # Your existing local deps layout (repo_root/deps/libpqxx/{include,libs})
  set(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
  set(LIBPQXX_INCLUDE "${REPO_ROOT}/deps/libpqxx/include")
  set(LIBPQXX_LIBDIR  "${REPO_ROOT}/deps/libpqxx/libs")

  target_include_directories(webcpp PRIVATE
    "${LIBPQXX_INCLUDE}"
    "${CMAKE_CURRENT_LIST_DIR}"  # your project dir
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
    # Windows system libs you had in Release
    ws2_32.lib secur32.lib crypt32.lib wldap32.lib advapi32.lib
  )

else()
  # ---- Preferred path (Windows + Linux): find_package(libpqxx CONFIG) ----
  # Works great with vcpkg on Windows and with libpqxx built+installed via CMake.
  find_package(libpqxx CONFIG QUIET)

  if(libpqxx_FOUND)
    target_link_libraries(webcpp PRIVATE libpqxx::pqxx)

  else()
    # ---- Linux-friendly fallback: pkg-config ----
    # Useful if your distro's libpqxx-dev doesn't ship a CMake config package.
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PQXX REQUIRED IMPORTED_TARGET libpqxx)
    target_link_libraries(webcpp PRIVATE PkgConfig::PQXX)
  endif()

  # Optional: If YOUR code directly uses OpenSSL/LZ4 APIs, keep these.
  # If not, you can delete this block.
  if(UNIX AND NOT APPLE)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
      target_link_libraries(webcpp PRIVATE OpenSSL::SSL OpenSSL::Crypto)
    endif()
  endif()
endif()
