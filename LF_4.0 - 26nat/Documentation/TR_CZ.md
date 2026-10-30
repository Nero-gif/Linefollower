# Technická dokumentace: Linefollower 3.0 2026 

**Tým:** Jakub Lukšík & Matěj Dvořák  
**Škola:** Střední škola informatiky a finančních služeb (INFIS)  
**Soutěž:** ROBO2026  
**Verze:** 4.0  
**Datum:** 30.10.2026

---

## 1. Úvod a cíle projektu
Projekt "Linefollower 4.0 2026" byl vyvinut jako soutěžní robot pro autonomní sledování černé čáry na světlém podkladu v rámci soutěže ROBO2026 jejíž unikátem je podmínka aby každý robot vezl a nevylil kelímek s vodou. Hlavním cílem byla konstrukce robota s vysokou dynamikou a precizním řízením, čehož bylo dosaženo kombinací výkonného mikrokontroléru Teensy 4.1 a lehkého 3D tištěného šasi vlastního designu.

## 2. Mechanická konstrukce
Konstrukce robota se zaměřuje na nízké těžiště, tuhost a nízko umístěné posazení kelímku s vodou, což je klíčové pro stabilitu v zatáčkách při vysokých rychlostech a schopnost udržet kelímek s vodou stabilní.
* **Návrh:** Šasi bylo kompletně navrženo v CAD prostředí **Autodesk Fusion 360**.
* **Výroba:** Hlavní díly jsou vyrobeny 3D tiskem z materiálu PLA pro snadný a levný tisk.
* **Pohon:** Robot využívá dva převodové motory řady N20

