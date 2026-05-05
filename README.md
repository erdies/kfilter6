# KFilter6

KFilter6 is a standalone Qt6 Widgets port of the original KFilter loudspeaker design tool.

The current development direction is intentionally modest and practical: KFilter6 is not meant to be a full room, baffle, and radiation-pattern simulator. It is a physical orientation tool for loudspeaker crossover and enclosure work. Its purpose is to make the consequences of changes to driver parameters, enclosure tuning, crossover networks, and summation behaviour visible while designing and listening.

The program is especially useful for answering questions such as:

- What happens when a capacitor, inductor, or resistor in the crossover is changed?
- How do the individual driver SPL curves combine?
- How do vector and energetic summation differ at driver transitions?
- What happens to impedance when the crossover network is changed?
- How do enclosure parameters such as `Vb` and `Fb` affect the design?
- What approximate bass-reflex tube length follows from a selected tube diameter?

KFilter6 currently remains compatible with the legacy KFilter project-file model. The internal naming still contains some historical terms where changing them would risk compatibility; the visible UI has been modernized where appropriate.

## Status

This repository contains the current Qt6 Widgets port.

The application is a plain Qt6 application, not a KDE Frameworks or KDE Plasma application. It should integrate well into KDE Plasma, but it does not intentionally depend on KDE libraries.

Current executable target:

```sh
kfilter_qt6
```

Current build system:

```sh
CMake + Qt6 + C++17
```

## Features

Current major functionality includes:

- Four-driver loudspeaker model.
- Driver Thiele/Small parameter editing.
- Enclosure and gain parameter editing.
- Crossover/network parameter editing.
- SPL and impedance plotting.
- Vector SPL sum.
- Energetic SPL sum.
- Total impedance curve.
- Read-only graphical network preview.
- Network preview modes:
  - All Active Drivers
  - Driver 1
  - Driver 2
  - Driver 3
  - Driver 4
- Configurable network-preview background color.
- Automatic light/dark contrast handling in the network preview.
- Bass-reflex tube helper using `Vb`, `Fb`, and tube diameter.
- Legacy `.kfp` project loading and saving.

## Conceptual notes

### Vector SPL sum

The vector SPL sum is phase-sensitive. It includes constructive and destructive interaction between driver outputs.

Use it when checking the concrete acoustic summation of drivers around crossover regions.

### Energetic SPL sum

The energetic SPL sum is the visible UI name for the historical scalar SPL summation. It sums SPL contributions energetically and ignores phase cancellation and phase addition.

This is useful as an approximation of the energy balance between drivers. It is not a full polar-integrated power response calculation, but it is a practical design guide when shaping driver transitions and avoiding energetically uneven crossover behaviour.

### Network preview

The network preview is a read-only schematic visualization of the current crossover topology. It is intended as a consistency and orientation aid while editing numeric network values.

The default view is **All Active Drivers**. A driver is considered active if at least one curve/total flag is enabled or at least one network-topology value is non-zero.

Explicit single-driver views remain available through:

```text
View -> Network Preview -> Driver View
```

### Bass-reflex tube helper

The `Enclosure and gain` section contains a tube helper:

```text
Tube diameter -> Tube length
```

The tube length is calculated from:

- `Vb`
- `Fb`
- selected tube diameter

The tube diameter is stored as a user setting per driver tab. It is not currently stored in the `.kfp` project file.

## Dependencies

Required:

- CMake 3.21 or newer
- C++17-capable compiler
- Qt6 Core
- Qt6 Widgets

Optional for development:

- Ninja
- CTest

On many Linux systems the required Qt functionality is provided by the Qt6 base development package. Exact package names differ by distribution.

## Build instructions

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run the application:

```sh
./build/kfilter_qt6
```

Run the smoke tests:

```sh
ctest --test-dir build --output-on-failure
```

## Useful CMake options

The default build enables the Qt6 application and the smoke tests.

Available options include:

