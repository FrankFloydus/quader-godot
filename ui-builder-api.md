# Quader Godot UI Builder API Draft

This document defines the proposed low-level C++ UI builder for the Quader Godot plugin.

The goal is to make Quader UI code read as intent:

```cpp
auto body = ui::column("SettingsBody")
	.match_parent_width()
	.hug_content_height()
	.gap(12);
```

instead of raw Godot mechanics:

```cpp
body->set_h_size_flags(Control::SIZE_EXPAND_FILL);
body->add_theme_constant_override(StringName("separation"), 12);
```

The builder creates real Godot `Control` nodes. It does not create a custom UI framework, retained diff engine, or reactive runtime. Godot remains the owner of the scene tree and layout.

## Design Rules

- Create parent and child nodes separately.
- Configure each node with readable methods.
- Insert children explicitly with `add(...)`.
- `add(...)` accepts one or many children.
- Method names describe UI intent, not Godot internals.
- All colors are HTML strings, for example `"#191919"` or `"#ffffff2e"`.
- Godot-specific escape hatches exist, but normal UI code should not need `add_theme_*`, `StringName`, size flags, anchors, or stylebox setup.
- Builder objects are temporary handles around Godot nodes. After `add(...)`, Godot owns the child node through the scene tree.

## Gum-Inspired Rules

The API should borrow the good parts from Gum:

- `add(...)` remains explicit. Create parent and children separately, then attach children to the parent.
- `dock(...)` and `anchor(...)` are first-class layout words for parent-relative placement.
- Stack containers expose `gap(...)`, direction, wrapping, and child sizing without hiding composition.
- Grid/form cells are local layout contexts. A child docked to a cell fills the cell, not the whole grid.
- Visual states use typed state names such as `Normal`, `Hover`, `Pressed`, `Focused`, and `Disabled`.

Example:

```cpp
auto top_bar = ui::surface("QuaderTopBar")
	.dock(ui::Dock::Top)
	.height(24)
	.background(kTopBarSurfaceColor);

auto edit = ui::menu_button("Edit")
	.min_size(42, 24)
	.font_size(14)
	.font_color(kMenuTextColor);

edit.state(ui::State::Normal).background(kTopBarSurfaceColor);
edit.state(ui::State::Hover).background(kMenuHoverSurfaceColor);
edit.state(ui::State::Pressed).background(kMenuPressedSurfaceColor);
edit.state(ui::State::Focused).empty();
```

## Naming Policy

Avoid vague or Godot-editor-specific names:

| Avoid | Use | Meaning |
|---|---|---|
| `full_rect()` | `match_parent()` | Make bounds match the parent. |
| `expand()` | `fill_available()` / `grow()` / `stretch()` | Split allocation, growth, and stretching. |
| `fill()` | `match_parent()` / `stretch()` / `fill_available()` | `fill` alone is ambiguous. |
| `margin()` for child inset | `padding()` | Godot `MarginContainer` offsets children inward. |
| `scroll()` as a noun | `scroll_area()` | Reserve `scroll_to_*()` for actions. |
| `size_flags()` | `fill_available()` / `hug_content()` / `stretch()` | Hide Godot flags behind intent. |
| `anchors_preset()` | `match_parent()` / `dock(...)` / `anchor(...)` / `pin_edges()` | Hide Godot anchor presets. |

## Core Namespace

All public builder API lives under:

```cpp
namespace quader_godot::ui;
```

Normal call sites may use:

```cpp
namespace ui = quader_godot::ui;
```

## Core Types

