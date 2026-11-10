# Building and packaging

Use CMake 3.30 or newer and a compiler accepting C++26. GCC 14 or newer is used for release builds. Linux needs OpenGL and the X11/Wayland development libraries used by GLFW. macOS builds use GCC for C++ and Apple's Objective-C compiler for Cocoa. Windows builds use MSYS2 UCRT64 GCC.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build -j3
ctest --test-dir build --output-on-failure
```

GLFW follows upstream `master` in local builds and CI; miniaudio uses a checksum-verified source archive. No game assets are fetched by CMake or the tests. `scripts/fetch-assets.sh` and `scripts/fetch-assets.bat` acquire the original game data separately.

For portable packages, configure with `-DAPACHE_STATIC_RUNTIME=ON -DBUILD_SHARED_LIBS=OFF`. This bundles GLFW and statically links the compiler runtimes. Linux still uses system graphics, windowing and C libraries.

Install into an empty staging directory, then package it:

```sh
cmake --install build --prefix "$PWD/package/apache" --component Runtime
cmake -DPACKAGE_ROOT="$PWD/package" -DOUTPUT_DIR="$PWD/dist" -DPLATFORM=linux-x86_64 -P cmake/package.cmake
```

The other platform names are `windows-x86_64` and `macos-arm64`. The install step copies the executable, setup helper, documentation and dependency licences from an explicit list. Packaging rejects unexpected files and symbolic links. Game resources, original executables, saves, inspector tools, captures and test caches are excluded.

The extensive original-code comparisons live in the sibling analysis workspace. CTest runs the self-contained resource, scene, profile, packet and localhost transport tests. Headless window checks use a private X server; they never send input to the desktop.

## Continuous builds

GitHub Actions builds Debug and Release configurations on Linux x86-64, Windows x86-64 and macOS Apple Silicon. CI uses `ubuntu-latest`, `windows-latest` and `macos-latest` so builds follow GitHub's current stable runner images. Every job runs the asset-independent CTest suite and checks the installed executable before uploading its package. Compiler caches are bounded to 500 MB per configuration; build artifacts expire after seven days.

Changes to game code, tests, build definitions or setup scripts on `master` publish the three tested Release packages and `SHA256SUMS` after all six jobs succeed. Pull requests run the same build checks without publishing. A manual run can also publish from `master` by selecting its `publish` option. Game assets are neither downloaded nor uploaded by CI.

Publishing a complete release retires older published releases and their downloadable packages. Draft releases and Git tags are retained.
