<!--
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2002-2026 Martin Erdtmann
-->

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

KFilter6 writes new `.kfp` project files as versioned JSON. Legacy text-based KFilter project files remain readable and are migrated to JSON the next time they are saved. The internal naming still contains some historical terms where changing them would risk compatibility; the visible UI has been modernized where appropriate.

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
- Interactive graphical network preview.
- Network preview modes:
  - All Drivers
  - Driver 1
  - Driver 2
  - Driver 3
  - Driver 4
- Configurable network-preview background color.
- Automatic light/dark contrast handling in the network preview.
- Bass-reflex tube helper using `Vb`, `Fb`, and tube diameter.
- Versioned JSON `.kfp` project saving with legacy `.kfp` loading compatibility.
- Per-driver SPL correction curves drawn from logarithmic-frequency waypoints.
- Import of absolute SPL measurement files with 0 dB calibration, a dedicated
  correction window, and optional logarithmic fades.
- Optional merging of correction curves into individual, vector-sum, and energetic-sum SPL curves.
- Per-driver export of stored relative SPL correction points as three-column FRD files with neutral `0 deg` phase.
- Persistent correction curves and merge state in JSON project format version 2.

## Conceptual notes

### Vector SPL sum

The vector SPL sum is phase-sensitive. It includes constructive and destructive interaction between driver outputs.

Use it when checking the concrete acoustic summation of drivers around crossover regions.

### Energetic SPL sum

The energetic SPL sum is the visible UI name for the historical scalar SPL summation. It sums SPL contributions energetically and ignores phase cancellation and phase addition.

This is useful as an approximation of the energy balance between drivers. It is not a full polar-integrated power response calculation, but it is a practical design guide when shaping driver transitions and avoiding energetically uneven crossover behaviour.

### SPL correction curves

The **Measurements** menu can be used to draw a relative SPL correction curve for each driver. Waypoints are stored in Hz and dB and connected over the logarithmic frequency axis.

Absolute measurement files can also be imported per driver. The import dialog calculates a constant offset from the median level in a user-selected calibration range, so that this reference range becomes `0 dB`. A separate correction window defines which part of the measurement is retained. Optional lower and upper fades use logarithmic frequency spacing and smoothstep weighting; outside the retained curve span the correction remains neutral. Additional columns such as phase are ignored.

The importer accepts text-oriented FRD/CSV/DAT-style files whose first two numeric columns contain frequency in Hz and level in dB. Whitespace, tabs, semicolons, and unambiguous comma-separated rows are supported. Duplicate frequencies are combined using the median level. Imported curves replace the existing correction curve of the selected driver only after explicit confirmation.

The **Export Measurement for Driver** submenu writes the stored correction points of the selected driver as a UTF-8, three-column FRD file. The columns contain frequency in Hz, relative correction in dB, and an intentionally neutral phase of `0 deg`. KFilter metadata marks the file as `magnitude-correction`, states that the values are not absolute SPL, and identifies the phase column as having no independent phase meaning. Only the stored points are exported; no interpolation grid, driver gain, simulated response, or merged response is added. Patch 163 does not yet implement the special reimport semantics for KFilter correction FRD files, so such a file should not currently be reimported as an absolute measurement without deliberate handling.

Patch 164 centralizes the effective measurement correction in `KFilterDoc`. Individual SPL curves, curve-label placement, vector sums, and energetic sums now obtain their dB correction or linear amplitude factor through the same document API. This is a refactoring only: correction points remain scalar dB values, interpolation remains uncached, and project/FRD formats and numerical results are unchanged.

With **Merge Measurement** enabled, the interpolated correction is applied to the corresponding simulated driver SPL curve; `0 dB` is neutral. For vector and energetic sums, the dB correction is converted to the linear pressure factor `10^(correctionDb / 20)`. The factor scales real and imaginary pressure components equally, so the simulated phase remains unchanged while the corrected magnitude enters both sum calculations.

The **Hide Measurement for Driver** submenu controls each driver independently. A checked driver keeps its simulated SPL curve visible but suppresses both the stored correction curve and its correction influence. With Merge enabled, hidden drivers therefore contribute their uncorrected simulation to vector and energetic sums, while non-hidden drivers contribute their corrected simulation. The same effective contribution is used for the individual curve and both sum modes, so mixed hide states remain mathematically and visually consistent.

Only the resulting relative correction points are project data. The original measurement file, absolute raw levels, calibration range, offset settings, and fade settings are not persisted. The current `.kfp` JSON format version 6 stores each correction curve together with its per-driver hide state, plus the project-wide merge switch. It also persists every driver's complete active-filter metadata and Baffle / Diffraction settings. Calculated complex transfer-response arrays remain transient and are rebuilt from the stored metadata after loading. PDF rendering follows the same effective per-driver correction state as the plot.

