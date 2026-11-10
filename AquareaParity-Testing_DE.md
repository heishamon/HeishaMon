# Die Aquarea-Parity-Testfirmware an der eigenen Wärmepumpe testen

Diese Anleitung richtet sich an HeishaMon-Besitzer, die mithelfen wollen herauszufinden, ob die neuen Topics (TOP170 bis TOP196) und Befehle (SET56 bis SET104) auf ihrer Baureihe funktionieren. Hintergrund und Belege: [AquareaParity.md](AquareaParity.md). Referenz der Topics und Befehle: [MQTT-Topics.md](MQTT-Topics.md). Englische Fassung: [AquareaParity-Testing.md](AquareaParity-Testing.md).

## Was bisher bekannt ist

- Alles wurde an **einem Gerät der M-Serie** erarbeitet (WH-SDC0316M9E8 mit WH-WXG09ME8). Über die Reihen H, J, K und L ist nichts bekannt.
- Die Testfirmware läuft auf einem großen HeishaMon (ESP32-S3) an diesem Gerät: Sie startet, behält alle bisherigen Topics, und die 27 neuen Topics zeigen die Werte des Installateur-Menüs.
- An diesem Gerät wurden die neuen **Befehle** über diese Firmware gesendet und zurückgelesen (Wärmepumpe aus): Die Werte kamen an, Nachbareinstellungen im selben Byte blieben erhalten, Befehle, die abgelehnt werden müssen, wurden abgelehnt, und alles wurde zurückgestellt. Bewusst ausgelassen: `SetZoneType`, `SetAntiFreezeMode 0`, `SetDefrostSignal 1`, `SetZ1SensorSettings 3`.
- Ob es eine Einstellung auf einer anderen Baureihe gibt, ist nicht bekannt. Sie kann dort durchaus funktionieren; es hat nur noch niemand ausprobiert.
- Das ist Testfirmware. Die Nutzung geschieht auf eigenes Risiko. Nicht an einer Anlage testen, an die man im Notfall nicht herankommt.

## Was neu ist

**27 neue Lese-Topics, 26 davon mit einem neuen Befehl.** Sie zeigen Einstellungen des Installateur-Menüs, die HeishaMon bisher nicht lesen konnte.

| Topic | Name | Bedeutung | Neuer Befehl |
|---|---|---|---|
| TOP170 | Static_Pressure | Option „Statischer Druck“ | SetStaticPressure |
| TOP171 | Heating_On_Outdoor_Temp | Außentemperatur, unter der die Heizung einschaltet | SetHeatingOnOutdoorTemp |
| TOP172 | Heating_On_Delay | Verzögerung, bevor die Heizung einschaltet | SetHeatingOnDelay |
| TOP173 | DHW_Booster_Delay | Einschaltverzögerung eines externen Speicher-Heizstabs | SetDHWBoosterDelay |
| TOP174 bis TOP176 | Efficiency_Stage1/2/3_Time | Stufenzeiten der Heizsteuerung „Effizienz“ | SetEfficiencyStage1Time, …2Time, …3Time |
| TOP177, TOP178 | Sensor_Offset_Remote_Th1, Sensor_Offset_Outdoor | Fühlerkalibrierung | SetSensorOffsetRemoteTh1, SetSensorOffsetOutdoor |
| TOP193, TOP195, TOP196 | Sensor_Offset_Return, Sensor_Offset_DHW_Tank, Sensor_Offset_Flow | Fühlerkalibrierung | SetSensorOffsetReturn, SetSensorOffsetDhwTank, SetSensorOffsetFlow |
| TOP179 | Anti_Stick_Mode | Antihaft-Modus (Aktoren bewegen) | keiner: die Wärmepumpe ignoriert Schreibversuche |
| TOP180, TOP181 | Bivalent_Operating_Mode, Bivalent_External_Pump | Unterpunkte der bivalenten Heizung | SetBivalentOperatingMode, SetBivalentExternalPump |
| TOP182, TOP183 | Extra_Pump_Use, Extra_Pump_Mode | Zusätzliche Pumpe: nicht benutzt / Heizen / Warmwasser, Komfort / Effizienz | SetExtraPump, SetExtraPumpMode |
| TOP184 bis TOP190 | SG_Ready_1/2_DHW/Heat/Cool_Power, SG_Ready_Stop_Power | SG-ready-Leistungsgrenzen (kW) | SetSGReadyDHWPower1, …HeatPower1, …CoolPower1, …DHWPower2, …HeatPower2, …CoolPower2, SetSGReadyStopPower |
| TOP191, TOP192 | SG_Ready_1/2_Cool_Capacity | SG-ready-Kühl-Offsets | SetSGReadyCoolCapacity1, …2 |
| TOP194 | Defrost_Signal | Abtausignal-Ausgang (teilt sich eine Klemme mit der bivalenten Heizung) | SetDefrostSignal |

