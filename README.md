# soyosource-powercontroller

Mit diesem Projekt ist es möglich die Einspeiseleistung einens SoyoSource GTN-1000W / GTN-1200W per Webinterface durch manuelle Steuerung, Zeitplan, MQTT oder mit einem Shelly EnergyMeter als Nulleinspeisung (3EM PRO, 3EM, EM, 1PM) zu steuern.

> **Hinweis zur Weiterführung:** Dieses Projekt wurde ursprünglich von [matlen67](https://github.com/matlen67/soyosource-powercontroller) entwickelt und Ende 2024 von ihm eingestellt. Es wird seit Juni 2026 in diesem Repository weiterentwickelt und gepflegt. **Vielen Dank an matlen67 für die großartige Vorarbeit und das solide Fundament, auf dem dieses Projekt aufbaut!**

## Changelog

## Update 12.06.2026
Projekt wird ab jetzt in diesem Repository weitergeführt, das Repository ist nun öffentlich. GitHub Action für automatische Release-Builds eingerichtet (bei einem Versions-Tag `v*` wird die Firmware automatisch gebaut und als Release mit `firmware.bin.gz` und `manifest.json` veröffentlicht).

Neue Funktion: Wählbare Meter-Quelle für die Nulleinspeisung (siehe Abschnitt "Meter-Quellen" unten):
- Shelly (HTTP) - wie bisher, unverändert (Standard)
- Tasmota (HTTP) - z.B. IR-Lesekopf am Stromzähler, Leistungswert über konfigurierbaren JSON-Pfad
- MQTT-Topic - beliebiges Topic (Zahl oder JSON), mit Ausfall-Überwachung: nach 30 s ohne Wert wird sicherheitshalber auf 0 W geregelt
- HomeWizard (HTTP) - P1 Meter, Energy Socket und kWh Meter über die lokale API

Bugfix: MQTT-Payloads wurden in einen zu kleinen Puffer (8 Byte) kopiert - Payloads über 7 Zeichen haben Speicher überschrieben. Puffer auf 512 Byte vergrößert und Längenprüfung ergänzt.

Neue Funktion: Firmware-Update direkt aus GitHub. Der Controller prüft einmal täglich (und per Button "Jetzt prüfen" in der ESP-Karte) anhand von `manifest.json` das neueste GitHub-Release. Ist eine neue Version verfügbar, erscheint ein "Update installieren"-Button. Ein Klick löst einen Neustart aus; direkt nach dem Booten (noch vor WiFiManager, Webserver und MQTT, damit genug freier Speicher zur Verfügung steht) verbindet sich der ESP kurz mit dem WLAN und lädt `firmware.bin.gz` vom GitHub-Release in 4-KB-Blöcken per HTTP-Range-Requests über eine wiederverwendete HTTPS-Verbindung herunter (GitHub liefert bei großen Antworten 16-KB-TLS-Records, für deren Puffer der Heap des ESP8266 nicht reicht - kleine Blöcke umgehen das). Vor dem Flashen wird die MD5-Prüfsumme aus `manifest.json` verifiziert; bei Fehlern bleibt die bisherige Firmware unverändert. Bei Erfolg startet der ESP automatisch mit der neuen Version neu. Die Firmware-Version wird jetzt aus `FW_VERSION` im Webinterface angezeigt (Kopfzeile war vorher hartkodiert). Auf Hardware getestet: Update von v1.241013 auf v1.260612 erfolgreich über OTA installiert.

Home Assistant Integration per MQTT Discovery hinzugefügt (siehe Abschnitt "Home Assistant Integration" unten).

Build-Fehler in platformio.ini behoben, damit das Projekt wieder erfolgreich mit PlatformIO kompiliert:
- Tippfehler beim Library-Namen korrigiert: `me-no-dev/ESP Async WebServer` -> `me-no-dev/ESPAsyncWebServer`.
- Build-Flag `TEMPLATE_PLACEHOLDER` portabel gemacht (`126` statt `(char)126` mit Anführungszeichen), damit der Build auch unter Linux (GitHub Actions) funktioniert.
- `ayushsharma82/AsyncElegantOTA@^2.2.8` wurde aus der PlatformIO-Registry entfernt; die Library wird jetzt als Version 2.2.8 direkt im Projekt unter `lib/AsyncElegantOTA` mitgeliefert (vendored), um Konflikte mit neueren ESPAsyncWebServer-Versionen zu vermeiden.

Verbesserungen am Webinterface und MQTT (auf Hardware getestet):
- MQTT-Server/Port lassen sich jetzt über "Save Settings" ändern, ohne dass ein Neustart des ESP nötig ist (Verbindung wird sofort mit den neuen Daten neu aufgebaut).
- Der MQTT-Verbindungsstatus wird jetzt jede Sekunde live im Webinterface aktualisiert (vorher nur beim Laden der Seite).
- Im Debug-Log wird bei fehlgeschlagener MQTT-Verbindung jetzt der PubSubClient-Fehlercode mit ausgegeben (`reconnect failed! state=...`).

Webinterface überarbeitet (auf Hardware getestet):
- Neue eigene Karte "Firmware-Update" mit den Funktionen "Jetzt prüfen", "Update installieren" und Status-Anzeige - der bisherige Update-Button am Seitenende (AsyncElegantOTA-Link) wurde entfernt.
- Neue Option "Automatische Updates": ist sie aktiviert, installiert der ESP ein erkanntes Firmware-Update beim nächsten Prüfzyklus automatisch, ohne Klick auf "Update installieren".
- Karte "Shelly 3EM" in "Energiezähler" umbenannt, da nun auch andere Meter-Quellen unterstützt werden. Neue Zeile "Erkannter Typ" zeigt den per Meter-Quelle erkannten Gerätenamen an.
- Im Energiezähler-Menü werden MQTT- und JSON-Pfad-Felder nur noch angezeigt, wenn die jeweils ausgewählte Meter-Quelle sie benötigt (nicht bei Shelly).

Bugfix Webinterface (auf Hardware getestet): Die Status-Anzeigen "Nulleinspeisung", "Batterieschutz" und "Timer" (EIN/AUS) sowie WiFi-Signalstärke/-Qualität wurden bisher nur beim Laden der Seite aktualisiert. Nach Umschalten der "Aktiv"-Checkbox (z.B. Batterieschutz aktivieren) blieb die Anzeige auf dem alten Wert stehen, bis die Seite manuell neu geladen wurde. Diese Felder werden jetzt wie alle anderen Live-Werte jede Sekunde aktualisiert. Ebenso wird die angezeigte Firmware-Version (Kopfzeile und Firmware-Update-Karte) jetzt jede Sekunde aktualisiert, damit nach einem Selbst-Update per OTA sofort die neue Version angezeigt wird, ohne die Seite neu laden zu müssen.

## Update 13.06.2026

Bugfix Webinterface (auf Hardware getestet): Im Energiezähler-Menü wurden die MQTT-Topic- und JSON-Pfad-Zeilen nach einem Neuladen der Seite teils fälschlich angezeigt, obwohl die gewählte Meter-Quelle (z.B. Shelly) sie nicht benötigt. Sie verschwanden erst, wenn die Meter-Quelle einmal manuell umgeschaltet wurde. Die Sichtbarkeit dieser Zeilen wird jetzt - wie die anderen Live-Werte - jede Sekunde neu anhand der aktuell gewählten Meter-Quelle berechnet, sodass sie auch direkt nach dem Laden korrekt ein-/ausgeblendet sind.

## Update 21.12.2024
*(Hinweis des ursprünglichen Entwicklers matlen67:)*
Ich bin auf einen Multiplus-II umgestiegen und werde daher an diesem Projekt nicht mehr weiterarbeiten.
Bis dato hat der soyosource-powercontroler einwandfrei funktioniert.

## Funktionsweise
Der SoyoSource kann die Energie DC-Seitig aus PV-Module oder aus einer Batterie beziehen. Die AC-Einspeiseleistung kann im Einstellmenü des SoyoSource als Festwert in Watt oder durch einen auf einer Phase angeschlossenen SoyoSource Limiter bereitgestellt werden. Der Limiter wird per RS485-Schnittstelle am SoyoSource angeschlossen und sendet dann die auf der Phase anliegende Leistung an den SoyoSource.

Hinweis. Die aktuellen Versionen der SoyoSource Einspeisewechselrichter geben keine Daten mehr über die RS485-Schnittstelle aus, somit ist ein Auslesen von SoyoSource Informationen nicht möglich. Leider liegen mir aktuell keine Informationen vor ob der Sendevorgang per Software/Hardware deaktiviert wurde oder ob es nur neue Parameter bedarf um den SoyoSource zum Senden zu bewegen.

Diese Steuerung in Verbindung mit der Schaltung aus Bild 1 ersetzt den SoyoSource Limiter. Damit die Leistungsvorgabe dieser Steuerung funktioniert, muss im Einstellmenü des SoyoSource der Limitermode aktiviert werden (Bild 2).
Die manuelle Steuerung über das Webinterface sowie per MQTT oder Zeitplan funktionieren soweit, lediglich die Nulleinspeisung habe ich erst im Dezember 2023 mit eingebaut und kann diese erst im Frühjar 2024 testen und optimieren.

Achtung, ich überneheme keinerlei Haftung für Schäden an Personen oder Hardware die durch dieses Projekt entstehen. Arbeiten an Spannungen größer 24V sollten nur von Fachpersonal durchgeführt werden!  
 

## PlatformIO
Dieses Projet wurde von der Ardunino IDE zu PlatformIO portiert

## Arduino IDE 2.1.0
Wer dieses Projekt weiterhin mit der Arduino IDE nutzen möchte muss die Datei main.cpp nach 'soyosource-powercontroller.ino' umbenennen und diese  mit der html.h in einen Ordner mit den Namen 'soyosource-powercontroller' kopieren.

#benötigte Librarys
 - ESPAsync_WiFiManager (https://github.com/khoih-prog/ESPAsync_WiFiManager)
 - ESPAsyncWebServer    (https://github.com/me-no-dev/ESPAsyncWebServer) Bitte Hinweis lesen
 - ESPAsyncTCP          (https://github.com/me-no-dev/ESPAsyncTCP)
 - ElegantOTA           (https://github.com/ayushsharma82/AsyncElegantOTA)
 - Uptime               (https://github.com/XbergCode/Uptime)


#### Hinweis ESPAsyncWebServer bei verwendung der Arduino IDE 
Innerhalb der Library ist das Prozentzeichen '%' als Platzhalter definiert. Variablen die vom Platzhalter umschlossen sind können so später durch gesendeten Code vom Webserver ersetzt werden um z.B. Daten von Sensoren dazustellen. Leider interpretiert der Webserver aber auch das Prozentzeichen in CSS oder HTML Code falsch, so das 
z.B. bei der Angabe des property's wie xyz{ widht: 90%; } das % Zeichen entfernt wird. Dieses führt folglich zu Fehldarstellungen der Website. Als Workaround hilft Angaben mit Prozentzeichen immer doppelt anzugeben xyz{ width:90%%; } oder man ersetzt in der Library das Platzhalter Zeichen.
Ich habe in meiner Library unter dem Library-Ordner ESP Async WebServer/src die Datei 'WebResponseImpl.h' angepasst und den Platzhalter ersetzt:

#define TEMPLATE_PLACEHOLDER '%' 

durch

#define TEMPLATE_PLACEHOLDER '~'

ersetzen

#### Wer platformio nutzt brauch das % nicht ändern, da ist es in der platformi.ini als build flag hinterlegt



## Schaltung
### Bauteile
- NodeMCU mit ESP8266 (ESP-12F) (4MB Flash)
- RS485 Entwicklungsboard TTL zu RS485, MAX485

Hinweis: Das RS485 Entwicklungsboard verwendet einen MAX485 Pegelwandler der für eine Versorgungsspannung von 5V ausgelegt ist. Da die GPIO's des ESP8266 dauerhaft nur 3.3V vertragen wird die Spannung Vcc vom RS485 Entwicklungsboard am 3.3V Ausgang des NodeMCU abgegriffen. Das RS485 Etwicklungsboard arbeitet auch zuverlässig mit 3.3V. Die 5V Spannungsversorgung des NodeMCU kann entweder über USB oder den Anschlus-Pin VIN erfolgen.


### Bild 1: Schaltung
<img src="https://github.com/matlen67/soyosource-powercontroller/blob/main/image/wiring_nodemcu_rs485.png" width="512">


### Bild 2: Einstellmenü SoyoSource
Hier muss 'Bat AutoLimit Grid' auf Y stehen

<img src="https://github.com/matlen67/soyosource-powercontroller/blob/main/image/display_setup.jpg" width="256">
  

## Webif
<img src="https://github.com/matlen67/soyosource-powercontroller/blob/main/image/webif_241026_lighning.png" width="512"> 

<img src="https://github.com/matlen67/soyosource-powercontroller/blob/main/image/webif_241026_darkmode.png" width="512"> 


## Meter-Quellen (Nulleinspeisung)

Die Quelle für den aktuellen Netzbezug ist im Webinterface in der Meter-Karte über das Dropdown "Meter-Quelle" wählbar:

| Quelle | Felder | Hinweise |
|---|---|---|
| Shelly (HTTP) | Meter IP | 3EM PRO, 3EM, EM, 1PM, Plus 1PM - der Typ wird automatisch erkannt. Phasen L1-L3 einzeln abwählbar. |
| Tasmota (HTTP) | Meter IP, JSON-Pfad | z.B. IR-Lesekopf am Stromzähler. Abfrage über `Status 8`; der JSON-Pfad zeigt auf den Leistungswert im `StatusSNS`-JSON, z.B. `MT175.P` oder `SML.Power_curr` (mit oder ohne `StatusSNS`-Präfix). |
| MQTT-Topic | MQTT-Topic, JSON-Pfad | Payload entweder nackte Zahl (z.B. `153`) oder JSON (dann JSON-Pfad angeben). MQTT muss aktiviert sein. Sicherheitsfunktion: kommt 30 s lang kein Wert, wird auf 0 W geregelt und "MQTT Meter offline" angezeigt. |
| HomeWizard (HTTP) | Meter IP | P1 Meter, Energy Socket, kWh Meter über die lokale API (`/api/v1/data`). Dafür in der HomeWizard Energy App die "Lokale API" aktivieren. Beim P1 Meter sind die Phasen L1-L3 einzeln abwählbar. |

Konvention: positiver Wert = Netzbezug. Liefert die Quelle das Vorzeichen umgekehrt, die Option "Wert invertieren" aktivieren.


## Home Assistant Integration

Wenn MQTT aktiviert ist, meldet sich der Controller per Home Assistant MQTT Discovery automatisch am Broker an. Es ist keine manuelle YAML-Konfiguration in Home Assistant nötig - das Gerät erscheint unter "SoyoSource soyo_xxxxxx" mit folgenden Entities:

**Sensoren**
- Power (W)
- Meter Power (W)
- Uptime
- WiFi Signal (%)

**Schalter**
- Nulleinspeisung
- Timer 1 / Timer 2
- Batterieschutz

**Zahlenwerte (Number)**
- Power Setpoint (W)
- Teiler Output
- Max Output (W)
- Nullpunkt-Offset (W)

Voraussetzung: Im Webinterface unter "MQTT" Server, Port und ggf. Zugangsdaten eintragen und aktivieren. Die Discovery-Konfigurationen werden bei jeder MQTT-Verbindung retained an `homeassistant/...` gesendet, Statuswerte alle 5 Sekunden aktualisiert.

Hinweis: Änderungen über Home Assistant werden nicht automatisch in `config.json` gespeichert - dafür im Webinterface auf "Save Settings" klicken.


