[![Join us on Slack chat room](https://img.shields.io/badge/Slack-Join%20the%20chat%20room-orange)](https://join.slack.com/t/panasonic-wemos/shared_invite/enQtODg2MDY0NjE1OTI3LTgzYjkwMzIwNTAwZTMyYzgwNDQ1Y2QxYjkwODg3NjMyN2MyM2ViMDM3Yjc3OGE3MGRiY2FkYzI4MzZiZDVkNGE)
[![Build binary](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml/badge.svg)](https://github.com/the78mole/HeishaMon/actions/workflows/main.yml)


# Panasonic Aquarea H-, J-, K- ja L-sarjan vesi-ilmalämpöpumpun protokollaa lukeva IoT-laite

Tämä projekti mahdollistaa Panasonic Aquarea vesi-ilmalämpöpumppujen toimittamien tietojen raportoimisen MQTT-palvelimelle tai JSON-muodossa HTTP:n kautta.

Ajantasainen, englanninkielinen [README.md](README.md) löytyy täältä. \
Eine deutschsprachige [README_DE.md](README_DE.md) findest du hier. \
Een nederlandse vertaling [README_NL.md](README_NL.md) vind je hier.

*Apua käännöksiin muille kielille otetaan mielellään vastaan.*

# Julkaistu versio
Viimeisin julkaistu versio on saatavilla [täältä](https://github.com/Egyras/HeishaMon/releases). Se voidaan asentaa Wemos D1 minille, Heishamon PCB:lle tai luultavasti mille tahansa muulle ESP8266-pohjaiselle kehitysalustalle, joka on yhteensopiva Wemos-build-asetusten kanssa (vähintään 4 Mt flash-muistia). Voit myös ladata lähdekoodin ja kääntää omat binäärit (katso tarvittavat kirjastot alta). ESP32-S3-binääri on tarkoitettu uudelle, hieman suuremmalle Heishamon PCB:lle.


# Käyttö
Nykyinen Arduino-softa pystyy kommunikoimaan Panasonicin Aquarea H, J, K ja L-sarjan vesi-ilmalämpöpumppujen kanssa. [Käyttäjien vahvistamia lämpöpumppumalleja löytyy täältä](HeatPumpType.md) \
Jos haluat kääntää softan itse, niin lataa kaikki tarvittavat kirjastot ja muista myös filesystem-tuki ESP8266:lle Arduino IDE:ssä.

Kun ensimmäisen kerran kytket virrat laitteeseen, kytkeydy HeishaMonin tarjoamaan avoimeen WiFi-hotspotiin, jota käytetään ensimmäisen käynnistyksen yhteydessä asetusten tekemiseen. Asetussivu löytyy osoitteesta http://192.168.4.1 ja sieltä voit määrittää WiFi-verkon ja MQTT-palvelimen asetukset. \

Kun olet tehnyt asetukset, laite käynnistyy uudestaan ja liittyy määrittelemääsi WiFi-verkkoon. Se alkaa kyselemään lämpöpumpulta tietoja säännöllisesti ja raportoi tiedot MQTT-palvelimelle. GPIO13/GPIO15-pinnejä käytetään tiedonsiirtoon ja voit pitää tietokoneesi kytkettynä USB-portissa, jos haluat lukea debug-tietoja tai päivittää firmwarea. \
Serial 1 (GPIO2) voidaan liittää toiseen sarjaväylään (vain GND ja TX laitteelta) debug-tietojen lukemiseen.

Kaikki vastaanotettu tieto raportoidaan eri MQTT-aiheiden alle (katso koko lista alempaa). Web UI:n kautta voi myös kytkeä päälle lokituksen ja viestien hexadumppauksen, jolloin nämä tiedot raportoidaan aiheen 'panasonic_heat_pump/log' alle.

Voit myös liittää 1-wire sensorin GPIO4-liitäntään ja sen arvo raporoidaan omassa MQTT-aiheessaan (panasonic_heat_pump/1wire/sensorid).

HeishaMon voi myös lukea kahden kWh-mittarin S0-portista sähkönkulutusta. Kytke GPIO12 ja GND kWh-mittarin S0-liitäntään ja jos tarvitset vielä toisen liitännän käytä GPIO14:ta siihen. Kulutus raportoidaan MQTT-aiheiden 'panasonic_heat_pump/s0/Watt/1' ja 'panasonic_heat_pump/s0/Watt/2' alla sekä JSON-tulosteessa. Hetkellisen tehon sijasta näet kulutuksen (kWh), jos vaihdat topiciin 'Watt'in sijasta 'Watthour', tai kokonaiskulutuksen 'WatthourTotal'-arvolla. WatthourTotal-arvon synkronoimiseksi kWh-mittarisi kanssa julkaise oikea arvo MQTT:llä topiciin panasonic_heat_pump/s0/WatthourTotal/1 tai panasonic_heat_pump/s0/WatthourTotal/2 'retain'-optiolla laitteen käynnistyksen aikana.

Firmwaren voi päivittää laitteen konfigurointisivun kautta kirjautumalla tunnuksella 'admin' ja valitsemallasi salasanalla (oletus on 'heisha').

JSON-muodossa lämpöpumpun tilan voi hakea osoitteesta http://heishamon.local/json (korvaa heishamon.local laitteesi IP-osoitteella, jos MDNS ei toimi verkossasi).

'integrations'-hakemistosta löydät esimerkkejä laitteen integroimisesta erilaisiin kotiautomaatiojärjestelmiin.

# Säännöt-toiminnallisuus
Säännöt-toiminnallisuus mahdollistaa lämpöpumpun ohjaamisen suoraan HeishaMonista käsin. Tämä on paljon luotettavampaa kuin ulkoisen kotiautomaation käyttäminen WiFin yli. Kun uusi sääntöjoukko julkaistaan, se validoidaan välittömästi ja otetaan käyttöön jos se on kelvollinen. Jos uusi sääntöjoukko on virheellinen, se jätetään huomiotta ja vanha sääntöjoukko ladataan uudelleen. Voit tarkistaa konsolista palautteen tästä. Jos jokin kelvollinen sääntöjoukko kaataa HeishaMonin, se poistetaan automaattisesti käytöstä seuraavalla käynnistyksellä, jolloin voit tehdä muutoksia. Tämä estää HeishaMonia joutumasta käynnistyssilmukkaan.

Sääntökirjastossa käytetyt tekniikat mahdollistavat erittäin suurten sääntöjoukkojen käytön, mutta parhaana käytäntönä on pitää ne alle 10 000 tavun koossa.

Huomaa, että komentojen lähettäminen lämpöpumpulle tapahtuu asynkronisesti. Syntaksin alussa lämpöpumpulle lähetetyt komennot eivät heti näy lämpöpumpun arvoissa myöhemmin. Siksi lämpöpumpun arvot tulisi lukea suoraan lämpöpumpulta eikä itse tallentamistasi arvoista.

## Syntaksi
Kaksi yleistä sääntöä: välilyönnit ovat pakollisia ja puolipisteitä käytetään rivin lopun merkkinä.

### Muuttujat
Sääntöjoukko käyttää seuraavaa muuttujarakennetta:

- `#`: Globaalit
Näihin muuttujiin pääsee käsiksi koko sääntöjoukossa, mutta ne on määriteltävä sääntöblokin sisällä. Älä käytä globaaleja kaikkiin muuttujiisi, sillä ne käyttävät muistia jatkuvasti.

- `$`: Lokaalit
Nämä muuttujat elävät sääntöblokin sisällä. Kun sääntöblokki päättyy, nämä muuttujat siivotaan ja käytetty muisti vapautetaan.

- `@`: Lämpöpumpun parametrit
Nämä ovat samat kuin Manage Topics -dokumentaatiosivulla ja HeishaMonin etusivulla listatut. Sääntöjoukko noudattaa myös MQTT:n ja REST API:n kautta käytettävää R/W-logiikkaa. Tämä tarkoittaa, että lukuaihiot eroavat kirjoitusaihioista. Lämpöpumpun tilan lukeminen tapahtuu `@Heatpump_State`:lla, tilan muuttaminen `@SetHeatpump`:lla.

- `%`: Päivämäärä- ja aikamuuttujat
Näitä voidaan käyttää päivämäärä- ja aikapohjaisiin sääntöihin. Tällä hetkellä tuetaan `%hour` (0-23), `%minute` (0-59), `%month` (1-12) ja `day` (1-7). Kaikki ovat tavallisia kokonaislukuja. Oikea NTP-konfiguraatio tarvitaan oikean järjestelmäpäivämäärän ja -ajan asettamiseen HeishaMoniin.

- `?`: Termostaattiparametrit
Nämä muuttujat heijastavat parametreja, jotka luetaan yhdistetyltä termostaatilta OpenTherm-toiminnallisuutta käytettäessä. Nimet ovat samat lukemiseen ja kirjoittamiseen, mutta kaikki arvot eivät tue lukemista ja/tai kirjoittamista.

- `ds18b20#2800000000000000`: Dallas 1-wire lämpötila-arvot
Käytä näitä muuttujia kytkettyjen antureiden lämpötilan lukemiseen. Nämä arvot ovat tietenkin vain luku -muuttujia. Anturin ID tulee risuaidan jälkeen.

- `s0#watt[hour[total]]_?`: S0-arvot porteille 1 ja 2. Korvaa kysymysmerkki porttinumerolla.
Esimerkiksi s0#watt_1 portista 1 ja s0#watthourtotal_2 portista 2 kokonaistuntikulutukseksi.


Kun muuttujaa kutsutaan mutta sillä ei vielä ole arvoa, arvo on `NULL`.

Muuttujat voivat olla tyyppiä Boolean (`1` tai `0`), float (`3.14`), integer (`10`) tai string. Merkkijonot määritellään yksin- tai kaksinkertaisilla lainausmerkeillä.

### Tapahtumat tai funktiot
Säännöt kirjoitetaan `event`- tai `function`-lohkoihin. Nämä ovat lohkoja, jotka laukaistaan kun jotain tapahtuu; joko uusi lämpöpumppu- tai termostaattiarvo on vastaanotettu tai ajastin laukesi. Tai niitä voidaan käyttää tavallisina funktioina.

```
on [tapahtuma] then
  [...]
end

on [nimi] then
  [...]
end
```

Tapahtumat voivat olla lämpöpumppu- tai termostaattiparametreja tai ajastimia:
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

Funktioita määriteltäessä nimeät lohkosi ja voit kutsua sitä mistä tahansa:
```
on foobar then
  [...]
end

on @Heatpump_State then
  foobar();
end
```

Funktioilla voi olla parametreja:
```
on foobar($a, $b, $c) then
  [...]

on @Heatpump_State then
  foobar(1, 2, 3);
end
```

Jos kutsut funktiota vähemmällä arvoilla kuin funktio odottaa, kaikilla muilla parametreilla on NULL-arvo.

On yksi erityisfunktio, joka kutsutaan järjestelmän käynnistyksen yhteydessä tai kun uusi sääntöjoukko tallennetaan:
```
on System#Boot then
  [...]
end
```

Tätä erityisfunktiota voidaan käyttää globaalien tai tiettyjen ajastimien alustamiseen.

### Operaattorit
Tavallisia operaattoreita tuetaan standardisella assosiatiivisuudella ja prioriteetilla. Tämä mahdollistaa myös tavallisen matematiikan käytön.
- `&&`: Ja
- `||`: Tai
- `==`: Yhtä suuri
- `!=`: Erisuuri kuin
- `>=`: Suurempi tai yhtä suuri
- `>`: Suurempi kuin
- `<`: Pienempi kuin
- `<=`: Pienempi tai yhtä suuri
- `-`: Miinus
- `%`: Modulo
- `*`: Kertolasku
- `/`: Jakolasku
- `+`: Plus
- `^`: Potenssi

Sulkeita voidaan käyttää operaattorien priorisoimiseen kuten normaalissa matematiikassa.

### Funktiot
- `coalesce`
Palauttaa ensimmäisen arvon, joka ei ole `NULL`. Esim. `$b = NULL; $a = coalesce($b, 1);` palauttaa 1. Tämä funktio hyväksyy rajattoman määrän argumentteja.

- `max`
Palauttaa syöteparametrien maksimiarvo.

- `min`
Palauttaa syöteparametrien minimiarvo.

- `isset`
Palauttaa boolean true, kun syötemuuttuja on vielä `NULL`, muissa tapauksissa false.

- `round`
Pyöristää syöte-floatin lähimpään kokonaislukuun.

- `floor`
Suurin kokonaislukuarvo, joka on pienempi tai yhtä suuri kuin syöte-float.

- `ceil`
Pienin kokonaislukuarvo, joka on suurempi tai yhtä suuri kuin syöte-float.

- `setTimer`
Asettaa ajastimen laukeamaan X sekunnin kuluttua. Ensimmäinen parametri on ajastimen numero ja toinen parametri sekuntien määrä ennen laukeamista. Ajastin laukeaa vain kerran ja se on asetettava uudelleen toistuvia tapahtumia varten.

- `print`
Tulostaa arvon konsoliin.

- `concat`
Yhdistää eri arvot yhdistetyksi merkkijonoksi. Esim.: `@SetCurves = concat('{zone1:{heat:{target:{high:', @Z1_Heat_Curve_Target_High_Temp, ',low:32}}}}');`

- `gpio`
Mahdollistaa GPIO-tilan asettamisen tai lukemisen. Kun kutsutaan yhdellä argumentilla, palautetaan GPIO-tila. Kun kutsutaan kahdella argumentilla, asetetaan GPIO:n tila. Tämä funktio asettaa vain digitaalisia pinnejä, joten tila voi olla vain 0 tai 1. Kaksi relettä suuressa heishamonissa ovat gpio21 ja gpio47. Käyttäjän konfiguroitavia ylimääräisiä GPIO:ita (katso alla) voidaan myös osoittaa niiden pinnumerolla. Katso esimerkki releiden vaihtamiseen kahden sekunnin välein.

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

### Ehdot
Ainoa tuettu ehtolauseke on `if`, `else` ja `elseif`:

```
if [ehto] then
  [...]
else
  if [ehto] then
    [...]
  end
end
```

```
if [ehto] then
  [...]
elseif [ehto] then
  if [ehto] then
    [...]
  else
    [...]
  end
elseif [ehto] then
  [...]
else
  [...]
end
```

### Esimerkkejä

*WAR-laskenta (säätilaan perustuva säätö)*
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

*Termostaatin asetusarvo*
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

# Tehdasasetusten palauttaminen
Tehdasasetukset voidaan palauttaa web-käyttöliittymän kautta, mutta jos web-käyttöliittymä ei ole käytettävissä, voit suorittaa kaksoisnollauksen. Kaksoisnollaus tulisi suorittaa ei liian nopeasti mutta ei liian hitaastikaan. Yleensä puoli sekuntia nollausten välillä toimii. Merkiksi siitä, että kaksoisnollaus suoritti tehdasasetusten palautuksen, sininen LED vilkkuu nopeasti (sinun täytyy nyt painaa nollausta uudelleen käynnistääksesi HeishaMonin normaalisti, jolloin WiFi-hotspot pitäisi näkyä).

# Lisätietoa
Alla vielä hieman teknisiä lisätietoja projektista, kuten esimerkiksi kaapelinrakennusohjeet ja apua piirilevyn suunnitteluun.

## Yhteys lämpöpumppuun
Yhteys voidaan muodostaa CN-CNT tai CN-NMODE liitäntöjen kautta. Jos sinulla on Panasonicin CZ-TAW1 WiFi-sovitin käytössäsi, voit vain kytkeä HeishaMon-laitteen sen sijasta käyttäen samaa kaapelia. HeishaMonin ja alkuperäisen CZ-TAW1-moduulin samanaikainen käyttö aktiivisina laitteina ei kuitenkaan ole mahdollista. HeishaMon voidaan asettaa "Vain kuuntele" -tilaan, jolloin ne voivat toimia rinnakkain. Ainoa haittapuoli on, että HeishaMon ei voi lähettää komentoja eikä käyttää valinnaista PCB-optiota.

Yhteysasetukset: TTL 5V UART 9600,8,E,1 \
\
CN-CNT Pin-out (ylhäältä alas) \
1 - +5V (250mA)  \
2 - 0-5V TX (lämpöpumpulta) \
3 - 0-5V RX (lämpöpumpulle)\
4 - +12V (250mA) \
5 - GND \
 \
CN-NMODE Pin-out (vasemmalta oikealle) \
"Huom! Kuten piirilevyllä sanotaan, vasen pinni on numero 4 ja oikealla reunassa on pinni 1. Älä laske 1:stä 4:ään vasemmalta!  \
4 - +5V (250mA)  \
3 - 0-5V TX (lämpöpumpulta) \
2 - 0-5V RX (lämpöpumpulle) \
1 - GND

Heishamon saa virtansa Aquarea-lämpöpumpulta kaapelin kautta (5V virta), erillistä virtalähdettä ei tarvita.

## Pitkän matkan yhteys
HeishaMonin voi kytkeä pitkän matkan päähän. Jopa 5 metriä toimii normaalilla kaapeloinnilla. Pidemmille matkoille TTL-RS485-konfiguraatio alla olevan kuvan mukaisesti on mahdollinen. Tämä vaatii kuitenkin HeishaMonin ulkoisen virransyötön 5V:llä (esimerkiksi USB-kaapelilla).

![TTL-over-RS485 HeishaMon pitkän matkan yhteys](optional-long-distance-heishamon.png)


## Mistä ostaa liittimiä
[RS-Online orders](Connectors_RSO.md)

[Conrad orders](Connectors_Conrad.md)

Käytä 24 AWG suojattua 4-johtimista kaapelia.


## Piirilevyn valmistaminen
Lämpöpumppuun kytkemiseen tarvittavat PCB:t on suunniteltu projektin jäsenten toimesta ja ne on lueteltu alla. Tärkein osa laitteistoa on tasomuunnos Panasonicin 5V:sta HeishaMonin 3,3V:iin sekä GPIO13/GPIO15-aktivointilinja käynnistyksen jälkeen. \
[PCB-suunnitelmat projektin jäseniltä](PCB_Designs.md) \
[Kuva Wemos D1 Beta](WEMOSD1.JPG) \
[Kuva ESP12-F](NewHeishamon.JPG)

Jos haluat päästä helpolla, voit myös ostaa valmiin piirilevyn tai kaapelin, joita projektin jäsenet ovat tehneet: \
[Lectronz kauppa](https://lectronz.com/products/heishamon-communication-pcb)  - Igor Ybema (aka TheHogNL) Hollannissa

## Arduino-imagen kääntäminen
Boards: \
esp8266 by esp8266 community version 3.0.2 [Arduino](https://github.com/esp8266/Arduino/releases/tag/3.0.2)

[Käytetyt kirjastot](LIBSUSED.md)


## MQTT-aiheet
[Lista tämän hetkisistä MQTT-aiheista](MQTT-Topics.md)

## Viestintäluotettavuus
HeishaMonilta lämpöpumpulle lähetetyt viestit saatetaan toisinaan hylätä, erityisesti jos useita asetuksia muutetaan lyhyessä ajassa. HeishaMonin ei ole mahdollista yrittää uudelleen kaikkia hylättyjä viestejä, joten käyttäjien tulisi toteuttaa oma uudelleenyrityslogiikkansa.

Suositeltu uudelleenyrityslogiikka on seuraava:
1. Lämpöpumpun asetuksen muuttamisen jälkeen HeishaMonin kautta, odota HeishaMonin raportoivan, että asetus on muuttunut.
2. Jos asetus ei ole päivittynyt 10 sekunnin kuluttua, aseta se uudelleen.

Tämä voidaan toteuttaa HeishaMonissa itsessään Sääntöjä käyttäen, kuten tässä esimerkissä:
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

## EEPROM-varoitus
Tähän päivään asti emme tiedä, miten lämpöpumpulle lähetetyt komennot käsitellään lämpöpumpussa itsessään. Todennäköisesti monet komennot kirjoitetaan EEPROM:iin tallennettavaksi sähkökatkon jälkeen, kuten käyttöveden lämpötilan asettaminen. EEPROM kestää monia kirjoitustapahtumia, mutta rajalla on raja. Emmekä tiedä rajaa myöskään. Varmista siis, ettet ylikuormita lämpöpumppua liian monilla komennoilla. Jokainen sekunti on liikaa. Muutama tunneissa, asetusta kohti, on todennäköisesti hyvä. Lämpöpumppu on kuitenkin hidas lämmitys-(jäähdytys-)laite, joten niin tiheät muutokset eivät todennäköisesti edes tuota mitään hyötyä.

## DS18b20 1-wire-tuki
Softa tukee myös DS18B20 1-wire lämpötilasensoreita. Kunnollinen 1-wire sensori (sis. 4,7 kOhm pull-up vastuksen) liitettynä GPIO4-liitäntään luetaan joka konfiguroiduin sekunnein (vähintään 5) ja tieto palautetaan 'panasonic_heat_pump/1wire/"sensor-hex-address"' aiheessa. Valmiissa piirilevyissä tämä 4,7 kOhm vastus on jo asennettu.

## Suuren piirilevyn releohjaus
Uudempi, suuri heishamon sisältää kaksi sisäänrakennettua relettä, joita voidaan kytkeä päälle ja pois MQTT-komennoilla. Releitä voidaan käyttää minkä tahansa kontaktikytkennän, jopa 230V verkkojännitteen (max 5A) kytkemiseen. Releen ohjaamiseksi lähetä arvo 1 tai 0 MQTT-aiheeseen "panasonic_heat_pump/gpio/relay/one" releelle yksi tai "panasonic_heat_pump/gpio/relay/two" releelle kaksi.

## Käyttäjän konfiguroitavat ylimääräiset GPIO:t
Kiinteiden relepinnien lisäksi HeishaMon tarjoaa joukon käyttäjän konfiguroitavia GPIO-pinnejä, jotka voidaan itsenäisesti asettaa Tulo (pull-up), Tulo tai Lähtö -tilaan asetusten web-valikon kautta.

**Käytettävissä olevat pinnit alustasta riippuen:**
- **ESP8266:** 3 ylimääräistä GPIO:ta (pinnit 1, 3, 16)
- **ESP32:** 5 ylimääräistä GPIO:ta (pinnit 33–37); relepinnit 21 ja 47 pysyvät kiinteinä

Jokaisen pinnin tila tallennetaan asetuksiin ja sovelletaan käynnistyksen yhteydessä ja asetusten tallentamisen yhteydessä.

**Lähtö-GPIO:iden ohjaaminen MQTT:n kautta:**
Lähetä `1`, `0`, `on`, `off`, `true` tai `false` aiheeseen:
```
panasonic_heat_pump/gpio/extra/N
```
jossa `N` on ylimääräisen GPIO:n 1-perusteinen indeksi (esim. `extra/1` ensimmäiselle ylimääräiselle pinnille).

**GPIO-tilojen lukeminen MQTT:n kautta:**
Tulo- ja lähtötilat julkaistaan automaattisesti samoihin aiheisiin muutoksen yhteydessä ja ensimmäisellä MQTT-yhteydellä (retained).

**HTTP API:**
Kaikki ylimääräiset GPIO-tilat voidaan lukea JSON-muodossa GET-pyynnöllä:
```
GET /gpio
```
Esimerkkivastaus:
```json
[{"pin":33,"mode":2,"state":1},{"pin":34,"mode":0,"state":0}, ...]
```
jossa `mode` on 0 = Tulo (pull-up), 1 = Tulo, 2 = Lähtö.

Lähtöpinnin asettaminen HTTP POST:lla:
```
POST /gpio?pin=N&value=1
```

**Ylimääräisten GPIO:iden käyttö säännöissä:**
`gpio()`-sääntöfunktio toimii ylimääräisten GPIO-pinnien kanssa niiden fyysisen pinnin numeron kautta, samoin kuin relepinnit.

## OpenTherm-tuki
Jos heishamon-piirilevysi tukee OpenThermia, ohjelmistoa voidaan käyttää myös OpenTherm-tietojen välittämiseen yhteensopivalta termostaatilta kotiautomaatiojärjestelmällesi MQTT:n tai JSON:n kautta. Jos otat OpenTherm-tuen käyttöön asetuksissa, verkkosivulle ilmestyy uusi välilehti. Siellä näet OpenTherm-arvot. Osa on tyyppiä R(ead) ja osa W(rite), ja osa on molempia. Read tarkoittaa, että termostaatti voi lukea tietoa heishamonilta. Toimitat tiedon MQTT:n kautta (tai säännöillä) päivittämällä tämän arvon MQTT-aiheessa 'opentherm/read'. Write-arvot ovat tietoa termostaatilta, kuten 'roomTemp'. Nämä ovat saatavilla MQTT-aiheessa 'opentherm/write'.

Käytettävissä olevat OpenTherm-muuttujat ovat:
### WRITE-arvot
- chEnable: Boolean, pitäisikö keskuslämmitys kytkeä päälle
- dhwEnable: Boolean, pitäisikö käyttöveden lämmitys kytkeä päälle
- coolingEnable: Boolean, pitäisikö jäähdytys kytkeä päälle
- roomTemp: Float-arvo termostaatin mittaamasta huonelämpötilasta
- roomTempSet: Float-arvo pyydetystä huonelämpötilan asetusarvosta termostaatilla
- chSetpoint: Float-arvo termostaatin laskemasta veden asetusarvosta
- maxRelativeModulation: Modulaatiomäärä (0-100%), jonka lämpöpumppu saa käyttää
- coolingControl: Jäähdytysmäärä (0-100%), jonka termostaatti pyytää lämpöpumpulta
### READ AND WRITE -arvot
- dhwSetpoint: Float-arvo nykyisestä käyttöveden asetusarvosta
- maxTSet: Float-arvo, joka määrittää maksimiveden asetusarvon
### READ-arvot
- chPressure: Float-arvo mitatusta vesipaineesta
- outsideTemp: Float-arvo mitatusta ulkolämpötilasta
- inletTemp: Float-arvo mitatusta veden tulolämpötilasta
- outletTemp: Float-arvo mitatusta veden lähtölämpötilasta
- dhwTemp: Float-arvo mitatusta käyttövesilämpötilasta
- relativeModulation: Nykyinen modulaatiomäärä (0-100%)
- flameState: Boolean, tuottaako keskuslämmitys lämpöä
- chState: Boolean, onko lämpöpumppu huone-/keskuslämmitystilassa
- dhwState: Boolean, onko lämpöpumppu käyttövesitilassa
- coolingState: Boolean, onko lämpöpumppu jäähdytystilassa
- dhwSetUppBound: Kokonaisluku (0-127), maksimi käyttövesilämpötila. Oletus: 75
- dhwSetLowBound: Kokonaisluku (0-127), minimi käyttövesilämpötila. Oletus: 40
- chSetUppBound: Kokonaisluku (0-127), maksimi CH-vesilämpötila. Oletus: 65
- chSetLowBound: Kokonaisluku (0-127), minimi CH-vesilämpötila. Oletus: 20

## Viestin formaatti:
[Tämän hetkinen lista dokumentoiduista dekoodatuista tavuista löytyy täältä](ProtocolByteDecrypt.md)

## MQTT TLS-tuki:
TLS-tuki MQTT:lle on tällä hetkellä toteutettu vain ESP32-versiolle. TLS on oletuksena poissa käytöstä ja sen voi ottaa käyttöön asetuksissa. Kun otat sen käyttöön, varmista, että MQTT-palvelimesi hyväksyy suojatut yhteydet heishamonilta, mukaan lukien PEM CA -varmenteen asettaminen (esim. itse allekirjoitettu). Lataa tekstitiedosto (plain text, pääte pem, esim. "CA.pem") varmenteesta heishamoniin asetussivun kautta. Varmenteen lataamisen jälkeen aktivoi TLS-tuki ja vaihda (normaaliasetuksissa) MQTT-portti 8883:een. !!TLS-ongelmiin ei tarjota tukea!! Google auttaa ongelmissa.

## Integraatio-esimerkkejä Open Source -kotiautomaatioon
[Openhab2](Integrations/Openhab2)

[Home Assistant](Integrations/Home%20Assistant)

[IOBroker Manual](Integrations/ioBroker_manual)

[Domoticz](Integrations/Domoticz)