[//]: # (* TODO: doplnit použité mototy včetně převodů)

* **Celková hmotnost:** 
* **Rozvor kol:**
* **Průměr kol:**

[//]: # (* TODO: doplnit hodnoty)

## 3. Hardware a elektronika
Elektronický systém je postaven na vlastnoručně navrženém a osazeném DPS v programu KiCad, s důrazem na čistotu signálů a efektivní napájení.

### Klíčové komponenty:
* **Řídicí jednotka:** Teensy 4.1 (ARM Cortex-M7, 600 MHz). Zvolena pro nadstandardní výpočetní výkon umožňující vysokou vzorkovací frekvenci PID smyčky.
* **Motorový budič:** 

[//]: # (* TODO: doplnit budič)
* **Senzorové pole:** 16 infračervených senzorů QTR-MD-16RC, optimalizovaných pro rychlou odezvu na změnu kontrastu. Lišta senzorů je připojena kabelem IDC na konektor J1.
* **Napájení:** 2x 18650 ( = 2s Li-Po, nominální napětí 7.4V, minimální napětí 6V, maximální napětí 8.4. Připojen přes konektory XT60. Celé napájení spíná výkonový P-kanálový MOSFET IRF4905 (Q1), jehož gate je řízena přes ochranný rezistor R1 (10 kΩ) z ovládacího spínače. Toto řešení umožňuje bezpečné odpojení celého silového okruhu bez mechanického přerušení kabeláže.
* **Stabilizace napětí:** 
  * Spínaný regulátor TSR 1-2450 (U2) zajišťuje stabilních 5 V pro Teensy 4.1 s účinností až 94 %.
  * Logické obvody jsou napájeny samostanou napájecí větví s regulovaným napětím 3.3 V pomocí TSR 1-2433, nikoliv mikrokontrolerem Teensy 4.1 z důvodu předpokládaného odběru vyššího než je udávané maximum vývodu Teensy. 

### Zapojení pinů (Teensy 4.1):
| Funkce | Pin | Směr | Popis |
| :--- | :--- | :--- | :--- |
| **Rozšíření** | D0 | - | Volný vývod vyvedený na konektor J8 |
| **Motor A - PWM** | D1 | OUT | TB6612FNG PWMA (J3) - rychlost motoru A |
| **Motor A - IN2** | D2 | OUT | TB6612FNG AIN2 (J3) - směr motoru A |
| **Motor A - IN1** | D3 | OUT | TB6612FNG AIN1 (J3) - směr motoru A |
| **Budič - STBY** | D4 | OUT | TB6612FNG STBY (J3) - povolení budiče |
| **Motor B - IN1** | D5 | OUT | TB6612FNG BIN1 (J3) - směr motoru B |
| **Motor B - IN2** | D6 | OUT | TB6612FNG BIN2 (J3) - směr motoru B |
| **Motor B - PWM** | D7 | OUT | TB6612FNG PWMB (J3) - rychlost motoru B |
| **Rozšíření** | D8, D9 | - | Volné vývody vyvedené na konektor J10 |
| **Kamera - CS** | D10 | OUT | Arducam MEGA 3 Mpx (J2) - SPI CS |
| **Kamera - MOSI** | D11 | OUT | Arducam MEGA 3 Mpx (J2) - SPI MOSI |
| **Kamera - MISO** | D12 | IN | Arducam MEGA 3 Mpx (J2) - SPI MISO |
| **Kamera - SCK** | D13 | OUT | Arducam MEGA 3 Mpx (J2) - SPI SCK (sdílený s LED na desce Teensy) |
| **Rozšíření** | D14 – D23 | - | Volné vývody vyvedené na konektor J11 (pin 1 = D23 … pin 10 = D14) |
| **Rozšíření** | D24 | - | Volný vývod vyvedený na konektor J9 |
| **Senzor čáry 10** | D25 | IN/OUT | QTR-MD-16RC (J1) - RC10 |
| **Senzor čáry 12** | D26 | IN/OUT | QTR-MD-16RC (J1) - RC12 |
| **Senzor čáry 14** | D27 | IN/OUT | QTR-MD-16RC (J1) - RC14 |
| **Senzor čáry 16** | D28 | IN/OUT | QTR-MD-16RC (J1) - RC16 |
| **Senzor čáry 15** | D29 | IN/OUT | QTR-MD-16RC (J1) - RC15 |
| **Senzor čáry 13** | D30 | IN/OUT | QTR-MD-16RC (J1) - RC13 |
| **Senzor čáry 11** | D31 | IN/OUT | QTR-MD-16RC (J1) - RC11 |
| **Senzor čáry 9** | D32 | IN/OUT | QTR-MD-16RC (J1) - RC09 |
| **Senzor čáry 7** | D33 | IN/OUT | QTR-MD-16RC (J1) - RC07 |
| **Senzor čáry 5** | D34 | IN/OUT | QTR-MD-16RC (J1) - RC05 |
| **Senzor čáry 3** | D35 | IN/OUT | QTR-MD-16RC (J1) - RC03 |
| **Senzor čáry 1** | D36 | IN/OUT | QTR-MD-16RC (J1) - RC01 |
| **Senzor čáry 2** | D37 | IN/OUT | QTR-MD-16RC (J1) - RC02 |
| **Senzor čáry 4** | D38 | IN/OUT | QTR-MD-16RC (J1) - RC04 |
| **Senzor čáry 6** | D39 | IN/OUT | QTR-MD-16RC (J1) - RC06 |
| **Senzor čáry 8** | D40 | IN/OUT | QTR-MD-16RC (J1) - RC08 |
| **Napětí baterie** | D41 / A17 | IN (analog) | BATT_SENSE - odporový dělič R3 (33 kΩ) / R4 (10 kΩ), filtrace C3 |
| **Napájení** | VIN | IN | 5 V z regulátoru U2 |
| **Zem** | GND | - | Společná zem |

[//]: # (TODO: doplnit minimálně odd/even konektory)
## 4. Algoritmus řízení
Software implementuje digitální **PID regulátor**, který vypočítává korekci směru na základě váženého průměru hodnot ze senzorového pole.

### Konfigurované parametry regulace:
* **P (Proporcionální složka):** $K_p = ___________$ – určuje okamžitou reakci na aktuální odchylku od středu.
* **I (Integrální složka):** $K_i = ____________$ – koriguje drobnou systematickou chybu v delších úsecích.
* **D (Derivační složka):** $K_d = ____________$ – predikuje budoucí chybu a tlumí překmity při rychlých změnách směru.

Robot pracuje s normalizovanou pozicí čáry v rozsahu 0 až 3000. Základní rychlost pohybu (`BASE_SPEED`) je nastavena na hodnotu 30 000 pro dosažení optimálního poměru mezi stabilitou a časem průjezdu.

Napájecí větev pro motory robota nenmá pevně regulované napětí. Konstrukčním napětím motorů je **6 V**, proto je na základě naměřené hodnoty napěťového děliče upravováno PWM. 

## 5. Bezpečnost a údržba
Vzhledem k použití LiPo akumulátorů jsou dodržovány tyto bezpečnostní zásady:
* Nabíjení probíhá výhradně v protipožárním vaku (LiPo Safe Bag).
* Napětí akumulátoru je monitorováno odporovým děličem na analogovém vstupu Teensy 4.1. Kritická spodní mez je **6.0 V** (3 V/článek) - pod touto hodnotou hrozí nevratné poškození článků. Plně nabitý akumulátor má napětí 8.4 V.
* Mechanické spoje a stav kabeláže jsou kontrolovány před každou jízdou.

## 6. Seznam součástek (BOM)
| Označení | Komponenta | Množství | Poznámka |
| :--- | :--- | :--- | :--- |
| **U1** | Teensy 4.1 | 1 ks | Řídicí mikrokontrolér |
| **U2** | TRACO TSR 1-2450 | 1 ks | DC/DC měnič 5 V pro Teensy (VIN) |
| **U3** | TRACO TSR 1-2433 | 1 ks | DC/DC měnič 3.3 V pro logiku (senzory, kamera, logika budiče) |
| **Q1** | IRF4905 (TO-220) | 1 ks | P-MOSFET, hlavní spínač napájení |
| **J1** | IDC-Header 2x12 | 1 ks | Konektor senzorové lišty QTR-MD-16RC |
| **J2** | JST XH 1x06 | 1 ks | Konektor kamery Arducam MEGA 3 Mpx (SPI) |
| **J3** | Pin header pro TB6612FNG | 1 ks | Patice modulu motorového budiče TB6612FNG |
| **J4** | Pin socket 1x02 | 1 ks | Propojka řízení emitorů ODD/EVN senzorové lišty |
| **J5, J6** | Pin socket 1x05 | 2 ks | Rozšiřující lišty GND |
| **J8, J9** | Pin socket 1x01 | 2 ks | Rozšiřující vývody Teensy (D0, D24) |
| **J10** | Pin socket 1x02 | 1 ks | Rozšiřující vývody Teensy (D8, D9) |
| **J11** | Pin socket 1x10 | 1 ks | Rozšiřující vývody Teensy (D14 – D23) |
| **M1, M2** | JST XH 1x02 | 2 ks | Konektory DC motorů (výstupy A a B budiče) |
| **SW1** | JST XH 1x02 | 1 ks | Konektor hlavního vypínače (ovládá gate Q1) |
| **BT2** | Pájecí body pro vodič 2 mm² | 1 ks | Připojení akumulátoru 2S |
| **D1** | LED 5 mm | 1 ks | Indikace zapnutého napájení (větev 6V) |
| **C1** | 1 µF | 1 ks | Keramický kondenzátor napájecí větve 6V |
| **C2** | 47 µF (elektrolytický) | 1 ks | Výstupní kondenzátor regulátoru U2 (5 V) |
| **C3** | 100 nF | 1 ks | Filtrace měření napětí baterie (BATT_SENSE) |
| **C4** | 100 nF | 1 ks | Blokovací kondenzátor napájecí větve 6V |
| **C5** | 100 nF | 1 ks | Blokovací kondenzátor větve 5 V |
| **C6** | 47 µF (elektrolytický) | 1 ks | Filtrační kondenzátor napájecí větve 6V |
| **R1** | 10 kΩ | 1 ks | Pull-up gate MOSFETu Q1 (udržuje Q1 vypnutý) |
| **R2** | 1 kΩ | 1 ks | Předřadný rezistor LED D1 |
| **R3** | 33 kΩ | 1 ks | Horní rezistor děliče napětí baterie |
| **R4** | 10 kΩ | 1 ks | Dolní rezistor děliče napětí baterie |
---

## 7. Přílohy
* **Příloha A:** Technický výkres šasi
* **Příloha B:** Schéma zapojení
* **Příloha C:** Kompletní zdrojový kód