```cpp
namespace quader_godot::ui {

template <typename GodotControl>
class ControlNode;

using Node = ControlNode<godot::Control>;
using Panel = ControlNode<godot::PanelContainer>;
using Row = ControlNode<godot::HBoxContainer>;
using Column = ControlNode<godot::VBoxContainer>;
using Grid = ControlNode<godot::GridContainer>;
using ScrollArea = ControlNode<godot::ScrollContainer>;
using Label = ControlNode<godot::Label>;
using Button = ControlNode<godot::Button>;
using MenuButton = ControlNode<godot::MenuButton>;
using PopupMenu = ControlNode<godot::PopupMenu>;
using ColorField = ControlNode<godot::ColorPickerButton>;
using NumberField = ControlNode<godot::SpinBox>;
using Window = ControlNode<godot::Window>;

class GridCell;
class StateStyle;

struct Size {
	float width = 0.0f;
	float height = 0.0f;
};

struct Insets {
	float top = 0.0f;
	float right = 0.0f;
	float bottom = 0.0f;
	float left = 0.0f;

	static constexpr Insets all(float value);
	static constexpr Insets vertical_horizontal(float vertical, float horizontal);
	static constexpr Insets symmetric(float inline_axis, float block_axis);
};

enum class Align {
	Start,
	Center,
	End,
	Stretch,
	Baseline,
};

enum class Scrollbar {
	Never,
	Auto,
	Always,
	Reserve,
	GrowThenScroll,
};

enum class TextRole {
	Body,
	PanelTitle,
	SectionTitle,
	Menu,
	Caption,
};

enum class Justify {
	Start,
	Center,
	End,
	SpaceBetween,
	SpaceAround,
	SpaceEvenly,
};

enum class Dock {
	Fill,
	Top,
	Bottom,
	Start,
	End,
};

enum class Anchor {
	TopStart,
	Top,
	TopEnd,
	Start,
	Center,
	End,
	BottomStart,
	Bottom,
	BottomEnd,
};

enum class State {
	Normal,
	Hover,
	Pressed,
	Focused,
	Disabled,
	Checked,
	CheckedHover,
};

} // namespace quader_godot::ui
```

## Factory Functions

Factories allocate Godot nodes and return typed builder handles.

```cpp
namespace quader_godot::ui {

Window window(const char *title);
Panel panel(const char *name);
Panel surface(const char *name);
Row row(const char *name);
Column column(const char *name);
Grid grid(const char *name);
Grid form(const char *name);
ScrollArea scroll_area(const char *name);
Label label(const char *text);
Label section_label(const char *text);
Button button(const char *text);
MenuButton menu_button(const char *text);
ColorField color_field(const char *label, godot::Color value);
ColorField color_field(const char *label, const char *html_color);
NumberField number_field(const char *label, double value);
Node spacer(const char *name = "Spacer");

template <typename GodotControl>
ControlNode<GodotControl> control(const char *name);

} // namespace quader_godot::ui
```

## Universal Node Methods

Available on every `ControlNode<T>`.

```cpp
auto node = ui::panel("Example")
	.name("DifferentName")
	.tooltip("Shown on hover")
	.visible(true)
	.disabled(false)
	.mouse_filter(MouseFilter::Pass)
	.focus_mode(FocusMode::None);
```

Full API:

```cpp
ControlNode &name(const char *name);
ControlNode &tooltip(const char *text);
ControlNode &visible(bool enabled = true);
ControlNode &hidden(bool enabled = true);
ControlNode &disabled(bool enabled = true);
ControlNode &focusable(bool enabled = true);
ControlNode &focus_mode(FocusMode mode);
ControlNode &mouse_filter(MouseFilter filter);
ControlNode &clip_contents(bool enabled = true);

GodotControl *node() const;
GodotControl &ref() const;

template <typename Fn>
ControlNode &tap(Fn &&fn);
```

`tap(...)` is the escape hatch for rare Godot calls:

```cpp
auto advanced = ui::button("Advanced")
	.tap([](Button &button) {
		button.set_shortcut_context(...);
	});
```

## Child Composition

All container-like controls support `add(...)`.

```cpp
parent.add(child);
parent.add(child_a, child_b, child_c);
```

Full API:

