# System chapter 6 Runtime View: design

Project 3 of the docs programme (see `2026-11-14-docs-foundations-design.md`, section "The whole programme").

## Intent

The docs serve the owner and agents as a reliable basis for decisions, and later as the base for a CE technical file in a separate private repo.
Project 2 gave every boundary of the machine a contract (`IF-nn`) with requirements (`IF-nn-Rnn`).
The contracts say what must hold at a boundary, but not in which order things happen across the whole machine.

System chapter 6 is empty today, apart from a note that links the Station runtime view.
The Station runtime view has five sequences from the software's point of view (a Pod connects, a Guest mixes a drink, a bottle runs empty and gets refilled, an Operator calibrates a dispenser, a Pod comes back after a power loss).
The protocol page shows the message patterns.

This project writes the runtime view of the whole machine, including power and fluid, for normal operation and for faults.
Each step names the contract it relies on, so the chain from quality goal to contract to runtime stays traceable.
Behaviour that is not decided yet is shown as open, with a link to the open requirement, and nothing is invented.

## Goals

- Chapter 6 shows the four normal scenarios (owner's choice a):
  1. Switching on: main switch, Power Management Unit, Station start, Pods start and are found, the machine is ready.
  2. A drink end to end: the Guest orders, the Station plans the recipe, the Pods pump and switch valves, the liquid flows through the Station fluid hub into the glass, the load cells report the amounts.
  3. Refilling: the Operator refills or replaces a bottle, and the fill level and the weighing carry on.
  4. Switching off: what happens in which order, and which state survives.
- A subpage "Fault Scenarios" shows the five fault scenarios (owner's choice b):
  5. The link to a Pod breaks during a pour.
  6. The power fails during a pour, and the machine comes back.
  7. A Guest cancels, and the Station stops the Pods (`EmergencyStop`).
  8. A bottle runs empty during a pour.
  9. The pump lid is opened while the pump can move.
- Each scenario names the contracts and requirements it relies on and lists what is still open.
- The `architecture-docs` skill describes the scenario template, so agents in all repos write runtime scenarios the same way.
- Tests guard the structure.

## Non-goals

- No new design decisions.
  An open behaviour stays open until the owner decides it, and the decision gets an ADR and updates the contract and the scenario.
- No copy of the Station sequences or the protocol patterns.
  A scenario links them for the software details.
- No quality scenarios (`QS-nn`, project 4) and no safety measures (`SM-nn`, project 5).
  Those projects link the runtime scenarios.
- No scenarios beyond the nine.
  Calibration at machine level and a cleaning cycle are not in this project (the owner confirmed the list of nine).

## Anti-goals

- A step that shows behaviour no source and no owner decision supports.
- A sequence so detailed that it repeats the protocol page message by message.
  A System scenario shows the blocks and the people, and one arrow can stand for a whole exchange that the protocol page or a Station sequence details.
- A fault scenario that hides an open point by ending in a safe state that nobody decided.

## Constraints

- Prose follows `writing-style`, the style checker reports 0 errors and 0 `glossary-term` warnings, and the glossary terms apply.
- Diagrams follow `writing-diagrams`: D2 with `shape: sequence_diagram`, the shared style include, the edge kinds (data, power, fluid, mechanical, interaction, fault), no layout engine on a sequence diagram, at most 1400 px wide.
- Cross-component xrefs use a link text that names the component or an ID.
- Commits carry no `Co-Authored-By:` trailer.
- Only owner-confirmed content is committed.

## Approaches

- **B, chosen. Chapter 6 for normal operation, a subpage "Fault Scenarios" for the faults.**
  Normal operation and faults read separately, the fault page becomes the reference for the safety concept (project 5) and the CE file, and it follows the pattern of the Hardware and Contracts pages under chapter 5.
  Two pages to maintain.
- **A. All nine scenarios on the chapter 6 page.**
  One page, but long, and the fault scenarios that matter most sit far down.
- **C. One page per scenario.**
  Each scenario has its own link, but nine small pages repeat the same frame and need an index page.

## The decided parts

### Pages

- `docs/system/modules/ROOT/pages/runtime-view.adoc` keeps its title `= 6 Runtime View`.
  The "not written yet" note goes away.
  A short lead says what the chapter shows, links the fault page and the Station runtime view, and then the scenarios 1 to 4 follow.
- `docs/system/modules/ROOT/pages/fault-scenarios.adoc`, title `= Fault Scenarios`, nested under chapter 6 in `nav.adoc`.
  A short lead says what a fault scenario shows, then the scenarios 5 to 9 follow.

### The scenario template

Each scenario is a level-2 section with an explicit anchor, so other pages and the later projects can link it:

```adoc
[[switching-on]]
== Switching On

<one or two sentences: the trigger and the end state>

.Switching on
[d2, target="system-6-switching-on", format="svg", link=self]
----
include::system:ROOT:partial$d2-style.d2[]

shape: sequence_diagram
...
----
<one-line caption that names the question of the diagram>

=== Contracts
* xref:contracts.adoc#IF-03[IF-03], xref:contracts.adoc#IF-03-R04[IF-03-R04]: <one line on how the scenario relies on it>

=== Open
* <one line per open behaviour, with the open requirement it depends on, or "Nothing open.">

=== Details
* <links to the Station sequence, the protocol pattern or the Mechanics page that show the details, or "No further details.">
```

- The anchor is a short kebab-case name of the scenario, and the D2 target is `system-6-<anchor>`, after the `<component>-<chapter>-<topic>` scheme of writing-diagrams, so it never clashes with the Station targets (`runtime-...`) (changed after the plan review).
- The participants of a sequence are taken from the blocks and parties of chapter 5 and chapter 3: Guest, Operator, Power Grid, Power Management Unit, Station, Pod, and, where the fluid matters, bottle and glass.
  A scenario shows only the participants it needs.
- A step whose behaviour is open keeps the edge class of its kind (a `fault` edge only for a fault that occurs) and ends its label with `(open, IF-01-R11)`, naming the open requirement or, when the open point is only an open question of a contract, the contract ID (my call).
  No new diagram class is needed.
  The Open section names it.
- Every Open bullet names at least one defined `IF-nn` or `IF-nn-Rnn`, or the section holds exactly the line "Nothing open." (my call).
- A fault scenario's first sentence names the end state, or says that the end state is open.

### The skill

The `architecture-docs` skill gets a section "Runtime scenarios" with the template above and the rules: one arrow may stand for a whole exchange, open behaviour is shown as open, every scenario names its contracts, the Station or subproject runtime view keeps the software details.
The plugin version goes from 0.6.0 to 0.7.0 in `plugins/cocktail-maker-docs/.claude-plugin/plugin.json` and `.claude-plugin/marketplace.json`, and the plugin test's minimum version goes to 0.7.0.

### Tests (in `docs/tests/test_docs_structure.py`)

- `runtime-view.adoc` has the four scenario anchors and `fault-scenarios.adoc` the five, in the order of the Goals list.
- Every scenario section has exactly one D2 block with `shape: sequence_diagram` and the subsections `Contracts`, `Open`, `Details` in this order.
- Every `Contracts` list names at least one contract, and every `IF-nn` or `IF-nn-Rnn` it names is defined on the Contracts page.
- Every Open bullet follows the rule above, and every `(open, …)` ID in a diagram label also appears in the scenario's Open section.
- The anchor tests compare the anchors on each page with the expected lists as an ordered prefix, so a page with only its lead passes and a wrong or misordered anchor fails (my call).
  The last task of each checkpoint adds an assert that its page is complete.
- `fault-scenarios.adoc` is in `nav.adoc` directly under chapter 6, and chapter 6 links it.
- The Station runtime view gets one line that links System chapter 6 and the Fault Scenarios page, and its title stays (my call).
- The existing diagram tests (style include, classes, width) cover the new diagrams.

### Owner checkpoints

1. **Normal operation.**
   An agent drafts the scenarios 1 to 4 in the scratchpad from the sources, each fact and step with its source, each assumption marked, and the questions for the owner.
2. **Faults.**
   An agent drafts the scenarios 5 to 9 the same way.

Sources: the contracts page, the protocol page, the power page, the safety page, the Hardware page, the ADRs, the Station runtime view and the Station code (`cocktail-maker/`), and the Mechanics docs (`cocktail-maker-3d`, `origin/main`).

### Execution

The tasks (my call):
1. The two pages with their leads (the fault page with its lead only), the nav entry, the Station back-link, the structure tests, the skill section and the version.
2. Checkpoint 1: an agent drafts the scenarios 1 to 4 in the scratchpad (`<scratchpad>/runtime/`), with a `questions.md` that lists, per scenario, every step as "from <source>" or "assumption" and the questions with suggested answers. Execution stops until the owner confirms.
3. The confirmed scenarios 1 to 4 go onto chapter 6, with the completeness assert for that page.
4. Checkpoint 2: the same for the scenarios 5 to 9. Execution stops until the owner confirms.
5. The confirmed scenarios 5 to 9 go onto the fault page, with its completeness assert.

The repo stays green after every task.

Same as project 2: one agent per task with a review after each, the two checkpoints, a final review of the whole branch, then the commits are squashed into one and merged into `main` locally.
Every commit carries no `Co-Authored-By:` trailer.
The plan describes the structure and the process, not the scenario text.

## Left to the builder

- The wording of the leads and of the skill section.
- The exact layout of each sequence diagram within the diagram rules.
- How the tests parse the sections.
- The task split of the plan.
