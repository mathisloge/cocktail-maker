# GUI `ui/` structure and theme design

## Intent

`gui/ui/` (≈3.4k lines of Slint, ~40 files) has grown without clear rules. It is hard to tell where a new
file belongs, layer boundaries from CLAUDE.md are already broken, and the theme exists on paper but is
bypassed by local overrides. Two pains, both equally important:

1. Not knowing where something belongs or what it may depend on.
2. New screens don't match existing ones without manual fine-tuning.

The goal is a structure and a theme that make future design tuning a change in one place, not twenty.

## Goals

- Every file has one obvious home, and every folder has a one-sentence import rule.
- A theme layer that is the single source of colors, typography, spacing, sizes and motion.
- Text styling through fixed text roles instead of per-use `font-size`/`letter-spacing`/`font-weight`
  overrides.
- The rules are written down in `gui/ui/CONTRIBUTING.md`.

## Non-goals

- No redesign. The art-deco look (amber/black/cream, Space Grotesk) stays. Small visual improvements that
  fall out of unifying near-duplicate values are welcome (e.g. three slightly different letter-spacings
  collapsing into one).
- No runtime theme switching, no light theme (single dark theme per CLAUDE.md).
- No new features, no behavior changes in pages or bridges.
- No automated enforcement (no pre-commit hook or lint script) for now.

## Anti-goals

- A refactor that only moves files but still allows local style overrides. That recreates today's state
  within months.
- Breaking the C++ side beyond the minimum: bridges in `gui/src/` depend on generated global/struct/enum
  names.

## Current state (findings)

- `components/` mixes generic building blocks (Button, text, Chip, Separator, PulseDot, CornerDeco) with
  domain components (RecipeCard, GlassCard, GlassIcon, CommandItem, CommandListItem, RecipeTags,
  BoostText, SystemLog) that import domain structs from `data.slint`, contrary to CLAUDE.md.
- `data.slint` is a catch-all: log types, the navigation enum `Page`, recipe, pod/dispenser and glass
  structs.
- Name clash: enum `Page` (navigation, in `data.slint`) vs. component `Page` (page base, in
  `pages/page.slint`).
- Imports are inconsistent: sometimes through root barrels `components.slint`/`pages.slint`/`popups.slint`,
  sometimes directly; path style varies (`./x`, `x`, `./../x`).
- Admin is split between `ui/admin-area.slint` (root) and `ui/admin/`.
- No hex colors outside `palette.slint` (good), but ~122 raw `px`/`pt`/`rem` literals elsewhere and many
  local overrides on `SmallText`/`MediumText` (e.g. `mix-error-page.slint` restyles `SmallText` three
  different ways). Palette tokens mix raw and semantic meaning (`text-secondary` is "amber").
- The `admin-area.slint` rework (custom tabs replacing `TabWidget`) is finished and committed
  (`deb99fc`). That version is the starting point.
- 4 plain `Text {}` elements outside the text components carry their own font settings (station title in
  `app-window.slint`, `AdminTab`, ...). 13 uses of std-widgets (`Slider`, `Spinner`, `ProgressIndicator`,
  `ScrollView`, ...) take their colors from the default Slint style, not from the theme.

## Approaches considered

- **A) Layers by dependency** (chosen). Folders ordered so each may only import folders above it. One-line
  rule per folder, easy to check by eye without tooling. Cost: one feature's files are spread across
  folders.
- **B) By feature** (`features/recipes|mix|admin` + `shared/`). Rejected: almost all screens share
  `RecipeContext`/`ProcessContext`, so most code would land in `shared/` anyway, not worth it at this size.
- **C) Minimal** (keep folders, only split domain components out and fix the theme). Rejected: keeps the
  root barrels and the split admin area, i.e. the current mess.

Naming: the user asked for widely used names. `components/` stays for generic building blocks (familiar,
least churn). Domain components go into `widgets/` (known from Feature-Sliced Design, distinct from
`components/`), chosen over the shadcn-style `components/ui/` + `components/<domain>/`, which hides the
boundary inside one folder.

## Decided structure

```
gui/ui/
  theme/        colors (primitive + semantic), typography, spacing/sizes, motion, font file
  components/   generic building blocks, import only theme/
  types/        data.slint split by domain: recipe, pod, glass, log, navigation
  globals/      the *Context globals, import theme/ and types/
  widgets/      domain components (RecipeCard, GlassCard, GlassIcon, CommandItem, CommandListItem,
                RecipeTags, BoostText, SystemLog), import theme/, components/, types/
  pages/        pages; admin/ as a subfolder (admin-area.slint moves in here)
  popups/       popups
  app-window.slint
  CONTRIBUTING.md
```