```cpp
template <typename Child>
ControlNode &add(Child &child);

template <typename First, typename... Rest>
ControlNode &add(First &first, Rest &...rest);

ControlNode &clear_children();
ControlNode &remove_child(Node &child);
```

`add(...)` preserves order:

```cpp
root.add(top_bar, body, bottom_bar);
```

For single-child containers such as a scroll area, `add(...)` should assert in debug builds if more than one direct content child is inserted.

## Parent Bounds Methods

These methods control anchors, docking, and parent-relative geometry.

```cpp
auto frame = ui::panel("SettingsFrame")
	.match_parent()
	.padding(12);

auto top_bar = ui::surface("QuaderTopBar")
	.dock(ui::Dock::Top)
	.height(24);
```

Full API:

```cpp
ControlNode &match_parent();
ControlNode &match_parent_width();
ControlNode &match_parent_height();

ControlNode &dock(Dock dock);
ControlNode &dock(Dock dock, float size);
ControlNode &anchor(Anchor anchor);
ControlNode &anchor(Anchor anchor, Size offset);

ControlNode &pin_edges(Insets edges = Insets::all(0.0f));
ControlNode &inset(float value);
ControlNode &inset(Insets value);

```

Mapping:

| Builder | Godot |
|---|---|
| `match_parent()` | `PRESET_FULL_RECT` with zero offsets |
| `match_parent().inset(12)` | `PRESET_FULL_RECT` plus offsets |
| `dock(Dock::Fill)` | full parent bounds |
| `dock(Dock::Top, 24)` | top-wide anchors plus fixed height |
| `dock(Dock::Bottom, 28)` | bottom-wide anchors plus fixed height |
| `dock(Dock::Start, 340)` | start/left-wide anchors plus fixed width |
| `dock(Dock::End, 340)` | end/right-wide anchors plus fixed width |
| `anchor(Anchor::Center)` | centered anchors |

`match_parent()` is a readable alias over `dock(Dock::Fill)`.

## Container Allocation Methods

These describe how a control behaves inside a Godot `Container`.

```cpp
auto scroll = ui::scroll_area("SettingsScroll")
	.fill_available()
	.scrollbar_y(ui::Scrollbar::Auto);
```

Full API:

```cpp
ControlNode &hug_content();
ControlNode &hug_content_width();
ControlNode &hug_content_height();

ControlNode &stretch();
ControlNode &stretch_width();
ControlNode &stretch_height();

ControlNode &grow(float weight = 1.0f);
ControlNode &grow_width(float weight = 1.0f);
ControlNode &grow_height(float weight = 1.0f);

ControlNode &fill_available(float weight = 1.0f);
ControlNode &fill_available_width(float weight = 1.0f);
ControlNode &fill_available_height(float weight = 1.0f);

ControlNode &align_self(Align alignment);
```

Mapping:

| Builder | Godot |
|---|---|
| `hug_content()` | shrink flags / minimum size behavior |
| `stretch_width()` | horizontal `SIZE_FILL` |
| `grow_width(weight)` | horizontal `SIZE_EXPAND` plus stretch ratio |
| `fill_available_width(weight)` | horizontal `SIZE_EXPAND_FILL` plus stretch ratio |
| `fill_available()` | horizontal and vertical `SIZE_EXPAND_FILL` |

## Size Methods

```cpp
auto sidebar = ui::panel("Sidebar")
	.min_width(340)
	.fill_available_height();
```

Full API:

```cpp
ControlNode &size(Size size);
ControlNode &size(float width, float height);
ControlNode &width(float value);
ControlNode &height(float value);

ControlNode &min_size(Size size);
ControlNode &min_size(float width, float height);
ControlNode &min_width(float value);
ControlNode &min_height(float value);

ControlNode &max_size(Size size);
ControlNode &max_size(float width, float height);
ControlNode &max_width(float value);
ControlNode &max_height(float value);

ControlNode &fixed_size(Size size);
ControlNode &fixed_size(float width, float height);
ControlNode &fixed_width(float value);
ControlNode &fixed_height(float value);
```