**22 vorhandene Topics, die bisher nur lesbar waren und jetzt auch geschrieben werden können.**

| Topic | Name | Neuer Befehl |
|---|---|---|
| TOP45 | Room_Holiday_Shift_Temp | SetRoomHolidayShiftTemp |
| TOP76, TOP81 | Heating_Mode, Cooling_Mode (Heiz-/Kühlkurve oder Festwert) | SetHeatingWaterMode, SetCoolingWaterMode |
| TOP100 | DHW_Installed | SetDHWInstalled |
| TOP101 bis TOP105 | Solar_Mode, Solar_On_Delta, Solar_Off_Delta, Solar_Frost_Protection, Solar_High_Limit | SetSolarMode, SetSolarOnDelta, SetSolarOffDelta, SetSolarFrostProtection, SetSolarHighLimit |
| TOP107 | Liquid_Type (Wasser / Glykol) | SetLiquidType |
| TOP109 | Anti_Freeze_Mode | SetAntiFreezeMode |
| TOP111 | Z1_Sensor_Settings | SetZ1SensorSettings |
| TOP132, TOP136, TOP137 | Bivalent_Advanced_Heat, …Start_Delay, …Stop_Delay | SetBivalentAPHeat, SetBivalentAPStartDelay, SetBivalentAPStopDelay |
| TOP146 | Zone_Configuration (Heizkreis 1 Raum / Schwimmbad) | SetZoneType |
| TOP147 | Internal_Heater_Capacity (3 / 6 / 9 kW) | SetInternalHeaterCapacity |
| TOP148 | DHW_Heater_Type | SetDHWHeaterType |
| TOP149 | SG_Ready_Control | SetSGReadyMode |
| TOP150 | Demand_Control | SetDemandControlEnable |
| TOP151 | DHW_Defrost | SetDHWDefrost |
| TOP153 | Pool_Delta | SetPoolDelta |

**Weboberfläche.** Die neuen Topics erscheinen in der Tabelle „Heatpump Values“ und in `/json`.

**Unverändert.** Alle bisherigen Topics behalten Nummer und Namen; bisherige Befehle senden dieselben Bytes wie vorher. Neue Befehle auf Bytes mit mehreren Einstellungen senden nur ihr eigenes Feld, wie die bisherigen.

## Was man braucht

- Einen HeishaMon, an den man im Notfall mit einem USB-C-Kabel herankommt (großes Board), oder den seriellen Adapter vom ersten Flashen (kleines Board).
- Die Firmware-Datei, die gerade läuft, für den Rückweg. Veröffentlichte Dateien liegen in [binaries/](binaries/).
- 30 Minuten für den Lesetest. Für den Schreibtest weitere 30 Minuten bei ausgeschalteter Wärmepumpe.
- Zugang zum Installateur-Menü des Bedienteils (zum Vergleichen) oder zur Aquarea Service Cloud.

## Schritt 1: Ausgangszustand festhalten

1. Die HeishaMon-Webseite öffnen und die oben angezeigte Firmware-Version notieren.
2. `http://<heishamon>/json` öffnen und die Seite speichern. Das ist die Aufzeichnung aller Werte vor dem Test.
3. Die Einstellungsseite abfotografieren. Einstellungen und Regeln bleiben bei einem Update erhalten, es gibt aber keinen Export.

