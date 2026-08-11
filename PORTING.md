<!--
SPDX-License-Identifier: GPL-3.0-or-later
Copyright (C) 2002-2026 Martin Erdtmann
-->

# KFilter Qt6/KF6 porting notes

## Current baseline

This tree is an incremental Qt6 bring-up of the original KDE3.1/KDevelop-era
KFilter sources. The goal is still: first compile and run the old application
behaviour as closely as possible, then refactor.

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

## Known intentional limitations

- The real application executable is not built yet.
- `KFilterDoc::saveModified()` currently returns `false` for modified documents
  because the old interactive KDE3 save prompt has not been ported yet.
- `initParamDialog()`, `initNetworkDialog()`, `initVolumeDialog()` and
  `initToolsWizard()` are currently no-ops.
- The old `KFilterView`, `KFilterApp`, dialogs and drawing code are still not
  part of the Qt6 build target.
- Remote project URLs are intentionally unsupported at this stage; project I/O
  is local-file-only.

## Next good step

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