## Spacing And Padding Methods

```cpp
auto body = ui::column("SettingsBody")
	.padding(12)
	.gap(8);
```

Full API:

```cpp
ControlNode &padding(float value);
ControlNode &padding(float vertical, float horizontal);
ControlNode &padding(Insets insets);

ControlNode &gap(float value);
ControlNode &row_gap(float value);
ControlNode &column_gap(float value);
```

Mapping:

| Builder | Godot |
|---|---|
| `padding(...)` | `MarginContainer` margins or panel content margins |
| `gap(...)` | box/grid separation constants |

For Godot `MarginContainer`, the public API still says `padding`, because it changes child inset, not outside margin.

## Alignment Methods

```cpp
auto row = ui::row("Toolbar")
	.gap(6)
	.justify_content(ui::Justify::Start)
	.align_items(ui::Align::Center);
```

Full API:

```cpp
ControlNode &justify_content(Justify alignment);
ControlNode &align_items(Align alignment);
```

`justify_content` follows the container axis:

- row: horizontal
- column: vertical

`align_items` follows the opposite axis:

- row: vertical
- column: horizontal

## Panel And Surface Methods

```cpp
auto top_bar = ui::surface("QuaderTopBar")
	.background(kTopBarSurfaceColor)
	.min_height(24)
	.fill_available_width();
```

Full API:

```cpp
Panel &background(const char *html_color);
Panel &background(const char *style_role, const char *html_color);
Panel &empty_style(const char *style_role);
Panel &border_color(const char *html_color);
Panel &border_width(int width);
Panel &corner_radius(int radius);
Panel &content_padding(float value);
Panel &content_padding(Insets insets);
```

State styling should normally use typed visual states:

```cpp
button.state(ui::State::Hover).background("#2e2e2e");
button.state(ui::State::Focused).empty();
```

Low-level style roles using `StyleOverride` stay available as an escape hatch, but they should not be the normal API in component code.

## Visual State Methods

Visual state methods are inspired by Gum's state-based styling. They let the code say which UI state is being styled without exposing Godot theme override names.

```cpp
auto edit_menu = ui::menu_button("Edit")
	.font_color(kMenuTextColor);

edit_menu.state(ui::State::Normal).background(kTopBarSurfaceColor);
edit_menu.state(ui::State::Hover).background(kMenuHoverSurfaceColor);
edit_menu.state(ui::State::Pressed).background(kMenuPressedSurfaceColor);
edit_menu.state(ui::State::Focused).empty();
```

Full API:

```cpp
StateStyle state(State state);

StateStyle &background(const char *html_color);
StateStyle &font_color(const char *html_color);
StateStyle &icon_color(const char *html_color);
StateStyle &border_color(const char *html_color);
StateStyle &border_width(int width);
StateStyle &corner_radius(int radius);
StateStyle &padding(float value);
StateStyle &padding(Insets insets);
StateStyle &empty();
```

Mapping:

| Builder State | Godot Style Roles |
|---|---|
| `State::Normal` | normal/default |
| `State::Hover` | hover |
| `State::Pressed` | pressed |
| `State::Focused` | focus |
| `State::Disabled` | disabled |
| `State::Checked` | checked / normal checked |
| `State::CheckedHover` | checked hover / hover pressed |

## Text Methods

Available on labels, buttons, menu buttons, and text controls where applicable.

```cpp
auto title = ui::label("Viewport")
	.text_role(ui::TextRole::SectionTitle)
	.font_color("#d8d8d8");
```

Full API:

```cpp
ControlNode &text(const char *value);
ControlNode &placeholder_text(const char *value);
ControlNode &font_size(int size);
ControlNode &font_color(const char *html_color);
ControlNode &font_color(const char *role, const char *html_color);
ControlNode &text_role(TextRole role);
ControlNode &horizontal_text_align(Align alignment);
ControlNode &vertical_text_align(Align alignment);
```

