#pragma once

namespace quader_godot::ui {

namespace StyleOverride {
inline constexpr const char Panel[] = "panel";
inline constexpr const char Normal[] = "normal";
inline constexpr const char Hover[] = "hover";
inline constexpr const char Pressed[] = "pressed";
inline constexpr const char Disabled[] = "disabled";
inline constexpr const char Focus[] = "focus";
inline constexpr const char ReadOnly[] = "read_only";
inline constexpr const char HoverPressed[] = "hover_pressed";
inline constexpr const char Cursor[] = "cursor";
inline constexpr const char CursorUnfocused[] = "cursor_unfocused";
inline constexpr const char Scroll[] = "scroll";
inline constexpr const char ScrollFocus[] = "scroll_focus";
inline constexpr const char GrabberArea[] = "grabber_area";
inline constexpr const char GrabberAreaHighlight[] = "grabber_area_highlight";
inline constexpr const char SplitBarBackground[] = "split_bar_background";
inline constexpr const char TabSelected[] = "tab_selected";
inline constexpr const char TabHover[] = "tab_hover";
inline constexpr const char TabUnselected[] = "tab_unselected";
inline constexpr const char TabDisabled[] = "tab_disabled";
} // namespace StyleOverride

namespace ColorOverride {
inline constexpr const char FontColor[] = "font_color";
inline constexpr const char FontHoverColor[] = "font_hover_color";
inline constexpr const char FontPressedColor[] = "font_pressed_color";
inline constexpr const char FontDisabledColor[] = "font_disabled_color";
inline constexpr const char FontFocusColor[] = "font_focus_color";
inline constexpr const char FontOutlineColor[] = "font_outline_color";
inline constexpr const char IconNormalColor[] = "icon_normal_color";
inline constexpr const char IconHoverColor[] = "icon_hover_color";
inline constexpr const char IconPressedColor[] = "icon_pressed_color";
inline constexpr const char IconDisabledColor[] = "icon_disabled_color";
inline constexpr const char IconFocusColor[] = "icon_focus_color";
inline constexpr const char SelectionColor[] = "selection_color";
inline constexpr const char CaretColor[] = "caret_color";
inline constexpr const char ClearButtonColor[] = "clear_button_color";
inline constexpr const char ClearButtonColorPressed[] = "clear_button_color_pressed";
inline constexpr const char ClearButtonColorHover[] = "clear_button_color_hover";
} // namespace ColorOverride

namespace ConstantOverride {
inline constexpr const char Separation[] = "separation";
inline constexpr const char HSeparation[] = "h_separation";
inline constexpr const char VSeparation[] = "v_separation";
inline constexpr const char MarginLeft[] = "margin_left";
inline constexpr const char MarginTop[] = "margin_top";
inline constexpr const char MarginRight[] = "margin_right";
inline constexpr const char MarginBottom[] = "margin_bottom";
inline constexpr const char IconSeparation[] = "icon_separation";
inline constexpr const char LineSeparation[] = "line_separation";
inline constexpr const char ParagraphSeparation[] = "paragraph_separation";
inline constexpr const char OutlineSize[] = "outline_size";
inline constexpr const char MinimumGrabThickness[] = "minimum_grab_thickness";
inline constexpr const char Autohide[] = "autohide";
} // namespace ConstantOverride

namespace FontOverride {
inline constexpr const char Font[] = "font";
inline constexpr const char BoldFont[] = "bold_font";
inline constexpr const char ItalicsFont[] = "italics_font";
inline constexpr const char MonoFont[] = "mono_font";
} // namespace FontOverride

namespace FontSizeOverride {
inline constexpr const char FontSize[] = "font_size";
inline constexpr const char NormalFontSize[] = "normal_font_size";
inline constexpr const char TitleFontSize[] = "title_font_size";
} // namespace FontSizeOverride

namespace IconOverride {
inline constexpr const char Arrow[] = "arrow";
inline constexpr const char Checked[] = "checked";
inline constexpr const char Unchecked[] = "unchecked";
inline constexpr const char Grabber[] = "grabber";
inline constexpr const char HGrabber[] = "h_grabber";
inline constexpr const char VGrabber[] = "v_grabber";
inline constexpr const char TouchDragger[] = "touch_dragger";
inline constexpr const char HTouchDragger[] = "h_touch_dragger";
inline constexpr const char VTouchDragger[] = "v_touch_dragger";
inline constexpr const char Close[] = "close";
inline constexpr const char Increment[] = "increment";
inline constexpr const char Decrement[] = "decrement";
} // namespace IconOverride

namespace SignalName {
inline constexpr const char IdPressed[] = "id_pressed";
inline constexpr const char CloseRequested[] = "close_requested";
inline constexpr const char ColorChanged[] = "color_changed";
inline constexpr const char ValueChanged[] = "value_changed";
inline constexpr const char VisibilityChanged[] = "visibility_changed";
} // namespace SignalName

namespace UiNodeName {
inline constexpr const char QuaderTopBar[] = "QuaderTopBar";
inline constexpr const char QuaderEditMenu[] = "QuaderEditMenu";
inline constexpr const char QuaderEditorBody[] = "QuaderEditorBody";
inline constexpr const char QuaderSidebar[] = "QuaderSidebar";
inline constexpr const char QuaderBottomBar[] = "QuaderBottomBar";
inline constexpr const char QuaderBottomBarStack[] = "QuaderBottomBarStack";
inline constexpr const char QuaderBottomBarDivider[] = "QuaderBottomBarDivider";
inline constexpr const char QuaderBottomBarStatusSurface[] = "QuaderBottomBarStatusSurface";
inline constexpr const char QuaderBottomBarAccent[] = "QuaderBottomBarAccent";
} // namespace UiNodeName

} // namespace quader_godot::ui
