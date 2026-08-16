<!--
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2002-2026 Martin Erdtmann
-->

# KFilter Qt6/KF6 porting notes

## Current baseline

This tree is the active Qt6/KF6 KFilter codebase evolved from the original
KDE3.1/KDevelop-era sources. The patch log below is chronological: statements such
as "not ported yet" describe the state at that historical patch and must not be
read as limitations of the current tree.

## Patch status

### Patch 001

- Added a minimal CMake-based Qt6 build.
- Built the UI-independent `driver` class as `kfilter_core`.
- Added `kfilter_driver_smoketest`.
- Excluded the optional legacy `wizard` component by default.

### Patch 002

- Added `KFilterProjectIo`, a Qt6-only serializer/deserializer for the legacy
  `.kfp` project file format.
- Replaced the old KDE3/Qt3 file-I/O pattern with `QFile`, `QTextStream` and
  `QLocale::c()` formatting inside the new helper.
- Added `kfilter_projectio_smoketest` for round-trip coverage of the legacy
  project data.

### Patch 003

- Ported `KFilterDoc` far enough to compile as part of the Qt6 core library.
- Replaced `KURL` with `QUrl` in the document interface.
- Replaced the old inline `openDocument()` / `saveDocument()` implementation
  with calls to `KFilterProjectIo`.
- Removed the immediate dependency on KDE3 UI classes from `KFilterDoc`.
- Temporarily stubbed the legacy dialogs and the interactive `saveModified()`
  prompt until the Qt6 widget shell exists.
- Added `kfilter_doc_smoketest`, which verifies `KFilterDoc` save/load and the
  `forceviewrefresh` signal emission on successful load.

## Historical limitations after Patch 003

- The real application executable was not built yet.
- `KFilterDoc::saveModified()` returned `false` for modified documents because
  the old interactive KDE3 save prompt had not been ported yet.
- `initParamDialog()`, `initNetworkDialog()`, `initVolumeDialog()` and
  `initToolsWizard()` were no-ops.
- The old `KFilterView`, `KFilterApp`, dialogs and drawing code were not yet
  part of the Qt6 build target.
- Remote project URLs were intentionally unsupported at that stage; project I/O
  was local-file-only.

## Historical next step after Patch 003

Port the application shell around `KFilterApp`/`main.cpp` with a minimal Qt6
`QApplication` + `QMainWindow`/`KXmlGuiWindow` setup. Keep the shell minimal:
File New/Open/Save/Save As can call the now-tested `KFilterDoc` methods before
any serious dialog or drawing refactoring starts.

## Patch 004: Minimal Qt6 Widgets application shell

Patch 004 adds a deliberately small Qt6 Widgets shell around the already ported
core/document layer:

- `mainqt6.cpp` is the new temporary Qt6 entry point.
- `KFilterQt6App` is a temporary `QMainWindow`-based bring-up shell.
- File/New/Open/Save/Save As are wired to `KFilterDoc`.
- The central widget is a read-only textual overview of the four drivers.
- The old KDE3 `KFilterApp`, `KFilterView`, and the legacy dialogs are still not
  part of the build.

This keeps the port moving in small, verifiable steps. The temporary shell can
be deleted or merged back into the real application class after the legacy view
and dialogs have been ported.

## Patch 005 status

The temporary Qt6 application shell now embeds a ported `KFilterView` above the
textual document overview. This restores the logarithmic plot area as a Qt6
`QWidget` without pulling in the original KDE3 `KFilterApp` class or the old
parameter/network/volume dialogs.

The `Bring-up -> Enable Default Curves` action is intentionally temporary. The
original application toggled pressure, impedance and summary curves through the
legacy dialogs; those dialogs are not ported yet, so this action provides a
simple way to verify that the plot path can draw actual curves under Qt6.

Still not ported in this patch:

- `CircuitOut` network schematic widget
- driver parameter dialog
- network dialog
- volume dialog
- KDE XMLGUI/action framework integration
- printing

## Patch 006 notes

- The temporary Qt6 shell now has a second bring-up action, `Enable Demo Curves + Summaries`.
- This action intentionally overwrites the in-memory document with four distinguishable driver configurations so that multiple pressure curves, impedance curves and the summary curves can be visually verified before the original parameter dialogs are ported.
- `KFilterView` now uses separate temporary colors for the four driver pressure/impedance curves and brighter summary colors. This is a bring-up aid, not the final color-management refactor.
- The vector pressure sum, scalar pressure sum and parallel impedance sum are still calculated through the original `KFilterDoc`/`driver` routines.

## Patch 007 notes

- Added `DriverParametersDialog`, a temporary Qt6 replacement for the legacy
  Qt3/KDE3 `driverinput` dialog.
- Added `Edit -> Driver Parameters...` to the temporary Qt6 shell.
- The dialog edits all four drivers in tabs and applies changes to the current
  document model.
- Supported parameters in this first pass: title, Rdc, Lsp, F0, Qts, Qes, Qms,
  Vas, Dm, Vb, Fb, V2, alignment proposal, gain, pressure/impedance activation,
  vector/scalar/impedance summary participation, phase inversion and full
  circuit calculation.
- Gain is edited in dB for usability, but still stored as the original linear
  `driver::gain` value.
- `Lsp` is shown in mH and converted back to the original internal H value.
- Delay editing is not implemented because the current `driver` class does not
  expose a delay field in the ported data model.

## Patch 008 notes

- No historical default driver values were changed.
- The temporary Qt6 overview now shows the most confusing legacy values with
  user-facing units:
  - `Lsp` is displayed in mH while keeping the internal H value visible.
  - `Qtc` is labelled as `Qts`, matching the legacy UI/file semantics.
  - `gain` is displayed in dB while keeping the original linear value visible.
- The temporary bring-up actions have clearer labels:
  - `Enable Historical Default Curves`
  - `Load Demo Curves + Summaries`
  - `Reset to Historical Defaults`
- Added `kfilter_defaults_smoketest`, a regression test that verifies the
  historical `driver::initContents()` values and `KFilterDoc::newDocument()`
  initialization for all four drivers.


## Patch 009: deterministic default initialization

Patch 008 added a regression test for the historical driver defaults. On real
builds this exposed an old initialization gap: `driver::initContents()` set
`Tiefpass_flag` to zero but did not initialize `TiefpassQ` and `Tiefpassfc`
before calling `Berechneparameter()`. The subsequent low-pass check therefore
depended on undefined stack/heap contents and could make the new defaults test
fail nondeterministically.

Patch 009 initializes the low-pass scratch state to zero before
`Berechneparameter()` is called and improves the defaults test diagnostics so a
future failure names the affected driver and field.

## Patch 010 notes

- Added `NetworkParametersDialog`, a temporary Qt6 replacement for the legacy
  Qt3/KDE3 `NetworkDialog`.
- Added `Edit -> Network / Filter Parameters...` to the temporary Qt6 shell.
- The dialog exposes the legacy 48 network units for all four drivers as
  8 network sections with 6 values each:
  - series R [Ohm]
  - series C [uF]
  - series L [mH]
  - shunt R [Ohm]
  - shunt C [uF]
  - shunt L [mH]
- Capacitor and inductor values are converted to the original internal units
  when applied: uF -> F and mH -> H.
- The old pixmap-based `CircuitOut` schematic preview is still not ported in
  this patch; the table editor is the first functional network-editing step.
- The overview now shows how many of the 48 network units are non-zero per
  driver. To make the network influence the calculation, enable full circuit
  calculation for the driver in `Driver Parameters...`.

## Patch 011 notes

- Replaced the legacy KDE3/Qt3 pixmap-and-`QLabel` based `CircuitOut` implementation
  with a Qt6 `QPainter` based read-only schematic preview widget.
- Added the circuit preview to the temporary Qt6 application shell below the plot.
- Added a driver selector for the preview so the network of driver 1-4 can be
  inspected without opening the table editor.
- The preview reads the same 48 network units edited by `NetworkParametersDialog`:
  series R/C/L and shunt R/C/L for each of the 8 network sections.
- Capacitors and inductors are displayed in the same UI units as the table
  editor: uF and mH. Internal storage remains F and H.
- The preview is intentionally read-only in this patch. Direct graphical editing
  is deferred until after the main port is stable.

## Patch 012 notes

- Reworked the Qt6 `CircuitOut` preview to follow the legacy network topology more closely.
- Each of the 8 network sections is now drawn as two distinct logical parts:
  - The main signal path uses the historical series branch semantics. If the
    section capacitor is non-zero, the element is displayed as a parallel R/C/L
    suck circuit inserted into the signal path. If the section capacitor is zero,
    the element is displayed as a simple series R/L path, matching the legacy
    special case where R represents the coil's internal resistance.
  - The shunt/parallel branch is displayed as a serial R/C/L trap circuit from
    the signal path to the return line.
- The preview now uses a continuous return line instead of ground symbols, closer
  to the original KDE3 dialog screenshot.
- Added an integrated 6 x 8 value table below the schematic so the visual
  topology and the edited network values can be checked together.
- No calculation or file-format semantics were changed in this patch.


