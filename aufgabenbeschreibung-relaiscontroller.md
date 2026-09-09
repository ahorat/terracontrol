# Aufgabenbeschreibung: WiFi 4-Kanal Relaiscontroller mit RTC

## 1. Projektübersicht

Firmware für einen ESP32-basierten 4-Kanal-Relaiscontroller (230VAC/5A pro Kanal) mit folgenden Kerneigenschaften:

- Jeder der 4 Relaiskanäle ist unabhängig konfigurierbar (Intervall-Muster, feste Ein-/Ausschaltzeit, Sonnenauf-/untergang)
- Konfiguration über eine vom ESP32 selbst ausgelieferte Weboberfläche
- Konfiguration wird persistent auf dem externen EEPROM (AT24C32, am DS3231-RTC-Modul) gespeichert
- Zeitbasis: DS3231 RTC (batteriegepuffert), synchronisiert per NTP wenn WLAN verfügbar
- Funktioniert vollständig autark ohne Netzwerkverbindung (RTC + gespeicherte Konfiguration reichen)
- WLAN: Access-Point-Modus zur Ersteinrichtung, danach Client-Modus; Reset-Pin zum Zurückwechseln in den AP-Modus

## 2. Hardware-Kontext

| Komponente | Typ |
|---|---|
| MCU | ESP32 (NodeMCU-32S, Arduino-Framework) |
| RTC | DS3231 mit AT24C32 EEPROM (I2C) |
| Relais | 4-Kanal-Modul, 5V-Steuerung, GPIO-getrieben |
| Reset-Taster | 1x GPIO, Pull-up, aktiv Low (Pin frei wählbar) |
| Speisung | 5V DC intern (230V→5V Wandler vorgeschaltet) |

Entwicklungsumgebung: Ubuntu-VM, Arduino-Framework (PlatformIO oder Arduino-CLI nach Wahl von Claude Code). Bibliotheksauswahl (WLAN-Konfig-Portal, RTC, Async-Webserver etc.) liegt bei Claude Code.

## 3. Funktionale Anforderungen

### 3.1 Kanal-Konfiguration (pro Kanal individuell, 4x unabhängig)

Jeder Kanal unterstützt genau einen der folgenden Betriebsmodi, umschaltbar über die Weboberfläche:

1. **Intervall-Muster**
   - Parameter: Intervalldauer (z. B. alle 30 Min), Einschaltdauer (z. B. 10 Sek)
   - Einschränkbar auf ein Zeitfenster (z. B. nur zwischen 08:00–20:00) und optional auf Wochentage
2. **Feste Tageszeit**
   - Parameter: Einschaltzeit, Ausschaltzeit (HH:MM), optional Wochentage
3. **Sonnenauf-/untergang**
   - Referenzort: Bern, Schweiz (Koordinaten fix im Code hinterlegt, kein GPS/manuelle Eingabe nötig)
   - Parameter: Offset in Minuten (z. B. "-30" = 30 Min vor Sonnenuntergang), separat für Sonnenauf- und -untergang, pro Kanal
   - Berechnung berücksichtigt Sommerzeit (Europe/Zurich, DST-Regeln fix im Code)

Zusätzlich, unabhängig vom Modus:

4. **Manueller Override**
   - Pro Kanal über die Weboberfläche auslösbar: Kanal für eine definierbare Dauer (z. B. 5 Min) manuell ein- oder ausschalten, unabhängig vom aktuell konfigurierten Modus/Zeitplan
   - Nach Ablauf der Override-Dauer kehrt der Kanal automatisch zum regulären, konfigurierten Zeitplan zurück
   - Alternativ: Override "bis auf Weiteres" (manuell wieder aufheben) — bitte sinnvolle Umsetzung wählen, die beide Fälle abdeckt (Dauer-basiert UND manuell aufhebbar)

### 3.2 Weboberfläche

- Wird direkt vom ESP32 ausgeliefert (kein externer Server nötig)
- Optisch ansprechend (CSS direkt eingebettet, responsive für Smartphone-Nutzung)
- Änderungen an der Konfiguration wirken **sofort** (live), kein Reboot nötig
- Startseite zeigt pro Kanal:
  - Aktueller Zustand (an/aus)
  - Aktiver Modus
  - Nächste geplante Schaltzeit
  - Override-Status (falls aktiv)
- Kein Passwortschutz (Zugriffsschutz erfolgt durch WLAN-Zugang selbst)
- Zusätzliche Seite/Bereich für:
  - WLAN-Konfiguration (SSID + Passwort fürs Zielnetzwerk, im AP-Modus)
  - Manuelle Zeiteinstellung (Datum/Uhrzeit direkt setzen)
  - Anzeige des aktuellen NTP-Sync-Status

### 3.3 WLAN-Verhalten

- **Erststart / kein bekanntes WLAN:** ESP32 öffnet eigenen Access Point (feste SSID, z. B. "RelayController-Setup"), über den die Weboberfläche zur Eingabe von Ziel-SSID/Passwort erreichbar ist
- **Nach erfolgreicher Konfiguration:** Wechsel in den Client-Modus (Verbindung zum konfigurierten WLAN), Weboberfläche bleibt darüber erreichbar
- **Reset-Pin:** Beim Drücken (z. B. > 3 Sek. halten, um Fehlauslösung zu vermeiden) wechselt das Gerät zurück in den AP-Modus, unabhängig vom aktuellen Zustand
- **Ohne Netzwerk / WLAN-Verlust:** Relaissteuerung läuft komplett unabhängig weiter, basierend auf RTC-Zeit und der zuletzt aus dem EEPROM geladenen Konfiguration
- **Reconnect:** Bei Verbindungsverlust automatischer Reconnect-Versuch alle ca. 5 Minuten (im Hintergrund, blockiert die Relaissteuerung nicht)

