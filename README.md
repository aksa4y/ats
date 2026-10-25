# ATSExhaust — Windows / SPF diagnostic preview

**Это незавершённый перенос: установка и телеметрия реализованы, управление видимым дымом на Windows пока отсутствует.**

Проект использует модель интенсивности выхлопа из [shortcord/modguard-linux-unoffical](https://github.com/shortcord/modguard-linux-unoffical).
Новый Windows-модуль загружается через SPF Framework. Он показывает русскую панель состояния и расчётную интенсивность выхлопа.
Он не устанавливает Linux `.so`, не загружает оригинальный MODGuard.dll и не устанавливает модели грузовиков.

## Быстрый запуск

1. Скачайте [ZIP исходников](https://github.com/aksa4y/ats/archive/refs/heads/main.zip).
2. Полностью распакуйте архив в постоянную папку.
3. Закройте ATS.
4. Запустите `run.bat`.
5. Если установщик Microsoft запросит права администратора, разрешите установку VC++ Runtime.
6. Откройте панель ATSExhaust в интерфейсе SPF.

`run.bat` ищет ATS в библиотеках Steam, включая библиотеки на других дисках.
Если найдено несколько копий, скрипт попросит указать нужную папку.
Можно передать путь вручную:

```bat
run.bat -GamePath "D:\SteamLibrary\steamapps\common\American Truck Simulator"
```

Для установки без запуска игры:

```bat
run.bat -NoLaunch
```

Требования: Windows 10/11 x64, Windows PowerShell 5.1, установленная ATS и доступ к GitHub и Microsoft.
Пакет Windows появляется в Releases после успешной сборки GitHub Actions.
Если релиз ещё не готов, `run.bat` завершится с объяснением ошибки.

## Файлы запуска

| Файл | Действие |
| --- | --- |
| `run.bat` | Скачать проверенный релиз, установить необходимые компоненты, запустить ATS. |
| `update.bat` | Обновить наш модуль и SPF из опубликованных релизов. Игра не запускается. |
| `diagnose.bat` | Записать путь игры, версию и SHA-256 исполняемого файла. Ничего не устанавливать. |
| `build.bat` | Собрать Windows DLL из исходников. При необходимости установить C++ Build Tools через winget. |

`update.bat` — файл, а не папка. Он поддерживает тот же аргумент `-GamePath`.
Скрипты сохраняют код ошибки. Для запуска без паузы задайте `ATS_NO_PAUSE=1`.

## Установка и сохранение других плагинов

Наш модуль находится здесь:

```text
<ATS>\bin\win_x64\plugins\spfPlugins\ATSExhaust\ATSExhaust.dll
```

Существующий SPF версии 1.2.6 или новее используется при обычном запуске.
`update.bat` получает актуальный SPF. Установщик меняет только DLL Framework и его файлы локализации.
Он не копирует демонстрационные плагины и не перезаписывает настройки других плагинов.
Все заменяемые файлы получают резервные копии в `.state/backups`.
При ошибке копирования установщик восстанавливает затронутые файлы.

Архивы проверяются по SHA-256, который возвращает GitHub Releases API.
DLL дополнительно проверяется по манифесту внутри архива.
Установщик VC++ Runtime проверяется по действительной цифровой подписи Microsoft.
При ошибке сети или проверки установка прекращается. Текущие файлы остаются на месте.
Для защищённой папки игры могут понадобиться права администратора.

## Что происходит после обновления ATS

Скрипт вычисляет SHA-256 вашего `amtrucks.exe` и получает последний опубликованный пакет.
В текущем диагностическом пакете `visual_smoke=false`. Проверенных Windows-профилей управления дымом нет.

**Автоматическое скачивание обновлений не означает автоматическое создание поддержки ATS 1.63, 1.64 или другой будущей версии.**
Если разработчики изменят устройство частиц, понадобится исследовать новую Windows-сборку и выпустить исправление.
SPF сам по себе не предоставляет API генерации выхлопного дыма.
Для будущего пакета с `visual_smoke=true` установщик требует точное совпадение SHA-256 игры с манифестом.

Для завершения переноса нужны Windows `amtrucks.exe` текущей сборки и проверка внутри игры с совместимыми моделями выхлопа.
Linux-адреса, ELF build ID и Linux-структуры нельзя переносить в Windows без проверки.
Текущая панель не является доказательством работающего визуального эффекта.

## Диагностика

После `diagnose.bat` откройте `.state/diagnostics.json`.
При ошибке откройте `.state/last-error.txt`.
Отчёт содержит локальный путь установки, версию и хеш игры.
Скрипт никуда не отправляет этот отчёт.

В панели SPF после начала поездки должна появиться надпись «Телеметрия поступает».
Полоса интенсивности реагирует на газ, обороты и нагрузку.
При паузе или выгрузке мира значение сбрасывается.

## Build and tests

The Windows workflow builds the DLL with MSVC and a static C++ runtime.
The workflow also runs C++ tests and installer tests with PowerShell 5.1 and PowerShell 7.
A release is published only after both operating-system jobs pass.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
pwsh -NoProfile -File tests/installer_test.ps1
```

On Linux, the shared library is a test target. It is not a Linux game plugin.
The source build does not install the resulting DLL into the game.
For a manual Windows installation, copy `build/Release/ATSExhaust.dll` to the dedicated SPF plugin folder.

Tests cover the exhaust model, plugin lifecycle, pause, invalid telemetry, timer reset, package compatibility, archive traversal, and installation rollback.
The repository does not include a game executable. Automated tests cannot confirm the visual effect inside ATS.

See [THIRD_PARTY.md](THIRD_PARTY.md) for source attribution and license limits.