## Patch 013-019 notes

- Iteratively corrected the Qt6 `CircuitOut` schematic preview against the
  legacy network topology and the supplied KDE3 screenshot.
- The series element is now displayed according to the historical semantics:
  `series C != 0` shows a parallel R/C/L suck circuit in the signal path, while
  `series C == 0` shows the special-case R/L series element.
- The shunt branch is displayed to the right of the series element as a serial
  R/C/L trap branch to the return line.
- Improved the contrast of the schematic preview and removed temporary explanatory
  text from the drawing area. These were display-only changes; no calculation or
  file-format logic was changed.

## Patch 020 notes

- Removed the temporary `Bring-up` menu from the normal Qt6 application window.
  The verification helper code is still present in the source for now, but it is
  no longer part of the user-facing menu structure.
- Updated the visible Qt6 shell wording from "bring-up shell" to "KFilter Qt6".
- Added a simple `Help -> About KFilter` menu item.
- Left the core calculation, project I/O, driver parameter dialog, network
  parameter dialog, plot view and schematic preview unchanged.

## Patch 021 notes

- Improved the normal Qt6 application document handling:
  - Window title now uses Qt's modified marker (`[*]`) and tracks the current
    document name/path.
  - `New`, `Open` and window close now consistently call the Qt6 `maybeSave()`
    path before discarding modified data.
  - The unsaved-changes prompt now names the affected document and defaults to
    saving.
  - `Save` remains available and falls back to `Save As` for untitled documents.
  - `Open`/`Save As` remember the last used local directory for the current run.
- Removed the remaining unused bring-up helper slots and action members from the
  Qt6 application shell source.
- No calculation, project file format, plot drawing or schematic drawing logic
  was changed in this patch.


## Patch 022

- Removed the temporary bottom debug/driver overview text widget from the Qt6 shell.
- Kept plot, schematic preview, state label, dialogs and file handling unchanged.

## Patch 023

- Improved the Qt6 shell layout after removing the temporary debug overview:
  - Plot and schematic preview are now grouped and controlled by a non-collapsible
    vertical splitter.
  - The schematic preview sits in a scroll area so the complete 8-section network
    drawing remains reachable if the window is made smaller.
  - The preview driver selector now has an explicit label.
  - The start window size and splitter sizes were adjusted for the plot plus
    network preview layout.
  - Added `View -> Reset Window Layout` to restore the default splitter ratio.
- No calculation, project file format, plot drawing, schematic drawing semantics
  or dialog data mapping were changed.


## Patch 024

Persisted basic user interface state with `QSettings`:

- main window geometry
- vertical splitter state
- last directory used by Open/Save As during the next session
- selected circuit preview driver

No calculation, project file format, plot, schematic topology, or dialog semantics were changed.

## Patch 025

Simplified the Qt6 `CircuitOut` preview so the component values are shown directly
next to the schematic symbols instead of being duplicated again in a separate
6×8 value table below the drawing.

- Removed the bottom value table from the read-only network preview.
- Series and shunt component values are now rendered directly below the
  corresponding symbols in the schematic.
- Reduced the preferred height of the preview widget because the separate table
  is no longer needed.
- No calculation, project file format or dialog semantics were changed.


## Patch 026

Small visual cleanup in the Qt6 `CircuitOut` preview:

- removed the temporary textual `Saug` annotation above the series parallel branch
- softened the panel background from pure white to a light warm gray for less glare
- no change to calculation, data model, project I/O or topology rendering semantics


## Patch 027

Minor visual adjustment in the Qt6 `CircuitOut` preview:

- darkened the panel background slightly again for a less bright overall appearance
- kept line/text contrast unchanged
- no change to calculation, data model, project I/O or topology rendering semantics


## Patch 028

Improved the Qt6 network preview by replacing the simple generic driver marker
with an illustrative loudspeaker equivalent-circuit sketch at the right-hand
side of the schematic. The visual now follows the selected enclosure proposal:

- 0: free air
- 1: closed box
- 2: vented box
- 3: vented box with V2 proposal (two-chamber sketch)

The new preview is intentionally illustrative only: it shows the driver model
and enclosure topology, but not numeric component values for the driver model.
The main crossover/network preview on the left remains unchanged.


## Patch 029

Adjusted the illustrative loudspeaker equivalent circuit on the right-hand side
of the Qt6 network preview:

- the parallel R/C/L branch is now drawn as three vertical parallel branches
  between the signal line and the lower return/mass line
- the lower ends of those branches are explicitly connected to the return line
- the series R/L branch remains in the signal path in front of the parallel
  motional branch representation

This is a visual/topology correction only; no calculation logic was changed.


## Patch 030

Fine-tuned the illustrative loudspeaker equivalent circuit on the right-hand
side of the Qt6 network preview:

- widened the driver preview area slightly
- spaced the vertical parallel R/C/L branches farther apart horizontally so the
  three branches are visually distinct instead of appearing stacked
- increased the preferred preview width slightly to accommodate the adjusted
  driver sketch

No calculation logic was changed.


## Patch 031

Further refined the illustrative driver/enclosure sketch on the right-hand side
of the Qt6 network preview:

- adjusted the enclosure rectangles so the lower enclosure edge intersects with
  the return/mass line
- added a simple loudspeaker icon at the right edge of the enclosure sketches
  (closed box, vented box, and V2 dual-chamber variant)
- kept the driver equivalent-circuit values omitted, because the illustration
  is intentionally symbolic only

No calculation logic was changed.


## Patch 032

Refined the illustrative bass-reflex port rendering in the Qt6 network preview:

- moved the reflex port to the left side of the vented enclosure sketches
- enlarged the port drawing where space permits
- changed the port symbol from a small generic rounded rectangle to a more
  tube-like sketch with an oval mouth to suggest perspective

Applied to both the single vented-box sketch and the dual-chamber V2 proposal
sketch. No calculation logic was changed.


## Patch 033

Further refined the illustrative driver/enclosure sketch on the right-hand side
of the Qt6 network preview:

- made the bass-reflex port significantly larger and more prominent
- kept the enlarged tube with an oval mouth on the left side of the vented-box
  sketches
- added an explicit positive-line connection from the enclosure input to the
  series R/L branch of the driver equivalent circuit inside the enclosure
  sketches

No calculation logic was changed.


## Patch 034

Further refined the illustrative enclosure sketches in the Qt6 network preview:

- in the `vented box with V2 proposal` variant, moved the bass-reflex port to
  the outer wall of the left chamber (opposite the loudspeaker symbol)
- lowered the chamber bottom edges slightly so they now run visibly below the
  return/mass line in all enclosure variants
- kept the positive signal connection into the driver equivalent circuit
  unchanged

No calculation logic was changed.


## Patch 035

Small visual refinement in the Qt6 network preview:

- the loudspeaker symbol is now shown in all driver sketch variants, including
  the `free air` case where no enclosure box is drawn
- reduced the free-air equivalent-circuit width slightly to leave room for the
  speaker symbol on the right-hand side

No calculation logic was changed.


## Patch 036

Enhanced the illustrative enclosure sketches in the Qt6 network preview by
showing the relevant enclosure/tuning values inside the box drawing:

- free air: no parameter text
- closed box: `Vb=...L`
- vented box: `Vb=...L` and `Fb=...Hz`
- vented box with V2 proposal: `Vb=...L`, `Fb=...Hz`, and `V2=...L`

This is a visual-only change. No calculation logic or project I/O was changed.

## Patch 037

Fixed a syntax error introduced in Patch 036 in the Qt6 `CircuitOut` box-parameter
text rendering code:

- repaired the `QStringLiteral("\\n")` separator used for the multi-line
  Vb/Fb/V2 text block
- no logic, layout or file-format changes

## Patch 059

Cleaned up the active Qt6 source tree by removing KDE3/Qt3-era files that are
no longer part of the build and have been superseded by the current Qt6 classes:

- `main.cpp` and `kfilter.cpp/.h` were the old KDE/KMainWindow application shell;
  the active application entry path is `mainqt6.cpp` + `kfilterqt6app.cpp/.h`.
- `driverinput.cpp/.h` was superseded by `DriverParametersDialog`.
- `networkdialog.cpp/.h` was superseded by `NetworkParametersDialog` and the
  Qt6 `CircuitOut` preview.
- `volumedialog.cpp/.h` was superseded by the enclosure section in
  `DriverParametersDialog`, including the tube length helper.
- `colordialog.cpp/.h` was the old KDE `KColorDialog` based plot color dialog;
  future color configuration should be reimplemented with plain Qt6 widgets when
  needed.

The legacy files are still preserved in `legacy_original_uploaded/` inside full
handover packages as historical reference, but they should not appear in the
active KFilter6 repository tree.


## Patch 071

Added a direct editing entry point from the Qt6 network preview to the driver
parameter dialog:

- the driver/enclosure sketch on the right-hand side of each preview row now has
  a hit-test area
- left-clicking that sketch opens `DriverParametersDialog`
- in the `All Drivers` view, the dialog opens on the tab belonging to the
  clicked driver
