# K7 - Race&Sex Carry Weight (SKSE plugin)

Public mirror of the source of **K7 - Race&Sex Carry Weight**, a Skyrim SKSE plugin that gives the player a base
carry weight by race and sex. Every release of the plugin is one commit here (`Release <version>`) with the tag
`K7RSCarryWeight-v<version>`; the history of the private development repository is not part of the mirror.

* What the plugin does, how to install, configure and remove it: [plugins/K7RSCarryWeight/README.md](plugins/K7RSCarryWeight/README.md).
* Русская версия: [README.ru.md](README.ru.md).

The mirror holds everything needed to build the plugin: the plugin itself (`plugins/K7RSCarryWeight/`), the shared
libraries (`libs/common`, `libs/skse`), the build scripts (`cmake/`, `tools/build.cmd`) and the SKSE Menu Framework
header (`third_party/`). CommonLibSSE-NG is a git submodule in `lib/commonlibsse-ng`.

## Build

Requirements: Visual Studio 2022 Build Tools (C++ workload), CMake 3.25+, Ninja,
[vcpkg](https://github.com/microsoft/vcpkg) with `VCPKG_ROOT` set.

```
git clone --recurse-submodules https://github.com/vorxa/K7RSCarryWeight.git
cd K7RSCarryWeight
tools\build.cmd release
```

The DLL is `build/release/plugins/K7RSCarryWeight/K7RSCarryWeight.dll`; the first build compiles CommonLibSSE-NG from
source and takes a while. Unit tests: `ctest --test-dir build/release`.

## License

GPL-3.0-or-later, see [LICENSE](LICENSE). CommonLibSSE-NG is GPL-3.0-or-later as well, and its modding exception does
not cover plugin code, so the plugin is distributed under the same terms.
