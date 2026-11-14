---
name: writing-diagrams
description: 'Use when drawing, changing or reviewing a diagram in any Sir-Mix-A-Lot docs (architecture chapters, protocol pages, guides): which diagram type to use, what goes in boxes and on arrows, layout and naming. Diagrams are D2, rendered through Kroki.'
---

# Writing diagrams

This skill builds on `writing-style`.
Follow `writing-style` for titles, captions and labels.
Follow `architecture-docs` for the chapter structure, and follow this skill for every diagram in it.
All diagrams are D2, rendered through Kroki.

## One diagram type per chapter

Each arc42 chapter has one diagram type.

| Chapter | Diagram type | It shows |
|---|---|---|
| 3 Context and Scope | context diagram | the machine as one box, with its actors and external systems |
| 5 Building Block View | building block diagram | the blocks of one zoom level |
| 6 Runtime View | sequence diagram | one scenario as messages in time order |
| 7 Deployment View | deployment diagram | the hardware and what runs on it |

A building block diagram shows one zoom level.
Never mix zoom levels in one picture.
To show the inside of a block, draw a new diagram on the chapter 5 page of that block.

```text
Wrong: System chapter 5 shows the Station, the Pods and the modules inside the Station.
Right: System chapter 5 shows the Station and the Pods as blocks.
       Station chapter 5 shows the modules inside the Station.
```

A sequence diagram sets `shape: sequence_diagram` at the top level of the block, right after the include line.
Never put it on a container, because a container without a class renders as a white box on the dark page.
The `.Title` line names the scenario.

```d2
shape: sequence_diagram
station: Station {class: block}
pod: Pod {class: block}
station -> pod: sends the dispense command {class: [data; key]}
pod -> station: reports the dispensed mass {class: data}
```

## Boxes

Every box carries a name, a type and the technology, separated by `·`.
The name is exactly the glossary term.
A person shape for an actor carries the name, and a context diagram may add a short description line below it.
The `actor` class fixes the size of the figure, so never set a size on a person.

```d2
# Wrong
host: Main controller
# Right
station: "Station · Raspberry Pi · Slint UI" {class: block}
pod: "Pod · Pod controller with Zephyr firmware" {class: block}
```

Every shape and every container carries exactly one shape class.
The class follows what the shape is in that diagram.

| Class | Put it on | Outline |
|---|---|---|
| `actor` | a person, such as the Guest or the Operator | light amber `#e8b86d` |
| `external` | a system outside the subject of the diagram, such as the Power Grid, or the Pods in the Station context diagram | sand `#d9cfae`, dashed |
| `boundary` | the frame of the machine or of a subproject, a group (a group of parts, a `loop` or `alt` group of a sequence diagram), and a note in a sequence diagram | amber `#c8913a`, width 2 |
| `block` | a building block without a specific area, such as the Station or a Pod | amber `#c8913a` |
| `power-part` | a power supply part: mains inlet, main switch, power supply, DC/DC converter | orange red `#d0654a` |
| `data-part` | a computer, a controller or other electronics: Raspberry Pi, Pod controller, stepper driver, servo driver, load cell amplifier | light slate `#86aebd` |
| `mechanical-part` | an actuator or a mechanical part: stepper motor, pinch valve, load cell | violet `#8c7ee0` |
| `fluid-part` | a part that carries liquid: fluid hub, dispenser unit | green `#5cad75` |
| `interaction-part` | a part that a person uses: touch display | light amber `#e8b86d` |

A part class changes only the outline colour of a `block`, and every outline is solid, because dashes belong to edges.
There is no fault class for boxes.

| Diagram | Part classes |
|---|---|
| detailed (hardware, power) | on every part |
| overview (context, building blocks, naming) | only on a box that clearly belongs to one area, such as the Power Management Unit; every other box stays `block`, `external` or `actor` |

The same Pod is `external` in the Station context diagram and `block` in a System diagram.
A note in a sequence diagram is part of the scenario and not a system outside it, so it uses `boundary` and never `external`.

