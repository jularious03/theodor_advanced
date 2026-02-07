# TheOdor Speicherproblem und DeepSleep Modus

## Zeitstempel

### Hardware-Lösung

**DS3231 Module = RTC-Modul (Real Time Clock):**

- günstig und präzise
- wird versorgt durch Knopfzelle (verbraucht wenig Strom ($1-3 /mu A$))
- Power-LEDs und Ladeschaltungen entfernen, um Strom zu sparen

### Software-Trick (kostenlose Alternative)

**Relativ-Zeit Methode:**

1. Startzeitpunkt dokumentieren
2. Controller zählt Sekunden nach dem Start
3. DeepSleep: Zähler wird im RTC-Memory (vom ESP) gespeichert, das er nicht gelöscht wird
4. Unix-Timestamp = Startzeitpunkt + vergangeneSekunden

### Datenformat & Speicherung

- ISO 8601: YYYY-MM-DD HH:MM:SS
- Unix-Timestamp: Sekunden seit 1970 (ideal für Berechnungen)
- CSV / JSONL: sicher gegen Datenverlust bei Stromausfällen

## DeepSleep Modus

- Aufwärmphase für den Lüfter von PMS3003 & Aufwärmzeit für CSS811, damit richtigen Werte gemessen werden können (_delay(180 000) = 3 Minuten_)
- Verkabelung: Transistor um Sensoren komplett abzuschalten, oder durchgehend mit Strom versorgen