### Active filters

The **Edit -> Active Filter Parameters...** dialog maintains one ordered active-filter
chain per driver. Supported sections are multiplied into the complex driver response
before the measurement-amplitude correction, so both magnitude and phase affect the
individual SPL curve, vector sum, and energetic sum through the same centralized
signal path. The optional diagnostic overlay shows `20 * log10(|H_active(f)|)` and
does not control whether the filter itself is active.

The current active-filter engine supports these transfer sections:

- Butterworth low-pass, orders 1 through 8;
- Butterworth high-pass, orders 1 through 8;
- Linkwitz-Riley low-pass/high-pass as LR2 or LR4;
- Butterworth band-pass, defined as high-pass at the lower cutoff multiplied by
  low-pass at the upper cutoff, with the selected order applied independently
  to both flanks;
- full-depth second-order Notch with center frequency `f0` and quality factor `Q`;
- Gain as a frequency-independent dB multiplier;
- Delay as a pure time delay with unity magnitude;
- Polarity as normal (`+1`) or inverted (`-1`) phase;
- first-order and second-order All-pass (AP1/AP2).

For a Butterworth Band-pass, `Frequency 1` is the lower cutoff and `Frequency 2`
is the upper cutoff. The lower cutoff must be strictly below the upper cutoff.
This is a crossover-style loudspeaker band-pass rather than a resonant `f0/Q`
band-pass section. Its complex transfer is

```text
H_band(f) = H_highpass(f, f_lower) * H_lowpass(f, f_upper)
```

so magnitude and phase from both flanks are preserved.

Linkwitz-Riley LR2/LR4 low-pass and high-pass sections use

```text
H_LR,N(f) = H_BW,N/2(f)^2
```

so each branch is -6.0206 dB at its crossover frequency.

Gain, Delay, and Polarity are elementary complex multipliers:

```text
H_gain(f)     = 10^(gainDb/20)
H_delay(f)    = exp(-j*2*pi*f*delaySeconds)
H_polarity(f) = +1 (normal) or -1 (inverted)
```

Delay must be finite and non-negative. Gain must produce a finite positive
linear multiplier. These sections are multiplied into the same complex chain as
the frequency-selective filters.

All-pass sections are unity-magnitude phase filters. KFilter implements the
normalized forms

```text
AP1: H(s) = (s - 1) / (s + 1)
AP2: H(s) = (s^2 - s/Q + 1) / (s^2 + s/Q + 1)
s = j*f/f0
```

AP1 uses only `frequencyHz`; its stored Q value is ignored. AP2 uses both
`frequencyHz` and a positive Q. Only orders 1 and 2 are supported for All-pass.
Both variants preserve `|H(f)| = 1` and affect only phase.

The Notch response is

```text
H(jw) = (1 - r^2) / (1 - r^2 + j*r/Q),  r = f/f0
```

so a raster point exactly at `f0` is nulled (`H = 0+0j`). Increasing `Q` narrows
the rejected band. The currently persisted `gainDb` field of a Notch section is
reserved metadata for a possible future finite-depth variant and does not affect
the current DSP; the dialog therefore exposes only center frequency and Q for Notch.
If any enabled section in a chain is unsupported or invalid, the complete active
filter stage for that driver is bypassed rather than applying a supported prefix.
Active-filter metadata was introduced in `.kfp` format version 5 and remains part of
the current version-6 format; calculated 150-point complex responses remain transient
cache data.

### Baffle / Diffraction

The **Edit -> Baffle / Diffraction Parameters...** dialog maintains one independent
Baffle processing stage per driver. Two models are available:

- **Simple Baffle Step**: width-only engineering shelf with midpoint
  `f0 = 115 / W[m]`, 0 dB LF reference, +3.0103 dB at `f0`, and approximately
  +6.02 dB at high frequency.
- **Rectangular Edge Diffraction**: sharp-edged, point-source, on-axis far-field
  model using baffle width/height and the driver centre position `(X,Y)`.

The Patch-192 rectangular model discretizes the perimeter into 200 edge sources by
default, distributed approximately in proportion to edge length while retaining all
four corners. For each edge source the angular increment `phi_j` seen from the driver
sets the normalized weight, and the complex response is

```text
H_rect(f) = 2 - sum_j w_j * exp(-j * k * b_j)
w_j       = phi_j / (2*pi)
k          = 2*pi*f / 343 m/s
```

`b_j` is the driver-centre-to-edge-source distance. Stage 2 replaces Stage 1; the two
responses are never multiplied together. There is no hidden observer-distance
parameter in this first rectangular model.

