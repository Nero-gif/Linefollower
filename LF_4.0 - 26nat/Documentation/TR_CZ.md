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
| **Enkodér** | D1 | IN | Vstup z magnetického enkodéru (přerušení IRQ_RISING) |
| **Motor L - PWM** | D3 | OUT | Rychlost levého motoru (PWM, 1 kHz, 16 bitů) |
| **Motor L - DIR** | D4 | OUT | Směr levého motoru |
| **Senzor čáry 1** | D5 | IN/OUT | QTR-8RC - krajní levý senzor |
| **Senzor čáry 2** | D6 | IN/OUT | QTR-8RC |
| **Senzor čáry 3** | D7 | IN/OUT | QTR-8RC |
| **Senzor čáry 4** | D8 | IN/OUT | QTR-8RC |
| **Senzor čáry 5** | D9 | IN/OUT | QTR-8RC |
| **Senzor čáry 6** | D10 | IN/OUT | QTR-8RC |
| **Senzor čáry 7** | D11 | IN/OUT | QTR-8RC |
| **Senzor čáry 8** | D12 | IN/OUT | QTR-8RC - krajní pravý senzor |
| **LED RGB - B** | D13 | OUT | Stavová LED (modrá) |
| **LED RGB - R** | D14 | OUT | Stavová LED (červená) |
| **LED RGB - G** | D15 | OUT | Stavová LED (zelená) |
| **Motor R - PWM** | D28 | OUT | Rychlost pravého motoru (PWM, 1 kHz, 16 bitů) |
| **Motor R - DIR** | D29 | OUT | Směr pravého motoru |

[//]: # (TODO: předělat tabulku )
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
| **U2, U4** | DRV8871 Breakout | 2 ks | H-můstek pro motory |
| **U3** | TSR 1-2450 | 1 ks | DC/DC měnič (5 V) |
| **J1** | XT60-M | 1 ks | Napájecí konektor |
| **J4** | IDC-Header 2x12 | 1 ks | Konektor pro senzorovou lištu QTR-8RC |
| **D2** | LED_RKGB (5 mm) | 1 ks | Stavová RGB LED |
| **C1, C2** | 100 µF / 25 V | 2 ks | Elektrolytické filtry napájení motorových budičů |
| **C3, C4** | 100 nF | 2 ks | Keramické blokovací kondenzátory regulátoru TSR |
| **R1-R3** | 100 Ω | 3 ks | Předřadné rezistory RGB LED (D2) |
| **R5** | 1 kΩ | 1 ks | Ochranný rezistor gate MOSFETu Q1 |
| **Q1** | IRLZ44N | 1 ks | N-MOSFET, hlavní spínač napájení (47 A / 55 V) |

[//]: # (TODO: aktuaizovat)
---

## 7. Přílohy
* **Příloha A:** Technický výkres šasi
* **Příloha B:** Schéma zapojení
* **Příloha C:** Kompletní zdrojový kód

