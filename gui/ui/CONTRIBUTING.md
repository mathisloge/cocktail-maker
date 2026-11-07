# Contributing to the Station UI

These rules keep `gui/ui/` easy to find your way around and the look easy to tune.
A design change should be an edit in `theme/`, not in twenty pages.

## Folders

| Folder        | Contains                                                         | May import                        |
|---------------|------------------------------------------------------------------|-----------------------------------|
| `theme/`      | Colors, typography, layout sizes, motion, the font file          | nothing                           |
| `components/` | Generic building blocks: Button, Chip, text roles, wrappers      | `theme/`                          |
| `types/`      | Structs and enums shared with C++, split by domain               | `types/`                          |
| `globals/`    | The `*Context` globals that the C++ bridges fill                 | `theme/`, `types/`                |
| `widgets/`    | Domain building blocks: RecipeCard, GlassCard, SystemLog, ...    | `theme/`, `components/`, `types/` |
| `pages/`      | One file per page. `pages/admin/` holds the admin area           | everything above                  |
| `popups/`     | Popups that `app-window.slint` opens                             | everything above                  |

`app-window.slint` is the entry point. It may import everything.

Rules on top of the table:

- `components/` never know about the domain. A component that needs data takes plain properties.
- `widgets/` never import `globals/`. They get their data through properties, so pages decide where it comes from.
- Pages and popups do not import each other. A popup that only one page uses lives next to that page,
  like `pages/admin/dispenser-calibration.slint`.

## Theme

- `theme/colors.slint` has two tiers.
  `Primitives` holds the raw values and is not exported.
  `Palette` holds the semantic tokens (`surface`, `text-interactive`, `border`, ...) and is the only one you use.
- `Typography` defines one group per text role (`<role>-size`, `<role>-weight`, `<role>-spacing`).
- `Layout` holds spacing, radii and the sizes that more than one file shares.
- `Motion` holds animation durations and easings.

Outside `theme/` there are no hex colors.
Raw lengths and durations are allowed for `0px`, `1px` hairlines, layout math such as `parent.width - 1px`,
and geometry that belongs to a single component and is used in that file only.
When such a value repeats within the file, give it a named `private property`.
When a second file needs it, move it to `Layout` or `Motion`.

## Text

Use the roles from `components/text.slint`: `DisplayText`, `HeadlineText`, `BrandText`, `TitleText`,
`BodyText`, `LabelText`, `CaptionText` and `DataText`.

- Set `text`, `tone`, `emphasized`, alignment, `wrap` and `overflow`.
- Never set `font-size`, `font-weight`, `letter-spacing` or `color` on a role.
- `tone` takes a `TextTone`. Data-driven colors, such as log levels, have their own tones.
- `emphasized: true` raises the weight to semi-bold.

If no role fits, add a token group to `Typography` and a role to `components/text.slint`, instead of overriding one.
A plain `Text {}` is only allowed inside `components/`, where it reads its values from `Typography`, like the label of `Button`.

## Std-widgets and own controls

Only `components/` import `std-widgets.slint`.
`ScrollArea`, `SelectBox` and `NumberBox` wrap a std-widget, so theming one is a change in its wrapper.

`LoadingSpinner`, `ProgressBar` (`components/`) and `BoostSlider` (`widgets/`) are drawn from Slint primitives.
Each one implements the interface of the std-widget it replaces, for example `implement SliderInterface <=> self;`
with the interfaces imported from `"std-widget-interfaces.slint"`.
The component declares every property and callback of the interface itself, and the compiler checks them.
Give a control a `FocusScope` of size 0 with `forward-focus`, like the std slider does.
A full-size `FocusScope` on top of a `TouchArea` takes its pointer events.

`Button` has two sizes. `compact: true` gives 48 px for rows and tool bars, otherwise it is 75 px tall.
`ListView` cannot be wrapped, because the Slint compiler treats it specially by name.
Use a `ScrollArea` with a `VerticalLayout` for short lists.
Use plain `VerticalLayout`/`HorizontalLayout` with `Layout` spacing instead of `VerticalBox`/`HorizontalBox`.

## Slint project file

`slint-project.json` sets the main file, the experimental features (`interfaces`) and the edition.
CMake and the Slint editor extension both read it.
The edition is still 1.18.
Edition 1.19 adds the size limits of `if` and `for` children to their parent,
and several pages report a fixed maximum width, so the window would resize with the page.
Those pages need layout fixes before the edition can go up.

## Naming

- Files are kebab-case and export one component, for example `recipe-card.slint` exports `RecipeCard`.
- Components are CamelCase, properties and callbacks kebab-case.
- Imports use explicit relative paths (`./`, `../`). There are no barrel files.
- The names of globals, structs and enums are part of the C++ API.
  Renaming one means changing the bridges in `gui/src/` as well.

## Checking a change

The hidden snapshot test renders every page and popup to `.ppm` files:

```sh
CM_SNAPSHOT_DIR=/tmp/before ./build/runtime/cocktail-maker-gui-tests "[.snapshot]"
# make the change, rebuild
CM_SNAPSHOT_DIR=/tmp/after ./build/runtime/cocktail-maker-gui-tests "[.snapshot]"
```

Compare the two directories with any image diff tool.
Some screens animate all the time and differ between two runs without any change.
The boot screen spinner, the progress bar and the pulse dots differ by a few hundred pixels.
The breathing ring of the error page and the success page can differ by more than ten thousand pixels,
so compare those two by eye.

Screens that need interaction, such as a selected Pod in the admin area, can be inspected live through
Slint's embedded MCP server.
Configure with `-DCM_SLINT_MCP=ON`, then open a page and keep it open:

```sh
CM_INSPECT_PAGE=admin_area_page SLINT_MCP_PORT=8765 ./build/runtime/cocktail-maker-gui-tests "[.inspect]"
```

The server answers MCP requests at `http://127.0.0.1:8765/mcp`.
It can take screenshots, list elements by id and click them.
`CM_INSPECT_PAGE` takes the snake_case name of a `Page` value, such as `cocktail_detail_page`.
The test selects the software renderer, because the FemtoVG renderer cannot take screenshots.