### 3.4 Zeit / RTC / NTP

- Beim Boot: Zeit vom DS3231 lesen und als Systemzeit übernehmen (funktioniert auch ohne WLAN)
- Falls WLAN verfügbar: NTP-Sync beim Boot sowie danach alle 12 Stunden; erfolgreicher Sync aktualisiert auch den DS3231
- Manuelle Zeiteinstellung über die Weboberfläche jederzeit möglich (überschreibt RTC direkt)
- Zeitzone/Sommerzeit: fix Europe/Zurich (DST-Umschaltung automatisch nach den entsprechenden Regeln, nicht konfigurierbar)
- Failsafe: Falls die RTC-Zeit ungültig ist (z. B. Power-Loss-Flag des DS3231 gesetzt, Zeit unplausibel) → alle Relais bleiben/werden aus, bis eine gültige Zeit vorliegt (NTP oder manuelle Eingabe)

### 3.5 EEPROM-Speicherung (AT24C32, 4KB)

- Einfache, feste Byte-Struktur pro Kanal (kein Wear-Leveling notwendig – Schreibzyklen bei gelegentlicher Rekonfiguration unkritisch)
- Struktur pro Kanal umfasst mindestens:
  - Modus (Intervall / feste Zeit / Sonnenauf-Untergang)
  - Modus-spezifische Parameter (siehe 3.1)
  - Zeitfenster-/Wochentagseinschränkung (falls zutreffend)
- Beim Boot: Konfiguration aller 4 Kanäle aus dem EEPROM laden und als Referenz für die State Machine verwenden
- Bei jeder Änderung über die Weboberfläche: sofort ins EEPROM zurückschreiben (Grundlage für "live, ohne Reboot")
- WLAN-Zugangsdaten (Ziel-SSID/Passwort): ebenfalls persistent speichern (EEPROM oder ESP32-NVS/Preferences – Wahl liegt bei Claude Code, sinnvollerweise getrennt von der RTC-EEPROM-Struktur, falls Platz/Architektur das nahelegt)

### 3.6 Relais-Logik / Sicherheit

- Failsafe-Zustand: alle Relais aus, wenn RTC-Zeit ungültig ist
- State Machine für die Zeitplan-Auswertung läuft unabhängig vom WLAN-Status (Kernanforderung)
- Manueller Override hat Vorrang vor dem regulären Zeitplan, solange er aktiv ist

## 4. Nicht-funktionale Anforderungen

- Framework: Arduino (C/C++), lauffähig auf ESP32
- OTA-Update: optional / nice-to-have, keine harte Anforderung
- Entwicklungsumgebung: Ubuntu-VM; Bibliotheksauswahl (z. B. Async-Webserver, RTC-Library, NTP-Client, Sonnenauf-/untergangsberechnung) liegt im Ermessen von Claude Code
- Code sollte so strukturiert sein, dass die 4 Kanäle über eine gemeinsame Logik/Konfigurationsstruktur behandelt werden (kein 4-facher Copy-Paste-Code)

## 5. Offene Punkte, die Claude Code sinnvoll selbst entscheiden darf

- Konkrete Bibliotheksauswahl (z. B. ESPAsyncWebServer vs. WebServer, RTClib vs. eigene DS3231-Ansteuerung, Sonnenauf-/untergangsberechnung z. B. via bekannter Formel/Library)
- Genaues EEPROM-Speicherlayout (Byte-Offsets, Struktur-Packing)
- Konkrete Umsetzung des Override-Mechanismus (Dauer-basiert + manuell aufhebbar, siehe 3.1 Punkt 4)
- Details des AP-Modus (feste IP, Captive Portal ja/nein)
- Visuelles Design der Weboberfläche (im Rahmen von "hübsch, direkt vom ESP32")

## 6. Akzeptanzkriterien (Kurzfassung)

- [ ] Gerät startet ohne konfiguriertes WLAN im AP-Modus, Weboberfläche erreichbar
- [ ] Ziel-WLAN kann über Weboberfläche eingerichtet werden, Gerät verbindet sich danach automatisch
- [ ] Reset-Pin bringt Gerät zuverlässig zurück in den AP-Modus
- [ ] Jeder der 4 Kanäle kann unabhängig auf Intervall / feste Zeit / Sonnenauf-Untergang konfiguriert werden
- [ ] Intervall-Modus respektiert optionale Zeitfenster-/Wochentagseinschränkung
- [ ] Sonnenauf-/untergangsberechnung für Bern inkl. Offset und korrekter Sommerzeit funktioniert
- [ ] Manueller Override pro Kanal funktioniert und kehrt nach Ablauf zum Zeitplan zurück
- [ ] Konfigurationsänderungen wirken sofort ohne Reboot
- [ ] Startseite zeigt Live-Status und nächste Schaltzeit pro Kanal
- [ ] Bei WLAN-Ausfall läuft die Relaissteuerung unverändert nach RTC weiter, Reconnect-Versuch alle ~5 Min
- [ ] NTP-Sync beim Boot und alle 12h, DS3231 wird dabei aktualisiert
- [ ] Manuelle Zeiteinstellung über Weboberfläche funktioniert
- [ ] Bei ungültiger RTC-Zeit bleiben alle Relais im Failsafe (aus)
- [ ] Konfiguration übersteht Stromausfall (EEPROM-Persistenz, korrektes Laden beim Boot)
