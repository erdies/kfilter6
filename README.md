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

Only the resulting relative correction points are project data. The original measurement file, absolute raw levels, calibration range, offset settings, and fade settings are not persisted. Measurement correction persistence was introduced with `.kfp` JSON format version 6 and remains part of the current format version 10: each correction curve is stored together with its per-driver hide state and the project-wide merge switch. The current format also persists every driver's complete active-filter, Baffle / Diffraction, and Floor Reflection settings. Calculated complex transfer-response arrays remain transient and are rebuilt from the stored metadata after loading. PDF rendering follows the same effective per-driver correction state as the plot.

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
- Linkwitz-Riley low-pass/high-pass as LR2, LR4, LR6, or LR8;
- Butterworth band-pass, defined as high-pass at the lower cutoff multiplied by
  low-pass at the upper cutoff, with the selected order applied independently
  to both flanks;
- full-depth second-order Notch with center frequency `f0` and quality factor `Q`;
- Parametric / Peaking EQ with center frequency `f0`, quality factor `Q`, and gain in dB;
- second-order Low Shelf and High Shelf with transition frequency `f0`, quality factor `Q`, and plateau gain in dB;
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

Linkwitz-Riley LR2/LR4/LR6/LR8 low-pass and high-pass sections use

```text
H_LR,N(f) = H_BW,N/2(f)^2
```

so each branch is -6.0206 dB at its crossover frequency. For ideal matched
electrical branches, LR4 and LR8 low-/high-pass responses are in phase at the
crossover, while LR2 and LR6 differ by 180 degrees and therefore require a
relative polarity inversion for flat acoustic summation.

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

Parametric / Peaking EQ uses the normalized analog peaking-biquad prototype

```text
A = 10^(gainDb/40)
H(s) = (s^2 + (A/Q)*s + 1) / (s^2 + s/(A*Q) + 1)
s = j*f/f0
```

At `f0`, the magnitude is exactly `10^(gainDb/20)`. A gain of `0 dB` is
therefore exactly neutral at every frequency. Positive and negative gains of the
same magnitude are reciprocal complex responses, and increasing `Q` narrows the
boost/cut region.

Low Shelf and High Shelf use a symmetric normalized second-order analog shelving
prototype. With `A = 10^(gainDb/40)` and `s = j*f/f0`, Low Shelf is

```text
H_LS(s) = (s^2 + sqrt(A)/Q*s + A) /
          (s^2 + s/(sqrt(A)*Q) + 1/A)
```

and High Shelf is its frequency-inverted counterpart `H_HS(s) = H_LS(1/s)`.
For Low Shelf, the low-frequency plateau is `10^(gainDb/20)` and the
high-frequency plateau is unity; High Shelf reverses those plateaus. At `f0`
the magnitude is exactly `A`, so the response is halfway to the requested
plateau gain in dB. A gain of `0 dB` is exactly neutral. Equal positive and
negative gains are reciprocal complex responses, including phase.

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
the current version-10 format; calculated 150-point complex responses remain transient
cache data.

### Baffle / Diffraction

The **Edit -> Baffle / Diffraction Parameters...** dialog maintains one independent
Baffle processing stage per driver. Two models are available:

- **Simple Baffle Step**: width-only engineering shelf with midpoint
  `f0 = 115 / W[m]`, 0 dB LF reference, +3.0103 dB at `f0`, and approximately
  +6.02 dB at high frequency.
- **Rectangular Edge Diffraction**: geometry-aware on-axis far-field edge model
  using baffle width/height and driver centre position `(X,Y)`, with the existing
  finite-piston spatial averaging and optional left/right 45-degree chamfers.

The Patch-192 rectangular model discretizes the perimeter into 200 edge sources by
default, distributed approximately in proportion to edge length while retaining all
four corners. For each edge source the angular increment `phi_j` seen from the driver
sets the normalized weight, and the complex response is

```text
H_rect(f) = 2 - sum_j w_j * exp(-j * k * b_j)
w_j       = phi_j / (2*pi)
k          = 2*pi*f / 343 m/s
```

`b_j` is the driver-centre-to-edge-source distance. This expression remains the
**unblended raw Rectangular reference**; there is no hidden observer-distance parameter.

Patch 246 changes the productive **Free field** magnitude because the raw edge model
was found to rise too strongly in the low-midrange for a compensation-oriented design
tool. KFilter now keeps the complete raw Rectangular/finite-piston/chamfer geometry and
its complex phase, but blends only magnitude in dB toward the established width-only
Simple Baffle Step response:

