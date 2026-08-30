# KFilter6 – Verträge

Dieses Dokument ist die einzige autoritative Quelle für die verbindlichen
Verträge des Projekts. Es wird mit jedem Patch mitgeführt, der einen dieser
Verträge berührt.

`PORTING.md` ist das chronologische Protokoll und erklärt, **wie** ein Stand
entstanden ist. `CONTRACTS.md` beschreibt, **was aktuell gilt**. Bei einem
Widerspruch gewinnt dieses Dokument, und der Widerspruch ist ein Fehler, der
behoben werden muss.

Stand: Patch 300.

## 1. Persistenzformate

```text
.kfp Projektformat           JSON v10
.kfp Legacy-Textformat       lesend unterstützt, wird nicht geschrieben
.kfd Driver-Slot-Format      v2, ausschließlich
```

Ältere JSON-Formatversionen bleiben lesend unterstützt und werden auf den
aktuellen Stand angehoben. Fehlende Objekte einer neueren Version werden exakt
auf ihre Defaults gesetzt.

Keine Formatänderung ohne explizite Versionserhöhung und ohne I/O-Regression im
Test.

### EnclosureType-Abbildung

```text
OpenBaffle = 0
Sealed     = 1
Vented     = 2
Bandpass   = 3
```

Diese Zuordnung ist Persistenzvertrag und wird in `driver.h` per
`static_assert` festgehalten.

### Passives Netzwerk

Der strukturierte Netzwerkzustand ist privat: 8 Sections, je ein Series- und
ein Parallel-Zweig, je R, C und L. Die historische Reihenfolge von 48 Einzel-
werten existiert ausschließlich an der Serialisierungsgrenze in
`networkserializationutils.h` und darf nicht in den Rechenkern zurückwandern.

## 2. Frequenzraster

```text
Stützstellen        150
Startfrequenz       20 Hz
Schrittfaktor       1.047128548  (= 10^(1/50), 50 Punkte je Dekade)
Endfrequenz         ca. 19 100 Hz
```

Druck- und Impedanzantwort eines Drivers sind jeweils 150 komplexe Samples auf
diesem Raster.

Bekannte Abweichung: das Raster endet bei rund 19,1 kHz, nicht bei 20 kHz. Für
20 kHz wären 151 Punkte nötig. Ob das ein Off-by-one aus der Pascal-Vorlage
oder Absicht ist, ist ungeklärt. Siehe `OPEN_POINTS.md`.

Bekannte Doppelung: `kfilterfrequencygrid.cpp` baut das Raster iterativ auf,
`driver.cpp` startet unabhängig davon bei `omega = 125.6637061` und
multipliziert selbst. Beide verwenden dieselbe Rekursion und driften daher
gleich, aber die Konstante ist dupliziert.

## 3. Driver- und Gehäusemodell

- `F0 != 0` aktiviert die T/S-basierte akustische Berechnung.
- `F0 == 0` umgeht sie vollständig; die Impedanz bleibt `Rdc + j*omega*Lsp`.
- `Ql` wirkt bei Sealed konsistent in Simplified SPL, Full Circuit und
  Impedanz. Open Baffle bleibt `Ql`-unabhängig.
- `Tube diameter` ist persistiert; `Tube length` wird im Dialog analytisch
  abgeleitet. Die Kopplung arbeitet in beide Richtungen.
- `pistonLowPassActive`, `show_reflex_only` und
  `LegacyFullCircuitCalibrationGain` bleiben als dormante beziehungsweise
  historische Pfade erhalten und werden nicht ohne eigenen Patch entfernt.

### Vented-Rolloff im vereinfachten Modus

Seit Patch 300 tragen alle vier Gehäusetypen im vereinfachten Modus die Phase.
Der Vented-Zweig bildet den Nenner des Hochpasses vierter Ordnung als komplexe
Zahl:

```text
H(s) = s^4 / (s^4 + a3*s^3 + a2*s^2 + a1*s + a0)   bei s = j*bw

Re(D) = bx - a2*bu + a0
Im(D) = a1*bw - a3*bu*bw
H     = bx / D
```