The resulting complex multiplier is inserted after the Active Filter response and
before the scalar Measurement correction in the centralized driver path, so both its
magnitude and phase also enter the vector SPL sum. Invalid rectangular geometry safely
bypasses only the Baffle stage.

The optional diagnostic overlay shows `20 * log10(|H_baffle(f)|)` with its own
dash-dot plot style. Diagnostic visibility never controls whether the Baffle stage
itself is active. **Hide Measurement** affects only the Measurement correction and
therefore does not bypass Baffle processing.

`.kfp` format version 6 stores each driver's Baffle enable state, model, width,
rectangular geometry, diagnostic visibility, and edge-source count. The 150-point
complex Baffle response and cache generation are not serialized. Projects through
format version 5 load with Baffle processing disabled and default settings.

### Network preview

The network preview is a schematic visualization of the current crossover topology. It is intended as a consistency and orientation aid while editing numeric network values.

The section R/C/L groups in the preview can be clicked for targeted network-section editing. The driver/enclosure sketch on the right-hand side can be clicked to open the driver parameter dialog on the matching driver tab. The small lamp next to each driver title shows whether at least one curve/total flag is enabled for that driver.

The default view is **All Drivers**. It shows all four driver slots regardless of the current plot visibility flags. The small lamp remains a plot-status indicator and can be clicked to toggle plot visibility for the corresponding driver.

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
-DKFILTER_BUILD_MEASUREMENT_CURVE_SMOKETEST=ON
-DKFILTER_BUILD_MEASUREMENT_IMPORT_SMOKETEST=ON
-DKFILTER_BUILD_MEASUREMENT_EXPORT_SMOKETEST=ON
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

The `Standard Filter Preset` area can insert simple Butterworth low-pass and
high-pass start values. The `Impedance correction` filter type inserts a
standard Zobel RC correction into Section 8 shunt values of the selected driver:

```text
R = Rdc
C = Lsp / Rdc^2
L = 0
```

If Section 8 already contains shunt values, KFilter asks before replacing them.

### 5. Use the network preview

The network preview shows the current network topology graphically. Click a section R/C/L group to edit that section, or click the driver/enclosure sketch on the right to open the matching driver parameter tab. The small lamp next to each driver title is lit when at least one curve/total flag is enabled for that driver; clicking it toggles plot visibility for that driver.

The default mode is:

```text
All Drivers
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

Projects are saved as versioned, human-readable JSON while retaining the `.kfp` extension. Legacy text-based `.kfp` files and JSON format versions 1 through 3 can still be opened; saving such a project rewrites it in the current version 4 format. SPL correction curves, their per-driver hide states, and the `Merge Measurements` state are project data.

## User settings

Some UI settings are stored through `QSettings` and are intentionally not part of the `.kfp` project file.

Examples:

- Network-preview background color.
- Plot-window background, grid, threshold and curve colors.
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

Patch 166 hardens the internal driver state handling. Calculation-relevant setter methods and network cleanup now invalidate cached SPL and impedance results automatically. Parameter calculation also resets its validity and phase flags on every run, so a previous invalid resonance frequency or bass-reflex enclosure cannot contaminate a later valid driver state.

Patch 167 adds neutral correction fast paths. Disabled merge state, all-zero correction curves, and curves outside the fixed SPL simulation raster now bypass per-sample interpolation when drawing individual curves and bypass interpolation, dB-to-linear conversion, and correction multiplication in SPL sums. A zero dB correction also returns amplitude factor 1 directly without evaluating `pow()`.

Patch 168 caches SPL correction values and amplitude factors on the fixed 150-point simulation raster. Each correction curve now exposes its points read-only and advances a unique revision whenever controlled mutation changes its contents. The document cache is rebuilt only after curve replacement or mutation, merge-state changes, project loading, or document clearing; drawing, labels, and both SPL summary modes reuse the prepared values.

Project-format compatibility rules:

- Increment `formatVersion` when a future change is not backward-compatible or older builds cannot preserve the newly added project data.
- Continue recognizing legacy text-based `.kfp` files through the dedicated legacy loader.
- New saves must use the current JSON format; do not add new fields to the legacy writer.

Terminology note:

- The UI term `Energetic SPL sum` corresponds to the historical internal scalar SPL summary calculation.
- Internal names such as `ScalarSummary`, `ScalarSummaryisActive`, and `PressureScalarSummary()` may remain for compatibility and to reduce unnecessary churn.

## License

KFilter6 is licensed under the GNU General Public License version 3 or later.

SPDX-License-Identifier: GPL-3.0-or-later

See the `LICENSE` file for the full GNU GPL version 3 license text.
