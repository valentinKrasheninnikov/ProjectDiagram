# Диаграмма проекта

Настольное приложение для редактирования диаграммы Ганта. Проекты загружаются из JSON и сохраняются в том же формате.

## Сборка

C++17, Qt Widgets, CMake и Ninja. Сборка проверена на Windows x64 с Qt 6.8.3 и MSVC 19.51.

Выполните команды в x64 Developer PowerShell for Visual Studio. При необходимости замените путь к Qt.

```powershell
$qt = 'C:/Qt/6.8.3/msvc2022_64'
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$qt"
cmake --build build/release
$env:Path = "$qt/bin;$env:Path"
& ./build/release/ProjectDiagram.exe
```