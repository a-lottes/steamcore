# BRASS-01

> **A Steampunk Arcade Console for an Alternate 1983**

> **Built with [aSPARK](https://github.com/a-lottes/aSPARK)** — every feature in this repo went from idea to
> release through the gated SPARK loop: Product Owner, Designer, Engineering Manager,
> Reviewer, QA Tester and Release Manager, one gate at a time. The complete paper trail
> lives in [`.spark/`](.spark/): 13 features, one folder each, plus the project's
> [constitution](.spark/constitution.md). The console has no browser surface, so QA runs by a
> declared substitute method (host unit tests, framebuffer dumps, serial transcripts) instead
> of `/demo-day`'s usual real-browser pass — see constitution §8.
>
> **New here? Read one feature end to end:** [`highscore-system`](.spark/highscore-system/) (shipped as
> `v0.8.0`) — [`spec.md`](.spark/highscore-system/spec.md) → [`plan.md`](.spark/highscore-system/plan.md)
> → [`review.md`](.spark/highscore-system/review.md) (passed in round 2)
> → [`qa.md`](.spark/highscore-system/qa.md) (passed, one hardware-gated AC honestly left `not capturable`)
> → [`release.md`](.spark/highscore-system/release.md).

BRASS-01 ist eine selbst entwickelte Mini-Retro-Konsole mit
Steampunk-Ästhetik und bewusst einfacher Arcade-Grafik im Stil der
frühen 1980er Jahre.

Die Konsole verbindet moderne Mikrocontroller-Hardware mit einer bewusst
limitierten Spieleplattform:

-   monochrome orange/schwarze Grafik
-   einfache Pixelgrafik
-   Arcade-Spiele mit sofort verständlichem Gameplay
-   physische Arcade-Bedienelemente
-   Steampunk-Gehäuse aus Holz, Messing und Kupfer
-   optionaler Anschluss eines klassischen Röhrenfernsehers
-   vollständig eigene Spiele und eigene Game Engine

Das Projekt soll nicht versuchen, alte Konsolen möglichst exakt zu
emulieren. Stattdessen entsteht eine **eigene fiktive Spieleplattform**,
die so wirken könnte, als wäre sie 1983 in einer alternativen
Steampunk-Zeitlinie entwickelt worden.

------------------------------------------------------------------------

## Projektvision

### BRASS-01

**BRASS-01** ist die Hardware-Plattform.

### SteamCore

**SteamCore** ist die Software-Plattform und Game Engine.

### Games

Die Spiele werden speziell für SteamCore entwickelt und nutzen bewusst
technische Einschränkungen, um einen einheitlichen Retro-Look zu
erzeugen.

Das Ziel ist nicht maximale Rechenleistung, sondern maximale Atmosphäre.

``` text
                     BRASS-01
                         |
                 +-------+-------+
                 |               |
              Hardware        SteamCore
                 |               |
        +--------+--------+  +---+----------------+
        |        |        |  |                    |
      Display  Input    Audio Engine          System UI
                               |
                         +-----+-----+
                         |     |     |
                      Games  Score  Save
```

------------------------------------------------------------------------

# Aktueller Hardware-Prototyp

Der erste Prototyp basiert auf einem:

**ESP32-S3-N16R8**

mit:

-   16 MB Flash
-   8 MB PSRAM
-   ESP32-S3 SoC
-   USB-C
-   Entwicklungs-/Breakout-Board mit herausgeführten GPIOs

Das aktuell verwendete Entwicklungsboard ist auf einem Breakout-Board
montiert, das die GPIOs über Schraubklemmen zugänglich macht. Dadurch
eignet es sich sehr gut für den frühen Hardware-Prototyp und für die
Entwicklung der Controller- und Display-Anbindung.

> Die konkrete Pinbelegung wird im Projekt dokumentiert, sobald die
> endgültige Hardware festgelegt ist. GPIOs sollten nicht anhand der
> Beschriftung des Breakout-Boards allein für die finale Verdrahtung
> angenommen werden.

------------------------------------------------------------------------

# Ziel-Hardware

Die finale BRASS-01-Hardware soll ungefähr folgende Komponenten
enthalten:

  Komponente        Ziel
  ----------------- ----------------------------------------
  MCU               ESP32-S3-N16R8
  Display           7-8" LCD, ca. 800x480
  Grafik            Orange auf Schwarz
  Controller        Arcade-Joystick + Buttons
  Audio             Mono-Lautsprecher
  Storage           Flash, optional MicroSD
  Stromversorgung   USB-C, optional Akku
  CRT-Ausgang       PAL Composite, optional
  Gehäuse           Holz + 3D-Druck + Messing/Kupfer
  Bedienung         Arcade Controls + mechanische Schalter

------------------------------------------------------------------------

# Display-Konzept

Das Display darf modern sein, die Grafik soll es nicht sein.

Die Spiele verwenden eine bewusst niedrige virtuelle Auflösung:

``` text
240 x 160
```

Das Bild wird anschließend ganzzahlig auf das physische Display skaliert.

Der aktuelle Prototyp verwendet ein **ILI9488 3.5" SPI-TFT** mit 320x480,
quer genutzt als 480x320. Die virtuelle Auflösung wird dafür mit Faktor 2
skaliert:

``` text
240 x 160   --  x2  -->   480 x 320
```

Die virtuelle Auflösung ist eine Compile-Time-Konstante. Das finale 7-8"-Panel
kann dadurch später eine andere Skalierung oder eine andere virtuelle Auflösung
bekommen, ohne dass Spielcode angepasst werden muss.

Durch die niedrige virtuelle Auflösung bleiben Pixel, Sprites und Animationen
bewusst einfach und grob.

## Farbmodell

Die Engine arbeitet grundsätzlich mit einer monochromen Palette:

``` text
BLACK
DARK ORANGE
ORANGE
BRIGHT ORANGE
```

Die tatsächliche Darstellung kann auf einem LCD als orangefarbene Grafik
auf schwarzem Hintergrund erfolgen.

Zusätzlich sind optionale CRT-Effekte vorgesehen:

-   Scanlines
-   Glow/Bloom
-   leichte Helligkeitsschwankungen
-   Pixel-Phosphor-Effekt
-   dezentes Flimmern
-   optionale Bildschirmkrümmung

Diese Effekte sollen sparsam eingesetzt werden. Die Grafik soll nach
Retro aussehen, nicht nach einem modernen Filter.

------------------------------------------------------------------------

# CRT-Unterstützung

Ein besonderes Feature von BRASS-01 ist die geplante Unterstützung eines
klassischen Röhrenfernsehers.

Wenn der Fernseher Composite Video unterstützt, kann später ein analoger
PAL-Ausgang ergänzt werden.

Geplantes Konzept:

``` text
                  SteamCore
                      |
                 Game Renderer
                      |
             +--------+--------+
             |                 |
          LCD Output      Composite PAL
             |                 |
          7" LCD             CRT TV
```

Für Fernseher mit SCART kann Composite über einen geeigneten Adapter
eingespeist werden.

Bei sehr alten Fernsehern, die ausschließlich einen Antenneneingang
besitzen, kann optional ein externer RF-Modulator verwendet werden.

> Der CRT-Ausgang ist zunächst ein Entwicklungsziel und kein Bestandteil
> des ersten Hardware-Prototyps.

------------------------------------------------------------------------

# Software-Architektur

SteamCore soll als kleine, deterministische Arcade Engine aufgebaut
werden.

Die folgende Übersicht zeigt die **fachliche Modulgliederung** der Engine, also
welche Verantwortlichkeiten es gibt — sie ist **kein Verzeichnislayout**. Das
Verzeichnislayout des Repositories steht weiter unten unter
[Repository-Struktur](#repository-struktur).

``` text
SteamCore
│
├── core       -- game loop, timing, state
├── graphics   -- framebuffer, sprites, text, particles, effects
├── input      -- joystick, buttons, controllers
├── audio      -- tones, effects, music
├── system     -- menu, settings, savegame
└── games      -- galactic, racer, maze, ...
```

Die genaue Aufteilung kann sich während der Entwicklung noch ändern.

------------------------------------------------------------------------

# Game Loop

Die Spiele sollen nach einem einfachen Arcade-Modell funktionieren:

``` text
START
  |
  v
INPUT
  |
  v
UPDATE
  |
  v
COLLISION
  |
  v
RENDER
  |
  v
AUDIO
  |
  +------> NEXT FRAME
```

Die Engine soll möglichst wenig Abhängigkeiten zwischen den Spielen
erzeugen.

Ein Spiel soll sich im Idealfall auf die eigentliche Spielmechanik
konzentrieren können:

``` cpp
void update(GameInput input);
void render();
void onCollision(Entity& a, Entity& b);
```

Die Engine übernimmt:

-   Timing
-   Framebuffer
-   Eingaben
-   Audio
-   Rendering
-   Score
-   Highscores
-   Game State
-   Persistenz

------------------------------------------------------------------------

# Grafikphilosophie

BRASS-01 folgt absichtlich strengen Grafikregeln.

## Do

-   große Pixel
-   einfache Sprites
-   wenige Animationsframes
-   klare Silhouetten
-   einfache Partikeleffekte
-   Orange/Schwarz
-   hohe Lesbarkeit
-   einfache geometrische Formen

## Don't

-   fotorealistische Grafiken
-   komplexe 3D-Modelle
-   große Texturen
-   moderne UI-Animationen
-   RGB-Regenbogen-Effekte
-   unnötige grafische Details

Ein Spiel soll aussehen, als hätte es mit sehr begrenzter Hardware
entwickelt werden müssen.

------------------------------------------------------------------------

# Geplante Spiele

## Galactic Invasion

Ein eigener Arcade-Shooter als Hommage an klassische
Space-Invader-/Galaxian-/Galaga-artige Spiele.

Mechanik:

-   Raumschiff horizontal bewegen
-   Gegnerformationen
-   gegnerische Angriffe
-   Schüsse
-   Wellen
-   Highscore
-   zunehmender Schwierigkeitsgrad

Steampunk-Interpretation:

-   mechanische Flugmaschinen
-   Aether-Schiffe
-   Clockwork-Gegner
-   elektrische Waffen

------------------------------------------------------------------------

## Steam Racer

Ein vertikales Arcade-Rennspiel.

Mechanik:

-   Fahrzeug steuern
-   Hindernisse ausweichen
-   Gegner überholen
-   Geschwindigkeit erhöhen
-   Punkte sammeln
-   Highscore

Die Fahrzeuge können als frühe mechanische Automobile interpretiert
werden.

------------------------------------------------------------------------

## The Great Machine

Ein Maze-/Arcade-Spiel innerhalb einer riesigen mechanischen Maschine.

Elemente:

-   Zahnräder
-   Ventile
-   Dampf
-   bewegliche Plattformen
-   Türen
-   Fallen
-   Schlüssel
-   Timer

------------------------------------------------------------------------

## Airship Battle

Ein Arcade-Spiel mit Luftschiffen.

Mögliche Mechaniken:

-   horizontale Bewegung
-   gegnerische Luftschiffe
-   Geschütze
-   Wolken
-   Hindernisse
-   Boss-Gegner

------------------------------------------------------------------------

## Mine Runner

Eine kleine Lore fährt durch ein endloses Bergwerk.

Mechaniken:

-   Weichen
-   Felsen
-   Tunnel
-   Hindernisse
-   Sammelobjekte
-   Geschwindigkeit
-   Highscore

------------------------------------------------------------------------

# Audio

Der Sound soll ebenfalls bewusst retro gehalten werden.

Keine komplexen modernen Soundtracks.

Stattdessen:

-   kurze Square-Wave-Töne
-   einfache Melodien
-   Explosionen
-   Schüsse
-   Treffer
-   UI-Klicks
-   mechanische Geräusche

Beispiel:

``` text
BOOT
 -> beep
 -> beep
 -> rising tone
 -> SYSTEM READY
```

Optional kann später ein einfacher Sound-Generator implementiert werden,
sodass viele Effekte direkt aus wenigen Parametern erzeugt werden.

------------------------------------------------------------------------

# Controller

BRASS-01 soll klassische Arcade-Bedienung unterstützen.

## Interne Controls

Geplant:

-   4/8-Wege-Joystick
-   FIRE
-   START
-   SELECT
-   optional weiterer Action-Button

Beispiel:

``` text
       [ JOYSTICK ]

      [ FIRE ] [ START ]

             [ SELECT ]
```

Die finale Anzahl und Anordnung wird zusammen mit dem Gehäuse
festgelegt.

## Externe Controller

Später sollen zusätzlich USB- und/oder Bluetooth-Controller unterstützt
werden.

------------------------------------------------------------------------

# Steampunk Industrial Design

Die Elektronik soll bewusst modern und unsichtbar bleiben.

Die sichtbare Hardware soll dagegen wie eine Maschine aus einer
alternativen industriellen Vergangenheit wirken.

## Materialien

-   dunkles Holz
-   Messing
-   Kupfer
-   schwarze Metallteile
-   Schrauben und Nieten
-   optional Glas
-   optionale dekorative Zahnräder

## Bedienelemente

-   mechanische Kippschalter
-   große runde Buttons
-   Messingknöpfe
-   Joystick mit Kugelgriff
-   optionales analoges Anzeigeinstrument
-   orange leuchtendes Deko-Element

## Designprinzip

**Modern inside. Mechanical outside.**

------------------------------------------------------------------------

# Boot Experience

Das Einschalten soll Teil des Spiels sein.

Beispiel:

``` text
BRASS-01

AETHER COMPUTING SYSTEM

INITIALIZING...

MEMORY ........ OK
DISPLAY ....... OK
INPUT .......... OK
AUDIO .......... OK
ENGINE ......... OK

SYSTEM READY

PRESS START
```

Danach erscheint das Hauptmenü:

``` text
        BRASS-01

 > GALACTIC INVASION
   STEAM RACER
   THE GREAT MACHINE
   AIRSHIP BATTLE
   MINE RUNNER

   HIGH SCORES
   SETTINGS
```

------------------------------------------------------------------------

# Highscore-System

Arcade-Spiele leben von Highscores.

SteamCore soll deshalb ein einheitliches Highscore-System bereitstellen.

Beispiel:

``` text
GALACTIC INVASION

1. AND   12500
2. MAX    9800
3. EVA    7200
4. TOM    6100
5. LEO    5400
```

Die Speicherung erfolgt zunächst im internen Flash.

Eine spätere Erweiterung auf MicroSD ist möglich.

------------------------------------------------------------------------

# Entwicklungsumgebung

Der genaue Software-Stack wird im Projekt festgelegt. Für die
ESP32-S3-Plattform sind insbesondere folgende Ansätze geeignet:

### ESP-IDF

Empfohlene Option für die finale Firmware.

Vorteile:

-   volle Kontrolle über Hardware
-   gute ESP32-S3-Unterstützung
-   Display- und GPIO-APIs
-   Audio
-   Bluetooth
-   USB
-   FreeRTOS
-   gute Grundlage für eine eigene Engine

### Arduino Core

Kann für frühe Prototypen und schnelle Hardwaretests verwendet werden.

### C++

Für die eigentliche Engine bietet sich C++ an.

Eine mögliche Kombination ist:

``` text
ESP-IDF
   +
C++
   +
SteamCore
   +
Game Modules
```

------------------------------------------------------------------------

# Entwicklungsphasen

## Phase 1 - Hardware Proof of Concept

-   [x] ESP32-S3-N16R8 vorhanden
-   [ ] GPIO testen
-   [ ] Buttons anschließen
-   [ ] Joystick anschließen
-   [ ] Audio testen
-   [x] Display auswählen (ILI9488 3.5" SPI, 480x320 quer)
-   [x] Display ansteuern (Spike in `firmware/system/`: 18bpp-Pixelformat
    am echten Panel verifiziert, siehe `.spark/constitution.md` §3)

## Phase 2 - SteamCore

-   [ ] Game Loop
-   [x] Framebuffer (240x160, 4-Farben-Palette, Clipping)
-   [x] Sprite-System (Blitting mit Transparenz und Stride)
-   [ ] Text Rendering
-   [ ] Input Abstraction
-   [ ] Collision System
-   [ ] Audio API
-   [ ] Score System
-   [ ] Highscore System
-   [ ] Game State Management

> Framebuffer und Sprite-System sind vollständig host-getestet (kein
> ESP-IDF nötig, siehe `docs/host-tests.md`) und über einen
> Dump-Format/PNG-Viewer visuell überprüfbar (`docs/dump-format.md`,
> `make view`). Das ILI9488-Panel ist inzwischen verdrahtet und über
> einen Hardware-Spike (`firmware/system/`) verifiziert — die beiden
> sind aber noch nicht verbunden: der echte Display-Treiber (Dirty
> Tiles, DMA-Push, Framebuffer-Integration) ist noch offen.

## Phase 3 - Erste Spiele

-   [ ] Galactic Invasion
-   [ ] Steam Racer
-   [ ] The Great Machine
-   [ ] Airship Battle
-   [ ] Mine Runner

## Phase 4 - Hardware

-   [ ] finales Display
-   [ ] Controller Panel
-   [ ] Lautsprecher
-   [ ] Stromversorgung
-   [ ] Gehäuse-Prototyp
-   [ ] 3D-CAD-Modell
-   [ ] Holz-/Messing-Design

## Phase 5 - CRT

-   [ ] Composite-Ausgang untersuchen
-   [ ] PAL-Ausgabe testen
-   [ ] CRT-Fernseher testen
-   [ ] CRT-Modus in SteamCore integrieren

## Phase 6 - Final Console

-   [ ] finales PCB / Wiring
-   [ ] Gehäuse
-   [ ] Beschriftungen
-   [ ] Frontpanel
-   [ ] Bootscreen
-   [ ] vollständiges Spielepaket

------------------------------------------------------------------------

# Design Constraints

Die technischen Einschränkungen sind ein bewusster Teil des Projekts.

  Bereich               Ziel
  --------------------- ------------------------
  Grafik                monochrom / Orange
  Virtuelle Auflösung   240x160 (x2 -> 480x320)
  Gameplay              Arcade
  Steuerung             maximal wenige Buttons
  Spielzeit             kurze Sessions
  UI                    minimal
  Sound                 einfache Synthese
  3D                    nicht erforderlich
  Online                nicht erforderlich
  Emulation             nicht erforderlich

Die Einschränkungen sollen Kreativität fördern und den Charakter der
Plattform erhalten.

------------------------------------------------------------------------

# Warum ESP32-S3?

Der ESP32-S3 ist für dieses Projekt interessant, weil er deutlich
leistungsfähiger ist als die Hardware, für die viele klassische
Arcade-Spiele entwickelt wurden, gleichzeitig aber noch eine sehr
direkte Embedded-Entwicklung erlaubt.

Das bedeutet:

``` text
                 Moderne Hardware
                       |
                    ESP32-S3
                       |
             +---------+---------+
             |                   |
          genug Leistung      bewusst
             |               eingeschränkt
             v                   v
        moderne Engine      Retro-Grafik
```

BRASS-01 nutzt also nicht die Grenzen der Hardware aus.

**Die Grenzen werden bewusst durch das Design definiert.**

------------------------------------------------------------------------

# Repository-Struktur

Eine mögliche finale Repository-Struktur:

``` text
brass-01/
│
├── README.md
├── LICENSE
├── docs/
│   ├── hardware.md
│   ├── architecture.md
│   ├── display.md
│   ├── controller.md
│   └── crt.md
│
├── firmware/
│   ├── steamcore/
│   └── system/
│
├── games/
│   ├── galactic-invasion/
│   ├── steam-racer/
│   ├── great-machine/
│   ├── airship-battle/
│   └── mine-runner/
│
├── hardware/
│   ├── prototype/
│   ├── pcb/
│   └── wiring/
│
├── enclosure/
│   ├── cad/
│   ├── 3d-print/
│   └── drawings/
│
├── assets/
│   ├── sprites/
│   ├── fonts/
│   ├── sounds/
│   └── ui/
│
└── tools/
```

------------------------------------------------------------------------

# Grundidee des Projekts

BRASS-01 ist kein Emulator.

Es ist keine Kopie einer alten Konsole.

Es ist eine **neue kleine Arcade-Plattform mit bewusst gesetzten
Retro-Regeln**.

Die Hardware ist modern.

Die Software ist selbst entwickelt.

Die Spiele sind neu.

Die Ästhetik ist eine Mischung aus:

``` text
1880s Steampunk
        +
1980s Arcade
        +
Modern Embedded Hardware
        =
BRASS-01
```

------------------------------------------------------------------------

# Status

**Project Status: Prototype / Early Development**

Aktuell vorhanden:

-   ESP32-S3-N16R8 Entwicklungsboard
-   GPIO Breakout / Schraubklemmen-Board
-   Konzept für 7-8" Display
-   Konzept für Arcade Controller
-   Konzept für eigene Game Engine
-   Konzept für optionalen CRT-Ausgang

Der nächste sinnvolle Schritt ist ein kleiner Hardware-Prototyp:

``` text
ESP32-S3
   |
   +-- Display
   +-- Joystick
   +-- FIRE
   +-- START
   +-- Speaker
   |
   v
First playable game
```

Danach wird die Hardware schrittweise in das endgültige BRASS-01-Gehäuse
überführt.

------------------------------------------------------------------------

## License

Die Lizenz wird festgelegt, sobald die Repository-Struktur und die
gewünschte Open-Source-Strategie definiert sind.

------------------------------------------------------------------------

**BRASS-01**

*Mechanical Entertainment System*

*Designed for an alternate 1983.*
