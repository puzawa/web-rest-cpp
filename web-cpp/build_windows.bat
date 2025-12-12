cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DUSE_BUNDLED_LIBPQXX=ON
cmake --build build --config Release