## Button Methods

```cpp
auto reset = ui::button("Reset to Defaults")
	.hug_content()
	.on_pressed(target, "reset_settings");
```

Full API:

```cpp
Button &flat(bool enabled = true);
Button &icon(godot::Ref<godot::Texture2D> texture);
Button &icon_alignment(Align alignment);
Button &on_pressed(godot::Object *target, const char *method);
Button &on_toggled(godot::Object *target, const char *method);
Button &toggle_mode(bool enabled = true);
Button &pressed(bool enabled = true);
```

## Menu Methods

```cpp
auto edit_menu = ui::menu_button("Edit")
	.flat()
	.min_size(42, 24)
	.font_size(14)
	.font_color(kMenuTextColor);

edit_menu.popup()
	.background(kPopupSurfaceColor)
	.padding(4)
	.item("Settings", kEditMenuSettingsId)
	.on_id_pressed(target, "on_edit_menu_id");
```

Full API:

```cpp
MenuButton &flat(bool enabled = true);
PopupMenu &popup();

PopupMenu &item(const char *label, int id);
PopupMenu &separator();
PopupMenu &disabled_item(const char *label, int id);
PopupMenu &check_item(const char *label, int id, bool checked = false);
PopupMenu &on_id_pressed(godot::Object *target, const char *method);
```

## Scroll Area Methods

```cpp
auto scroll = ui::scroll_area("SettingsScroll")
	.fill_available()
	.scrollbar_y(ui::Scrollbar::Auto)
	.scrollbar_x(ui::Scrollbar::Never)
	.keep_focused_child_visible();
```

Full API:

```cpp
ScrollArea &scrollbar_x(Scrollbar mode);
ScrollArea &scrollbar_y(Scrollbar mode);
ScrollArea &keep_focused_child_visible(bool enabled = true);
ScrollArea &scroll_to_child(godot::Control *child);
```

## Form Methods

`form(...)` creates a two-column grid intended for label-control rows.

```cpp
auto grid = ui::form("GridColors");

grid.add(
	ui::color_field("Minor", settings.grid_minor_color)
		.edit_alpha()
		.on_changed(target, "set_grid_minor_color"),

	ui::color_field("Major", settings.grid_major_color)
		.edit_alpha()
		.on_changed(target, "set_grid_major_color")
);
```

Form-specific API:

```cpp
Grid &columns(int count);
Grid &label_width(float width);
Grid &row_gap(float value);
Grid &column_gap(float value);
```

## Grid Cell Methods

Grid and form cells are local layout contexts, inspired by Gum's grid behavior. If a control docks to a cell, it fills that cell, not the entire grid.

```cpp
auto viewport_grid = ui::grid("ViewportGrid")
	.columns(2)
	.rows(2)
	.gap(6);

auto label = ui::label("Background")
	.hug_content();

auto picker = ui::color_field("Background", settings.background_color)
	.dock(ui::Dock::Fill)
	.on_changed(target, "set_background_color");

viewport_grid.cell(0, 0).add(label);
viewport_grid.cell(0, 1).add(picker);
```

Full API:

```cpp
GridCell cell(int row, int column);
GridCell cell(int row, int column, int row_span, int column_span);

GridCell &padding(float value);
GridCell &padding(Insets insets);
GridCell &align_items(Align alignment);
GridCell &justify_content(Justify alignment);

template <typename Child>
GridCell &add(Child &child);
```

Form helpers can keep the common label/control pattern concise:

```cpp
auto form = ui::form("ViewportSettings")
	.row_gap(6)
	.column_gap(10);

form.row(
	"Background",
	ui::color_field("Background", settings.background_color)
		.dock(ui::Dock::Fill)
		.on_changed(target, "set_background_color")
);
```

Form row API:

