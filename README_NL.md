[![Join us on Slack chat room](https://img.shields.io/badge/Slack-Join%20the%20chat%20room-orange)](https://join.slack.com/t/panasonic-wemos/shared_invite/enQtODg2MDY0NjE1OTI3LTgzYjkwMzIwNTAwZTMyYzgwNDQ1Y2QxYjkwODg3NjMyN2MyM2ViMDM3Yjc3OGE3MGRiY2FkYzI4MzZiZDVkNGE)
[![Build binary](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml/badge.svg)](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml)


# Panasonic H, J, K & L Series Aquarea lucht-water warmtepomp protocol

Dit project maakt het mogelijk om informatie van de Panasonic Aquarea warmtepomp uit te lezen en de gegevens te rapporteren aan een MQTT-server of als JSON-formaat via HTTP.

Eine deutschsprachige [README_DE.md](README_DE.md) findest du hier. \
De Engelstalige originele documentatie vind je hier: [README.md](README.md). \
Suomen kielellä [README_FI.md](README_FI.md) luettavissa täällä.

*Hulp bij het vertalen naar andere talen is van harte welkom.*

# Huidige releases
De laatste release is beschikbaar [hier](https://github.com/Egyras/HeishaMon/releases). Het voor ESP8266 gecompileerde binary kan worden geïnstalleerd op een Wemos D1 mini, op het HeishaMon PCB en in het algemeen op elk ESP8266-gebaseerd board dat compatibel is met de Wemos build-instellingen (minimaal 4 MB flash). Je kunt de code ook downloaden en zelf compileren (zie vereiste bibliotheken hieronder). Het ESP32-S3 binary is voor de nieuwere, grote versie van heishamon.


# Gebruik van de software
HeishaMon kan communiceren met de Panasonic Aquarea H, J, K en L-serie. [Door gebruikers bevestigde typen warmtepompen vind je hier](HeatPumpType.md) \
Als je dit image zelf wilt compileren, zorg er dan voor dat je de genoemde bibliotheken gebruikt en ondersteuning voor een bestandssysteem op de ESP8266, selecteer daarvoor de juiste flash-optie in de Arduino IDE.

Bij het opstarten zonder geconfigureerd wifi wordt een open-wifi-hotspot zichtbaar waarmee je je wifi-netwerk en je MQTT-server kunt configureren. De configuratiepagina is beschikbaar op http://192.168.4.1 . \

Na het configureren en opstarten kan het image gegevens lezen van en communiceren met je warmtepomp. De GPIO13/GPIO15-verbinding wordt gebruikt voor communicatie, zodat je je computer/uploader aangesloten kunt laten als je dat wilt. \
Serial 1 (GPIO2) kan worden gebruikt om een andere seriële verbinding (alleen GND en TX van het board) te verbinden om wat debug-gegevens uit te lezen.

Alle ontvangen gegevens worden naar verschillende MQTT-topics gestuurd (zie hieronder voor topic-beschrijvingen). Er is ook een MQTT-topic 'panasonic_heat_pump/log' dat debug-logging en een hexdump van de ontvangen pakketten biedt (indien ingeschakeld in het webportaal).

Je kunt een 1-wire netwerk aansluiten op GPIO4 dat rapporteert in aparte MQTT-topics (panasonic_heat_pump/1wire/sensorid).

De software kan ook Watt meten op een S0-poort van twee kWh-meters. Je hoeft alleen GPIO12 en GND te verbinden met de S0 van één kWh-meter en als je een tweede kWh-meter nodig hebt, gebruik dan GPIO14 en GND. Het wordt gerapporteerd op MQTT-topic panasonic_heat_pump/s0/Watt/1 en panasonic_heat_pump/s0/Watt/2 en ook in de JSON-uitvoer. Je kunt 'Watt' in het vorige topic vervangen door 'Watthour' om de verbruiksteller in WattHour (per mqtt bericht) te krijgen, of door 'WatthourTotal' om het totale verbruik in WattHour te krijgen. Om de WatthourTotal te synchroniseren met je kWh-meter, publiceer de juiste waarde naar MQTT naar het topic panasonic_heat_pump/s0/WatthourTotal/1 of panasonic_heat_pump/s0/WatthourTotal/2 met de 'retain'-optie terwijl heishamon herstart.

Firmware updaten is zo eenvoudig als naar het firmware-menu gaan en na authenticatie met gebruikersnaam 'admin' en wachtwoord 'heisha' (of een ander wachtwoord dat tijdens de setup is opgegeven), de binary daar uploaden.

Een JSON-uitvoer van alle ontvangen gegevens (warmtepomp en 1-wire) is beschikbaar op de URL http://heishamon.local/json (vervang heishamon.local door het IP-adres van je heishamon-apparaat als MDNS niet werkt voor jou).

In de map 'integrations' vind je voorbeelden hoe je je automatiseringsplatform kunt koppelen aan de HeishaMon.

# Regels-functionaliteit
De regels-functionaliteit stelt je in staat om de warmtepomp te bedienen vanuit de HeishaMon zelf. Dit maakt het veel betrouwbaarder dan het omgaan met externe domotica via WiFi. Bij het plaatsen van een nieuwe regelset wordt deze direct gevalideerd en bij geldigheid gebruikt. Als een nieuwe regelset ongeldig is, wordt deze genegeerd en de oude regelset opnieuw geladen. Je kunt de console controleren voor feedback hierover. Als een nieuw geldige regelset de HeishaMon toch laat crashen, wordt deze automatisch uitgeschakeld bij de volgende herstart, zodat je wijzigingen kunt aanbrengen. Dit voorkomt dat de HeishaMon in een boot-lus terechtkomt.

De technieken die in de regelbibliotheek worden gebruikt, stellen je in staat om te werken met zeer grote regelsets, maar de beste praktijk is om ze onder de 10.000 bytes te houden.

Let op dat het sturen van opdrachten naar de warmtepomp asynchroon gebeurt. Opdrachten die aan het begin van je syntax naar de warmtepomp worden gestuurd, worden niet onmiddellijk weergegeven in de waarden van de warmtepomp later. Daarom moeten warmtepomp-waarden worden gelezen van de warmtepomp zelf in plaats van die op basis van waarden die je zelf bijhoudt.

## Syntax
Twee algemene regels: spaties zijn verplicht en puntkomma's worden gebruikt als einde-van-regel teken.

### Variabelen
De regelset gebruikt de volgende variabele structuur:

- `#`: Globalen
Deze variabelen zijn toegankelijk in de hele regelset, maar moeten worden gedefinieerd binnen een regelblok. Gebruik geen globalen voor al je variabelen, want dit gebruikt continu geheugen.

- `$`: Lokalen
Deze variabelen leven binnen een regelblok. Als een regelblok eindigt, worden deze variabelen opgeruimd en het gebruikte geheugen vrijgemaakt.

- `@`: Warmtepomp-parameters
Deze zijn dezelfde als vermeld op de documentatiepagina Manage Topics en op de HeishaMon-startpagina. De regelset volgt ook de R/W-logica zoals gebruikt via de MQTT en REST API. Dit betekent dat de leestopics verschillen van de schrijftopics. Het lezen van de warmtepompstatus gaat via `@Heatpump_State`, het wijzigen van de status via `@SetHeatpump`.

- `%`: Datum- en tijdvariabelen
Deze kunnen worden gebruikt voor datum- en tijdgebaseerde regels. Momenteel worden `%hour` (0-23), `%minute` (0-59), `%month` (1-12) en `day` (1-7) ondersteund. Alle zijn gewone integers. Een correcte NTP-configuratie is nodig om de juiste systeemdatum en -tijd in te stellen op de HeishaMon.

- `?`: Thermostaatparameters
Deze variabelen weerspiegelen parameters die worden gelezen van de aangesloten thermostaat bij gebruik van de OpenTherm-functionaliteit. De namen zijn hetzelfde voor lezen en schrijven, maar niet alle waarden ondersteunen lezen en/of schrijven.

- `ds18b20#2800000000000000`: Dallas 1-wire temperatuurwaarden
Gebruik deze variabelen om de temperatuur van de aangesloten sensoren te lezen. Deze waarden zijn uiteraard alleen-lezen. Het ID van de sensor moet na het hekje worden geplaatst.

- `s0#watt[hour[total]]_?`: De S0-waarden voor poort 1 en 2. Vervang het vraagteken door het poortnummer.
Bijvoorbeeld s0#watt_1 voor watt van poort 1 en s0#watthourtotal_2 voor totale watturren van poort 2.


Als een variabele wordt aangeroepen maar nog geen waarde heeft, is de waarde `NULL`.

Variabelen kunnen van het type Boolean (`1` of `0`), float (`3.14`), integer (`10`) en string zijn. Strings worden gedefinieerd met enkelvoudige of dubbele aanhalingstekens.

### Gebeurtenissen of functies
Regels worden geschreven in `event`- of `function`-blokken. Dit zijn blokken die worden geactiveerd wanneer er iets is gebeurd; ofwel een nieuwe warmtepomp- of thermostaatwaarde is ontvangen of een timer is afgegaan. Of kunnen worden gebruikt als gewone functies.

```
on [event] then
  [...]
end

on [naam] then
  [...]
end
```

Gebeurtenissen kunnen Warmtepomp- of thermostaatparameters of timers zijn:
```
on @Heatpump_State then
  [...]
end

on ?setpoint then
  [...]
end

on timer=1 then
  [...]
end
```

Bij het definiëren van functies geef je je blok gewoon een naam en kun je het van overal aanroepen:
```
on foobar then
  [...]
end

on @Heatpump_State then
  foobar();
end
```

Functies kunnen parameters hebben die je kunt aanroepen:
```
on foobar($a, $b, $c) then
  [...]

on @Heatpump_State then
  foobar(1, 2, 3);
end
```

Als je een functie aanroept met minder waarden dan de functie verwacht, hebben alle andere parameters een NULL-waarde.

Er is momenteel één speciale functie die wordt aangeroepen bij het opstarten van het systeem of bij het opslaan van een nieuwe regelset:
```
on System#Boot then
  [...]
end
```

Deze speciale functie kan worden gebruikt om je globalen of bepaalde timers initieel in te stellen.

### Operatoren
Reguliere operatoren worden ondersteund met hun standaard associativiteit en prioriteit. Dit stelt je ook in staat om reguliere wiskunde te gebruiken.
- `&&`: En
- `||`: Of
- `==`: Gelijk aan
- `!=`: Niet Gelijk aan
- `>=`: Groter dan of gelijk aan
- `>`: Groter dan
- `<`: Kleiner dan
- `<=`: Kleiner dan of gelijk aan
- `-`: Min
- `%`: Modulo
- `*`: Vermenigvuldigen
- `/`: Delen
- `+`: Plus
- `^`: Macht

Haakjes kunnen worden gebruikt om operatoren te prioriteren zoals in de reguliere wiskunde.

### Functies
- `coalesce`
Geeft de eerste waarde terug die niet `NULL` is. Bijv. `$b = NULL; $a = coalesce($b, 1);` geeft 1 terug. Deze functie accepteert een onbeperkt aantal argumenten.

- `max`
Geeft de maximale waarde van de invoerparameters terug.

- `min`
Geeft de minimale waarde van de invoerparameters terug.

- `isset`
Geeft boolean true terug wanneer de invoervariabele nog `NULL` is, in alle andere gevallen false.

- `round`
Rondt de invoer-float af op het dichtstbijzijnde gehele getal.

- `floor`
De grootste gehele getal waarde die kleiner dan of gelijk aan de invoer-float is.

- `ceil`
De kleinste gehele getal waarde die groter dan of gelijk aan de invoer-float is.

- `setTimer`
Stelt een timer in die na X seconden afgaat. De eerste parameter is het timernummer en de tweede parameter het aantal seconden voordat hij afgaat. Een timer gaat slechts één keer af en moet opnieuw worden ingesteld voor terugkerende gebeurtenissen.

- `print`
Drukt een waarde af op de console.

- `concat`
Voegt verschillende waarden samen tot een gecombineerde string. Bijv.: `@SetCurves = concat('{zone1:{heat:{target:{high:', @Z1_Heat_Curve_Target_High_Temp, ',low:32}}}}');`

- `gpio`
Maakt het instellen of ophalen van een GPIO-toestand mogelijk. Bij aanroep met één argument wordt de GPIO-toestand geretourneerd. Bij aanroep met twee argumenten wordt de toestand van een GPIO ingesteld. Deze functie stelt alleen digitale pinnen in, zodat de toestand slechts 0 of 1 kan zijn. De twee relais op de grote heishamon zijn gpio21 en gpio47. De gebruikers-configureerbare extra GPIO's (zie hieronder) kunnen ook worden aangesproken via hun pinnummer. Zie het voorbeeld om de relais elke twee seconden om te schakelen.

```
on System#Boot then
   setTimer(10, 2);
end

on timer=10 then
   setTimer(20, 2);
   gpio(21,0);
   gpio(47,1);
end

on timer=20 then
   setTimer(10, 2);
   gpio(21,1);
   gpio(47,0);
end
```

### Condities
De enige ondersteunde condities zijn `if`, `else` en `elseif`:

```
if [conditie] then
  [...]
else
  if [conditie] then
    [...]
  end
end
```

```
if [conditie] then
  [...]
elseif [conditie] then
  if [conditie] then
    [...]
  else
    [...]
  end
elseif [conditie] then
  [...]
else
  [...]
end
```

### Voorbeelden

*WAR berekenen (Weerafhankelijke regeling)*
```
on calcWar($Ta1, $Tb1, $Ta2, $Tb2) then
	#maxTa = $Ta1;

	if @Outside_Temp >= $Tb1 then
		#maxTa = $Ta1;
	elseif @Outside_Temp <= $Tb2 then
		#maxTa = $Ta2;
	else
		#maxTa = $Ta1 + (($Tb1 - @Outside_Temp) * ($Ta2 - $Ta1) / ($Tb1 - $Tb2));
	end
end
```

*Thermostaatinstelpunt*
```
on ?roomTemp then
	calcWar(32, 14, 41, -4);

	$margin = 0.25;

	if ?roomTemp > (?roomTempSet + $margin) then
		if @Heatpump_State == 1 then
			@SetHeatpump = 0;
		end
	elseif ?roomTemp < (?roomTempSet - $margin) then
		if @Heatpump_State == 0 then
			@SetHeatpump = 1;
		end
	else
		@SetZ1HeatRequestTemperature = round(#maxTa);
	end
end
```

# Fabrieksreset
Een fabrieksreset kan worden uitgevoerd via de webinterface, maar als de webinterface niet beschikbaar is, kun je een dubbele reset uitvoeren. De dubbele reset moet niet te snel maar ook niet te langzaam worden uitgevoerd. Meestal werkt een halve seconde tussen beide resets. Om aan te geven dat de dubbele reset een fabrieksreset heeft uitgevoerd, knippert de blauwe LED snel (Je moet nu opnieuw op reset drukken om HeishaMon normaal opnieuw op te starten, waarbij een WiFi-hotspot zichtbaar moet zijn).

# Verdere informatie
Hieronder vind je enkele technische details over het project. Hoe je je eigen kabels bouwt. Hoe je je eigen PCB bouwt enz.

## Verbindingsdetails:
Communicatie kan worden tot stand gebracht via een van de twee aansluitingen: CN-CNT of CN-NMODE. Als je een bestaande Panasonic CZ-TAW1 WiFi-interface hebt die je wilt vervangen door HeishaMon, is het slechts een kwestie van de kabel uit de CZ-TAW1 halen en opnieuw verbinden met je HeishaMon-apparaat. Het is echter niet mogelijk om HeishaMon en de originele CZ-TAW1-module samen te gebruiken als actief apparaat. Het is wel mogelijk om HeishaMon in de "Alleen luisteren"-modus te zetten, waardoor HeishaMon en de originele CZ-TAW1-module naast elkaar kunnen bestaan. Het enige nadeel hiervan is dat HeishaMon geen opdrachten kan sturen en de optionele PCB-optie niet kan gebruiken.

Communicatieparameters: TTL 5V UART 9600,8,E,1  \
 \
CN-CNT Pin-indeling (van boven naar beneden) \
1 - +5V (250mA)  \
2 - 0-5V TX (van de warmtepomp) \
3 - 0-5V RX (naar de warmtepomp)\
4 - +12V (250mA) \
5 - GND \
 \
CN-NMODE Pin-indeling (van links naar rechts) \
"Let op! Zoals op de PCB staat afgedrukt, is de linkerpin pin 4 en de rechterpin is pin 1. Tel niet 1 tot 4 van links!  \
4 - +5V (250mA)  \
3 - 0-5V TX (van de warmtepomp) \
2 - 0-5V RX (naar de warmtepomp) \
1 - GND

HeishaMon ontvangt stroom van Panasonic via de kabel (5V stroom).

## Verbinding over lange afstand
Het is mogelijk om de HeishaMon over een lange afstand aan te sluiten. Tot 5 meter werkt met normale bekabeling. Voor grotere afstanden is een TTL-naar-RS485-configuratie zoals weergegeven in de afbeelding hieronder mogelijk. Dit vereist echter dat HeishaMon extern wordt gevoed met 5V stroom (bijvoorbeeld via een USB-kabel).

![TTL-over-RS485 HeishaMon lange afstand verbinding](optional-long-distance-heishamon.png)


## Waar je connectoren kunt kopen
[RS-Online bestellingen](Connectors_RSO.md)

[Conrad bestellingen](Connectors_Conrad.md)

Gebruik een afgeschermde 24 AWG 4-aderige kabel.


## De HeishaMon-hardware zelf
De PCB's die nodig zijn om verbinding te maken met de warmtepomp zijn ontworpen door projectleden en worden hieronder vermeld. Het belangrijkste onderdeel van de hardware is een niveauverschuiving van 5V van Panasonic naar 3,3V van de HeishaMon en een GPIO13/GPIO15-activeringslijnen na het opstarten. \
[PCB-ontwerpen van de projectleden](PCB_Designs.md) \
[Foto Wemos D1 Beta](WEMOSD1.JPG) \
[Foto ESP12-F](NewHeishamon.JPG)

Om het makkelijk te maken kun je een compleet PCB bestellen bij sommige projectleden: \
[Lectronz shop](https://lectronz.com/products/heishamon-communication-pcb) van Igor Ybema (ook bekend als TheHogNL) uit Nederland

## Het Arduino-image zelf bouwen
Boards: \
esp8266 by esp8266 community version 3.0.2 [Arduino](https://github.com/esp8266/Arduino/releases/tag/3.0.2)

Alle [bibliotheken die we gebruiken](LIBSUSED.md) die nodig zijn voor het compileren.


## MQTT-topics
[Huidige lijst van gedocumenteerde MQTT-topics vind je hier](MQTT-Topics.md)

## Communicatiebetrouwbaarheid
Berichten van HeishaMon naar de warmtepomp worden af en toe verworpen, vooral als meerdere instellingen binnen korte tijd worden gewijzigd. Het is niet haalbaar voor HeishaMon om alle verworpen berichten opnieuw te verzenden, dus gebruikers moeten hun eigen herhaallogica implementeren.

De aanbevolen herhaallogica is als volgt:
1. Na het wijzigen van een warmtepomp-instelling via HeishaMon, wacht je tot HeishaMon meldt dat de instelling is gewijzigd.
2. Als de instelling na 10 seconden niet is bijgewerkt, stel je deze opnieuw in.

Dit kan op de HeishaMon zelf worden geïmplementeerd met Regels, zoals in dit voorbeeld:
```
on StopExternalControl then
  if @External_Control != 0 then
    @SetExternalControl = 0;
    settimer(9,10);
  end
end

on timer=9 then
  if @External_Control != 0 then
    @SetExternalControl = 0;
  end
end
```

## EEPROM-waarschuwing
Tot op heden weten we niet hoe de opdrachten die naar de warmtepomp worden gestuurd, worden verwerkt in de warmtepomp zelf. Waarschijnlijk worden veel opdrachten naar EEPROM geschreven om beschikbaar te zijn na een stroomuitval, zoals het instellen van de DHW-temperatuur. Een EEPROM kan veel schrijfbewerkingen aan, maar er is een limiet. En we kennen de limiet ook niet. Zorg er dus voor dat je de warmtepomp niet overlaadt met te veel opdrachten. Elke seconde is veel te frequent. Een paar per uur, per instelling, is waarschijnlijk prima. Bovendien is een warmtepomp een traag verwarmings-(koel-)apparaat, dus zo frequent wijzigingen aanbrengen is waarschijnlijk toch niet zinvol.

## DS18b20 1-wire ondersteuning
De software ondersteunt ook het uitlezen van DS18B20 1-wire temperatuursensoren. Een correcte 1-wire configuratie (met 4,7kOhm pull-up weerstand) aangesloten op GPIO4 wordt elke geconfigureerde seconden (minimaal 5) gelezen en gestuurd naar het topic panasonic_heat_pump/1wire/"sensor-hex-adres". Op de kant-en-klare boards is deze 4,7kOhm weerstand al geïnstalleerd.

## Relaisbesturing op het grote board
De nieuwere, grote, heishamon bevat twee ingebouwde relais die kunnen worden in- en uitgeschakeld via MQTT-opdrachten. De relais kunnen worden gebruikt voor elke contactschakelaar, zelfs 230V netspanning (max. 5A). Om het relais te bedienen, stuur gewoon een waarde van 1 of 0 naar het MQTT-topic "panasonic_heat_pump/gpio/relay/one" voor relais één of "panasonic_heat_pump/gpio/relay/two" voor relais twee.

## Gebruikers-configureerbare extra GPIO's
Naast de vaste relaispennen stelt HeishaMon een aantal gebruikers-configureerbare GPIO-pinnen beschikbaar die elk onafhankelijk kunnen worden ingesteld op Ingang (pull-up), Ingang of Uitgang via het instellingen-webmenu.

**Beschikbare pinnen per platform:**
- **ESP8266:** 3 extra GPIO's (pinnen 1, 3, 16)
- **ESP32:** 5 extra GPIO's (pinnen 33–37); relaispennen 21 en 47 blijven vast

De modus voor elke pin wordt opgeslagen in de instellingen en toegepast bij het opstarten en wanneer instellingen worden opgeslagen.

**Uitgangs-GPIO's bedienen via MQTT:**
Stuur `1`, `0`, `on`, `off`, `true` of `false` naar:
```
panasonic_heat_pump/gpio/extra/N
```
waarbij `N` de 1-gebaseerde index van de extra GPIO is (bijv. `extra/1` voor de eerste extra pin).

**GPIO-toestanden lezen via MQTT:**
Ingangs- en uitgangstoestanden worden automatisch gepubliceerd naar dezelfde topics bij wijziging en bij de eerste MQTT-verbinding (retained).

**HTTP API:**
Je kunt alle extra GPIO-toestanden als JSON lezen via een GET-verzoek:
```
GET /gpio
```
Voorbeeldantwoord:
```json
[{"pin":33,"mode":2,"state":1},{"pin":34,"mode":0,"state":0}, ...]
```
waarbij `mode` 0 = Ingang (pull-up), 1 = Ingang, 2 = Uitgang is.

Een uitgangspin instellen via HTTP POST:
```
POST /gpio?pin=N&value=1
```

**Extra GPIO's gebruiken in regels:**
De `gpio()`-regelfunctie werkt met de extra GPIO-pinnen via hun fysieke pinnummer, hetzelfde als de relaispennen.

## OpenTherm-ondersteuning
Als je heishamon-board OpenTherm ondersteunt, kan de software ook worden gebruikt om OpenTherm-informatie van een compatibele thermostaat via MQTT of JSON door te sturen naar je thuisautomatisering. Als je OpenTherm-ondersteuning inschakelt in de instellingen, verschijnt er een nieuw tabblad op de webpagina met OpenTherm-waarden. Sommige zijn van het type R(ead) en sommige zijn W(rite), en sommige zijn beide. Read betekent dat de thermostaat die informatie van de heishamon kan lezen. Je verstrekt die informatie via MQTT (of via de regels) door deze waarde bij te werken op het MQTT-topic 'opentherm/read'. De Write-waarden zijn informatie van de thermostaat, zoals 'roomTemp'. Deze zijn beschikbaar op het MQTT-topic 'opentherm/write'.

De beschikbare OpenTherm-variabelen zijn:
### WRITE-waarden
- chEnable: Boolean die aangeeft of centrale verwarming ingeschakeld moet worden
- dhwEnable: Boolean die aangeeft of DHW-verwarming ingeschakeld moet worden
- coolingEnable: Boolean die aangeeft of koeling ingeschakeld moet worden
- roomTemp: Float-waarde van de gemeten kamertemperatuur door de thermostaat
- roomTempSet: Float-waarde van het gevraagde kamertemperatuur-instelpunt op de thermostaat
- chSetpoint: Float-waarde van het berekende water-instelpunt door de thermostaat
- maxRelativeModulation: Modulatiehoeveelheid (0-100%) die de warmtepomp mag gebruiken
- coolingControl: Koelhoeveelheid (0-100%) die de thermostaat van de warmtepomp vraagt
### READ AND WRITE-waarden
- dhwSetpoint: Float-waarde van het huidige DHW-instelpunt
- maxTSet: Float-waarde die het maximale water-instelpunt definieert
### READ-waarden
- chPressure: Float-waarde van de gemeten waterdruk
- outsideTemp: Float-waarde van de gemeten buitentemperatuur
- inletTemp: Float-waarde van de gemeten waterinlaattemperatuur
- outletTemp: Float-waarde van de gemeten wateruitlaattemperatuur
- dhwTemp: Float-waarde van de gemeten DHW-temperatuur
- relativeModulation: Huidige modulatiehoeveelheid (0-100%)
- flameState: Boolean die aangeeft of de centrale verwarming warmte levert
- chState: Boolean die aangeeft of de warmtepomp in ruimte-/centrale verwarmingsmodus is
- dhwState: Boolean die aangeeft of de warmtepomp in DHW-modus is
- coolingState: Boolean die aangeeft of de warmtepomp in koel-modus is
- dhwSetUppBound: Integer (0-127), maximale DHW-temperatuur. Standaard: 75
- dhwSetLowBound: Integer (0-127), minimale DHW-temperatuur. Standaard: 40
- chSetUppBound: Integer (0-127), maximale CH-watertemperatuur. Standaard: 65
- chSetLowBound: Integer (0-127), minimale CH-watertemperatuur. Standaard: 20

## Protocol byte decrypt info:
[Huidige lijst van gedocumenteerde gedecodeerde bytes vind je hier](ProtocolByteDecrypt.md)

## MQTT TLS-ondersteuning:
TLS-ondersteuning voor MQTT is momenteel alleen geïmplementeerd voor de ESP32-versie. TLS is standaard uitgeschakeld en kan worden ingeschakeld in de instellingen. Zorg er bij het inschakelen voor dat je MQTT-broker beveiligde verbindingen van heishamon accepteert, inclusief het instellen van een PEM CA-certificaat (bijv. zelfondertekend). Upload een tekstbestand (plain text met suffix pem, bijv. "CA.pem") van het certificaat naar heishamon in het instellingenscherm. Na het uploaden van het certificaat, activeer TLS-ondersteuning en schakel (in normale setups) de MQTT-poort over naar 8883. !!Er is geen ondersteuning voor TLS-problemen!! Google kan je helpen bij problemen.

## Integratie-voorbeelden voor Open Source-automatiseringssystemen
[Openhab2](Integrations/Openhab2)

[Home Assistant](Integrations/Home%20Assistant)

[IOBroker Handleiding](Integrations/ioBroker_manual)

[Domoticz](Integrations/Domoticz)