## Schritt 2: Flashen

1. Eine Testfassung dieser Änderung herunterladen. Wo sie zu finden ist, steht im Pull Request, der diese Topics hinzufügt (in den Pull Requests von HeishaMon nach „aquarea parity“ suchen).
2. Die Datei für das eigene Board wählen: `HeishaMon_ESP32-…bin` für das große, `HeishaMon_ESP8266-…bin` für das kleine Board.
3. Auf der HeishaMon-Webseite **Firmware** öffnen, die Datei auswählen und prüfen, dass das MD5-Feld gefüllt ist (es wird aus dem Dateinamen übernommen; ist es leer, die Zeichenfolge vor `.bin` aus dem Dateinamen eintragen).
4. Das Update starten und die Erfolgsmeldung und den Neustart abwarten (etwa 20 Sekunden ohne Werte).

Schlägt der Upload fehl oder stimmt die MD5 nicht, bleibt die alte Firmware erhalten. Es gibt **keinen automatischen Rückfall**, wenn die neue Firmware startet und sich dann falsch verhält: dann die vorherige Datei über dieselbe Seite flashen, oder per USB, falls die Seite nicht erreichbar ist.

## Schritt 3: Prüfen, dass alles läuft wie vorher (ohne Befehle)

30 Minuten beobachten:

| Prüfung | Erwartung |
|---|---|
| Version oben auf der Webseite | der Name der Testfassung |
| Freier Speicher und „Correct data“ auf der Webseite | etwa wie vor dem Update |
| MQTT-Werte | kommen an wie vorher |
| Uptime | zählt durch, keine Neustarts |
| CZ-TAW1 am Proxy-Port (falls genutzt) | die Cloud arbeitet weiter |

Stimmt etwas davon nicht: zurück zur vorherigen Firmware und melden (Schritt 6).

## Schritt 4: Lesetest (auf jeder Baureihe ungefährlich)

Die Webseite (Tabelle „Heatpump Values“) oder `/json` öffnen und die neuen Zeilen aus der Tabelle „Was neu ist“ mit dem Installateur-Menü des Bedienteils vergleichen. In diesem Schritt wird nichts geschrieben.

Zu jeder Zeile eines notieren: **stimmt**, **weicht ab** (beide Werte aufschreiben), **Menüpunkt gibt es an meinem Gerät nicht**. Ein Wert, der sich nie ändert und keinen Sinn ergibt (zum Beispiel 0 Minuten, -128 °C), bedeutet meist, dass das Byte auf dieser Baureihe nicht benutzt wird. Auch das ist ein nützliches Ergebnis.

Wer kann, ändert eine dieser Einstellungen **am Bedienteil** und prüft, ob das Topic folgt. Das belegt die Byte-Position für die eigene Baureihe, ohne dass HeishaMon etwas sendet.

## Schritt 5: Schreibtest (freiwillig, Wärmepumpe ausgeschaltet)

Nur für Einstellungen, bei denen Schritt 4 gestimmt hat. Vorher die Wärmepumpe ausschalten (warten, bis der Verdichter von selbst steht oder mindestens 20 Minuten gelaufen ist).

Einen Befehl senden: MQTT-Topic `<basis>/commands/<Name>` mit dem Wert als Payload, oder `http://<heishamon>/command?<Name>=<Wert>` im Browser. Das Log auf der Seite **Console** zeigt, ob der Befehl gesendet oder abgelehnt wurde.

Für jeden Test: Testwert senden, 40 Sekunden warten, Topic und Bedienteil prüfen, alten Wert wieder senden, 40 Sekunden warten, erneut prüfen.

Vorschläge für die ersten Tests (jeder ändert eine harmlose Einstellung; zum Zurückstellen den eigenen bisherigen Wert verwenden):