```cpp
template <typename Field>
Grid &row(const char *label, Field &field);

template <typename LabelNode, typename Field>
Grid &row(LabelNode &label, Field &field);
```

## Color Field Methods

```cpp
auto minor = ui::color_field("Minor", settings.grid_minor_color)
	.edit_alpha()
	.on_changed(target, "set_grid_minor_color");
```

Full API:

```cpp
ColorField &value(godot::Color color);
ColorField &value(const char *html_color);
ColorField &edit_alpha(bool enabled = true);
ColorField &on_changed(godot::Object *target, const char *method);
```

## Number Field Methods

```cpp
auto line_size = ui::number_field("Minor Line", settings.minor_line_size)
	.range(0.05, 8.0)
	.step(0.01)
	.on_changed(target, "set_minor_line_size");
```

Full API:

```cpp
NumberField &value(double value);
NumberField &range(double minimum, double maximum);
NumberField &min(double value);
NumberField &max(double value);
NumberField &step(double value);
NumberField &suffix(const char *text);
NumberField &on_changed(godot::Object *target, const char *method);
```

## Window Methods

```cpp
auto window = ui::window("Quader Settings")
	.size(480, 720)
	.min_size(420, 520)
	.transient()
	.wrap_controls()
	.on_close_requested(target, "hide_settings_window");
```

Full API:

```cpp
Window &title(const char *text);
Window &size(Size size);
Window &size(float width, float height);
Window &min_size(Size size);
Window &min_size(float width, float height);
Window &transient(bool enabled = true);
Window &wrap_controls(bool enabled = true);
Window &popup_centered(Size size);
Window &on_close_requested(godot::Object *target, const char *method);
```

## Mounting

Mounting attaches a root builder node to an existing Godot parent.

```cpp
ui::mount_into(root_container, body);
```

Full API:

```cpp
template <typename Parent, typename Child>
void mount_into(Parent *parent, Child &child);

template <typename Parent, typename First, typename... Rest>
void mount_into(Parent *parent, First &first, Rest &...rest);
```

Equivalent manual form:

```cpp
root_container->add_child(body.node());
```

## Example: Top Bar

```cpp
Control *QuaderTopBar::render() const {
	auto top_bar = ui::surface("QuaderTopBar")
		.dock(ui::Dock::Top, 24)
		.background(kTopBarSurfaceColor)
		.fill_available_width();

	auto row = ui::row("QuaderTopBarContent")
		.gap(0)
		.align_items(ui::Align::Center)
		.justify_content(ui::Justify::Start);

	auto edit_menu = ui::menu_button("Edit")
		.name("QuaderEditMenu")
		.flat()
		.min_size(42, 24)
		.font_size(14)
		.font_color(kMenuTextColor);

	edit_menu.state(ui::State::Normal).background(kTopBarSurfaceColor);
	edit_menu.state(ui::State::Hover).background(kMenuHoverSurfaceColor);
	edit_menu.state(ui::State::Pressed).background(kMenuPressedSurfaceColor);
	edit_menu.state(ui::State::Focused).empty();

	edit_menu.popup()
		.background(kPopupSurfaceColor)
		.padding(4)
		.font_size(14)
		.font_color(kMenuTextColor)
		.item("Settings", kEditMenuSettingsId)
		.on_id_pressed(target_, "on_edit_menu_id");

	row.add(edit_menu);
	top_bar.add(row);

	return top_bar.node();
}
```

## Example: Bottom Bar

```cpp
Control *QuaderBottomBar::render() const {
	auto bottom_bar = ui::surface("QuaderBottomBar")
		.dock(ui::Dock::Bottom, 28)
		.background(kBottomBarSurfaceColor)
		.fill_available_width();

	auto stack = ui::column("QuaderBottomBarStack")
		.gap(0)
		.fill_available();

	auto divider = ui::panel("QuaderBottomBarDivider")
		.background(kBottomBarDividerColor)
		.fixed_height(1)
		.fill_available_width();

	auto status = ui::surface("QuaderBottomBarStatusSurface")
		.background(kBottomBarSurfaceColor)
		.fill_available();

	auto accent = ui::panel("QuaderBottomBarAccent")
		.background(kBottomBarAccentColor)
		.fixed_height(3)
		.fill_available_width();

	stack.add(divider, status, accent);
	bottom_bar.add(stack);

	return bottom_bar.node();
}
```