- the network preview selection itself is left unchanged
- hover feedback uses the pointing-hand cursor and a status-bar hint

No calculation logic or project file I/O was changed.

## Patch 072

Added a standard impedance-correction preset to the Qt6 network/filter dialog:

- the `Filter type` combo now contains `Impedance correction`
- selecting it disables the low-pass/high-pass-only controls because order,
  frequency and characteristic are not used for the correction
- inserting the preset writes a standard Zobel RC correction into Section 8 of
  the selected driver's shunt branch:
  - Shunt R = driver Rdc
  - Shunt C = driver Lsp / Rdc^2, displayed in uF
  - Shunt L = 0
- if Section 8 already has shunt values, the dialog asks before replacing them
- the calculation uses the driver's actual `Rdc` and `Lsp` values, not the
  manually editable preset impedance field

No project file I/O or core network calculation logic was changed.

## Patch 074

Added a small visual activity lamp next to each driver title in the Qt6 network
preview:

- the lamp is lit when at least one curve/total flag is enabled for that driver
- the lamp is off when all curve/total flags are disabled
- this state is tracked separately from the existing preview-active logic, which
  may also keep a driver visible because it has network topology values
- the lamp is drawn for both single-driver preview mode and the `All Active
  Drivers` preview rows

This is a visual-only change. No calculation logic or project file I/O was
changed.


## Patch 155: Versioned JSON project files

- `KFilterProjectIo::saveToFile()` now writes `.kfp` projects as indented, versioned JSON.
- The JSON root contains an explicit format name and `formatVersion = 1`.
- Each of the four driver entries separates persistent driver parameters from the 48 network values.
- `fullCircuit` is now persisted as part of the JSON driver state.
- The existing text-based `.kfp` parser remains available as a read-only legacy loader.
- Opening a legacy project and saving it migrates the file to JSON automatically.
- JSON detection is content-based rather than extension-based.
- Saves use `QSaveFile` so replacement of an existing project is atomic.
- Failed loads are transactional: destination drivers are updated only after the complete file has validated.
- Measurement/correction curves remain transient in this patch and are intentionally not part of format version 1 yet.
- A later persistent measurement schema must use a newer format version so Patch 155 builds reject it instead of silently dropping unknown measurement data on save.

## Patch 156: Persistent SPL correction curves

- SPL correction curves are now project data rather than view-local transient state.
- `KFilterDoc` owns one `KFilterMeasurementCurve` per driver plus the global
  `Merge Measurement` state.
- The JSON project format is advanced to `formatVersion = 2` so Patch 155 builds
  reject files containing persistent measurement data instead of opening and
  later discarding unknown fields.
- JSON format version 1 from Patch 155 remains readable and loads with empty
  correction curves and merge disabled.
- Legacy text-based `.kfp` files remain readable with empty correction curves;
  saving them writes current JSON format version 2.
- Each driver can contain an optional `measurements.splCorrection` object with
  an explicit curve type and a strictly frequency-ordered list of
  `frequencyHz`/`valueDb` points.
- `project.measurementSettings.mergeCorrectionCurves` persists the global merge
  switch. It is restored only when at least one curve has two or more points.
- Measurement loading is transactional together with driver and network data.
  Invalid point values or non-increasing frequencies leave the current project
  unchanged.
- Creating, replacing, clearing or changing the merge state now marks the
  document as modified.
- The existing PDF behaviour is unchanged: correction curves and merged SPL
  rendering are still suppressed during PDF export.

## Patch 157: Measurement-aware SPL sums

- `Merge Measurement` now affects the vector and energetic SPL sums in addition
  to the individual driver SPL curves.
- The interpolated dB correction is converted to a linear pressure-amplitude
  factor with `10^(correctionDb / 20)`.
- For the vector sum, the same positive real factor scales both the real and
  imaginary simulated pressure components before complex addition. The existing
  simulated phase is therefore preserved; no measurement phase is synthesized.
- For the energetic sum, the corrected real and imaginary components are squared
  and accumulated, which is equivalent to applying `10^(correctionDb / 10)` to
  each driver's pressure-energy contribution.
- Outside the frequency span of a correction curve, the neutral factor `1.0` is
  used. Curves with fewer than two points remain non-mergeable.
- Impedance calculations and stored driver-core results remain unchanged.
- PDF export keeps its established suppression policy: the view requests sum
  calculations without measurement merge while PDF rendering is active.
- The document smoke test now checks vector-sum correction, energetic-sum
  correction, neutral behaviour outside the curve range, PDF-style suppression,
  and preservation of simulated phase in a cancellation scenario.


## Patch 158: Calibrated measurement import

- Added a per-driver `Import Measurement for Driver` submenu under
  **Measurements**.
- Text-oriented FRD/CSV/DAT-style files are parsed from their first two numeric
  columns as frequency in Hz and absolute SPL level in dB. Additional columns,
  including phase, are ignored.
- The parser accepts whitespace, tabs, semicolons, decimal commas when the
  column separation is otherwise unambiguous, and comma-separated rows using
  decimal points. Invalid rows are skipped and duplicate frequencies are
  combined with the median level.
- The import dialog separates the 0 dB calibration range from the retained
  correction window. The automatic offset is the negative median SPL level in
  the selected calibration range; a manual offset can be added.
- Only points inside the correction window are converted into the existing
  relative `KFilterMeasurementCurve`. Exact boundary points are synthesized by
  logarithmic interpolation when necessary.
- Optional lower and upper fades operate in logarithmic frequency space using a
  smoothstep weight. Enabled fades begin or end at a neutral 0 dB boundary.
- A two-panel preview shows the absolute raw measurement separately from the
  calibrated measurement and final relative correction curve.
- Importing replaces an existing curve only after explicit confirmation. The
  imported result immediately uses the existing merge, sum, JSON persistence,
  and manual editing paths.
- Patch 158 intentionally stores only the resulting correction points. Source
  files, raw measurements, calibration metadata, and fade settings are not yet
  persisted, and correction-curve export remains deferred.
- Added `kfilter_measurement_import_smoketest` for parser cleanup, duplicate
  handling, median calibration, manual offset, correction-window extraction,
  and lower/upper fade behaviour.


## Patch 163: FRD export of stored correction curves

- Added an **Export Measurement for Driver** submenu under **Measurements**,
  parallel to the existing per-driver import action.
- Export actions are available only for drivers that currently contain a stored
  SPL correction curve and are disabled while measurement drawing or a network
  section editor is active.
- The export writes only the stored correction control points. It does not add
  interpolation points, Driver Gain, simulated SPL data, or merged response data.
- FRD output uses three columns: frequency in Hz, relative correction in dB, and
  an intentionally neutral phase value of `0.000` degrees.
- The UTF-8 header identifies the file as `magnitude-correction`, states that the
  levels are relative rather than absolute SPL, and marks the phase meaning as
  `none`. Driver description, patch level, and correction range are included as
  metadata.
- Frequencies are sorted before writing. Invalid/non-finite points and duplicate
  frequencies are rejected, and output is committed atomically with `QSaveFile`.
- Number formatting is forced to the C locale so FRD data always uses a decimal
  point regardless of the desktop locale.
- Added `kfilter_measurement_export_smoketest`, covering metadata, neutral phase,
  sorted output, locale independence, empty curves, and duplicate frequencies.
- Reimport recognition of KFilter-specific correction metadata remains a later
  step; the existing measurement importer still treats input as an absolute
  measurement requiring calibration.

## Patch 164: Centralized SPL correction calculation

- Added `KFilterDoc::splCorrectionDb()` as the single document-level entry point
  for interpolating the effective scalar measurement correction on the fixed SPL
  simulation raster.
- Exposed `KFilterDoc::splCorrectionAmplitudeFactor()` as the corresponding
  centralized dB-to-linear conversion used by vector and energetic sums.
- Individual merged driver curves and their label anchors no longer interpolate
  `KFilterMeasurementCurve` directly in `KFilterView`; they use the same document
  API as the sum calculations.
- Invalid driver/sample indices, disabled merge state, non-mergeable curves,
  out-of-range frequencies, and non-finite interpolation results all resolve to
  the neutral correction (`0 dB` / amplitude factor `1.0`).
- This patch is intentionally a behaviour-preserving refactoring. It adds no
  correction cache, no complex phase representation, and no JSON or FRD format
  changes.
- The document smoke test now verifies the centralized dB and amplitude APIs,
  including merge-disabled, outside-range, invalid-index, and logarithmic
  interpolation cases.

## Patch 166: Driver state consistency

- All calculation-relevant `driver` setter methods now invalidate both the SPL
  and impedance caches themselves. Callers no longer need an additional
  `setmodified()` after using `setRdc()`, `setLsp()`, `setF0()`, `setQtc()`,
  `setQes()`, `setQms()`, `setVas()`, `setDm()`, `setQl()`, or
  `setFullCircuit()`.
- `cleanupNetwork()` now invalidates the caches after clearing all network
  elements.
- `Berechneparameter()` initializes `Parameter_flag`, `AkustikESB_flag`, and
  `Phase_flag` from a defined valid baseline on every run. This prevents a
  previous invalid-F0 or bass-reflex state from leaking into a later valid
  closed-box or free-air calculation.