Dependency order (a folder may import only from folders listed before it):
`theme → components → types → globals → widgets → pages/popups → app-window`.

- `components/` must not import `types/` or `globals/`. If a generic component needs data, it takes plain
  properties.
- `widgets/` may import `types/` but not `globals/`: they get data via properties, so they stay reusable
  and previewable. Pages and popups bind globals.
- `pages/` and `popups/` must not import each other, except a page may own a popup that is only used by it
  (e.g. dispenser calibration inside admin).

## Theme design

- **Colors, two tiers.** Primitive tokens name the raw value (`amber`, `amber-light`, `cream`, `ink`, ...,
  with alpha variants). Semantic tokens name the purpose (`surface`, `surface-card`, `surface-elevated`,
  `text-primary`, `text-interactive`, `text-label`, `text-muted`, `border`, `border-strong`, `status-*`,
  `overlay-*`, `log-*`, `boost-*`). Only semantic tokens are used outside `theme/`. Values stay as today.
- **Typography roles.** A small set of text components in `components/` (working set: `DisplayText`,
  `TitleText`, `BodyText`, `LabelText`, `CaptionText`), each bound to a typography token group (size,
  weight, letter-spacing, default color). Consumers set text, alignment, wrapping and, where a role
  allows it, a semantic color variant. They do not override size, weight or letter-spacing. If a use
  doesn't fit a role, the fix is a new role or token, not an override. `SmallText`/`MediumText`/`LargeText`
  are replaced.
- **Strict no-override rule** (user decision). Plain `Text {}` is allowed only inside `components/`
  (the text roles themselves and components that need it internally). Std-widgets with visible styling
  (`Slider`, `Spinner`, `ProgressIndicator`, `ScrollView`, ...) are used only through theme-aware wrapper
  components in `components/`, so their colors and sizes come from the theme. Pages, popups and widgets
  never import `std-widgets.slint` directly. Pure layout helpers (`VerticalBox`, `HorizontalBox`) are
  replaced by `VerticalLayout`/`HorizontalLayout` with theme spacing.
- **Sizes and spacing.** Existing spacing/radius/font-size scales stay. Recurring magic numbers become
  tokens (border widths, card/row heights, icon sizes, fixed panel widths like the 380px error-page
  column, the long-press duration where it is a design value).
- **Motion.** Mount animation duration/easing move from `Palette` to a motion token group.
- The font file and `default-font-family` live with the theme.

## Other decisions

- The **component** `Page` is renamed (e.g. `PageBase`), the **enum** `Page` keeps its name: the enum is
  used in ~26 places in C++, the component nowhere.
- Global, struct and enum names exported to C++ stay unchanged, so moving files needs no C++ change.
- Root barrels `components.slint`, `pages.slint`, `popups.slint` are removed. Imports go directly to the
  file, always with an explicit relative path (`./`, `../`).
- `gui/CMakeLists.txt` keeps `ui/app-window.slint` as entry. The font import moves with the theme.
- `gui/ui/CONTRIBUTING.md` documents: folder purposes, the dependency order, theme usage rules (semantic
  tokens only, text roles without overrides, no raw lengths except `0px`/`1px` hairlines and layout math),
  naming (kebab-case files, one exported component per file, CamelCase components) and how to add a new
  token or role. CLAUDE.md's GUI section gets a short pointer to it and is updated where it describes the
  folders.

## Verification

- `cmake --build --preset developer -j2` and `ctest` (GUI tests under `xvfb-run` if headless) stay green.
- Visual check: screenshots of every page and popup before and after, compared side by side. Differences
  must be either none or an intended unification.
- The builder decides which differences count as intended unification (user decision). No review gate:
  merge the ~10 letter-spacing values and the differing text defaults into the text roles in one pass, and
  list every visible change in the final summary so the user can check it.

## Left to the builder

- Exact primitive token names, the final list of text roles, and which magic numbers become tokens vs.
  stay local layout math.
- Split of `types/` into files and the exact rename of the `Page` component.
- Whether `theme/` exposes one global per concern (`Palette`, `Typography`, `Metrics`, `Motion`) or keeps
  `Palette`/`Layout` names. Prefer clear names and accept the rename churn inside Slint.
- Order of migration steps, keeping the build green after each commit. Commits go on the current branch,
  Conventional Commits, without a Co-Authored-By trailer.
- The exact set of std-widget wrappers and their names.
- How to capture the before/after screenshots.