| Befehl | Testwert | Zu beobachten |
|---|---|---|
| `SetHeatingOnDelay` | eigener Wert + 30 (Vielfache von 30 Minuten) | Heating_On_Delay |
| `SetEfficiencyStage1Time` | eigener Wert + 5 (Vielfache von 5 Minuten) | Efficiency_Stage1_Time; Efficiency_Stage2_Time muss gleich bleiben |
| `SetSensorOffsetOutdoor` | eigener Wert + 0.5 | Sensor_Offset_Outdoor; Sensor_Offset_Remote_Th1 muss gleich bleiben |
| `SetStaticPressure` | der andere Wert (0 / 1) | Static_Pressure; Extra_Pump_Use muss gleich bleiben |

Regeln für den Schreibtest:

- Immer nur ein Befehl. Vor dem nächsten Befehl das nächste Lesebild abwarten.
- Nach jedem Test `/json` mit der Kopie aus Schritt 1 vergleichen. Nur der getestete Wert darf abweichen.
- Bei der ersten unerwarteten Änderung oder einem Fehlercode aufhören, den alten Wert am Bedienteil wiederherstellen und melden.

Diese Befehle **nicht** senden, außer man weiß, was sie an der eigenen Anlage bewirken:

- `SetBuffer 1` ohne Pufferfühler (Fehler H43 an der M-Serie, bleibt bis zum Zurücksetzen).
- `SetSolarMode 1` oder `2` ohne Solarfühler (Fehler H28 an der M-Serie bei laufender Wärmepumpe; sie schaltet ab und bleibt bis zum Zurücksetzen aus).
- `SetAntiFreezeMode 0`.
- `SetInternalHeaterCapacity` mit einem Wert über dem verbauten Heizstab (nicht getestet, Ergebnis unbekannt).
- Schalter für Hardware, die es an der Anlage nicht gibt: `SetDHWInstalled`, `SetSolarMode`, `SetSGReadyMode`, `SetBivalentControl`, `SetZoneType`, `SetZ1SensorSettings`.
- `SetHeatingWaterMode` / `SetCoolingWaterMode` (sie verstellen die Heiz- und Kühlkurven).
- `SetDefrostSignal` zusammen mit der bivalenten Heizung (die Wärmepumpe schaltet eines von beiden ab).

## Schritt 6: Melden

Bitte im Pull Request melden oder dort, wo die Testfassung herkommt, auch wenn alles funktioniert:

1. Modell der Wärmepumpe (Innen- und Außengerät) und Baureihe, HeishaMon-Board (klein / groß), Firmware-Version vorher und die getestete Version.
2. Ergebnis von Schritt 3 (läuft wie vorher: ja / nein, was abwich).
3. Die Tabelle aus Schritt 4 mit stimmt / weicht ab / nicht vorhanden.
4. Zu Schritt 5: Befehl, gesendeter Wert, was Topic und Bedienteil zeigten, und die Logzeilen der Console.
5. Welche Heizstab-Leistungen das Installateurmenü des Bedienteils anbietet (zum Beispiel nur 3 kW oder 3 / 6 / 9 kW) und was Internal_Heater_Capacity anzeigt. Lesen genügt; keinen höheren Wert schreiben.
6. Falls die Anlage einen Puffer-, Solar-, Pool- oder Warmwasserfühler hat: die Werte von Buffer_Temp, Solar_Temp, Pool_Temp und DHW_Temp aus `/json` und ob die zugehörige Funktion (Buffer_Installed, Solar_Mode, DHW_Installed) ein- oder ausgeschaltet ist. Lesen genügt. An der M-Serien-Testanlage ohne diese Fühler stehen dort -78 °C (Warmwasser: -128 °C); gesucht ist, was ein angeschlossener Fühler zeigt, auch bei ausgeschalteter Funktion.
7. Die Ausgabe von `/json` vorher und nachher, wer sie teilen möchte.

## Zurück zur alten Firmware

Die vorherige Firmware-Datei über die Seite **Firmware** flashen. Die Einstellungen bleiben. Ist die Webseite nicht erreichbar, per USB flashen; ein Werksreset geht über die BOOT-Taste (länger als 10 Sekunden halten, großes Board), danach erscheint der Hotspot `HeishaMon-Setup`.