- The driver smoke test now covers every calculation setter, network cleanup,
  phase-state recovery after an enclosure transition, and recovery after an
  invalid `F0 == 0` state.
- Public calculation fields such as `Vb`, `Fb`, `V2`, `GTypProposal`, `gain`,
  and `InvertPhase` remain directly writable for compatibility. Existing
  callers must still invoke `setmodified()` after changing those fields.

## Patch 167: Neutral SPL-correction fast paths

- Added `KFilterMeasurementCurve::isNeutral()` for exact all-zero correction
  detection and `overlapsFrequencyRange()` for inexpensive simulation-raster
  overlap checks. These methods do not cache state because measurement points
  remain publicly accessible in the current model.
- Added `KFilterDoc::splCorrectionActiveForDriver()` as the shared decision for
  whether a driver needs correction processing on the fixed 150-point SPL
  raster. Disabled merge state, invalid drivers, non-mergeable curves, exact
  all-zero curves, and curves wholly outside the raster use the neutral path.
- Individual SPL drawing and label-anchor calculation determine correction
  activity once per driver. Without an effective correction they reuse the
  unmodified simulation values and perform no per-sample interpolation.
- Vector and energetic SPL sums now bypass correction lookup, dB-to-linear
  conversion, and multiplication for neutral drivers. Non-neutral corrections
  retain the existing multiplication order.
- `splCorrectionAmplitudeFactor()` returns `1.0` immediately when the resolved
  correction is exactly `0 dB`, avoiding an unnecessary `pow(10, 0)`.
- Extended the measurement-curve and document smoke tests for neutral-curve
  detection, frequency-range overlap, merge-disabled handling, out-of-raster
  curves, and partly overlapping active curves.
- This patch intentionally adds no persistent interpolation cache and does not
  encapsulate or remove legacy `driver` APIs or legacy calculation states such
  as `F0 == 0`.

## Patch 168: Cached SPL-correction raster

- `KFilterMeasurementCurve` now keeps its point vector private and exposes it as
  a read-only reference through `points()`. All content changes use controlled
  methods such as `appendPoint()`, `setPointValue()`, `removeLastPoint()`, and
  `clear()`.
- Every successful content mutation, copy assignment, or move assignment gives
  the curve a new process-unique revision. Failed mutations and assignments of
  an already stored point value do not change the revision.
- `KFilterDoc` keeps one transient SPL-correction cache per driver. Each cache
  contains the interpolated dB corrections and corresponding linear amplitude
  factors for the fixed 150-point simulation raster.
- Cache entries are keyed by the curve revision and merge state. They are also
  explicitly invalidated after merge-state changes, project loading, document
  clearing, and selective/global measurement clearing.
- Neutral curves, non-mergeable curves, disabled merge state, and curves outside
  the simulation raster produce cached neutral arrays (`0 dB` and factor `1`).
- Vector and energetic SPL sums obtain the cache once per active driver and use
  the prepared factor array directly. Individual curve drawing and label lookup
  continue to use the centralized document API, which now serves cached values
  instead of repeating interpolation and dB-to-linear conversion.
- The cache is transient only. Patch 168 changes neither the JSON `.kfp` format
  nor FRD import/export semantics, and it does not alter legacy `driver` APIs or
  intentional legacy calculation states such as `F0 == 0`.

## Patch 170: Hide Measurements state

- Added an independent **Hide Measurements** switch directly below
  **Merge Measurements** in the Measurements menu.
- With Hide disabled, the existing behaviour is unchanged: unmerged correction
  curves are visible, while Merge applies their scalar magnitude correction to
  individual SPL curves and both SPL sums.
- With Hide enabled, all stored measurement/correction curves are hidden and
  their correction influence is neutralized. This also applies when Merge
  remains enabled, so the plot and PDF output show a purely uncorrected
  simulation without changing the stored Merge state.
- Disabling Hide immediately restores the prior Merge behaviour. Merge and Hide
  are therefore persisted and managed as independent project states.
- During interactive drawing, only the currently edited correction curve is
  shown temporarily even when Hide or Merge would otherwise suppress stored
  measurement curves.
- The print measurement-status box is omitted while Hide is active, and plot
  legends do not label simulated curves as merged in that state.
- The transient SPL-correction cache now includes Hide in its validity key and
  returns neutral arrays (`0 dB` and factor `1`) whenever Hide is active.
- The JSON project format is advanced to `formatVersion = 3` and persists
  `project.measurementSettings.hideMeasurements`. Versions 1 and 2 remain
  readable and load with Hide disabled; invalid version-3 hide values fail
  transactionally.
- Clearing the final measurement curve or all measurement curves resets both
  Merge and Hide because neither state can remain effective without stored
  measurements.
- Document and project-I/O smoke tests cover cache invalidation, vector and
  energetic sums, independent Merge/Hide restoration, selective clearing,
  format-version compatibility, and transactional validation.

## Patch 171: Per-driver Hide Measurement states

- Replaced the global **Hide Measurements** action with the
  **Hide Measurement for Driver** submenu. Each stored driver measurement has
  an independent checkable hide state; drivers without a measurement cannot be
  checked.
- A hidden measurement is excluded from every effective correction path for
  that driver. The individual SPL curve uses the uncorrected simulation, and
  the vector and energetic SPL sums use that same uncorrected driver
  contribution. Non-hidden drivers continue to use their merged corrections.
- The centralized correction cache now includes the hide state of its own
  driver only. Toggling one checkbox invalidates only that driver's cache, so
  mixed corrected/uncorrected sums cannot diverge from the visible individual
  curves.
- With Merge disabled, non-hidden measurement curves remain visible references;
  hidden measurement curves are omitted. During interactive drawing, the active
  curve remains temporarily visible regardless of its stored hide state.
- Plot legends and the PDF measurement-status box describe mixed states per
  driver. If every stored measurement is hidden, the PDF is intentionally
  rendered as a pure simulation without a measurement annotation.
- The JSON project format is advanced to `formatVersion = 4`. The `hidden`
  boolean is stored inside each driver's `measurements.splCorrection` object;
  the global `measurementSettings.hideMeasurements` field is no longer written.
- Version 3 projects migrate deterministically: a true global hide flag becomes
  `hidden = true` for every driver containing a measurement. Versions 1 and 2
  remain readable with all per-driver hide states disabled.
- Selective clearing resets only the cleared driver's hide state. Clearing all
  measurements resets all hide states and the merge switch.
- Smoke tests cover selective cache invalidation, mixed vector and energetic
  sums, per-driver persistence, version-3 migration, older-format compatibility,
  and transactional rejection of invalid per-driver hide values.

## Patch 181: Active-filter project persistence

- Advanced the JSON `.kfp` project format to `formatVersion = 5`.
- Each driver now stores an `activeFilter` object containing chain enable state,
  diagnostic-plot visibility, and the complete ordered section list.
- Every section persists its own enable state, filter type, and all parameters
  currently represented by `ActiveFilterSection`, including metadata for
  DSP-unsupported types and characteristics. Persistence is therefore independent
  of transfer-engine support.
- Complex 150-point transfer-response arrays and response-cache state are not
  serialized. They remain derived transient data and are rebuilt from the loaded
  metadata.
- JSON versions 1 through 4 and the legacy text format remain readable. Because
  those formats predate active-filter persistence, they load with default empty
  active-filter chains.
- Version-5 active-filter parsing is transactional: malformed or unknown current
  metadata rejects the project without modifying the destination document state.
- `KFilterDoc::openDocument()` now restores active-filter chains instead of
  resetting them after project load. `newDocument()` and `deleteContents()` still
  reset all chains.
- Project-I/O smoke coverage now round-trips all modeled active-filter section
  types and all four current crossover characteristics, including unsupported
  DSP metadata. Document smoke coverage verifies that stale in-memory chains are
  replaced by persisted state.

## Patch 182: Second-order active Notch filter

- Added DSP support for the existing `ActiveFilterType::Notch` model section.
- The implementation is the canonical analog full-depth second-order notch
  `H(s) = (s^2 + w0^2) / (s^2 + (w0/Q)s + w0^2)`, evaluated in normalized form
  on the shared 150-point KFilter frequency grid.
- Notch parameters are center frequency `f0 > 0` and quality factor `Q > 0`.
  A grid point exactly at `f0` produces the valid complex multiplier `0+0j`;
  increasing Q narrows the stop band.
- The existing Notch `gainDb` model field remains persisted for forward
  compatibility but is intentionally not transfer-relevant in Patch 182. It no
  longer invalidates the transfer cache and is disabled in the Notch editor.
- Diagnostic plotting and the centralized driver/vector/energy signal path use
  the new Notch response automatically. An exact `H=0` diagnostic point is drawn
  at the visible SPL floor instead of creating a gap in the dashed transfer curve.
  A chain containing another unsupported enabled section still bypasses the
  complete active-filter stage.
- Updated active-filter dialog/status text to reflect `.kfp` version-5
  persistence and current Notch support. Applying active-filter parameters now
  marks the document modified so persisted edits participate in normal project
  save handling.