Bis Patch 299 wurde daraus nur `|H| = bx / sqrt(Re^2 + Im^2)` gebildet. Die
Phase wurde also nie verworfen, sondern nie gebildet. Der Betragsverlauf ist
durch die Umstellung unverändert; geändert hat sich ausschließlich die
Vektorsumme über die Übernahmefrequenz, die vorher physikalisch nicht korrekt
war.

Es gibt bewusst keinen Schalter für dieses Verhalten. Die frühere Rechnung ohne
Phase war eine Performance-Maßnahme der DOS-Vorlage, keine
Modellierungsvariante.

## 4. Validierung der Driver-Parameter

Seit Patch 298 prüfen alle Deserialisierungspfade die T/S- und Gehäuse-
parameter über `driverparametervalidation.{h,cpp}`, bevor Werte auf eine
`driver`-Instanz angewendet werden. Betroffen sind der JSON-`.kfp`-Pfad, der
Legacy-Text-`.kfp`-Pfad und der `.kfd`-Pfad.

```text
1. Rdc, Lsp, F0, Qts, Qes, Qms, Vas, Dm, Vb, Fb, V2 endlich und >= 0
2. enclosureTypeProposal in 0..3
3. Rdc > 0
4. wenn F0 != 0:  Qts > 0, Qes > 0, Qms > 0
5. wenn F0 != 0 und Vb > 0 und Fb > 0 und proposal >= Vented:  Vas > 0
```

Regel 4 und 5 sind bewusst bedingt. `F0 == 0` umgeht den akustischen Pfad, die
Güten sind dann unbenutzt. Regel 5 bildet exakt die Bedingung nach, unter der
`calculateParameters()` durch `Vas` dividiert.

`Ql > 0` und `gainLinear > 0` werden weiterhin an ihren eigenen Aufrufstellen
geprüft.

`Qts` wird **nicht** gegen `1/Qms + 1/Qes` gegengeprüft. Publizierte
Herstellerdaten sind an dieser Stelle routinemäßig inkonsistent.

Keine Datei, die vor Patch 298 endliche Ergebnisse lieferte, wird abgelehnt.

## 5. Verarbeitungskette

```text
raw complex driver response
 -> H_active(f)
 -> H_baffle(f)
 -> H_floor(f)
 -> measurement amplitude correction
 -> effective complex driver response
```

Active Filters und Measurement-Amplitudenkorrektur arbeiten komplex. Der
width-anchored `n=2`-Baffle-LF-Hybrid ohne +6-dB-Hard-Clamp bleibt produktiv.
Floor Reflection bleibt als experimentell gekennzeichnet.

## 6. Teststrategie

Registrierte CTests: 23.

Ein Patch gilt als validiert, wenn er baut und `ctest` vollständig durchläuft.
Beschriftungen und pixelgenaue Ausrichtung werden nicht automatisiert getestet,
wenn ein Fehler unmittelbar visuell erkennbar ist und kein funktionaler Vertrag
betroffen ist.

Verhaltensverträge gehören als Assertion in einen Test, nicht als
Quelltextsuche in ein Prüfskript.

Referenzumgebung des zuletzt belegten Laufs:

```text
Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Qt 6.4.2, Release
0 Fehler, 0 Warnungen, 23/23 Tests bestanden
```

## 7. Patch- und Übergabeverfahren

Seit Patch 299 ist das Git-Repository die autoritative Basis. Tarball,
Manifest, `SHA256SUMS` und die früheren Verify-Skripte entfallen; Git
garantiert Integrität und Rekonstruierbarkeit über die Objekt-Hashes.

```text
Repository   https://github.com/erdies/kfilter6.git
Branch       main
Tags         patch-<n> auf jedem Patchstand
```

- Ein Patch ist genau ein Commit und erhöht `KFILTER_PATCH_LEVEL` in
  `CMakeLists.txt`.
- Ein Patch geht gegen seinen direkten Vorgänger, nicht kumulativ gegen eine
  ältere Baseline.
- Übergabe erfolgt als `git format-patch`, Anwendung mit `git am -3`.
- Eine Neuverpackung verbraucht keine Patchnummer mehr. Patch 297 war die
  letzte dieser Art und ist in dieser Linie nicht enthalten; der Patch-Level
  springt von 296 auf 298.
- Zeilenenden sind LF, erzwungen über `.gitattributes`.