## Arrows

Every arrow points one way, and its label is a verb or an intent.
A physical quantity carries its unit, and a protocol goes in the label.
Every arrow carries exactly one kind class, or `normal` when it has no kind.
The meanings are fixed site-wide, so a colour means the same in every diagram of every component.

| Class | Kind | Colour | Line |
|---|---|---|---|
| `power` | electrical power | orange red `#d0654a` | solid, width 2 |
| `data` | data and communication | light slate `#86aebd` | dashed (`stroke-dash: 4`), width 2 |
| `fluid` | liquid flow | green `#5cad75` | solid, width 2 |
| `mechanical` | a physical, mechanical link | violet `#8c7ee0` | short dashes (`stroke-dash: 2`), width 2 |
| `interaction` | a person using the machine, or the machine telling or asking a person | light amber `#e8b86d` | solid, width 2 |
| `fault` | an actual fault or an error path | pink `#e07aa8` | solid, width 2 |
| `normal` | an arrow without a kind, such as "uses" | muted sand `#867f6b` | solid, width 1 |

Colour is never the only cue, because some readers cannot tell the colours apart.
The label and the line style carry the meaning too, so write a label that names the kind of the connection.

| In a sequence diagram | Class |
|---|---|
| a message that stands for an exchange with a Pod, also when it goes through the Pod Communication block of the Station | `data` |
| an action of the Guest or the Operator, and what the UI shows them | `interaction` |
| a warning or a request that the machine shows to a person | `interaction` |
| an actual fault or error path, such as a protocol mismatch, a timeout or a pour that counts as an empty bottle | `fault` |
| any other call inside the Station | `normal` |

A diagram highlights at most one connection, the most important one.
Add `key` after its kind, as in `{class: [data; key]}`.
`key` adds only width 3 and a bold label, and the arrow keeps the colour and the line style of its kind.
An arrow without a kind can be the key edge too, as `{class: [normal; key]}`.
In a D2 class array the later class wins, so `key` comes last, or the width of the kind replaces the key width.

```d2
# Wrong
station <-> pod: data
power_management_unit -- pod
# Right
station -> pod: sends commands over USB CDC {class: [data; key]}
power_management_unit -> pod: supplies 24 V DC {class: power}
```

## Reading direction

Flows go left to right, so set `direction: right`.
Hierarchies go top to bottom, so set `direction: down`.
A diagram with more than about 10 boxes is too large.
Split it into two diagrams, or zoom out one level.
The page shows a diagram at most about 800 px wide, so a wider diagram shrinks its labels.
Keep the rendered SVG at most 1400 px wide.
To make it narrower, break long labels with `\n`, set `direction: down`, or put related shapes in a `boundary` group.
A docs test checks the width of every diagram in CI.
The context example below is a flow, and the block form example is a hierarchy.

## Layout engine

The shared style selects the TALA layout engine, and TALA is the default.
TALA usually suits clustered architecture views with containers, such as a building block or a context diagram.
`elk` or `dagre` often suit a long flowing graph or a DAG, such as a chain, a pipeline or a sequence of steps.
The comparison decides, not the diagram type.
Render the diagram with TALA and the engine you consider, look at both at 800 px, and keep the one that reads better.
Keep another engine only when it stays at most 1400 px wide.
`dagre` places the label of a container above its box, while TALA and `elk` place it inside.
To choose another engine, add exactly this line right after the include line:

```d2
vars: { d2-config: { layout-engine: elk } }
```

The only engines a diagram may set are `elk` and `dagre`, because the style already sets `tala`.
The line must come after the include, because the later `vars` wins and the style would override an earlier one.
A sequence diagram ignores the engine, so it never sets one.
Never edit the shared style to change the engine.

## Consistency

The same element keeps the same name, shape and colour in every diagram.
Actors are person shapes, external systems are dashed, and the machine or the subproject is a framed boundary.
The class of an element gives it its shape and colour, so set the class and nothing else.
The System page Diagram Conventions is the legend of the whole site.
Never draw a legend box inside a diagram.
A caption may name the kinds that a diagram uses or link the legend, also from another component, with `xref:system::diagram-conventions.adoc[diagram conventions]`.
Anything unusual goes in the caption.