- Extended the response smoke test for exact notch null, analytical magnitude,
  phase sign around `f0`, Q-dependent bandwidth, invalid parameters, transfer
  cache behavior, sample application, and mixed unsupported-chain bypass.
  Document/dialog smoke tests additionally cover Notch integration and editor
  control semantics.
- `.kfp` remains `formatVersion = 5`; no project-format change is required.

## Patch 183: Butterworth active Band-pass filter

- Added DSP support for the existing `ActiveFilterType::BandPass` model section
  when its characteristic is Butterworth.
- KFilter defines this as a loudspeaker/crossover-style band-pass: a Butterworth
  high-pass at `lowerFrequencyHz` multiplied by a Butterworth low-pass at
  `upperFrequencyHz`. The selected `order` applies independently to each flank.
- Valid parameters are order 1 through 8, finite positive cutoffs, and
  `lowerFrequencyHz < upperFrequencyHz`. Other characteristics remain explicitly
  unsupported, preserving the complete-chain bypass rule.
- The complex HP and LP responses are multiplied point-by-point on the shared
  150-point frequency grid, so both magnitude and phase enter the centralized
  driver/vector/energy signal path.
- The Active Filter dialog now reports Band-pass as supported and exposes both
  cutoff controls for Butterworth Band-pass. Q remains disabled because it is not
  transfer-relevant for this characteristic.
- Response smoke coverage verifies orders 1 through 8 against an independent
  HP*LP cascade, low/high-frequency attenuation, invalid cutoff ordering,
  unsupported characteristics, and cache invalidation. Dialog/document smoke
  tests cover editor semantics and centralized simulation integration.
- `.kfp` remains `formatVersion = 5`; Band-pass metadata was already persisted by
  Patch 181, so no project-format migration is required.


## Patch 190: Baffle-processing core / Simple Baffle Step

- Added per-driver `BaffleSettings` and a Qt-independent complex Baffle response
  engine on the shared 150-point frequency grid.
- Implemented Stage-1 **Simple Baffle Step** using the engineering midpoint
  `f0 = 115 / W[m]` and a first-order complex shelf from 0 dB to +6.02 dB.
- Added one transient `BaffleResponseCache` per driver. Transfer arrays are
  rebuilt only when transfer-relevant Baffle settings change.
- Extended the centralized effective driver path to
  `raw -> H_active -> H_baffle -> measurement amplitude correction`.
- Unsupported or invalid Baffle settings bypass only the Baffle stage; Active
  Filters and Measurement processing remain independent.
- Added response and document smoke coverage for magnitude, phase, cache
  behavior, vector/energetic sums, Hide Measurement independence, and isolated
  invalid-Baffle bypass.
- Patch 190 intentionally left Baffle metadata transient and kept `.kfp`
  `formatVersion = 5` pending the follow-up UI/persistence patch.

## Patch 191: Stage-1 Baffle UI, persistence, and diagnostic plot

- Added **Edit -> Baffle / Diffraction Parameters...** with one tab per driver,
  live preview, Apply/OK/Cancel semantics, Stage-1 width editing, calculated
  midpoint display, and diagnostic-plot visibility.
- Rectangular Edge Diffraction remains visible as a forward-compatible model
  label but is deliberately disabled in the Patch-191 selector because its DSP
  is not implemented yet.
- Added a dedicated Baffle diagnostic curve showing
  `20 * log10(|H_baffle(f)|)` with a dash-dot style and its own legend entry.
  Diagnostic visibility does not control processing.
- Advanced the JSON `.kfp` format to `formatVersion = 6`. Each driver stores its
  complete `BaffleSettings` metadata: enable state, model, width, reserved
  rectangular geometry, diagnostic visibility, and edge-source count.
- JSON versions 1 through 5 and legacy text projects remain readable. Because
  those formats predate Baffle persistence, they load with default disabled
  Baffle settings. Version 5 still restores its active-filter metadata normally.
- Calculated 150-point Baffle responses and cache generations remain transient
  derived data and are never serialized.
- Added Baffle dialog smoke coverage and extended project/document persistence
  tests for version-6 round trips, version-5 compatibility, forward-compatible
  Rectangular metadata, and transactional rejection of invalid Baffle data.


## Patch 192: Rectangular Edge Diffraction DSP and minimum geometry UI

- Activated **Rectangular Edge Diffraction** as the second productive Baffle model.
- Implemented the validated on-axis far-field edge-source expression
  `H = 2 - sum(w_j * exp(-j*k*b_j))`, with `w_j = phi_j/(2*pi)` and
  `c = 343 m/s`. No hidden observer-distance parameter is used.
- The rectangular perimeter is discretized with the persisted `edgeSourceCount`
  (default 200), approximately proportional to physical edge length while all four
  corners remain explicit contour points.
- Added strict DSP validation: width/height must be positive, the driver centre must
  lie strictly inside the rectangle, and at least four edge sources are required.
  Invalid geometry returns a unity Baffle response and does not affect Active Filters
  or Measurement processing.
- Extended the Baffle dialog with the minimum Stage-2 geometry controls: baffle height,
  Driver X from left, and Driver Y from top. Stage-2 geometry controls are disabled
  while Simple Baffle Step is selected.
- Existing `.kfp` format version 6 already contains all required Stage-2 fields, so no
  project-format version change is needed.
- Extended response tests with Stage-2 finite/bounds checks, exact geometry/frequency
  scaling, N=200 -> 400 convergence, position sensitivity, invalid-geometry bypass,
  and Stage-2 cache invalidation. The document smoke test also verifies productive
  Rectangular processing in the centralized complex driver path.
- Not included yet: Copy Geometry, a baffle sketch, user-editable edge-source count,
  piston directivity, edge radius/chamfer, or off-axis/directivity modelling.

## Patch 225: Floor Reflection product settings and persistence

- Added the Qt-independent `FloorReflectionSettings` product model with one
  per-driver settings entry in `KFilterDoc`.
- Patch-225 defaults are intentionally neutral and backward-compatible:
  Floor Reflection disabled, cabinet bottom at 0 mm above the floor, listener
  height 1050 mm, horizontal distance 2500 mm, and `Hard / rigid` surface.
- Source height is deliberately **not** persisted independently. The later
  productive response stage will derive it from cabinet/baffle geometry, so
  project data cannot contain contradictory source-height definitions.
- Advanced `.kfp` JSON to `formatVersion = 9`. Each driver now stores a
  `floorReflection` object containing `enabled`, `cabinetBottomAboveFloorMm`,
  `listenerHeightAboveFloorMm`, `horizontalDistanceMm`, and `surfacePreset`.
- JSON versions 1 through 8 and legacy text projects remain readable. Because
  they predate Floor Reflection persistence, all four settings entries load
  with the Patch-225 defaults while existing driver, measurement, active-filter
  and Baffle metadata retains its prior migration behavior.
- Only the validated `hardRigid` surface preset is accepted in Patch 225.
  Malformed/negative placement metadata or unsupported surface values fail
  transactionally without modifying the destination document state.
- `KFilterDoc::newDocument()` resets Floor Reflection metadata to defaults.
- Patch 225 itself did not multiply Floor Reflection into `effectivePressureSample()`;
  that acoustic integration is introduced by Patch 226 below.
- Added model-default, project round-trip/version-8 compatibility, invalid-data
  transactionality, document persistence, and document-reset smoke coverage.

## Patch 226: Productive ideal-rigid Floor Reflection integration

- Added `floorreflectionprocessing.cpp/.h` as the product boundary between the
  persisted placement model and the validated Stage-F0 image-source solver.
- Enabled `Hard / rigid` Floor Reflection is now applied as a complex
  `H_floor(f)` stage in the centralized pressure path after Baffle/Diffraction
  and before Measurement correction.
- Source height is derived, not stored: `cabinet bottom above floor + baffle
  height - Driver Y from top`. This requires a positive baffle height and a
  driver centre within the vertical baffle extent; invalid geometry bypasses
  only Floor Reflection.
- Floor Reflection deliberately does not depend on the Baffle `enabled` flag,
  Baffle model, or Free-field/Rigid-floor boundary selector. Baffle geometry is
  reused only to obtain the source height, keeping placement reflection and
  enclosure diffraction as independent transfer stages.
- Added `FloorReflectionResponseCache`; disabled settings are a neutral fast
  path and edits to retained placement values while disabled do not regenerate
  the response. Enabled transfer-relevant edits do.
- `KFilterDoc::floorReflectionResponse()` exposes the same cached response used
  by single-driver SPL, vector SPL summary, and energetic SPL summary paths.
- The product regression verifies disabled neutrality, exact agreement with the
  F0 reference geometry, complex multiplication, cabinet elevation, invalid
  geometry bypass, independence from Baffle boundary selection, and cache
  invalidation. The document regression additionally checks phase in the vector
  sum and `RigidFloorContactDiffractionOnly * H_floor` without reintroducing the
  +6.0206 dB boundary gain previously isolated in Stage F2.
