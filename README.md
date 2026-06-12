
## Update 12.06.2026
Home Assistant Integration per MQTT Discovery hinzugefügt (siehe Abschnitt "Home Assistant Integration" unten).

Build-Fehler in platformio.ini behoben, damit das Projekt wieder erfolgreich mit PlatformIO kompiliert:
- Tippfehler beim Library-Namen korrigiert: `me-no-dev/ESP Async WebServer` -> `me-no-dev/ESPAsyncWebServer`.
- `ayushsharma82/AsyncElegantOTA@^2.2.8` wurde aus der PlatformIO-Registry entfernt; die Library wird jetzt als Version 2.2.8 direkt im Projekt unter `lib/AsyncElegantOTA` mitgeliefert (vendored), um Konflikte mit neueren ESPAsyncWebServer-Versionen zu vermeiden.

Verbesserungen am Webinterface und MQTT (auf Hardware getestet):
- MQTT-Server/Port lassen sich jetzt über "Save Settings" ändern, ohne dass ein Neustart des ESP nötig ist (Verbindung wird sofort mit den neuen Daten neu aufgebaut).
- Der MQTT-Verbindungsstatus wird jetzt jede Sekunde live im Webinterface aktualisiert (vorher nur beim Laden der Seite).
- Im Debug-Log wird bei fehlgeschlagener MQTT-Verbindung jetzt der PubSubClient-Fehlercode mit ausgegeben (`reconnect failed! state=...`).

## Update 21.12.2024
Ich bin auf einen Multiplus-II umgestiegen und werde daher an diesem Projekt nicht mehr weiterarbeiten.
Bis dato hat der soyosource-powercontroler einwandfrei funktioniert.

# soyosource-powercontroller
Mit diesem Projekt ist es möglich die Einspeiseleistung einens SoyoSource GTN-1000W / GTN-1200W per Webinterface durch manuelle Steuerung, Zeitplan, MQTT oder mit einem Shelly EnergyMeter als Nulleinspeisung (3EM PRO, 3EM, EM, 1PM) zu steuern.
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


