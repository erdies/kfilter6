# KFilter6 – Offene Punkte

Stand: Patch 299. Dieses Dokument wird mitgeführt und bei jedem Patch
aktualisiert, der einen Punkt erledigt oder einen neuen aufwirft.

## Vorgeschlagene Reihenfolge

1. **Numerische Regressionsbasis** (Patch 300). Eingecheckte Referenzwerte für
   die 150 Stützstellen definierter Parametersätze, geprüft von einem Test.
   Solange die fehlt, ist die Auflage „nur mit numerischer Regression" für das
   Driver-Refactoring eine Absicht ohne Werkzeug. Muss vor jeder
   Strukturänderung am Rechenkern stehen.
2. **UI-Zustand aus dem Rechenkern lösen.** `DriverPlotState` ist reiner
   Darstellungszustand und liegt in `driver`. Billiger und risikoärmer als das
   Auseinandernehmen der Rechenkette, und ein sinnvoller erster Schritt des
   Refactorings.
3. **Dependency-Audit der abgeleiteten Rechenmember** in `driver`, in kleinen
   Schritten und nur mit numerischer Regression.

## Numerik und Modell

- **Vented im vereinfachten Modus trägt nur den Betrag bei.** Die
  Phasendrehung des Hochpasses vierter Ordnung fehlt, die Vektorsumme über die
  Übernahmefrequenz ist dadurch bei Bassreflex nicht korrekt. Siehe
  `CONTRACTS.md` Abschnitt 3. Zu klären: ist das Legacy-Verhalten gewollt oder
  soll der vereinfachte Pfad komplex werden?
- **Frequenzraster endet bei 19,1 kHz statt 20 kHz.** 149 Schritte ab 20 Hz mit
  Faktor 10^(1/50). Für 20 kHz wären 151 Punkte nötig. Off-by-one aus der
  Pascal-Vorlage oder Absicht?
- **Rasterdefinition ist doppelt vorhanden.** `driver.cpp` startet unabhängig
  von `kfilterfrequencygrid.cpp` bei `omega = 125.6637061`. Beide driften
  gleich, aber die Konstante gehört an eine Stelle.
- **`double pi = 3.141592654` lokal in `driver.cpp`** statt `M_PI`. Relativer
  Fehler rund 1,3e-10, unkritisch, aber unnötig.
- **`findLastNetworkSectionIndex()` steht in beiden Frequenzschleifen** und
  wird 150-mal pro Berechnung ausgewertet, obwohl das Ergebnis invariant ist.
- **`Qts`, `Qms` und `Qes` sind unabhängig setzbar**, ohne Prüfung gegen
  `1/Qts = 1/Qms + 1/Qes`. Ein nicht blockierender Konsistenzhinweis gehört in
  den Driver-Parameters-Dialog, nicht in den Loader.

## Architektur

- **`driver` ist ein God-Object**: T/S-Parameter, Gehäusemodell, passives
  Netzwerk, zwei Ergebnis-Caches und UI-Sichtbarkeitszustand in einer Klasse.
- **Drei Monolithen** halten fast ein Drittel des Codes:
  `kfilterqt6app.cpp` (~102 KB), `kfilterprojectio.cpp` (~97 KB),
  `circuitout.cpp` (~81 KB).
- **Dialog-Smoketests kompilieren die Dialogquellen ein zweites Mal** direkt in
  ihr Testtarget, statt gegen eine gemeinsame UI-Bibliothek zu linken. Bei drei
  Dialogtests erträglich, skaliert aber schlecht.

## Weitere Themen

- Baffle-/Diffraction-Performance, insbesondere die 45-Grad-Phasenberechnung,
  separat untersuchen; nicht mit UI- oder Persistenzänderungen vermischen.
- Frequenzraster-Vergrößerung und mögliche Parallelisierung erst anhand der
  vorhandenen Supplements und mit Performance- und Numerikmessungen planen.
- Platzierung des Driver-Titels im Network-/Filter-Dialog optional erneut
  bewerten. Aktuell bleibt die bewusst akzeptierte Variante unterhalb der
  Tabelle erhalten.

## Weiterhin bindend

- Floor Reflection bleibt als experimentell gekennzeichnet.
- Der width-anchored `n=2`-Baffle-LF-Hybrid bleibt produktiv.
- Active Filters, Measurements und Baffle/Floor-Verarbeitung bleiben in der in
  `CONTRACTS.md` dokumentierten komplexen Verarbeitungskette.
- Keine Formatänderung ohne explizite Versionierung und I/O-Regression.

## Erledigt

- **Patch 298** – Fehlende Validierung der T/S-Parameter beim Laden. Eine
  Datei mit `Rdc`, `Qts`, `Qes` oder `Qms` gleich null erzeugte
  nicht-endliche Stützstellen über das gesamte Raster, ohne Fehlermeldung.
- **Patch 299** – Verträge und offene Punkte liegen jetzt im Repository und
  werden mit dem Code versioniert statt in externen Handover-Paketen.