- No GUI is added in Patch 226. Existing projects remain acoustically unchanged
  because Floor Reflection defaults to disabled; persisted version-9 projects
  that explicitly set `enabled=true` now activate the response as intended.


## Patch 227: Floor Reflection product GUI

- Extended the existing **Edit -> Baffle / Diffraction Parameters...** per-driver
  dialog with a dedicated Floor Reflection group. No second placement dialog is
  introduced.
- Added live-edit controls for Floor Reflection enable state, cabinet-bottom
  height above the floor, listener height, horizontal listening distance, and
  the surface preset. The first product GUI deliberately exposes only the
  validated `Hard / rigid floor` preset.
- Source height remains derived rather than stored or edited independently:
  `cabinet bottom + Baffle height - Driver Y from top`. A status line reports the
  derived source height and detects invalid/degenerate placement geometry.
- Floor Reflection remains independent from Baffle / Diffraction enable state and
  from `Rigid floor contact (diffraction only)`. Because the Floor Reflection DSP
  still needs physical source geometry, Baffle height and Driver Y remain editable
  whenever Floor Reflection is enabled, including while the Baffle stage is
  bypassed or uses Simple Baffle Step.
- The Floor controls participate in the dialog's existing live-preview and
  Apply/OK/Cancel transaction semantics. Cancel restores both Baffle and Floor
  Reflection settings to the last applied state.
- Extended `kfilter_baffle_dialog_smoketest` with default-state, independent-stage,
  source-height derivation, decimal-comma input, live-preview, Apply and Cancel
  coverage for Floor Reflection.
- No `.kfp` format change is required; Patch 225 already introduced all persisted
  Floor Reflection fields in format version 9. No Side View Preview and no porous
  or measured material model is added by Patch 227.

## Patch 228: Porous floor-surface diagnostic (Miki)

- Added the Qt-independent `floorsurfacemodel.cpp/.h` material boundary without
  changing the persisted `FloorSurfacePreset` product enum. The normal product
  GUI and signal path therefore remain limited to `Hard / rigid floor` in this
  patch.
- Implemented two low-level surface types: ideal rigid (`Gamma = +1 + j0`) and a
  locally reacting Miki porous layer backed by an acoustically rigid wall.
- The Miki implementation follows Yasushi Miki, *Acoustical properties of porous
  materials - Modifications of Delany-Bazley models -*, J. Acoust. Soc. Jpn. (E)
  11(1), 19-24 (1990), DOI `10.1250/ast.11.19`: normalized characteristic
  impedance from Eqs. (29)-(31), propagation constant from Eqs. (32)-(34), and
  rigid-backed surface impedance `Zs = Zc * coth(gamma*l)` from Eq. (35).
- The complex pressure reflection coefficient is then calculated from the local
  surface impedance and the incidence angle already produced by the Floor
  Reflection image-source geometry. This keeps placement geometry and material
  physics as separate testable components.
- Added `calculateFloorReflectionResponseWithSurfaceModel()` as a diagnostic
  combination helper. It reuses the validated F0 path geometry and
  `calculateFloorReflectionSample()`; Patch 228 does **not** route this helper
  into `floorreflectionprocessing.cpp`, project persistence or the Qt GUI yet.
- The default diagnostic reference uses `thickness = 10 mm` and
  `flow resistivity = 100000 Pa*s/m^2`, the same parameter pair used by Miki's
  Fig. 3 rigid-backed example. It is deliberately labelled as a Miki reference
  case, not as a universal carpet preset.
- Added `kfilter_floor_surface_diagnostic` with:
  - `--summary h_source h_listener distance [thickness_mm sigma]`
  - `--curve h_source h_listener distance [thickness_mm sigma]`
  - `--material frequency_hz incidence_deg [thickness_mm sigma]`
  The optional parameters make it possible to explore material strength before
  committing any user-facing preset taxonomy.
- Extremely small `f/sigma` is reported explicitly. Miki notes that the earlier
  Delany-Bazley fit carried an extrapolation warning below `f/sigma = 0.01`; the
  modified model is physically better behaved but its prediction was not fully
  verified there. Patch 228 therefore outputs `legacy_low_ratio_flag` and a
  separate `passivity_warning` instead of silently clamping complex `Gamma`.
- Added `kfilter_floor_surface_model_smoketest`. It checks fixed complex 1-kHz
  values from Miki Eqs. (29)-(35), oblique-incidence reflection, passive behavior
  in the reference `f/sigma >= 0.01` range, low-frequency warning behavior, invalid
  input rejection, and exact equality of the new rigid-surface path with the
  previously validated Stage-F0 ideal-rigid response.
- The locally reacting approximation remains an engineering model. Later product
  integration should document the model range and surface-preset provenance; see
  also Yasuda, Ueno & Sekine, *Acoust. Sci. & Tech.* 36(5), 459-462 (2015), DOI
  `10.1250/ast.36.459`.


## Patch 229: Productive Miki reference floor surface

- Extended the persisted `FloorSurfacePreset` enum with
  `MikiReference10mm100k` while keeping `HardRigid` as the default and exact
  Stage-F0 reference path.
- Promoted the Patch-228 diagnostic material case into the normal product path
  without creating a second material implementation.
  `floorreflectionprocessing.cpp` maps the new preset to the existing
  `MikiPorousRigidBacking` solver using:
  - thickness `10 mm`
  - flow resistivity `100000 Pa*s/m^2`
  - acoustically rigid backing
- The UI label is **Porous floor - Miki reference** and the status text marks it
  as **experimental**. It is deliberately not named `Carpet`; the parameters are
  a documented Miki reference case, not a universal measured floor covering.
- Advanced `.kfp` JSON `formatVersion` from 9 to 10 and added the stable surface
  string `mikiReference10mm100k`. Version-9 projects continue to load normally
  with their existing `hardRigid` surface values. Unknown surface strings still
  fail transactionally.
- Extended product-processing regression coverage so the productive Miki preset
  must be complex-sample identical to
  `calculateFloorReflectionResponseWithSurfaceModel()` from Patch 228. Switching
  the surface preset invalidates the Floor Reflection response cache.
- Extended project-I/O, document, and Baffle-dialog smoke tests for Miki preset
  round-trip, v9 compatibility, live GUI selection/status, Apply/Cancel behavior,
  and productive signal-path integration.
- The Patch-228 low-`f/sigma` diagnostic caveat remains applicable. Patch 229 does
  not clamp or reinterpret the Miki response and does not claim high material
  accuracy in that extrapolative region.
- The optional side-view preview remains deferred.

## Patch 236: Pre-handover cleanup

- Centralized `KFilterView`'s 150-point plotting raster on `kfilterfrequencygrid.h`;
  the historical angular-frequency drawing coordinate keeps its exact recurrence but
  now uses the same authoritative sample count, minimum frequency and step as the
  Active Filter, Baffle and Floor Reflection response grids.
- Removed unused KDE3-era view-list/dialog-refresh/save-prompt stubs from
  `KFilterDoc`. Project save prompting remains correctly owned by `KFilterQt6App`.
- Renamed the main-window hover cleanup slot to the generic
  `clearCircuitPreviewHover()` and removed duplicate active-section-editor guards
  from the private dialog-opening helpers; public entry points retain the guard.
- Updated stale current-state documentation: `.kfp` format version 10 is current,
  historical porting limitations are explicitly labelled as historical, and the
  Active Filter response header describes the implemented section families.
- Extended the Baffle/Diffraction preview hit regression to cover free-air, sealed,
  vented and bandpass loudspeaker-symbol placements while retaining strict
  separation from the Driver Parameters hit area.
- No `driver.*` refactoring or simulation-model change is part of this cleanup.


## Patch 246: Productive low-midrange magnitude hybrid for Free-field Rectangular Baffle

- Promoted the Patch-245 width-anchored `n=2` candidate into the productive
  **Free-field Rectangular Edge Diffraction** response. The selected law is
  `r = f/fBS`, `w = r^2/(1+r^2)`, with `fBS = 115/W[m]`.
- The validated raw Rectangular engine remains unchanged and retains all geometry:
  width/height, driver position, N=200 contour discretization, M=73 finite-piston
  spatial averaging, and optional left/right 45-degree chamfers.
- Only magnitude is hybridized in dB:
  `D = Dsimple + w*(Draw-Dsimple)`. The raw Rectangular complex phase is preserved
  by multiplying each raw sample by a positive real scale factor.
- The blend anchor is width-only. Cabinet height therefore continues to influence
  the raw diffraction response but no longer also changes how early KFilter trusts
  that raw LF magnitude.
- Added `calculateBaffleUnblendedRectangularResponseForDiagnostic()` so historical
  Sharp/finite-piston/chamfer goldens and developer diagnostics can continue to test
  the raw model independently from the productive hybrid.
- The Patch-242 `sqrt(W*H)` candidate and Patch-244/245 `n=1` / `n=1.5` variants
  remain diagnostic references. The Patch-246 LF smoke test verifies that the
  productive Sharp response is the `n=2` candidate across the full geometry matrix.
- Productive Chamfer45 uses the same LF hybrid after its raw Stage-3A response; the
  analytical chamfer goldens remain attached to the unblended diagnostic reference.
