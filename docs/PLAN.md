# План реализации pico-next

## Context

`pico-next` — порт эмулятора **ZX Spectrum Next** на микроконтроллер RP2350 с внешней PSRAM, наследник проекта `pico-spec` (порт ESPectrum для RP2040/RP2350, эмулирует классические Spectrum 48K/128K/Pentagon).

Spectrum Next — это FPGA-реализация Z80-совместимой машины с существенно расширенной архитектурой: процессор Z80N с extended instruction set, иерархическая MMU с 8K-страницами вместо 16K, до 2 МБ RAM, новые видео-слои (Layer 2, Tilemap, Sprites) поверх Enhanced ULA, 256-цветные палитры, копроцессор Copper, Z80-совместимый DMA, тройной AY + DACs, и собственная ОС NextZXOS поверх esxDOS API. Полная спецификация — [SpecNext Wiki](https://wiki.specnext.dev/Specifications).

**Цель проекта**: получить эмулятор Spectrum Next, который грузит ROMы с SD-карты (как реальный Next) и поэтапно покрывает функционал core 3.x.

## Прогресс (Milestone 1 — boot до splash)

| Готово | Что | Коммит |
|--------|-----|--------|
| ✅ | NextReg storage + I/O \$243B/\$253B | `194e1eb` |
| ✅ | Pimoroni Pico Plus 2 = PICO_DV target | `62fa010` |
| ✅ | Z80N opcodes minimum (NEXTREG/MUL/ADD/SWAPNIB + trap) | `18d34d7` |
| ✅ | 2 МБ Next-RAM в butter PSRAM | `9e38f46` |
| ✅ | 8K MMU + NextReg \$50-\$57 → slot pointers | `5ba4d3e` |
| ✅ | SD ROM loader (`enNextZX.rom` / `enNxtmmc.rom`) | `e99da95` |
| ✅ | MemESP::ram[] → NextRAM bridge для Video.cpp | `e2e2054` |
| ✅ | Полный Z80N opcode set (24 опкода + trap) | `827f678` |
| ✅ | NextReg palette \$40-\$44 storage | `16a7a34` |
| ✅ | Port \$7FFD ↔ NextReg \$56/\$57 sync | `be74620` |
| ✅ | Layer 2 state + Port \$123B enable | `c16ea71` |

## Что осталось до полной splash visibility

- **ULA palette routing** в Video.cpp: индексировать через `NextReg::palette[0]` вместо фиксированной таблицы из 16 цветов. Без этого splash рисуется классической палитрой — узнаваем, но цвета могут быть «не те».
- **Layer 2 рендеринг** в Video.cpp: оверлей `NextRAM[start_page*16K]` через `NextReg::palette[1]` поверх ULA. Без этого иконка Spectrum Next в splash не появится — только текст.
- **NextReg side effects**: \$02 reset, \$08 peripheral config, \$68 ULA Control, \$8C Alt ROM, \$8E 128K paging — пока store-only.
- **Port \$1FFD**: +3-стиль расширенного paging для ROM-bank переключения внутри 64KB Next-ROM.

Эти куски не блокируют первый flash-test — splash будет частично видимым с классической палитрой и без Layer 2-логотипа. Доделываются по фидбэку UART-лога.

---

**Текущее состояние**: репозиторий `drewpo28/pico-next` пуст. Содержимое `pico-spec` (ветка `claude/pico-next-cleanup-JtWGm`) пользователь запушит в `main` отдельно через несколько дней. Этот план описывает работу **после** заливки.

## Целевое железо

- **Платы**: Pimoroni Pico Plus 2 (RP2350B, 8 МБ PSRAM, 16 МБ flash) — основная; Adafruit Feather RP2350 HSTX (8 МБ PSRAM) — вторичная (HDMI через HSTX). RP2040 и RP2350 без PSRAM **не поддерживаются** (2 МБ RAM Next физически не влезают).
- **Выводы**: HDMI (HSTX на Feather; DVI-bitbang/HSTX на Pico Plus 2), VGA через резистивную лестницу (как в pico-spec), I2S/PWM audio, SD-карта через SPI или SDIO, USB-host для клавиатуры/мыши, PS/2 опционально.
- **Карта PSRAM**: ~2 МБ под RAM Next, 192 КБ под ROM-комплект (NextZXOS), резерв под Layer 2 framebuffer, tilemap, sprite-RAM, palette-RAM.

## Стратегия

Сохраняем структуру `pico-spec` (подтверждено пользователем). Существующие модули (`Z80`, `MemESP`, `VideoEnt`, `AySound`, `OSDMain`, и т.д. — точные имена возьму после заливки) расширяются Next-возможностями через сборочные флаги. Новая функциональность лежит в `src/next/` и подключается через CMake-опцию `PICO_NEXT_FEATURES=ON`. pico-next — самостоятельный проект (не fork с трекингом upstream), pico-spec используется только как одноразовая стартовая база. Старый Spectrum-код остаётся работоспособным как «Spectrum compatibility mode» Next.

## Фазы реализации

### Фаза 0 — Bootstrap (после заливки кода)

Цель: переименовать проект и собраться на целевой плате.

- **Ветка**: вся работа этого плана идёт в существующую ветку `claude/pico-next-cleanup-JtWGm` (ту же, в которую пользователь заливает содержимое pico-spec). Не создавать `claude/adapt-pico-next-config-Fubt6`, не пушить в `main` — `main` остаётся нетронутым до момента готовности к мерджу. Designated-branch из системных инструкций (`claude/adapt-pico-next-config-Fubt6`) перекрыта прямым указанием пользователя.
- В `CMakeLists.txt`: `project(pico-next ...)`, версия сброшена в 0.1.0.
- Добавить CMake-опции `PICO_NEXT_FEATURES` (default ON для RP2350+PSRAM), `PICO_NEXT_BOARD` (`pimoroni_pico_plus2` / `adafruit_feather_rp2350`), `PICO_NEXT_PSRAM_SIZE`.
- Добавить board-presets для целевых плат под `boards/` (или эквивалент в pico-sdk-style).
- Обновить `README.md`: новый scope, целевое железо, ссылка на SpecNext Wiki.
- Обновить `CLAUDE.md`: документировать новые директории `src/next/`, конвенции коммитов, ссылку на этот план.
- Удалить упоминания «ESPectrum port» в местах, где это неточно для Next.
- Build & flash на плате — проверить, что классический Spectrum-режим работает (регрессии не введены).

### Milestone 1 — Реальный boot NextZXOS до стартового экрана

Цель: загрузить с SD-карты настоящий `enNextZX.rom` и **выполнять его на эмуляторе** до момента, когда ROM сам нарисует [стартовый экран NextZXOS](https://wiki.specnext.dev/File:NextZXOS.png). Никакого fake-splash из ассетов — только честная эмуляция.

Это «вертикальный срез» через ранние фазы: берём из каждой минимум, достаточный для того, чтобы NextZXOS дошёл до отрисовки приветствия. Полные реализации этих подсистем продолжаются дальше по плану.

**Что должно работать к этой вехе** (subset фаз 1–6):

1. **PSRAM-backed RAM (Фаза 1, минимум)** — 2 МБ Next-RAM в PSRAM, без оптимизаций кэширования. 16K/8K-страницы линейно лежат в PSRAM.
2. **Z80N CPU (Фаза 2, минимум)** — полный классический Z80 уже есть в pico-spec. Добавить только те Z80N-инструкции, которые встречаются на пути boot до отрисовки splash. Минимум: `NEXTREG reg,val`, `NEXTREG reg,A`, `MUL DE`, `ADD HL,A/BC,A/DE,A/A`, `SWAPNIB`. Остальные Z80N-опкоды как `trap → assert(0)` для отлова на этапе тестирования.
3. **MMU 8K + NextReg (Фаза 3, минимум)** — таблица NextReg в RAM, обработка opcode `NEXTREG` + I/O $243B/$253B. Регистры, которые трогает boot: $00–$0E (machine/config/board ID), $50–$57 (MMU), $07 (CPU speed), $40–$44 (palette), $68 (ULA control), $69 (LoRes/timing), $8C–$8F (Alt ROM). Остальные — read/write into table без побочных эффектов.
4. **SD card + ROM loader (Фаза 4, минимум)** — FAT-mount, чтение `enNextZX.rom` (или путь из `/tbblue/config.ini`) в Alt ROM area PSRAM. Без BIOS-меню — просто прямая загрузка на старте. Если SD не вставлена / файла нет → ошибка на UART + красная рамка ULA.
5. **Видеотракт (Фаза 6, минимум)** — Enhanced ULA с базовой 16-цветной палитрой через NextReg $40–$44. Достаточно для того, чтобы NextZXOS отрисовал свой стартовый экран (он использует ULA-режим). Layer 2 / Tilemap / Sprites не нужны для самого приветствия.
6. **Interrupts (Фаза 14, минимум)** — VBI 50/60 Гц, IM1. Этого хватит, чтобы ROM прошёл свою boot-последовательность.

**Что НЕ нужно к этой вехе**: turbosound, DACs, Copper, DMA, Layer 2, Tilemap, Sprites, NextZXOS API hooks, esxDOS API hooks, BIOS-меню, .NEX loader, mouse, joystick (хватит клавиатуры). Это всё — последующие фазы.

**Что произойдёт фактически**: после прошивки плата мгновенно начинает выполнять Z80N-код из `enNextZX.rom`, и через секунду на HDMI/VGA появляется реальный стартовый экран NextZXOS, отрисованный самой ОС. Без эмулятора — нет картинки.

**Verification milestone 1**:
- На SD-карте подготовлен distro System/Next 24.11 (минимум `enNextZX.rom`, `enNxtmmc.rom`, `/tbblue/config.ini`).
- `cmake --build build && picotool load build/pico-next.uf2 -f`.
- На HDMI/VGA отображается реальный стартовый экран NextZXOS (как [на скриншоте Wiki](https://wiki.specnext.dev/File:NextZXOS.png)).
- В UART-логе видна успешная загрузка ROM, инициализация NextReg, переход на `org $0000`, и список выполненных Z80N-опкодов (для дебага).
- Если эмулятор натыкается на нереализованный Z80N-опкод → trap с дампом регистров в UART (быстрый цикл доработки).

---

### Фаза 1 — PSRAM как основная RAM

Цель: расширить адресное пространство до 2 МБ Next-RAM в PSRAM.

- Включить XIP-cached доступ к PSRAM через RP2350 QSPI/QMI (см. [Pico SDK PSRAM API](https://forums.raspberrypi.com/viewtopic.php?t=390346)).
- Завести аллокатор `next_psram.c`: пулы под RAM_BANK[224×8K], ROM-image, видео-буферы, sprite-RAM.
- Переписать `MemESP`-эквивалент так, чтобы 16K-банки 0–7 (классические) были view'ом на 8K-страницы PSRAM.
- Benchmark: latency PSRAM read в худшем случае; если не успевает за 28 МГц Z80N — добавить кэш горячих страниц в SRAM (доступ через MMU-translation table).

### Фаза 2 — Z80N CPU

Цель: расширить эмулятор Z80 инструкциями Z80N.

- Файл `src/next/z80n_ops.c`: реализовать дополнительные опкоды по [Extended Z80 instruction set](https://wiki.specnext.dev/Extended_Z80_instruction_set) — `MUL DE`, `ADD HL,A/BC,A/DE,A/A`, `SWAPNIB`, `MIRROR`, `PIXELDN`, `PIXELAD`, `SETAE`, `TEST n`, `BSLA/BSRA/BSRL/BSRF/BRLC DE,B`, `LDIX/LDDX/LDIRX/LDDRX/LDPIRX`, `OUTINB`, `JP (C)`, `PUSH nn`, `NEXTREG reg,val/NEXTREG reg,A`.
- Программируемые тактовые частоты 3.5/7/14/28 МГц через NextReg $07. Перенести цикловую модель Z80-эмулятора так, чтобы тактирование менялось во время исполнения.
- Wait-states: документация говорит что 28 МГц с wait-states — измерить целевую частоту бэкенда и при необходимости использовать second core RP2350.
- Тесты: гонять [NexTest](https://wiki.specnext.dev/NexTest) (если возможно собрать без полного Next-стека) и [Z80 Instruction Table](https://wiki.specnext.dev/Z80_Instruction_Table) корректность.

### Фаза 3 — MMU 8K + NextReg infrastructure

Цель: переключиться с 16K-paging Spectrum на 8K-paging Next.

- Файл `src/next/mmu.c`: 8 слотов по 8K, регистры `NextReg $50-$57` ([Memory Mapping Register](https://wiki.specnext.dev/Memory_Mapping_Register)). Текущий 16K-paging остаётся как режим совместимости (синхронизация через [Memory map](https://wiki.specnext.dev/Memory_map): запись в `Port 7FFD` пишет одновременно $56/$57).
- Файл `src/next/nextreg.c`: контроллер NextReg-регистров через `I/O $243B` (select) / `$253B` (data) ([Board feature control](https://wiki.specnext.dev/Board_feature_control)). Таблица регистров с read/write handlers. Также опкод `NEXTREG reg,val` через CPU.
- Реализовать AllRAM / CP/M mode (Port $1FFD bit 0).
- Alt ROM 32K (NextReg $8C) — программируемый ROM.
- Reset Register (NextReg $02), Core/Machine type (NextReg $03), Config Mode (NextReg $06).

### Фаза 4 — SD card, BIOS-like меню и загрузка core

Цель: грузить ROMы с SD как реальный Next, с входом в «BIOS» (config menu).

Реальный Next делает следующее: при power-on FPGA читает с SD `machines/next/CONFIG.bin` и список core'ов, затем грузит `enNextZX.rom`/`enNxtmmc.rom` в Alt ROM, и стартует NextZXOS. Если зажать **NMI button** во время загрузки — открывается меню Configuration. Воспроизводим этот UX.

- SD-driver: проверить, что в pico-spec уже есть (есть в ESPectrum). Если spi-based — оставить; добавить SDIO 4-bit как опцию (быстрее для PSRAM-load).
- FAT32 long-file-names — нужны для NextZXOS distro (`enNextZX.rom`, `tbblue.fw`, и пр.). Использовать `ff.h`/petit-fatfs.
- Layout SD: `sys/`, `c/`, `tmp/`, `dot/`, `nextzxos/` — как distro.
- **Pico-side BIOS menu** (`src/next/biosmenu.c`): отрисовка через ULA framebuffer до запуска эмуляции. Пункты: Pick core (по умолчанию NextZXOS), Reset, Hard reset, Machine type (48/128/+2A/+3/Next), Video timing (50/60Hz), Joystick mapping, Mouse, Scandoubler on/off, Save config. Вход в меню — конфигурируемая клавиша (по умолчанию F1) или удержание hotkey при boot — эмулирует NMI/Drive buttons.
- Загрузчик ROM: при старте читает `enNextZX.rom` (`enNxtmmc.rom`) в Alt ROM area PSRAM, синхронизирует NextReg config, выходит из meta-mode и передаёт управление Z80N.

### Фаза 5 — NextZXOS + esxDOS API

Цель: пользовательский опыт «загрузился — увидел Browser».

- Распространять distro **не в репозитории** (лицензия). README: инструкция «скачайте System/Next distro с specnext.com и распакуйте на SD-карту».
- Реализовать **esxDOS hooks** на Pico-стороне: перехват RST $08, разбор команд (`f_open`, `f_read`, `f_write`, `f_close`, `f_seek`, `f_opendir`, `f_readdir`, `f_unlink`, и т.д. — см. [ESXDOS](https://wiki.specnext.dev/ESXDOS) и [NextZXOS API](https://wiki.specnext.dev/NextZXOS)). Минимальный набор для запуска Browser + .NEX loader.
- Поддержка «dot commands» (`/dot/*` на SD) через esxDOS API.
- NextReg $03 = 8 (Next) на старте; машина type выставляется через BIOS-меню или config.
- Real-time clock — пробросить из RP2350 RTC.

### Фаза 6 — Enhanced ULA + 256-цветные палитры

Цель: вывести расширенные палитры и режимы ULA.

- 8 палитр × 256 цветов (ULA×2, Layer 2×2, Sprites×2, Tilemap×2) — [Palettes](https://wiki.specnext.dev/Palettes). NextReg $40-$44 (palette index/value/8-bit value/control).
- [ULA Control Register](https://wiki.specnext.dev/ULA_Control_Register) (NextReg $68).
- [Timex Sinclair Video Mode Control](https://wiki.specnext.dev/Timex_Sinclair_Video_Mode_Control) — Timex HiRes (512×192) и HiColour ULA.
- LoRes 128×96 (NextReg $69 bit 7).
- Pico-видеотракт: расширить scanline renderer pico-spec под палитрный indexed-pixel pipeline. Палитра ресолвится в финальный 5:6:5 / 8:8:8 для HSTX/DVI.

### Фаза 7 — Layer 2

Цель: добавить независимый bitmap-слой 256×192 / 320×256 / 640×256.

- Файл `src/next/layer2.c`. Page registers NextReg $12/$13/$70 ([Layer 2](https://wiki.specnext.dev/Layer_2)). 16K страницы в PSRAM.
- Scroll (NextReg $16/$17), clip-window (NextReg $18 + clip-index), priority over ULA (NextReg $15).
- Видео-микшер: композитинг ULA + Layer 2 с priority (`Port $123B`).

### Фаза 8 — Tilemap

Цель: hardware tilemap 40×32 / 80×32, 16/2-цвет, 8×8 тайлы.

- Файл `src/next/tilemap.c`. NextReg $6B-$6F, $4C ([Tilemap](https://wiki.specnext.dev/Tilemap)).
- Tilemap-RAM в PSRAM (или часть в SRAM для скорости).
- Scroll, clip-window, palette select, ULA-mix mode.

### Фаза 9 — Hardware Sprites

Цель: 128 sprites × 256-цветов, 64 пиксельных attribute slots.

- Файл `src/next/sprites.c`. Port `$303B` + NextReg `$15`/`$34`/`$35`-`$38` ([Sprites](https://wiki.specnext.dev/Sprites)).
- Sprite priority по [Sprite Status/Slot Select](https://wiki.specnext.dev/Sprite_Status/Slot_Select).
- Per-scanline sprite renderer — sprite-over-border, scaling, mirror, rotation, anchor.
- Сложно по производительности: вероятно потребует second core (Cortex-M33).

### Фаза 10 — Copper

Цель: программируемый display coprocessor.

- Файл `src/next/copper.c`. 2K Copper-RAM, инструкции WAIT и NEXTREG-write ([Copper](https://wiki.specnext.dev/Copper)). NextReg $60-$62, $98.
- Запускается синхронно со scanline-рендерером.

### Фаза 11 — zxnDMA

Цель: Z80 DMA-compatible.

- Файл `src/next/dma.c`. Подмножество Z80 DMA chip (WR0-WR5, режим Burst/Continuous) ([DMA](https://wiki.specnext.dev/DMA)). Порт `$6B`, NextReg $10.
- Transfer в lockstep с CPU-cycles.

### Фаза 12 — Audio

Цель: turbosound (3×AY) + 4×DAC.

- Расширить существующий AY-модуль pico-spec до 3 чипов, режимы Mono/ABC/ACB ([Audio / Music](https://wiki.specnext.dev/Audio_/_Music)). Селектор через `$FFFD`.
- Файл `src/next/dacs.c`: 4×8-bit DAC ([SpecDrum/DAC](https://wiki.specnext.dev/SpecDrum/DAC)). NextReg $08, ports $0F/$1F/$F1/$F3/$DF/$0B и т.д.
- Микшинг → I2S/PWM/HDMI-audio.
- Опционально: MIDI out (уже есть в pico-spec на RP2350).

### Фаза 13 — Ввод

Цель: гибкие joystick-режимы и PS/2 + USB-host.

- Kempston1/2, Cursor, Sinclair1/2, Megadrive (MD 3/6-button) — конфиг через [Peripheral 1 Setting Register](https://wiki.specnext.dev/Peripheral_1_Setting_Register) + BIOS-меню.
- PS/2 keyboard + mouse через [Kempston Mouse](https://wiki.specnext.dev/Mouse).
- USB-host (TinyUSB): HID keyboard, mouse, joystick → маппится на эмулируемые порты.
- Megadrive pad ([M30 8BitDo wireless](https://wiki.specnext.dev/M30_8BitDo_wireless_MegaDrive_pad)) — через USB или Bluetooth (если плата поддерживает).

### Фаза 14 — Interrupts

Цель: корректный VBI/VBL/line/CTC.

- 3 режима по [Interrupts](https://wiki.specnext.dev/Interrupts). По умолчанию Mode 0 (classic).
- Line interrupt ([Raster Interrupt Control Register](https://wiki.specnext.dev/Raster_Interrupt_Control_Register), NextReg $22-$23).
- IM2 vectors NextReg $C0.
- Hardware mode 2: VBI, VBL, ESP UART, Pi UART, CTC timers (NextReg $C2-$C8) — реализуем минимум, без ESP/Pi UART (плат на Pico не существует).

## Критичные файлы для модификации

После заливки кода точные пути уточнятся. Ожидаемые места:

- `CMakeLists.txt` (project name, options, board presets) — Фаза 0
- `src/CMakeLists.txt` / `src/next/CMakeLists.txt` (новая подсистема) — Фаза 0
- `src/Z80.cpp` или `Z80.c` (или Z80-эмулятор pico-spec) — Фаза 2 (Z80N opcodes)
- `src/MemESP.cpp` / `src/Memory.c` (memory model) — Фазы 1, 3
- `src/VideoEnt.cpp` / scanline renderer — Фазы 6–10
- `src/AySound.cpp` — Фаза 12
- `src/ESPectrum.cpp` (main loop / init) — Фазы 0, 3, 14
- `src/OSDMain.cpp` или эквивалент — Фаза 4 (BIOS menu — может потребовать полную переработку)
- `src/FileSystem.cpp` / SD-driver — Фазы 4, 5
- `README.md`, `CLAUDE.md` — Фаза 0
- **Новые** файлы в `src/next/`: `nextreg.{h,c}`, `mmu.{h,c}`, `z80n_ops.{h,c}`, `layer2.{h,c}`, `tilemap.{h,c}`, `sprites.{h,c}`, `copper.{h,c}`, `dma.{h,c}`, `dacs.{h,c}`, `biosmenu.{h,c}`, `esxdos_api.{h,c}`, `nex_loader.{h,c}`, `psram.{h,c}`, `rom_loader.{h,c}` (Milestone 1 — чтение `enNextZX.rom` с SD)

## Verification

После каждой фазы:

1. **Build**: `cmake -B build -DPICO_BOARD=pimoroni_pico_plus2 -DPICO_NEXT_FEATURES=ON && cmake --build build` — должно собраться без ошибок.
2. **Milestone 1**: SD c System/Next distro вставлена → плата грузит `enNextZX.rom` → реальная Z80N-эмуляция → ROM сам рисует стартовый экран NextZXOS на HDMI/VGA. UART-лог подтверждает выполнение boot-последовательности ROM.
3. **Flash & boot**: `picotool load build/pico-next.uf2 -f` — после Milestone 1 плата всегда стартует в NextZXOS (BIOS-меню добавляется в Фазе 4).
3. **Regression**: классический ZX 48K/128K `.tap`/`.z80`/`.sna` запускается через старое меню pico-spec (Фазы 0–3).
4. **Next-specific** (Фазы 4+): загрузить System/Next distro 24.11 на SD, проверить выход в NextZXOS Browser.
5. **NEX**: запустить тестовые `.NEX` файлы (`Z80HEAP.nex`, демо с itch.io).
6. **NexTest**: гонять [Formal Core Test Instructions](https://wiki.specnext.dev/Formal_Core_Test_Instructions) — таблица должна показать ↑↑↑ по реализованным подсистемам.
7. **Reference compare**: запускать те же программы в CSpect и сравнивать поведение (особенно цикловую точность Z80N, scroll Layer 2, sprite priority).
8. **Timing**: на RP2350 проверять, что 28 МГц-режим тянется (профилировать обоими ядрами M33).

## Ключевые источники

- [SpecNext Wiki — Specifications](https://wiki.specnext.dev/Specifications)
- [Memory map](https://wiki.specnext.dev/Memory_map) / [Memory Mapping Register](https://wiki.specnext.dev/Memory_Mapping_Register)
- [Extended Z80 instruction set](https://wiki.specnext.dev/Extended_Z80_instruction_set)
- [Video Modes](https://wiki.specnext.dev/Video_Modes) / [Layer 2](https://wiki.specnext.dev/Layer_2) / [Tilemap](https://wiki.specnext.dev/Tilemap) / [Sprites](https://wiki.specnext.dev/Sprites) / [Palettes](https://wiki.specnext.dev/Palettes)
- [Copper](https://wiki.specnext.dev/Copper) / [DMA](https://wiki.specnext.dev/DMA)
- [Audio / Music](https://wiki.specnext.dev/Audio_/_Music) / [SpecDrum/DAC](https://wiki.specnext.dev/SpecDrum/DAC)
- [NextZXOS](https://wiki.specnext.dev/NextZXOS) / [ESXDOS](https://wiki.specnext.dev/ESXDOS) / [NEX file format](https://wiki.specnext.dev/NEX_file_format)
- [Interrupts](https://wiki.specnext.dev/Interrupts) / [Board feature control](https://wiki.specnext.dev/Board_feature_control)
- [Kempston Joystick](https://wiki.specnext.dev/Kempston_Joystick) / [Mouse](https://wiki.specnext.dev/Mouse)
- Целевое железо: [Pimoroni Pico Plus 2](https://shop.pimoroni.com/products/pimoroni-pico-plus-2), [Adafruit Feather RP2350 HSTX + 8MB PSRAM](https://www.adafruit.com/product/6130)
- Канонический репозиторий: [drewpo28/pico-next](https://github.com/drewpo28/pico-next) — самостоятельный проект, без upstream-синхронизации с pico-spec. Код pico-spec используется только как стартовая база (one-shot seed через ветку `claude/pico-next-cleanup-JtWGm`).
- Референсные эмуляторы (только для сравнения поведения): [CSpect](https://mdf200.itch.io/cspect), [ZEsarUX](https://github.com/chernandezba/zesarux)

## Открытые вопросы (на будущее)

- **PSRAM-latency vs Z80N@28MHz**: подтвердится только профилированием. Запасной план — горячие 8K-страницы в SRAM-кэше.
- **Сборка HDMI**: HSTX на Pico Plus 2 vs DVI-bitbang — выбрать после Фазы 0.
- **Лицензия NextZXOS distro**: проверить, можно ли упоминать конкретные файлы и/или прикладывать инструкцию по сборке SD.
- **Совместимость со старыми save-ами pico-spec** (configs / snapshots): сохраняем формат или мигрируем?