```sh
-DKFILTER_BUILD_QT6_APP=ON
-DKFILTER_BUILD_DRIVER_SMOKETEST=ON
-DKFILTER_BUILD_PROJECTIO_SMOKETEST=ON
-DKFILTER_BUILD_DOCUMENT_SMOKETEST=ON
-DKFILTER_BUILD_DEFAULTS_SMOKETEST=ON
-DKFILTER_ENABLE_WIZARD=OFF
```

Example development build with Ninja:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

## Basic manual

### 1. Start a project

Start KFilter6 and either create a new project or open an existing `.kfp` file.

The application works with four driver slots. Not every slot has to be used.

### 2. Edit driver parameters

Open:

```text
Edit -> Driver Parameters...
```

For each driver, edit the relevant Thiele/Small parameters, enclosure values, gain, and curve options.

Important fields include:

- `Driver name`
- `Rdc`
- `Lsp`
- `Fs`
- `Qts`
- `Qes`
- `Qms`
- `Vas`
- `Diameter`
- `Vb`
- `Fb`
- `Enclosure type`
- `Gain`

The button:

```text
Calculate Qts from Qes and Qms
```

updates `Qts` from the entered `Qes` and `Qms` values.

### 3. Enable curves and totals

In the driver-parameter dialog, use `Curves and totals` to decide what should be plotted and summed:

- `Show SPL curve`
- `Show impedance curve`
- `Include in vector SPL sum`
- `Include in energetic SPL sum`
- `Include in total impedance`
- `Invert polarity`
- `Use full crossover simulation`

Use vector and energetic sums together. The vector sum shows phase-sensitive interaction, while the energetic sum is useful for checking the broader energy balance between drivers.

### 4. Edit the crossover network

Open:

```text
Edit -> Network / Filter Parameters...
```

Each driver has eight network sections. Each section contains:

- series R
- series C
- series L
- shunt R
- shunt C
- shunt L

Capacitors and inductors are edited in user-friendly units, while the internal model keeps the historical storage units.

### 5. Use the network preview

The network preview shows the current network topology graphically.

The default mode is:

```text
All Active Drivers
```

You can switch to a single driver through:

```text
View -> Network Preview -> Driver View
```

The preview background can be changed through:

```text
View -> Network Preview -> Background Color...
```

Reset it through:

```text
View -> Network Preview -> Reset Background Color
```

### 6. Save the project

Use:

```text
File -> Save
File -> Save As...
```

Projects use the legacy KFilter `.kfp` file format.

## User settings

Some UI settings are stored through `QSettings` and are intentionally not part of the `.kfp` project file.

Examples:

- Network-preview background color.
- Last used bass-reflex tube diameter per driver tab.
- Window and toolbar layout settings.

## Repository layout

Typical source files:

```text
CMakeLists.txt
mainqt6.cpp
kfilterqt6app.cpp / .h
kfilterdoc.cpp / .h
kfilterprojectio.cpp / .h
driver.cpp / .h
kfilterview.cpp / .h
circuitout.cpp / .h
driverparametersdialog.cpp / .h
networkparametersdialog.cpp / .h
tools/
```

The old KDE3/Qt3 files may exist in historical branches or handover packages as reference material, but the active application path is the Qt6 path beginning at:

```text
mainqt6.cpp
kfilterqt6app.cpp
```

## Development notes

KFilter6 is being ported and improved incrementally. The preferred change style is small, reviewable patches that keep the application buildable after each step.

Important compatibility rule:

- Do not change the `.kfp` project-file format unless the migration and compatibility implications are explicitly handled.

Terminology note:

- The UI term `Energetic SPL sum` corresponds to the historical internal scalar SPL summary calculation.
- Internal names such as `ScalarSummary`, `ScalarSummaryisActive`, and `PressureScalarSummary()` may remain for compatibility and to reduce unnecessary churn.

## License

Add the applicable license information for this repository here, or refer to a separate `LICENSE` file if one exists.