```text
fBS = 115 / W[m]
r   = f / fBS
w   = r^2 / (1 + r^2)
D   = Dsimple + w * (Draw - Dsimple)
```

Thus the response stays close to the conservative Simple Baffle Step at low frequency,
is exactly halfway between Simple and raw Rectangular magnitude at `fBS`, and converges
smoothly toward the raw geometry model at higher frequency. The blend weight depends
only on baffle width; height, driver position, finite-piston averaging and chamfers still
enter through `Draw`. The raw Rectangular phase is preserved by a positive real magnitude
rescale. This is an engineering hybrid/trust law, not a claim that `n=2` is a fundamental
acoustic constant. The separate **Rigid floor contact (diffraction only)** boundary mode
remains on its previously validated normalized image-geometry path and is not hybridized.
The Simple and raw Rectangular responses are blended, never multiplied.

The resulting complex multiplier is inserted after the Active Filter response and
before the scalar Measurement correction in the centralized driver path, so both its
magnitude and preserved Rectangular phase also enter the vector SPL sum. Invalid
rectangular geometry safely bypasses only the Baffle stage.

The optional diagnostic overlay shows `20 * log10(|H_baffle(f)|)` with its own
dash-dot plot style. Diagnostic visibility never controls whether the Baffle stage
itself is active. **Hide Measurement** affects only the Measurement correction and
therefore does not bypass Baffle processing.

The dialog uses live preview for field edits. In Rectangular Edge Diffraction mode,
the driver symbol in the geometry preview can also be dragged: X/Y follow the pointer
while dragging, and the acoustic response is recalculated when the mouse button is
released. **Apply** or **OK** commits the current Baffle and Floor Reflection settings;
**Cancel** restores the last applied state.

Baffle persistence was introduced with `.kfp` format version 6 and remains part of
the current format version 10. Each driver's Baffle enable state, model, width,
rectangular geometry, diagnostic visibility, and edge-source count are stored. The
150-point complex Baffle response and cache generation are not serialized. Projects
through format version 5 load with Baffle processing disabled and default settings.

### Floor Reflection

Patch 229 exposes the productive Floor Reflection stage in the existing
**Edit -> Baffle / Diffraction Parameters...** dialog. The stage remains independent
from Baffle / Diffraction processing and from the `Rigid floor contact (diffraction only)`
boundary selector. Per driver the dialog provides:

- **Enable floor reflection for this driver**
- **Cabinet bottom above floor**
- **Listener height above floor**
- **Listening distance**
- **Surface**, with **Hard / rigid floor** and the experimental
  **Porous floor - Miki reference**

Source height is not an independent input. It is derived from
`cabinet bottom above floor + Baffle height - Driver Y from top`. Therefore Baffle
height and Driver Y remain editable whenever Floor Reflection is enabled, even if
Baffle / Diffraction itself is bypassed or uses the width-only Simple Baffle Step
model. The dialog status line reports the derived source height or explains when
invalid geometry causes only the Floor Reflection stage to be bypassed. Floor
Reflection edits use the same live-preview and Apply/OK/Cancel semantics as the
existing Baffle controls. `Hard / rigid floor` remains the exact validated reference
path. `Porous floor - Miki reference` uses a 10 mm Miki porous layer with flow
resistivity 100000 Pa*s/m^2 on a rigid backing. It is intentionally labelled as an
experimental engineering reference rather than a claim to represent a specific
carpet. The planned side-view geometry preview remains deferred.

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

#### Complete driver-slot import/export (`.kfd`)

`Import Driver...` and `Export Driver...` in the Driver Parameters dialog use the
KFilter driver-slot format (`.kfd`). Format version 2 stores the complete state
associated with that driver slot:

- driver parameters, curve/total enable flags, polarity and enclosure state
- all 48 passive network values
- the driver's SPL measurement/correction curve and its per-driver Hide state
- the current project `Merge Measurements` state as import metadata
- the complete Active Filter chain, including enable/diagnostic state
- Baffle / Diffraction settings, including enable/diagnostic state and chamfers
- Floor Reflection settings, including enable state and surface preset
- the Driver Parameters dialog's bass-reflex tube-diameter hint