```d2
direction: right
guest: Guest {class: actor}
operator: Operator {class: actor}
power_grid: "Power Grid · mains socket · 230 V AC" {class: external}
machine: "Sir-Mix-A-Lot · cocktail-maker" {class: boundary}
guest -> machine: orders a drink {class: [interaction; key]}
operator -> machine: refills, cleans and calibrates {class: interaction}
power_grid -> machine: supplies 230 V AC {class: power}
```

## Title, caption and source

Every diagram has a title and a one-line caption.
The caption says which question the diagram answers.
Put the `.Title` line above the block and the caption sentence right below it.
The diagram source sits next to its text in the `.adoc` file, never in a separate file.
The `target` name follows `<component>-<chapter>-<topic>`, for example `system-3-context` or `station-6-dispense`.
The attribute `link=self` makes a click on the diagram open its image at full size.

```adoc
.Modules of the Station
[d2, target="station-5-building-blocks", format="svg", link=self]
----
include::system:ROOT:partial$d2-style.d2[]
direction: down
gui: "cm.gui · C++ module · Slint" {class: block}
lib: "cm · C++ module · stdexec" {class: block}
pod: "Pod · Pod controller with Zephyr firmware" {class: external}
gui -> lib: calls the domain model {class: normal}
lib -> pod: sends commands over USB CDC {class: [data; key]}
----
Which modules make up the Station, and which module talks to the Pods?
```

## Visual style

The shared style (colours, shapes) lives in `docs/system/modules/ROOT/partials/d2-style.d2`.
The script `tools/palette/generate_palette.py` generates it from `gui/ui/theme/colors.slint`, so never edit it by hand.
Every D2 block starts with this line, as its first line inside the block delimiters:

```adoc
include::system:ROOT:partial$d2-style.d2[]
```

The include resolves from every component, so Station and Mechanics diagrams use the same line.
Write no `style` blocks and no local `classes` block, and set no `shape` other than `sequence_diagram`.
The shared style selects the TALA layout engine, and "Layout engine" says when and how a diagram chooses another one.
The short D2 examples in this skill leave the include line out to stay short, and a real block always has it.
The tables in "Boxes" and "Arrows" list every class.
A shape, a container or an edge without a class renders in the default light colours of D2, which is a defect, and a docs test fails on it.

Every docs build fetches the diagrams from Kroki and serves them as SVG files.
A local build uses `kroki.io`, and CI uses its own pinned Kroki.
To check a diagram as an author, put the content of `d2-style.d2` before the snippet, because the include line only resolves inside the build.
Then send the result to `kroki.io`.
This is fine in every project repo, also in cocktail-maker-3d, because the docs of all repos are public.
A local Kroki (`yuzutech/kroki:0.36.0`) or `d2` v0.9.0 or newer works too, for example when you are offline.

## Before you finish

- The diagram type fits the chapter, and the diagram shows one zoom level.
- It has a title, a caption that names its question, a `target` that follows the pattern, and `link=self`.
- The rendered SVG is at most 1400 px wide.
- Every box has a glossary name, a type and the technology.
- Every arrow points one way and carries a verb, with units and protocols in the label.
- The block starts with the include line for the shared style.
- Every shape and every container has exactly one shape class, and a detailed diagram gives every part its part class.
- Every edge has exactly one kind class or `normal`, at most one edge adds `key` as `[<kind>; key]`, and every label names the kind of its connection.
- There is no legend box, and the caption names or links the legend when the diagram needs it.
- A sequence diagram sets `shape: sequence_diagram` at the top level and no layout engine.
- A diagram that sets a layout engine sets `elk` or `dagre` in one `vars` line right after the include.
- Flows go left to right, hierarchies go top to bottom, and there are no more than about 10 boxes.
- The prose follows `writing-style`, and the checker reports no errors.
