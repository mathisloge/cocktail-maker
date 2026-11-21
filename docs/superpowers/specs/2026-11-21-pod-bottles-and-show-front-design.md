# Plastic Pod bottles and the Station show front

Date: 2026-11-21.
Source: the input of the Mechanics design round (cocktail-maker-3d) on HDPE reservoirs in the Pod, and the System architect session on the same day.

## Intent

The Mechanics round asks to hold the ingredients of a Pod in HDPE or PP containers instead of glass bottles.
Square 2 l containers make the Pod half as deep and give it twice the volume, and they do not break when an Operator lifts them out.
In the same session the user moved the visible part of the machine to the Station.
The Station gets a show front with 8 acrylic columns, and the Pods stand hidden wherever there is room.
The Station then looks the same with any number of Pods.

This is a docs change in the System component.
No Station code changes now.

## Goals

* A Pod may hold its ingredients in food-contact HDPE or PP bottles, and the food-safety argument (QG-02) still holds.
* The System docs say who owns the show front and what it may and may not do.

## Non-goals

* Choosing a bottle product.
  Mechanics asks the makers for a declaration of compliance.
* Choosing the electronics of the show front (WLED controller, own controller or Raspberry Pi drivers).
* Defining the light animations or what a column stands for.
* Placing the carbonated bottles of SYS-013.
  This stays with Mechanics, and SYS-013 is unchanged.

## Anti-goals

* A rule that accepts "food-safe" or a glass-and-fork symbol without a declaration that covers alcohol.
* A show front that can stop or delay a drink, or that a Pod knows about.

## Facts found

* QG-02 already says "only food-safe parts touch it" and names no material, so QG-02 does not change.
  The list of materials lives in SYS-009, QS-09, IF-05-R01 and IF-06-R01.
* SYS-009 names only tubes and stainless steel parts, so today it does not even cover the glass bottle.
* EU 10/2011 (consolidated PDF on EUR-Lex, version 14.07.2026):
  * Annex III: simulant D1 (50 % ethanol) "shall be used for alcoholic foods with an alcohol content of above 20 %".
    Simulant C covers beverages of 6 to 20 % vol, and A and B cover aqueous and acidic foods such as juices.
    No simulant exists for spirits above 50 % vol.
  * Annex V Table 1: a contact of more than 1 day and up to 3 days is tested for 3 days.
    Table 2: a contact between 20 °C and 40 °C is tested at 40 °C.
    Table 3: OM2 (10 d at 40 °C) covers "any long-term storage at room temperature or below".
  * Annex IV item 8 lists the food types, the time and temperature and the surface-to-volume ratio that a declaration of compliance covers.
* No candidate bottle has a published declaration that covers alcohol.
  Rixius RX2-M0-2000-60 has none that we could find.
  Hünersdorff ECO 2 L (article 810250) is called "lebensmittelgeeignet" without simulants, and its 29 mm opening is narrow for a dip tube and a vent.
* The silicone tube is released for D1, so spirits above 50 % vol are already a gap today.
* An ingredient stays in a bottle for at most 48 h (user decision).
  Ethanol loss through the wall and stress cracking do not matter over that time, and aroma uptake still does.

## Approaches

### Wetted parts

1. **Allow food-contact HDPE and PP for bottles, caps and fittings, with a declaration condition and a 48 h limit.**
   Chosen.
   The condition is checkable, and the 48 h limit keeps the needed test conditions low.
2. Keep glass bottle sets as hidden bottles.
   Rejected by Mechanics: the Pod is twice as deep and holds half the volume.
3. Allow HDPE and PP without a time limit.
   Rejected: it needs OM2 and specific migration tests of 10 d, and it brings back ethanol loss and stress cracking.
4. Bottles with an outlet at the bottom and a quick coupling, as in commercial mobile drink dispensers.
   Rejected: liquid always stands at the coupling, and a leak empties the bottle onto the load cell and toward the electronics (IF-07-R04).

### Show front

1. **The show front is a block of the Station, driven only by the Station from its own events.**
   Chosen.
   It needs no protocol change, and the Station looks the same with any number of Pods.
2. Columns, air pumps and LED lines on each Pod, driven by the free outputs of the Octopus.
   Rejected: the LED lines would cross the Pod to Station boundary (IF-07), and the look of the machine would depend on the number of Pods.

## Decisions

### SYS-015: Food-contact plastic bottles, superseding SYS-009

* These parts may touch an ingredient or a drink: food-grade tubes, stainless steel parts, glass, and HDPE or PP.
* HDPE and PP are allowed for bottles, caps and fittings, and not for tubes.
* An HDPE or PP part needs a declaration of compliance under EU 1935/2004 and EU 10/2011.
  The declaration covers simulants A, B, C and D1 (or all food types), at least 3 days at 40 °C, and repeated use.
* No 3D printed part touches a liquid (unchanged from SYS-009).
* An ingredient stays in a bottle for at most 48 h, and then the Operator empties and rinses the bottle.
* Each bottle holds one ingredient, and an Operator replaces a bottle that smells or is worn.
* SYS-009 gets the status Superseded and points at SYS-015.

### SYS-016: The Station owns the show front

* The show front has 8 acrylic columns with water and 30 to 40 % propylene glycol, one air pump per column and one WS2811 LED line (24 V) per column.
* The Station alone drives it, from events it already has: pour started, pour finished, ingredient, recipe, idle and fault.
* A rule in the Station software maps these events to a column and an effect.
  The animations and the meaning of a column stay open.
* No Pod knows about the show front, and the protocol does not change.
* A failed show front never stops or delays a drink.
* Its effects never block the Station execution context (STA-003).
* The electronics stay open, with a WLED controller on a wired link as the favourite.

## Page changes

* `adr/pages/015-food-contact-plastic-bottles.adoc` (new), `adr/pages/016-station-show-front.adoc` (new), both in `nav.adoc` and on the chapter 9 page.
* `adr/pages/009-wetted-parts.adoc`: status Superseded, `:adr-superseded-by:` SYS-015.
* `quality-requirements.adoc`: QS-09 takes the material list of SYS-015, and its checks point at SYS-015.
* `contracts.adoc`:
  * IF-05: R01 takes the material list and points at SYS-015.
    New R07 for the 48 h limit and new R08 for one ingredient per bottle.
    The open question on the barbed fittings is answered by SYS-015 and leaves the list.
    The guarantees name SYS-015 instead of SYS-009.
  * IF-06: R01 and the guarantees point at SYS-015.
  * IF-07: R04 also names the liquid of the show front.
* SYS-016 states that the liquid of a show column never reaches the Raspberry Pi, the display or the power supply, and never touches the flow path of a drink.
  The Station has no contracts page, so the Station risks page lists the leak protection of the show front as an open point.
* `building-blocks.adoc` and `hardware.adoc`: the show front as a block of the Station.
* Spirits above 50 % vol are covered by no release of the tube or the bottle.
  This is a small risk at 48 h of contact.
  The System risks page is still a placeholder, so the gap goes into the bad consequences of SYS-015 and into the open questions of IF-05.
* QG-02 does not change.

## Terms

* "Bottle" stays the term for the container that holds an ingredient in a Pod, whatever its material.
  The glossary and many pages already use it, for example "Fill level".
* "Show column" is the new term for an acrylic column of the show front, so that it is never mixed up with a bottle.
  It goes into the System glossary together with "Show front".

## Left to the builder

* The exact wording of the ADRs and requirements, following `writing-style` and `writing-adrs`.
* The D2 diagram changes on the building block and hardware pages.