## Example: Resizable Body With Sidebar

```cpp
Control *make_quader_editor_body(Control *viewport) {
	auto body = ui::split_horizontal("QuaderEditorBody")
		.fill_available()
		.dragger_width(4)
		.dragger_color(kSidebarSplitterSurfaceColor)
		.dragging_enabled();

	auto viewport_content = ui::adopt("Viewport", viewport)
		.fill_available();

	auto sidebar = ui::surface("QuaderSidebar")
		.background(kSidebarSurfaceColor)
		.padding(8)
		.min_width(340)
		.stretch_height();

	body.add(viewport_content, sidebar);

	return body.node();
}
```

`ui::adopt(...)` wraps an existing Godot node without allocating it.

## Example: Settings Window

```cpp
Window *make_quader_viewport_settings_window(
		Object *target, const ViewportVisualSettings &settings) {
	auto window = ui::window("Quader Settings")
		.size(480, 720)
		.min_size(420, 520)
		.wrap_controls()
		.transient()
		.on_close_requested(target, "hide_settings_window");

	auto frame = ui::panel("SettingsFrame")
		.match_parent()
		.padding(12);

	auto scroll = ui::scroll_area("SettingsScroll")
		.fill_available()
		.scrollbar_y(ui::Scrollbar::Auto)
		.scrollbar_x(ui::Scrollbar::Never)
		.keep_focused_child_visible();

	auto body = ui::column("SettingsBody")
		.match_parent_width()
		.hug_content_height()
		.gap(12);

	auto grid_colors = ui::form("GridColors")
		.column_gap(10)
		.row_gap(6);

	grid_colors.add(
		ui::color_field("Minor", settings.grid_minor_color)
			.edit_alpha()
			.on_changed(target, "set_grid_minor_color"),

		ui::color_field("Major", settings.grid_major_color)
			.edit_alpha()
			.on_changed(target, "set_grid_major_color"),

		ui::color_field("X Axis", settings.grid_x_axis_color)
			.edit_alpha()
			.on_changed(target, "set_grid_x_axis_color"),

		ui::color_field("Z Axis", settings.grid_z_axis_color)
			.edit_alpha()
			.on_changed(target, "set_grid_z_axis_color")
	);

	auto mesh_grid_colors = ui::form("MeshGridColors")
		.column_gap(10)
		.row_gap(6);

	mesh_grid_colors.add(
		ui::color_field("Mesh Minor", settings.mesh_grid_minor_color)
			.edit_alpha()
			.on_changed(target, "set_mesh_grid_minor_color"),

		ui::color_field("Mesh Major", settings.mesh_grid_major_color)
			.edit_alpha()
			.on_changed(target, "set_mesh_grid_major_color")
	);

	auto overlay_sizes = ui::form("OverlaySizes")
		.column_gap(10)
		.row_gap(6);

	overlay_sizes.add(
		ui::number_field("Cyan Component Wire", settings.source_wire_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_source_wire_line_size"),

		ui::number_field("Orange Mesh/Face Wire", settings.selection_face_wire_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_selection_face_wire_line_size"),

		ui::number_field("Orange Edge Wire", settings.selection_edge_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_selection_edge_line_size")
	);

	auto viewport = ui::form("ViewportSettings")
		.column_gap(10)
		.row_gap(6);

	viewport.add(
		ui::color_field("Background", settings.background_color)
			.on_changed(target, "set_background_color")
	);

	body.add(
		ui::section_label("Grid Colors"),
		grid_colors,
		ui::section_label("Mesh Grid Colors"),
		mesh_grid_colors,
		ui::section_label("Overlay Sizes"),
		overlay_sizes,
		ui::section_label("Viewport"),
		viewport
	);

	scroll.add(body);
	frame.add(scroll);
	window.add(frame);

	return window.node();
}
```