- `RigidFloorContactDiffractionOnly` is intentionally unchanged. Its normalized
  unfolded image-geometry response is a separate boundary model and is not blended
  toward the Free-field Simple Baffle Step. Existing floor diagnostics remain raw.
- No UI control and no project-format field is added. The exponent is deliberately
  fixed as a model constant rather than exposed as a user-tunable compensation knob.

## Patch 250: Remove persistent Baffle-dialog explanatory banner

- Removed the multi-line explanatory label from the top of the
  **Baffle / Diffraction / Floor Reflection** dialog. The dialog now starts directly
  with the per-driver tabs, leaving persistent UI space to controls and live status.
- No replacement info button, tooltip or collapsible banner was introduced. Stable
  model descriptions and operating semantics belong in the documentation rather than
  occupying permanent dialog space.
- Extended `README.md` so the removed operational details remain documented: field
  edits use live preview, the geometry-preview driver can be dragged in Rectangular
  mode with response recalculation on release, and Apply/OK/Cancel retain their
  existing commit/restore semantics.
- Extended `kfilter_baffle_dialog_smoketest` to ensure the obsolete
  `baffleStageNotice` widget is no longer created.
- No Baffle, Floor Reflection, persistence, cache, project-format or DSP behavior is
  changed.


## Patch 251: Complete `.kfd` driver-slot state

- Advanced the private KFilter driver-slot format from version 1 to version 2. Version
  1 is intentionally rejected; no backward-compatibility path is retained because
  the format has not yet had compatibility-sensitive external use.
- `.kfd` now stores the complete state associated with one driver slot: driver and
  passive-network data, SPL Measurement points plus per-driver Hide state, the
  project Merge-Measurements state as import metadata, the complete Active Filter
  chain, Baffle / Diffraction state, Floor Reflection state, and the existing tube-
  diameter dialog hint.
- Reused the project-I/O serializers and validators for Measurements, Active Filters,
  Baffle and Floor Reflection. `.kfp` and `.kfd` therefore share these JSON semantics
  instead of maintaining a second copy of the new persistence logic.
- Driver import now operates on `KFilterDoc`, allowing the Driver Parameters dialog to
  replace all associated per-driver state while retaining its existing live-preview
  and Apply/OK/Cancel semantics. Cancel restores the complete pre-dialog state, not
  just the legacy `driver` object.
- Defined the project-wide Merge import policy explicitly: when no other driver has
  Measurement data, the imported `.kfd` Merge state is adopted. If any other driver
  already has Measurement data, the current project's Merge state is retained. The
  effective document state still disables Merge when no mergeable curve exists.
- The main window resynchronizes Measurement actions after the Driver Parameters
  dialog closes so imported/restored Hide and Merge states are reflected immediately.
- Driver-slot writes now use `QSaveFile`, avoiding a partially replaced `.kfd` if
  validation, writing or final commit fails.
- Added `kfilter_driverio_smoketest` for v2 complete-state round trip, the Merge import
  policy and intentional rejection of v1 files.

## Patch 268: Dormant Driver low-pass cleanup; bass-reflex diagnostic retained

- Audited the two dormant legacy paths identified by the Patch-263 Driver
  refactoring add-on separately rather than treating them as equivalent dead code.
- Removed the old internal Driver low-pass state (`LowPassL`, `LowPassC`,
  `LowPassQ`, `LowPassFc`, and `lowPassFlag`). In the current Qt6 code there was
  no setter, UI, persistence field, or other write path for `LowPassQ` or
  `LowPassFc`; both were initialized to zero and therefore the path could not be
  activated by the program.
- The removed low-pass implementation is preserved here for reconstruction. With
  `Q = LowPassQ`, `fc = LowPassFc`, angular frequency `omega`, it calculated:

  ```text
  LowPassL = 1 / (2*pi*Q*fc)
  LowPassC = Q / (2*pi*fc)

  Zc = 1 / (1 + j*omega*LowPassC)
  Z  = Zc + j*omega*LowPassL
  response *= abs(Zc / Z)
  ```

  This was a magnitude-only multiplication inside the historical Driver acoustic
  path. It was not numerically identical to the modern complex Active Filter
  low-pass implementation, so Patch 268 does not claim mathematical replacement;
  it removes an unreachable implementation while retaining its formula here.
- `show_reflex_only` is intentionally **not removed**. Historical KFilter versions
  could use this path to display only the bass-reflex-port output of a vented
  enclosure. The current UI has no control for it and there is currently little
  practical design need for that isolated curve, but the calculation can still
  have diagnostic/academic value. An inline source comment now records that
  provenance so a later cleanup does not mistake it for unexplained dead state.
- No Bass Reflex, enclosure, passive-network, Active Filter, Baffle, Measurement,
  Floor Reflection, persistence, or project-format behavior is changed.


## Patch 269: Restore historical Driver natural-roll-off approximation

- Patch 268 removed the dormant internal Driver low-pass after confirming that
  the current Qt6 program has no write path for `LowPassQ` / `LowPassFc` and
  therefore cannot activate it. Subsequent reconstruction of the original
  design intent showed that this was too aggressive for a preservation-oriented
  refactoring.
- The historical low-pass is **not** a predecessor or duplicate of the modern
  user-configurable Active Filter system. It belongs to the physical Driver
  model: it was intended as a 0 dB-normalized approximation of the natural
  upper roll-off of a loudspeaker treated as an ideal piston radiator, so that
  together with the T/S-derived low-frequency high-pass behavior the Driver
  response exhibits the expected natural band-pass characteristic.
- `LowPassL`, `LowPassC`, `LowPassQ`, `LowPassFc`, `lowPassFlag`, their parameter
  calculation and their magnitude-only response multiplication are therefore
  restored exactly in the runtime path.
- The current Qt6 source still does **not** contain a productive setter, UI,
  persistence field, or reconstructed T/S-to-`LowPassQ`/`LowPassFc` derivation.
  The feature consequently remains dormant. Patch 269 intentionally does not
  invent that missing derivation or reactivate the feature.
- The preserved implementation is:

  ```text
  LowPassL = 1 / (2*pi*Q*fc)
  LowPassC = Q / (2*pi*fc)

  Zc = 1 / (1 + j*omega*LowPassC)
  Z  = Zc + j*omega*LowPassL
  response *= abs(Zc / Z)
  ```

- This correction illustrates the preservation rule for further Driver cleanup:
  code that is currently unreachable but has reconstructed physical/modeling
  meaning is retained until that meaning and its original parameter source have
  been fully understood.
- `show_reflex_only` remains retained and documented as the historical
  bass-reflex-port-only diagnostic path introduced in Patch 268.
- No current Driver output, persistence format, Active Filter behavior, Baffle,
  Measurement, Floor Reflection, or passive-network behavior is intentionally
  changed by this patch.

## Patch 270: Driver Stage-D semantic audit and frequency-grid state cleanup

- Began Stage D with an explicit mathematical audit before renaming or relocating
  the remaining Driver coefficients. The current names are intentionally retained
  unless their role is unambiguous across every enclosure path.
- Removed the private `FrequencyFactor` member. It was initialized to the literal
  `1.047128548`, which is exactly the project-wide `KFilterFrequencyStep` used by
  `kfilterfrequencygrid.h`. Pressure and impedance iteration now multiply `omega`
  by that shared constant directly. This removes duplicate configuration state
  without changing the historical iterative frequency sequence or its floating-
  point operation order.

### Reconstructed coefficient semantics

The following identities are derived directly from the current equations. `Qtc`
continues to mean the legacy field that the Qt6 UI presents as `Qts`.

#### `R`, `C`, `L`: reflected motional parallel-RLC branch

For `omega0 = 2*pi*F0` the Driver calculates:

```text
R = Qms * Rdc / Qe
C = Qe / (omega0 * Rdc)
L = Rdc / (omega0 * Qe)
```

`calculateEquivalentCircuit()` uses these values as the admittance

```text
Ymotional = 1/R + j*(omega*C - 1/(omega*L))
```

before adding the resulting branch impedance to `Rdc + j*omega*Lsp`.
The formulas satisfy exactly:

```text
1/sqrt(L*C) = omega0
R*sqrt(C/L) = Qms
```

so these are the electrical-side motional equivalent-circuit R/L/C values, not
arbitrary calculation coefficients.

#### `acousticHighPassCapacitance`, `acousticHighPassInductance`: normalized simplified second-order high-pass

The non-`fullCircuit` Open-Baffle/Sealed path implements the normalized topology

```text
Zp = 1 / (1 + 1/(s*acousticHighPassInductance))
Zc = 1 / (s*acousticHighPassCapacitance)
Hhp(s) = Zp / (Zp + Zc)
       = s^2*acousticHighPassInductance*acousticHighPassCapacitance
         / (1 + s*acousticHighPassInductance + s^2*acousticHighPassInductance*acousticHighPassCapacitance)
```

For free air/Open Baffle:

```text
acousticHighPassCapacitance = Qts / omega0
acousticHighPassInductance = 1 / (Qts * omega0)
```

