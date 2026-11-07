# K7 - Race&Sex Carry Weight (плагин SKSE)

Публичное зеркало исходников **K7 - Race&Sex Carry Weight** — плагина SKSE для Skyrim, который задаёт игроку базовый
переносимый вес по расе и полу. Каждый выпуск плагина — один коммит здесь (`Release <версия>`) с тегом
`K7RSCarryWeight-v<версия>`; история закрытого репозитория разработки в зеркало не входит.

* Что делает плагин, как его установить, настроить и удалить: [plugins/K7RSCarryWeight/README.ru.md](plugins/K7RSCarryWeight/README.ru.md).
* English version: [README.md](README.md).

В зеркале есть всё, что нужно для сборки плагина: сам плагин (`plugins/K7RSCarryWeight/`), общие библиотеки
(`libs/common`, `libs/skse`), сборочные скрипты (`cmake/`, `tools/build.cmd`) и заголовок SKSE Menu Framework
(`third_party/`). CommonLibSSE-NG подключена как git submodule в `lib/commonlibsse-ng`.

## Сборка

Нужны: Visual Studio 2022 Build Tools (рабочая нагрузка C++), CMake 3.25+, Ninja,
[vcpkg](https://github.com/microsoft/vcpkg) с заданной переменной `VCPKG_ROOT`.

```
git clone --recurse-submodules https://github.com/vorxa/K7RSCarryWeight.git
cd K7RSCarryWeight
tools\build.cmd release
```

DLL получается в `build/release/plugins/K7RSCarryWeight/K7RSCarryWeight.dll`; первая сборка компилирует CommonLibSSE-NG
из исходников и занимает много времени. Юнит-тесты: `ctest --test-dir build/release`.

## Лицензия

GPL-3.0-or-later, см. [LICENSE](LICENSE). CommonLibSSE-NG тоже под GPL-3.0-or-later, и его исключение для модов не
распространяется на код плагина, поэтому плагин распространяется на тех же условиях.