## Example: Dense Toolbar

```cpp
auto toolbar = ui::row("ToolModeToolbar")
	.gap(4)
	.padding(4)
	.align_items(ui::Align::Center)
	.justify_content(ui::Justify::Start)
	.fill_available_width();

auto select = ui::button("Select")
	.icon(icons.select)
	.tooltip("Select")
	.toggle_mode()
	.hug_content()
	.on_pressed(target, "activate_select_tool");

auto move = ui::button("Move")
	.icon(icons.move)
	.tooltip("Move")
	.toggle_mode()
	.hug_content()
	.on_pressed(target, "activate_move_tool");

auto rotate = ui::button("Rotate")
	.icon(icons.rotate)
	.tooltip("Rotate")
	.toggle_mode()
	.hug_content()
	.on_pressed(target, "activate_rotate_tool");

auto scale = ui::button("Scale")
	.icon(icons.scale)
	.tooltip("Scale")
	.toggle_mode()
	.hug_content()
	.on_pressed(target, "activate_scale_tool");

toolbar.add(select, move, rotate, scale, ui::spacer().grow());
```

## Example: Settings Section Helper

Reusable components can return builder handles.

```cpp
ui::Grid make_overlay_size_section(Object *target, const ViewportVisualSettings &settings) {
	auto section = ui::form("OverlaySizes")
		.column_gap(10)
		.row_gap(6);

	section.add(
		ui::number_field("Cyan Component Wire", settings.source_wire_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_source_wire_line_size"),

		ui::number_field("Hover/Remove Wire", settings.hover_wire_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_hover_wire_line_size"),

		ui::number_field("Open Boundary Wire", settings.open_edge_line_size)
			.range(0.25, 8.0)
			.step(0.01)
			.on_changed(target, "set_open_edge_line_size")
	);

	return section;
}
```

Call site:

```cpp
auto overlay_sizes = make_overlay_size_section(target, settings);

body.add(
	ui::section_label("Overlay Sizes"),
	overlay_sizes
);
```

## Low-Level Escape Hatches

Normal code should avoid raw Godot theme calls, but the builder exposes explicit escape hatches for cases not worth wrapping yet.

```cpp
auto custom = ui::button("Special")
	.tap([](Button &button) {
		button.add_theme_constant_override("h_separation", 2);
	});
```

Optional Godot-native access:

```cpp
Button *raw = custom.node();
```

## Implementation Notes

- `ControlNode<T>` should be move-only.
- A node not attached to a parent should be freed safely by the builder destructor.
- After `add(...)`, ownership transfers to Godot's parent node.
- `add(...)` should debug-assert if a child already has a parent.
- `ScrollArea::add(...)` should debug-assert on more than one content child.
- Public methods should use readable API names; Godot constants stay in the adapter implementation.
- `PanelBuilder` can be folded into this API as the implementation detail behind panel styling.
- `Surface` can remain an atom component or become a direct factory over `PanelContainer`.

## Open Decisions

- Whether methods should use dot style on values:

```cpp
auto body = ui::column("Body").gap(8);
```

or pointer style:

```cpp
auto body = ui::column("Body")->gap(8);
```

The preferred draft is dot style because it reads cleaner and builder handles are value objects.

- Whether `padding(...)` should create a real `MarginContainer` wrapper or set content margins on a panel. The method name should stay the same; implementation may vary by node type.
- Whether `form(...)` returns a `GridContainer` wrapper or a custom composite component that owns labels plus controls.
- Whether `style_role` arguments should be `const char *` constants or a typed enum.
