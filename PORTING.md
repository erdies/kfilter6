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