which gives the standard second-order high-pass with resonance `F0` and quality
factor `Qts`.

For a sealed enclosure with `alpha = Vas/Vb`:

```text
acousticHighPassCapacitance = Qts / omega0
acousticHighPassInductance = 1 / (omega0 * Qts * (1 + alpha))
```

which corresponds to:

```text
Fs_box = F0 * sqrt(1 + alpha)
Q_box  = Qts * sqrt(1 + alpha)
```

An older retained Driver source calculated these two values explicitly as `Fs`
and `SystemQ`; they were later removed only because the stored results themselves
were unused. This historical source therefore corroborates the transfer-function
interpretation above.

#### `pistonLowPassInductance`, `pistonLowPassCapacitance`: normalized second-order natural-roll-off model

For `omegac = 2*pi*pistonLowPassFrequency`:

```text
pistonLowPassInductance = 1 / (omegac * pistonLowPassQ)
pistonLowPassCapacitance = pistonLowPassQ / omegac
```

The preserved legacy circuit gives:

```text
Hlp(s) = 1 / (1 + s*pistonLowPassInductance + s^2*pistonLowPassInductance*pistonLowPassCapacitance)
       = 1 / (1 + s/(omegac*Q) + s^2/omegac^2)
```

Thus the dormant model is exactly a unity-passband second-order low-pass with
`fc = pistonLowPassFrequency` and `Q = pistonLowPassQ`. The historical Driver intentionally applies
only `abs(Hlp)` to the acoustic response, so its phase is discarded.

#### `L2`, `C2`, `R2`: enclosure-dependent secondary branch

These names are overloaded and must not yet be mechanically renamed.

For a vented enclosure:

```text
L2 = Vb * motionalInductance / Vas
C2 = 1 / (L2 * (2*pi*Fb)^2)
R2 = 2*pi*Fb*L2 / Ql
```

and the equivalent circuit uses the series branch

```text
Zbox = R2 + j*(omega*L2 - 1/(omega*C2))
```

so this branch is tuned exactly to `Fb`, with

```text
omega_b*L2/R2 = Ql
```

For Open Baffle, however, `L2` is simply assigned `motionalInductance`; for Sealed it becomes

```text
L2 = 1 / (1/motionalInductance + Vas/(Vb*motionalInductance))
```

and is used as the effective inductive/compliance term of the motional parallel
branch. In Bandpass mode `L2/C2/R2` retain the vented branch role while
`motionalInductance` itself is additionally modified by `V2`. Because `L2` changes semantic role between
these cases, Stage D deliberately leaves this trio unchanged for now.

#### `ventedDenominatorA0` ... `ventedDenominatorA3`: normalized fourth-order vented-box denominator

In the simplified vented response let:

```text
x = f / F0
p = s / (2*pi*F0)
```

The magnitude code is exactly equivalent to:

```text
Hvented(p) = p^4 / D(p)

D(p) = p^4
     + ventedDenominatorA3*p^3
     + ventedDenominatorA2*p^2
     + ventedDenominatorA1*p
     + ventedDenominatorA0
```

because evaluation at `p = j*x` gives the implemented real/imaginary denominator
terms:

```text
real = x^4 - ventedDenominatorA2*x^2 + ventedDenominatorA0
imag = ventedDenominatorA1*x - ventedDenominatorA3*x^3
```

The four stored values are therefore dimensionless coefficients of the normalized
fourth-order vented-box high-pass denominator. They are not generic constants.

#### `radiationCapacitance`: ideal-piston radiation term in full-circuit mode

The full-circuit path uses:

```text
radiationCapacitance = 1 / (2*pi*(34000/Dm))
Hrad(s) = 1 / (1 + 1/(s*radiationCapacitance))
        = s*radiationCapacitance / (1 + s*radiationCapacitance)
```

with `Dm` in cm and the historical sound-speed constant `34000 cm/s`. The
capacitance was intentionally introduced to approximate the driver's radiation
resistance in the extended equivalent-circuit model. In combination with the
electromechanical circuit it produces the ideal-piston band-pass behaviour. This
is the full-circuit alternative to the separate normalized piston low-pass model.

#### `Norm` and `calibrate`: meaning still incomplete

The source establishes only:

```text
calibrate = 12.58925412
Norm = sqrt(8/Rdc) * calibrate * sqrt(2)
```

`12.58925412` is numerically the linear gain corresponding to approximately
`+22 dB`, but no surviving source comment explains why that calibration was
chosen or what absolute reference it represents. `Norm` is applied only in the
full-circuit acoustic paths after the radiation term. Both values remain intact
until their physical/normalization provenance is recovered; Stage D must not
rename them based on speculation.

### Stage-D consequence

The audit separates the remaining values into three categories:

```text
clearly understood:
    motionalResistance/motionalCapacitance/motionalInductance
    acousticHighPassCapacitance/acousticHighPassInductance
    pistonLowPassInductance/pistonLowPassCapacitance
    ventedDenominatorA0/ventedDenominatorA1/ventedDenominatorA2/ventedDenominatorA3
    FrequencyFactor (duplicate grid constant; removed in Patch 270)

understood but semantically overloaded:
    L2/C2/R2

physical provenance still incomplete:
    Norm/calibrate
```

Patch 272 applies the remaining safe naming cleanups from the clearly understood
families. `L2/C2/R2`, `Norm`, and `calibrate` stay unchanged because their naming
or physical normalization provenance still needs further analysis.

## Patch 271: Motional-branch and radiation-model semantic naming

- Renamed the unambiguously reconstructed electrical-side motional equivalent-
  circuit members without changing any formula or calculation order:

  ```text
  R -> motionalResistance
  C -> motionalCapacitance
  L -> motionalInductance
  ```

  They continue to form the parallel motional admittance

  ```text
  Ymotional = 1/motionalResistance
            + j*(omega*motionalCapacitance
                 - 1/(omega*motionalInductance))
  ```

  before transformation back to an impedance and addition of the voice-coil
  impedance `Rdc + j*omega*Lsp`.

- Renamed `RadiationC` to `radiationCapacitance`. Historical clarification from
  the original KFilter model establishes that this is an intentionally added
  normalized capacitance used in the **full-circuit** calculation to approximate
  the driver's radiation resistance for ideal-piston behaviour. Together with
  the electromechanical equivalent circuit it produces the idealized driver's
  natural band-pass response.

- Preserved the separate historical `LowPassL/LowPassC` path. It is not redundant
  with `radiationCapacitance`: it is the simplified, 0 dB-normalized alternative
  used to reproduce the ideal-piston upper roll-off without evaluating the full
  equivalent-circuit/radiation model. Both approaches were historically useful
  for different analysis tasks and must remain available internally even though
  no new UI is introduced at this stage.

- The full-circuit model can be less useful in normal practical loudspeaker design
  because its absolute-efficiency behaviour can obscure the relative response,
  and real drivers often depart substantially from ideal-piston behaviour. It is
  therefore intentionally retained as an optional modelling capability rather
  than promoted to a new user-facing control during this refactoring.

- Corrected the two stale inline `noch nicht aktiv` comments beside the motional
  resistance and radiation-capacitance setup. Both values are used by existing
  full-circuit calculation paths; the old comments no longer described the
  surviving Qt6 code.

- `L2/C2/R2`, `AcousticC/AcousticL`, `Consta..Constd`, `Norm`, `calibrate`, and the
  `LowPass*` family remain otherwise unchanged. Patch 271 is a naming/documentation
  patch only and deliberately does not alter model selection, UI, persistence,
  formulas, or floating-point operation order.


## Patch 272: Simplified acoustic-model semantic naming

- Renamed the normalized simplified high-pass members without changing their
  values, formulas, storage duration, or calculation order:

  ```text
  AcousticC -> acousticHighPassCapacitance
  AcousticL -> acousticHighPassInductance
  ```

- Renamed the fourth-order simplified vented-box denominator coefficients by
  their polynomial order:

  ```text
  Consta -> ventedDenominatorA0
  Constb -> ventedDenominatorA1
  Constc -> ventedDenominatorA2
  Constd -> ventedDenominatorA3
  ```

  The naming follows the already reconstructed denominator

  ```text
  D(p) = p^4 + A3*p^3 + A2*p^2 + A1*p + A0
  ```

  and therefore replaces opaque historical names without changing the
  underlying vented-box approximation.

- Renamed the preserved simplified ideal-piston upper-roll-off model so it can
  no longer be confused with a user Active Filter:

  ```text
  LowPassL   -> pistonLowPassInductance
  LowPassC   -> pistonLowPassCapacitance
  LowPassQ   -> pistonLowPassQ
  LowPassFc  -> pistonLowPassFrequency
  lowPassFlag -> pistonLowPassActive
  ```

  This remains the historical 0 dB-normalized second-order physical Driver-model
  approximation. Its original parameter derivation is still not present in the
  Qt6 code, so the path remains dormant but intentionally preserved. No UI or
  setter was added.

- `L2/C2/R2`, `Norm`, `calibrate`, `show_reflex_only`, and `Unit[49]` remain
  unchanged. Patch 272 is a naming/documentation patch only.