The Merge flag is project-wide rather than driver-local. Import therefore uses a
conflict-avoiding rule: if no **other** driver already has Measurement data, the
project adopts the Merge state stored in the imported `.kfd`. If another driver
already has Measurement data, the current project's Merge state is retained. The
imported driver's Hide state is always restored when a measurement curve is present.

`.kfd` version 1 is intentionally not supported. The format had not yet been released
for compatibility-sensitive use, so version 2 stays simpler by requiring the complete
driver-slot state instead of maintaining partial legacy semantics.

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

The network preview shows the current network topology graphically and also acts as direct navigation to the main per-driver editors. Click a section R/C/L group to edit that section; click the AC source at the far left for **Network / Filter Parameters**; click the driver/enclosure area for **Driver Parameters**; click the radiation-wave symbol immediately to the right of the loudspeaker for **Baffle / Diffraction Parameters**; and click the Active Filter strip to the right of the driver title for **Active Filter Parameters**. The small lamp next to each driver title is lit when at least one curve/total flag is enabled for that driver; clicking it toggles plot visibility for that driver. Hovering these interactive areas shows their action in the status bar.

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

Projects are saved as versioned, human-readable JSON while retaining the `.kfp` extension. Legacy text-based `.kfp` files and JSON format versions 1 through 9 can still be opened; saving such a project rewrites it in the current version 10 format. SPL correction curves, per-driver active-filter and Baffle settings, and Floor Reflection placement/surface metadata are project data. Floor Reflection remains disabled by default for backward-compatible project behavior; version 10 adds the experimental porous-surface preset introduced in Patch 229.

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

Patch 225 introduces the product-level Floor Reflection settings model and project persistence without changing the acoustic signal path. Each driver stores an enable flag, cabinet-bottom height above the floor, listener height, horizontal listening distance, and the currently sole `Hard / rigid` surface preset. The `.kfp` JSON format is advanced to version 9; versions 1 through 8 and legacy text projects load with Floor Reflection disabled and the documented defaults.

Patch 226 activates the validated ideal-rigid Floor Reflection response in the centralized complex driver path. Source height is derived from `cabinetBottomAboveFloorMm + baffleHeight - driverYFromTop`; the response is independent of whether Baffle/Diffraction processing itself is enabled. The processing order is Driver -> Active Filter -> Baffle/Diffraction -> Floor Reflection -> Measurement. Disabled, invalid or unsupported Floor Reflection remains a neutral bypass.

Patch 227 adds the product GUI for those settings to the existing per-driver Baffle / Diffraction dialog. Baffle height and Driver Y stay editable while Floor Reflection is enabled even if the Baffle processing stage itself is bypassed. The first GUI release exposes only the validated `Hard / rigid floor` surface; the planned side-view preview and porous/material models remain deferred.

Patch 228 adds a **developer diagnostic** for frequency- and angle-dependent porous floor surfaces based on Miki's empirical model for a rigid-backed porous layer. This diagnostic is intentionally not yet a product preset: the normal application and `.kfp` format still expose only `Hard / rigid floor`. The command-line tool can compare the rigid response with a documented 10 mm / 100000 Pa*s/m^2 Miki reference case or with explicitly supplied thickness/flow-resistivity values before any user-facing carpet/underlay presets are chosen.

Patch 229 promotes that exact 10 mm / 100000 Pa*s/m^2 Miki reference case into the productive Floor Reflection surface selector. The low-level Patch-228 material solver is reused without duplication, `Hard / rigid floor` remains unchanged, and the new preset is explicitly marked experimental. The `.kfp` JSON format advances to version 10 so older builds reject projects that may contain the new surface value instead of silently losing it. Version-9 Floor Reflection projects continue to load as `Hard / rigid`.

Project-format compatibility rules:

- Increment `formatVersion` when a future change is not backward-compatible or older builds cannot preserve the newly added project data.
- Continue recognizing legacy text-based `.kfp` files through the dedicated legacy loader.
- New saves must use the current JSON format; do not add new fields to the legacy writer.

Terminology note:

- The UI term `Energetic SPL sum` corresponds to the historical internal scalar SPL summary calculation.
- The per-driver visibility/summary selections are grouped in `DriverPlotState`; the historical calculation name `PressureScalarSummary()` remains for compatibility.

## License

KFilter6 is licensed under the GNU General Public License version 3 or later.

SPDX-License-Identifier: GPL-3.0-or-later

See the `LICENSE` file for the full GNU GPL version 3 license text.
