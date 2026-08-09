#!/usr/bin/env python3
from pathlib import Path
import shutil
import sys

if len(sys.argv) != 2:
    raise SystemExit("usage: apply-engine-overlay.py PATH_TO_LUANTI")

project = Path(__file__).resolve().parents[1]
luanti = Path(sys.argv[1]).resolve()
overlay = project / "engine-overlay" / "src" / "navycraft"
target = luanti / "src" / "navycraft"

required = {
    "cmake": luanti / "src" / "CMakeLists.txt",
    "scripting_server": luanti / "src" / "script" / "scripting_server.cpp",
    "main_cpp": luanti / "src" / "main.cpp",
    "networkprotocol": luanti / "src" / "network" / "networkprotocol.h",
    "clientopcodes": luanti / "src" / "network" / "clientopcodes.cpp",
    "serveropcodes": luanti / "src" / "network" / "serveropcodes.cpp",
    "serverpackethandler": luanti / "src" / "network" / "serverpackethandler.cpp",
    "client_h": luanti / "src" / "client" / "client.h",
    "client_cpp": luanti / "src" / "client" / "client.cpp",
    "gameui_cpp": luanti / "src" / "client" / "gameui.cpp",
    "localplayer_cpp": luanti / "src" / "client" / "localplayer.cpp",
    "chat_console_cpp": luanti / "src" / "gui" / "guiChatConsole.cpp",
    "chat_console_h": luanti / "src" / "gui" / "guiChatConsole.h",
    "server_h": luanti / "src" / "server.h",
    "server_cpp": luanti / "src" / "server.cpp",
    "mapblock_mesh_h": luanti / "src" / "client" / "mapblock_mesh.h",
    "mapblock_mesh_cpp": luanti / "src" / "client" / "mapblock_mesh.cpp",
}
missing = [str(path) for path in required.values() if not path.exists()]
if missing:
    raise SystemExit("not a compatible Luanti 5.16.1 source tree; missing: " + ", ".join(missing))

if target.exists():
    shutil.rmtree(target)
shutil.copytree(overlay, target)


def replace_once(text: str, old: str, new: str, label: str) -> str:
    if new in text:
        return text
    if old not in text:
        raise SystemExit(f"Luanti layout changed: {label} anchor missing")
    return text.replace(old, new, 1)


def replace_any_once(text: str, old_values: list[str], new: str, label: str) -> str:
    if new in text:
        return text
    for old in old_values:
        if old in text:
            return text.replace(old, new, 1)
    raise SystemExit(f"Luanti layout changed: {label} anchor missing")


# Keep chat and command input Lua-configurable so HUD layout changes do not
# require another native engine rebuild.
gameui_cpp = required["gameui_cpp"]
text = gameui_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "version.h"\n',
    '#include "version.h"\n#include "navycraft/chat_style.h"\n',
    "NavyCraft chat style include")
recent_chat_old = '''void GameUI::updateChatSize()
{
\t// Update gui element size and position
\ts32 chat_y = 5;

\tif (m_flags.show_minimal_debug)
\t\tchat_y += m_guitext->getTextHeight();
\tif (m_flags.show_basic_debug)
\t\tchat_y += m_guitext2->getTextHeight();

\tconst v2u32 window_size = RenderingEngine::getWindowSize();

\tcore::rect<s32> chat_size(10, chat_y, window_size.X - 20, 0);
\tchat_size.LowerRightCorner.Y = std::min((s32)window_size.Y,
\t\t\tm_guitext_chat->getTextHeight() + chat_y);

\tif (chat_size == m_current_chat_size)
\t\treturn;
\tm_current_chat_size = chat_size;

\tm_guitext_chat->setRelativePosition(chat_size);
}
'''
recent_chat_new = '''void GameUI::updateChatSize()
{
\tconst v2u32 window_size = RenderingEngine::getWindowSize();
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();

\tFontMode font_mode = FM_Unspecified;
\tif (style.recent_font_mode == navycraft::ChatFontMode::Standard)
\t\tfont_mode = FM_Standard;
\telse if (style.recent_font_mode == navycraft::ChatFontMode::Mono)
\t\tfont_mode = FM_Mono;
\tconst unsigned int font_size = style.recent_font_size > 0 ?
\t\t\trangelim(style.recent_font_size, 5, 72) : FONT_SIZE_UNSPECIFIED;
\tm_guitext_chat->setOverrideFont(g_fontengine->getFont(font_size, font_mode));
\tif (style.recent_text_color.enabled) {
\t\tm_guitext_chat->setOverrideColor(video::SColor(
\t\t\tstyle.recent_text_color.alpha,
\t\t\tstyle.recent_text_color.red,
\t\t\tstyle.recent_text_color.green,
\t\t\tstyle.recent_text_color.blue));
\t}

\tconst s32 left = MYMAX(0, style.recent_margin_left);
\tconst s32 right = MYMAX(left + 1, (s32)window_size.X - MYMAX(0, style.recent_margin_right));
\tconst s32 top = MYMAX(0, style.recent_margin_top);
\ts32 bottom = (s32)window_size.Y - MYMAX(0, style.recent_margin_bottom);
\tif (bottom <= top)
\t\tbottom = (s32)window_size.Y;
\tconst s32 extra_spacing = MYMAX(0,
\t\t\t((s32)m_recent_chat_count - 1) * style.recent_line_spacing);
\tconst s32 chat_height = m_guitext_chat->getTextHeight() + extra_spacing;
\ts32 chat_y1 = top;
\ts32 chat_y2 = bottom;
\tif (style.recent_anchor == navycraft::ChatAnchor::BottomLeft) {
\t\tchat_y2 = bottom;
\t\tchat_y1 = chat_y2 - chat_height;
\t\tif (chat_y1 < top)
\t\t\tchat_y1 = top;
\t} else {
\t\tchat_y1 = top;
\t\tchat_y2 = chat_y1 + chat_height;
\t\tif (chat_y2 > bottom)
\t\t\tchat_y2 = bottom;
\t}
\tif (chat_y2 <= chat_y1)
\t\tchat_y2 = chat_y1 + 1;

\tcore::rect<s32> chat_size(
\t\t\tleft,
\t\t\tchat_y1,
\t\t\tright,
\t\t\tchat_y2);

\tif (chat_size == m_current_chat_size)
\t\treturn;
\tm_current_chat_size = chat_size;

\tm_guitext_chat->setRelativePosition(chat_size);
}
'''
recent_chat_previous = '''void GameUI::updateChatSize()
{
\tconst v2u32 window_size = RenderingEngine::getWindowSize();
\tconst s32 margin = 10;
\tconst s32 hotbar_clearance = 62;
\tconst s32 chat_height = m_guitext_chat->getTextHeight();
\ts32 chat_y = (s32)window_size.Y - hotbar_clearance - chat_height;
\tif (chat_y < margin)
\t\tchat_y = margin;

\tcore::rect<s32> chat_size(
\t\t\tmargin,
\t\t\tchat_y,
\t\t\t(s32)window_size.X - margin,
\t\t\t(s32)window_size.Y - hotbar_clearance);

\tif (chat_size == m_current_chat_size)
\t\treturn;
\tm_current_chat_size = chat_size;

\tm_guitext_chat->setRelativePosition(chat_size);
}
'''
recent_chat_previous_lua_style = '''void GameUI::updateChatSize()
{
\tconst v2u32 window_size = RenderingEngine::getWindowSize();
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();

\tif (style.recent_font_size > 0) {
\t\tm_guitext_chat->setOverrideFont(g_fontengine->getFont(
\t\t\trangelim(style.recent_font_size, 5, 72), FM_Unspecified));
\t}
\tif (style.recent_text_color.enabled) {
\t\tm_guitext_chat->setOverrideColor(video::SColor(
\t\t\tstyle.recent_text_color.alpha,
\t\t\tstyle.recent_text_color.red,
\t\t\tstyle.recent_text_color.green,
\t\t\tstyle.recent_text_color.blue));
\t}

\tconst s32 left = MYMAX(0, style.recent_margin_left);
\tconst s32 right = MYMAX(left + 1, (s32)window_size.X - MYMAX(0, style.recent_margin_right));
\tconst s32 top = MYMAX(0, style.recent_margin_top);
\ts32 bottom = (s32)window_size.Y - MYMAX(0, style.recent_margin_bottom);
\tif (bottom <= top)
\t\tbottom = (s32)window_size.Y;
\tconst s32 extra_spacing = MYMAX(0,
\t\t\t((s32)m_recent_chat_count - 1) * style.recent_line_spacing);
\tconst s32 chat_height = m_guitext_chat->getTextHeight() + extra_spacing;
\ts32 chat_y1 = top;
\ts32 chat_y2 = bottom;
\tif (style.recent_anchor == navycraft::ChatAnchor::BottomLeft) {
\t\tchat_y2 = bottom;
\t\tchat_y1 = chat_y2 - chat_height;
\t\tif (chat_y1 < top)
\t\t\tchat_y1 = top;
\t} else {
\t\tchat_y1 = top;
\t\tchat_y2 = chat_y1 + chat_height;
\t\tif (chat_y2 > bottom)
\t\t\tchat_y2 = bottom;
\t}
\tif (chat_y2 <= chat_y1)
\t\tchat_y2 = chat_y1 + 1;

\tcore::rect<s32> chat_size(
\t\t\tleft,
\t\t\tchat_y1,
\t\t\tright,
\t\t\tchat_y2);

\tif (chat_size == m_current_chat_size)
\t\treturn;
\tm_current_chat_size = chat_size;

\tm_guitext_chat->setRelativePosition(chat_size);
}
'''
text = replace_any_once(text, [recent_chat_old, recent_chat_previous,
    recent_chat_previous_lua_style],
    recent_chat_new, "Lua-configurable recent chat placement")
gameui_cpp.write_text(text, encoding="utf-8")

chat_console_cpp = required["chat_console_cpp"]
text = chat_console_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "util/string.h"\n',
    '#include "util/string.h"\n#include "navycraft/chat_style.h"\n',
    "NavyCraft chat console style include")
text = replace_any_once(text, [
    '''inline u32 getScrollbarSize(IGUIEnvironment* env)
{
\treturn env->getSkin()->getSize(gui::EGDS_SCROLLBAR_SIZE);
}

inline video::SColor navycraftColor(const navycraft::ChatColor &color,
\t\tvideo::SColor fallback)
{
\tif (!color.enabled)
\t\treturn fallback;
\treturn video::SColor(color.alpha, color.red, color.green, color.blue);
}

inline core::rect<s32> navycraftConsoleRect(v2u32 screensize, s32 height,
\t\tconst navycraft::ChatStyle &style)
{
\tconst s32 left = MYMAX(0, style.console_margin_left);
\tconst s32 right = MYMAX(left + 1,
\t\t\t(s32)screensize.X - MYMAX(0, style.console_margin_right));
\tconst s32 top = MYMAX(0, style.console_margin_top);
\ts32 bottom = (s32)screensize.Y - MYMAX(0, style.console_margin_bottom);
\tif (bottom <= top)
\t\tbottom = (s32)screensize.Y;
\theight = MYMAX(0, MYMIN(height, bottom - top));
\tif (style.console_anchor == navycraft::ChatAnchor::BottomLeft)
\t\treturn core::rect<s32>(left, bottom - height, right, bottom);
\treturn core::rect<s32>(left, top, right, top + height);
}
''',
    '''inline u32 getScrollbarSize(IGUIEnvironment* env)
{
\treturn env->getSkin()->getSize(gui::EGDS_SCROLLBAR_SIZE);
}
'''],
    '''inline u32 getScrollbarSize(IGUIEnvironment* env)
{
\treturn env->getSkin()->getSize(gui::EGDS_SCROLLBAR_SIZE);
}

inline video::SColor navycraftColor(const navycraft::ChatColor &color,
\t\tvideo::SColor fallback)
{
\tif (!color.enabled)
\t\treturn fallback;
\treturn video::SColor(color.alpha, color.red, color.green, color.blue);
}

inline FontMode navycraftFontMode(navycraft::ChatFontMode mode, FontMode fallback)
{
\tif (mode == navycraft::ChatFontMode::Standard)
\t\treturn FM_Standard;
\tif (mode == navycraft::ChatFontMode::Mono)
\t\treturn FM_Mono;
\treturn fallback;
}

inline core::rect<s32> navycraftConsoleRect(v2u32 screensize, s32 height,
\t\tconst navycraft::ChatStyle &style)
{
\tconst s32 left = MYMAX(0, style.console_margin_left);
\tconst s32 right = MYMAX(left + 1,
\t\t\t(s32)screensize.X - MYMAX(0, style.console_margin_right));
\tconst s32 top = MYMAX(0, style.console_margin_top);
\ts32 bottom = (s32)screensize.Y - MYMAX(0, style.console_margin_bottom);
\tif (bottom <= top)
\t\tbottom = (s32)screensize.Y;
\theight = MYMAX(0, MYMIN(height, bottom - top));
\tif (style.console_anchor == navycraft::ChatAnchor::BottomLeft)
\t\treturn core::rect<s32>(left, bottom - height, right, bottom);
\treturn core::rect<s32>(left, top, right, top + height);
}
''',
    "NavyCraft chat console helpers")
text = replace_any_once(text, [
    '''void GUIChatConsole::setCursor(
\tbool visible, bool blinking, f32 blink_speed, f32 relative_height)
{
\tif (visible)
\t{
\t\tif (blinking)
\t\t{
\t\t\t// leave m_cursor_blink unchanged
\t\t\tm_cursor_blink_speed = blink_speed;
\t\t}
\t\telse
\t\t{
\t\t\tm_cursor_blink = 0x8000;  // on
\t\t\tm_cursor_blink_speed = 0.0;
\t\t}
\t}
\telse
\t{
\t\tm_cursor_blink = 0;  // off
\t\tm_cursor_blink_speed = 0.0;
\t}
\tm_cursor_height = relative_height;
}

void GUIChatConsole::applyNavyCraftChatStyle()
{
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tif (m_navycraft_style_revision == style.revision)
\t\treturn;
\tm_navycraft_style_revision = style.revision;
\tm_height_speed = style.console_height_speed;
\tm_background_color = navycraftColor(style.console_background_color,
\t\tm_background_color);
\tif (style.console_font_size > 0) {
\t\tm_font.grab(g_fontengine->getFont(
\t\t\trangelim(style.console_font_size, 5, 72), FM_Mono));
\t}
\tif (m_font) {
\t\tcore::dimension2d<u32> dim = m_font->getDimension(L"M");
\t\tm_fontsize = v2u32(dim.Width,
\t\t\tdim.Height + MYMAX(0, style.console_line_spacing));
\t}
\tm_fontsize.X = MYMAX(m_fontsize.X, 1);
\tm_fontsize.Y = MYMAX(m_fontsize.Y, 1);
\tif (m_screensize.X != 0 && m_screensize.Y != 0)
\t\treformatConsole();
}
''',
    '''void GUIChatConsole::setCursor(
\tbool visible, bool blinking, f32 blink_speed, f32 relative_height)
{
\tif (visible)
\t{
\t\tif (blinking)
\t\t{
\t\t\t// leave m_cursor_blink unchanged
\t\t\tm_cursor_blink_speed = blink_speed;
\t\t}
\t\telse
\t\t{
\t\t\tm_cursor_blink = 0x8000;  // on
\t\t\tm_cursor_blink_speed = 0.0;
\t\t}
\t}
\telse
\t{
\t\tm_cursor_blink = 0;  // off
\t\tm_cursor_blink_speed = 0.0;
\t}
\tm_cursor_height = relative_height;
}
'''],
    '''void GUIChatConsole::setCursor(
\tbool visible, bool blinking, f32 blink_speed, f32 relative_height)
{
\tif (visible)
\t{
\t\tif (blinking)
\t\t{
\t\t\t// leave m_cursor_blink unchanged
\t\t\tm_cursor_blink_speed = blink_speed;
\t\t}
\t\telse
\t\t{
\t\t\tm_cursor_blink = 0x8000;  // on
\t\t\tm_cursor_blink_speed = 0.0;
\t\t}
\t}
\telse
\t{
\t\tm_cursor_blink = 0;  // off
\t\tm_cursor_blink_speed = 0.0;
\t}
\tm_cursor_height = relative_height;
}

void GUIChatConsole::applyNavyCraftChatStyle()
{
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tif (m_navycraft_style_revision == style.revision)
\t\treturn;
\tm_navycraft_style_revision = style.revision;
\tm_height_speed = style.console_height_speed;
\tm_background_color = navycraftColor(style.console_background_color,
\t\tm_background_color);
\tconst unsigned int font_size = style.console_font_size > 0 ?
\t\t\trangelim(style.console_font_size, 5, 72) : FONT_SIZE_UNSPECIFIED;
\tm_font.grab(g_fontengine->getFont(font_size,
\t\tnavycraftFontMode(style.console_font_mode, FM_Mono)));
\tif (m_font) {
\t\tcore::dimension2d<u32> dim = m_font->getDimension(L"M");
\t\tm_fontsize = v2u32(dim.Width,
\t\t\tdim.Height + MYMAX(0, style.console_line_spacing));
\t}
\tm_fontsize.X = MYMAX(m_fontsize.X, 1);
\tm_fontsize.Y = MYMAX(m_fontsize.Y, 1);
\tif (m_screensize.X != 0 && m_screensize.Y != 0)
\t\treformatConsole();
}
''',
    "NavyCraft chat console style application")
text = replace_once(text,
    '''\t// Animation
\tu64 now = porting::getTimeMs();
''',
    '''\tapplyNavyCraftChatStyle();

\t// Animation
\tu64 now = porting::getTimeMs();
''',
    "NavyCraft chat console draw style refresh")
text = replace_once(text,
    '''void GUIChatConsole::reformatConsole()
{
\ts32 cols = m_screensize.X / m_fontsize.X - 2; // make room for a margin (looks better)
\ts32 rows = m_desired_height / m_fontsize.Y - 1; // make room for the input prompt
\tif (cols <= 0 || rows <= 0)
\t\tcols = rows = 0;

\tupdateScrollbar(true);

\trecalculateConsolePosition();
\tm_chat_backend->reformat(cols, rows);
}
''',
    '''void GUIChatConsole::reformatConsole()
{
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tconst core::rect<s32> rect = navycraftConsoleRect(m_screensize,
\t\t\t(s32)m_desired_height, style);
\ts32 cols = rect.getWidth() / m_fontsize.X - 2; // make room for a margin
\ts32 rows = rect.getHeight() / m_fontsize.Y - 1; // make room for the input prompt
\tif (cols <= 0 || rows <= 0)
\t\tcols = rows = 0;

\trecalculateConsolePosition();
\tm_chat_backend->reformat(cols, rows);
\tupdateScrollbar(true);
}
''',
    "NavyCraft chat console reformat")
console_old = '''void GUIChatConsole::recalculateConsolePosition()
{
\tcore::rect<s32> rect(0, 0, m_screensize.X, m_height);
\tDesiredRect = rect;
\trecalculateAbsolutePosition(false);
}
'''
console_new = '''void GUIChatConsole::recalculateConsolePosition()
{
\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tcore::rect<s32> rect = navycraftConsoleRect(m_screensize, m_height, style);
\tDesiredRect = rect;
\trecalculateAbsolutePosition(false);
}
'''
console_previous = '''void GUIChatConsole::recalculateConsolePosition()
{
\tcore::rect<s32> rect(0, m_screensize.Y - m_height, m_screensize.X, m_screensize.Y);
\tDesiredRect = rect;
\trecalculateAbsolutePosition(false);
}
'''
text = replace_any_once(text, [console_old, console_previous],
    console_new, "Lua-configurable chat console placement")
text = replace_once(text,
    '''void GUIChatConsole::drawBackground()
{
\tvideo::IVideoDriver* driver = Environment->getVideoDriver();
\tif (m_background != NULL)
\t{
\t\tcore::rect<s32> sourcerect(0, -m_height, m_screensize.X, 0);
\t\tdriver->draw2DImage(
\t\t\tm_background,
\t\t\tv2s32(0, 0),
\t\t\tsourcerect,
\t\t\t&AbsoluteClippingRect,
\t\t\tm_background_color,
\t\t\tfalse);
\t}
\telse
\t{
\t\tdriver->draw2DRectangle(
\t\t\tm_background_color,
\t\t\tcore::rect<s32>(0, 0, m_screensize.X, m_height),
\t\t\t&AbsoluteClippingRect);
\t}
}
''',
    '''void GUIChatConsole::drawBackground()
{
\tvideo::IVideoDriver* driver = Environment->getVideoDriver();
\tconst core::rect<s32> rect = AbsoluteRect;
\tif (m_background != NULL)
\t{
\t\tcore::rect<s32> sourcerect(0, 0, rect.getWidth(), rect.getHeight());
\t\tdriver->draw2DImage(
\t\t\tm_background,
\t\t\trect.UpperLeftCorner,
\t\t\tsourcerect,
\t\t\t&AbsoluteClippingRect,
\t\t\tm_background_color,
\t\t\tfalse);
\t}
\telse
\t{
\t\tdriver->draw2DRectangle(
\t\t\tm_background_color,
\t\t\trect,
\t\t\t&AbsoluteClippingRect);
\t}
}
''',
    "NavyCraft chat console absolute background")
text = replace_once(text,
    '''\tcore::recti rect;
\tif (m_scrollbar->isVisible())
\t\trect = core::rect<s32> (0, 0, m_screensize.X - getScrollbarSize(Environment), m_height);
\telse
\t\trect = AbsoluteClippingRect;
''',
    '''\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tconst video::SColor text_color = navycraftColor(style.console_text_color,
\t\tvideo::SColor(255, 255, 255, 255));
\tcore::recti rect;
\tif (m_scrollbar->isVisible())
\t\trect = core::rect<s32>(AbsoluteRect.UpperLeftCorner.X,
\t\t\tAbsoluteRect.UpperLeftCorner.Y,
\t\t\tAbsoluteRect.LowerRightCorner.X - getScrollbarSize(Environment),
\t\t\tAbsoluteRect.LowerRightCorner.Y);
\telse
\t\trect = AbsoluteClippingRect;
\tconst s32 origin_x = AbsoluteRect.UpperLeftCorner.X;
\tconst s32 origin_y = AbsoluteRect.UpperLeftCorner.Y;
\tconst s32 panel_height = AbsoluteRect.getHeight();
''',
    "NavyCraft chat console text clipping")
text = replace_any_once(text, [
    '''\t\ts32 line_height = m_fontsize.Y;
\t\ts32 y = row * line_height + m_height - m_desired_height;
\t\tif (y + line_height < 0)
\t\t\tcontinue;
''',
    '''\t\ts32 line_height = m_fontsize.Y;
\t\ts32 y = origin_y + row * line_height + panel_height - m_desired_height;
\t\tif (y + line_height < origin_y)
\t\t\tcontinue;
''',
],
    '''\t\ts32 line_height = m_fontsize.Y;
\t\ts32 y = origin_y + row * line_height + panel_height - (s32)m_desired_height;
\t\tif (y + line_height < origin_y)
\t\t\tcontinue;
''',
    "NavyCraft chat console text y origin")
text = replace_once(text,
    '''\t\t\ts32 x = (fragment.column + 1) * m_fontsize.X;
''',
    '''\t\t\ts32 x = origin_x + (fragment.column + 1) * m_fontsize.X;
''',
    "NavyCraft chat console text x origin")
text = replace_any_once(text, [
    '''\t\t\t\tm_font->draw(
\t\t\t\t\tfragment.text.c_str(),
\t\t\t\t\tdestrect,
\t\t\t\t\tvideo::SColor(255, 255, 255, 255),
\t\t\t\t\tfalse,
\t\t\t\t\tfalse,
\t\t\t\t\t&rect);
''',
    '''\t\t\tm_font->draw(
\t\t\t\tfragment.text.c_str(),
\t\t\t\tdestrect,
\t\t\t\tvideo::SColor(255, 255, 255, 255),
\t\t\t\tfalse,
\t\t\t\tfalse,
\t\t\t\t&rect);
''',
],
    '''\t\t\t\tm_font->draw(
\t\t\t\t\tfragment.text.c_str(),
\t\t\t\t\tdestrect,
\t\t\t\t\ttext_color,
\t\t\t\t\tfalse,
\t\t\t\t\tfalse,
\t\t\t\t\t&rect);
''',
    "NavyCraft chat console text color")
text = replace_any_once(text, [
    '''\tu32 row = m_chat_backend->getConsoleBuffer().getRows();
\ts32 y = row * font_height + m_height - m_desired_height;
''',
    '''\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tconst video::SColor prompt_color = navycraftColor(style.prompt_text_color,
\t\tvideo::SColor(255, 255, 255, 255));
\tconst s32 origin_x = AbsoluteRect.UpperLeftCorner.X;
\tconst s32 origin_y = AbsoluteRect.UpperLeftCorner.Y;
\tconst s32 panel_height = AbsoluteRect.getHeight();
\tu32 row = m_chat_backend->getConsoleBuffer().getRows();
\ts32 y = origin_y + row * font_height + panel_height - m_desired_height;
''',
],
    '''\tconst navycraft::ChatStyle style = navycraft::currentChatStyle();
\tconst video::SColor prompt_color = navycraftColor(style.prompt_text_color,
\t\tvideo::SColor(255, 255, 255, 255));
\tconst s32 origin_x = AbsoluteRect.UpperLeftCorner.X;
\tconst s32 origin_y = AbsoluteRect.UpperLeftCorner.Y;
\tconst s32 panel_height = AbsoluteRect.getHeight();
\tu32 row = m_chat_backend->getConsoleBuffer().getRows();
\ts32 y = origin_y + row * font_height + panel_height - (s32)m_desired_height;
''',
    "NavyCraft chat console prompt y origin")
text = replace_once(text,
    '''\tcore::rect<s32> destrect(
\t\tfont_width, y, font_width + text_width, y + font_height);
''',
    '''\tcore::rect<s32> destrect(
\t\torigin_x + font_width, y,
\t\torigin_x + font_width + text_width, y + font_height);
''',
    "NavyCraft chat console prompt x origin")
text = replace_once(text,
    '''\t\tvideo::SColor(255, 255, 255, 255),
''',
    '''\t\tprompt_color,
''',
    "NavyCraft chat console prompt color")
text = replace_any_once(text, [
    '''\t\t\ts32 x = font_width + text_to_cursor_pos_width;
''',
    '''\ts32 x = font_width + text_to_cursor_pos_width;
''',
],
    '''\t\t\ts32 x = origin_x + font_width + text_to_cursor_pos_width;
''',
    "NavyCraft chat console cursor x origin")
text = replace_any_once(text, [
    '''\t\t\tif (event.MouseInput.Y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )
\t\t\t{
\t\t\t\t// Translate pixel position to font position
\t\t\t\tbool was_url_pressed = m_cache_clickable_chat_weblinks &&
\t\t\t\t\t\tweblinkClick(event.MouseInput.X / m_fontsize.X,
\t\t\t\t\t\t\t\tevent.MouseInput.Y / m_fontsize.Y);

\t\t\t\tif (!was_url_pressed
\t\t\t\t\t\t&& event.MouseInput.Event == EMIE_MMOUSE_PRESSED_DOWN) {
\t\t\t\t\t// Paste primary selection at cursor pos
\t\t\t\t\tconst c8 *text = Environment->getOSOperator()
\t\t\t\t\t\t\t->getTextFromPrimarySelection();
\t\t\t\t\tif (text)
\t\t\t\t\t\tprompt.input(utf8_to_wide(text));
\t\t\t\t}
\t\t\t}
''',
    '''\t\t\tif (event.MouseInput.Y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )
\t\t\t{
\t\t\t\t// Translate pixel position to font position
\t\t\t\tweblinkClick(event.MouseInput.X / m_fontsize.X,
\t\t\t\t\t\t\tevent.MouseInput.Y / m_fontsize.Y);
\t\t\t}
''',
    '''\tif (event.MouseInput.Y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )
\t{
\t\t// Translate pixel position to font position
\t\tweblinkClick(event.MouseInput.X / m_fontsize.X,
\t\t\t\tevent.MouseInput.Y / m_fontsize.Y);
\t}
''',
],
    '''\t\t\tconst s32 local_x = event.MouseInput.X - AbsoluteRect.UpperLeftCorner.X;
\t\t\tconst s32 local_y = event.MouseInput.Y - AbsoluteRect.UpperLeftCorner.Y;
\t\t\tif (local_x >= 0 && local_y >= 0 &&
\t\t\t\t\tlocal_y / m_fontsize.Y < (m_height / m_fontsize.Y) - 1 )
\t\t\t{
\t\t\t\t// Translate pixel position to font position
\t\t\t\tbool was_url_pressed = m_cache_clickable_chat_weblinks &&
\t\t\t\t\t\tweblinkClick(local_x / m_fontsize.X,
\t\t\t\t\t\t\t\tlocal_y / m_fontsize.Y);

\t\t\t\tif (!was_url_pressed
\t\t\t\t\t\t&& event.MouseInput.Event == EMIE_MMOUSE_PRESSED_DOWN) {
\t\t\t\t\t// Paste primary selection at cursor pos
\t\t\t\t\tconst c8 *text = Environment->getOSOperator()
\t\t\t\t\t\t\t->getTextFromPrimarySelection();
\t\t\t\t\tif (text)
\t\t\t\t\t\tprompt.input(utf8_to_wide(text));
\t\t\t\t}
\t\t\t}
''',
    "NavyCraft chat console mouse origin")
text = replace_once(text,
    '''\tif (update_size) {
\t\tconst core::rect<s32> rect (m_screensize.X - getScrollbarSize(Environment), 0, m_screensize.X, m_height);
\t\tm_scrollbar->setRelativePosition(rect);
\t}
''',
    '''\tif (update_size) {
\t\tconst s32 width = MYMAX((s32)getScrollbarSize(Environment), DesiredRect.getWidth());
\t\tconst core::rect<s32> rect(width - getScrollbarSize(Environment),
\t\t\t0, width, DesiredRect.getHeight());
\t\tm_scrollbar->setRelativePosition(rect);
\t}
''',
    "NavyCraft chat console scrollbar size")
chat_console_cpp.write_text(text, encoding="utf-8")

chat_console_h = required["chat_console_h"]
text = chat_console_h.read_text(encoding="utf-8")
text = replace_once(text,
    '''\tvoid reformatConsole();
\tvoid recalculateConsolePosition();
''',
    '''\tvoid reformatConsole();
\tvoid recalculateConsolePosition();
\tvoid applyNavyCraftChatStyle();
''',
    "NavyCraft chat console style method")
text = replace_once(text,
    '''\t// console open/close animation speed [screen height fraction / second]
\tf32 m_height_speed = 5.0f;
''',
    '''\t// console open/close animation speed [screen height fraction / second]
\tf32 m_height_speed = 5.0f;
\tu64 m_navycraft_style_revision = 0;
''',
    "NavyCraft chat console style revision")
chat_console_h.write_text(text, encoding="utf-8")

# Build wiring.
cmake = required["cmake"]
text = cmake.read_text(encoding="utf-8")
text = replace_once(text,
    "add_subdirectory(server)\n",
    "add_subdirectory(server)\nadd_subdirectory(navycraft)\n",
    "server subdirectory")
text = replace_once(text,
    "set(common_SRCS\n\t${common_HDRS}\n",
    "set(common_SRCS\n\t${common_HDRS}\n\t${navycraft_SRCS}\n",
    "common source list")
text = replace_once(text,
    "list(APPEND client_SRCS\n\t${benchmark_client_SRCS}\n",
    "list(APPEND client_SRCS\n\t${navycraft_client_SRCS}\n\t${benchmark_client_SRCS}\n",
    "client source list")
text = replace_once(text,
    "include_directories(\n\t${PROJECT_BINARY_DIR}\n\t${PROJECT_SOURCE_DIR}\n\t${PROJECT_SOURCE_DIR}/script\n)\n",
    "include_directories(\n\t${PROJECT_BINARY_DIR}\n\t${PROJECT_SOURCE_DIR}\n\t${PROJECT_SOURCE_DIR}/script\n\t${PROJECT_SOURCE_DIR}/navycraft\n\t${PROJECT_SOURCE_DIR}/client\n)\n",
    "NavyCraft include directory")
cmake.write_text(text, encoding="utf-8")

# Lua API registration.
scripting_server = required["scripting_server"]
text = scripting_server.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "lua_api/l_ipc.h"\n',
    '#include "lua_api/l_ipc.h"\n#include "navycraft/script_api.h"\n',
    "scripting include")
text = replace_once(text,
    "\tModApiIPC::Initialize(L, top);\n",
    "\tModApiIPC::Initialize(L, top);\n\tModApiNavyCraft::Initialize(L, top);\n",
    "scripting API initializer")
scripting_server.write_text(text, encoding="utf-8")

# Native binary identity probes. These run before normal engine startup so the
# package verifier can prove the compiled exe exposes the expected fork identity.
main_cpp = required["main_cpp"]
text = main_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "config.h"\n',
    '#include "config.h"\n#include "navycraft/construct/construct_handshake.h"\n',
    "NavyCraft main include")
text = replace_once(text,
    '\tallowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show version information"))));\n',
    '\tallowed_options->insert(std::make_pair("version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show version information"))));\n'
    '\tallowed_options->insert(std::make_pair("navycraft-version", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show NavyCraft native engine version and exit"))));\n'
    '\tallowed_options->insert(std::make_pair("navycraft-protocol", ValueSpec(VALUETYPE_FLAG,\n'
    '\t\t\t_("Show NavyCraft native construct protocol and exit"))));\n',
    "NavyCraft main options")
text = replace_once(text,
    '\tif (cmd_args.getFlag("version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tprint_version(std::cout);\n'
    '\t\treturn 0;\n'
    '\t}\n\n',
    '\tif (cmd_args.getFlag("version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tprint_version(std::cout);\n'
    '\t\treturn 0;\n'
    '\t}\n'
    '\tif (cmd_args.getFlag("navycraft-version")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tconst auto identity = navycraft::makeServerConstructIdentity();\n'
    '\t\tstd::cout << identity.engine_major << "." << identity.engine_minor\n'
    '\t\t\t<< "." << identity.engine_patch << std::endl;\n'
    '\t\treturn 0;\n'
    '\t}\n'
    '\tif (cmd_args.getFlag("navycraft-protocol")) {\n'
    '\t\tporting::attachOrCreateConsole();\n'
    '\t\tstd::cout << navycraft::ConstructHandshakeCodec::CURRENT_PROTOCOL << std::endl;\n'
    '\t\treturn 0;\n'
    '\t}\n\n',
    "NavyCraft main probe handlers")
main_cpp.write_text(text, encoding="utf-8")

# Reserve eight server-to-client commands after stock 5.16.1's 0x64 command.
networkprotocol = required["networkprotocol"]
text = networkprotocol.read_text(encoding="utf-8")
protocol_old = '''\tTOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,\n\t/*\n\t\tstd::string data, zstd-compressed, for each particle:\n\t\t\tu32 len\n\t\t\tu8[len] serialized ParticleParameters\n\t*/\n\n\tTOCLIENT_NUM_MSG_TYPES = 0x65,\n'''
protocol_new = '''\tTOCLIENT_SPAWN_PARTICLE_BATCH = 0x64,\n\t/*\n\t\tstd::string data, zstd-compressed, for each particle:\n\t\t\tu32 len\n\t\t\tu8[len] serialized ParticleParameters\n\t*/\n\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION = 0x65,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM = 0x66,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE = 0x67,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_RESET = 0x68,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT = 0x69,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE = 0x6A,\n\tTOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION = 0x6B,\n\tTOCLIENT_NAVYCRAFT_HANDSHAKE = 0x6C,\n\n\tTOCLIENT_NUM_MSG_TYPES = 0x6D,\n'''
text = replace_once(text, protocol_old, protocol_new, "network protocol command tail")
networkprotocol.write_text(text, encoding="utf-8")

# Reserve client-to-server rider, interaction and native handshake commands after stock 5.16.1's 0x53 command.
text = networkprotocol.read_text(encoding="utf-8")
toserver_old = '''	TOSERVER_UPDATE_CLIENT_INFO = 0x53,
	/*
		v2s16 render_target_size
		f32 gui_scaling
		f32 hud_scaling
		v2f32 max_fs_info
	*/

	TOSERVER_NUM_MSG_TYPES = 0x54,
'''
toserver_new = '''	TOSERVER_UPDATE_CLIENT_INFO = 0x53,
	/*
		v2s16 render_target_size
		f32 gui_scaling
		f32 hud_scaling
		v2f32 max_fs_info
	*/

	TOSERVER_NAVYCRAFT_RIDER_STATE = 0x54,
	TOSERVER_NAVYCRAFT_INTERACTION = 0x55,
	TOSERVER_NAVYCRAFT_HANDSHAKE = 0x56,

	TOSERVER_NUM_MSG_TYPES = 0x57,
'''
text = replace_once(text, toserver_old, toserver_new, "server protocol command tail")
networkprotocol.write_text(text, encoding="utf-8")

# Client opcode dispatch table.
clientopcodes = required["clientopcodes"]
text = clientopcodes.read_text(encoding="utf-8")
opcode_old = '''\t{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,\n};\n'''
opcode_new = '''\t{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     TOCLIENT_STATE_CONNECTED, &Client::handleCommand_SpawnParticleBatch }, // 0x64,\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructSection }, // 0x65\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructTransform }, // 0x66\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructRemove }, // 0x67\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructReset }, // 0x68\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructEffect }, // 0x69\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructProjectile }, // 0x6A\n\t{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftConstructArticulation }, // 0x6B\n\t{ "TOCLIENT_NAVYCRAFT_HANDSHAKE", TOCLIENT_STATE_CONNECTED, &Client::handleCommand_NavyCraftHandshake }, // 0x6C\n};\n'''
text = replace_once(text, opcode_old, opcode_new, "client opcode table tail")
clientopcodes.write_text(text, encoding="utf-8")

# Client-to-server packet factory entry.
text = clientopcodes.read_text(encoding="utf-8")
client_factory_old = '''	{ "TOSERVER_UPDATE_CLIENT_INFO", 2, true }, // 0x53
};
'''
client_factory_new = '''	{ "TOSERVER_UPDATE_CLIENT_INFO", 2, true }, // 0x53
	{ "TOSERVER_NAVYCRAFT_RIDER_STATE", 0, false }, // 0x54
	{ "TOSERVER_NAVYCRAFT_INTERACTION", 0, true }, // 0x55
	{ "TOSERVER_NAVYCRAFT_HANDSHAKE", 0, true }, // 0x56
};
'''
text = replace_once(text, client_factory_old, client_factory_new, "client rider packet factory")
clientopcodes.write_text(text, encoding="utf-8")

# Client declarations and owned scene state.
client_h = required["client_h"]
text = client_h.read_text(encoding="utf-8")
text = replace_once(text,
    "class SSCSMController;\n",
    "class SSCSMController;\nnamespace navycraft { class ClientConstructScene; class ClientConstructEffects; }\n",
    "NavyCraft client forward declaration")
text = replace_once(text,
    "\tvoid handleCommand_Camera(NetworkPacket* pkt);\n",
    "\tvoid handleCommand_Camera(NetworkPacket* pkt);\n"
    "\tvoid handleCommand_NavyCraftHandshake(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructSection(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructTransform(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructRemove(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructReset(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructEffect(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructProjectile(NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftConstructArticulation(NetworkPacket *pkt);\n",
    "NavyCraft handler declarations")
text = replace_once(text,
    "\tvoid loadMods();\n",
    "\tfriend class LocalPlayer;\n\n"
    "\tvoid loadMods();\n"
    "\tbool isNavyCraftProtocolReady() const noexcept;\n"
    "\tvoid sendNavyCraftHandshake();\n"
    "\tvoid ensureNavyCraftConstructScene();\n"
    "\tvoid stepNavyCraftConstructScene(float dtime);\n"
    "\tvoid beginNavyCraftLocalPlayerMove(LocalPlayer *player, float dtime, "
    "v3f &position, v3f &speed);\n"
    "\tvoid finishNavyCraftLocalPlayerMove(LocalPlayer *player, float dtime, "
    "v3f &position, v3f &speed, bool &touching_ground);\n"
    "\tvoid resetNavyCraftConstructScene();\n"
    "\tvoid resetNavyCraftConnection();\n"
    "\tbool sendNavyCraftInteraction(InteractAction action, const PointedThing &pointed);\n",
    "NavyCraft client helper declarations")
text = replace_once(text,
    "\tstd::unique_ptr<ModChannelMgr> m_modchannel_mgr;\n",
    "\tstd::unique_ptr<ModChannelMgr> m_modchannel_mgr;\n\n"
    "\tstd::unique_ptr<navycraft::ClientConstructScene> m_navycraft_construct_scene;\n"
    "\tstd::unique_ptr<navycraft::ClientConstructEffects> m_navycraft_construct_effects;\n"
    "\tdouble m_navycraft_client_time = 0.0;\n"
    "\tu64 m_navycraft_interaction_sequence = 0;\n"
    "\tu64 m_navycraft_handshake_nonce = 0;\n"
    "\tbool m_navycraft_handshake_sent = false;\n"
    "\tbool m_navycraft_handshake_accepted = false;\n",
    "NavyCraft client members")
client_h.write_text(text, encoding="utf-8")


# Server packet sender declaration.
server_h = required["server_h"]
text = server_h.read_text(encoding="utf-8")
text = replace_once(text,
    "class Settings;\n",
    "class Settings;\nnamespace navycraft { struct ConstructWireMessage; }\n",
    "NavyCraft server forward declaration")
text = replace_once(text,
    "\tvoid Send(NetworkPacket *pkt);\n\tvoid Send(session_t peer_id, NetworkPacket *pkt);\n",
    "\tvoid Send(NetworkPacket *pkt);\n\tvoid Send(session_t peer_id, NetworkPacket *pkt);\n"
    "\tvoid handleCommand_NavyCraftHandshake(NetworkPacket *pkt);\n"
    "\tbool IsNavyCraftPeerReady(session_t peer_id);\n"
    "\tvoid ClearNavyCraftHandshakeState(session_t peer_id);\n"
    "\tvoid SendNavyCraftConstructMessage(\n"
    "\t\tsession_t peer_id, const navycraft::ConstructWireMessage &message);\n"
    "\tvoid handleCommand_NavyCraftRiderState(NetworkPacket *pkt);\n"
    "\tbool ReconcileNavyCraftRider(session_t peer_id, v3f &position, v3f &speed);\n"
    "\tvoid ClearNavyCraftRiderState(session_t peer_id);\n"
    "\tvoid SendNavyCraftFullState(session_t peer_id);\n"
    "\tvoid StepNavyCraftNativeSimulation(float dtime);\n"
    "\tvoid StepNavyCraftCarriedObjects(float dtime);\n"
    "\tvoid handleCommand_NavyCraftInteraction(NetworkPacket *pkt);\n"
    "\tvoid StepNavyCraftConstructTimers(float dtime);\n"
    "\tvoid ClearNavyCraftInteractionState(session_t peer_id);\n",
    "NavyCraft server sender declaration")
server_h.write_text(text, encoding="utf-8")

# Step and reset the native scene with the normal client lifecycle.
client_cpp = required["client_cpp"]
text = client_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    '#include "client/texturepaths.h"\n',
    '#include "client/texturepaths.h"\n#include "navycraft/client/client_construct_effects.h"\n#include "navycraft/client/client_construct_scene.h"\n',
    "NavyCraft complete client construct types")
text = replace_once(text,
    "\tm_address_name = address_name;\n",
    "\tresetNavyCraftConnection();\n\tm_address_name = address_name;\n",
    "client reconnect reset")
text = replace_once(text,
    "\tm_env.step(dtime);\n\tm_sound->step(dtime);\n",
    "\tstepNavyCraftConstructScene(dtime);\n"
    "\tm_env.step(dtime);\n"
    "\tm_sound->step(dtime);\n",
    "client scene step")
text = replace_once(text,
    "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n",
    "void Client::interact(InteractAction action, const PointedThing& pointed)\n{\n"
    "\tif (sendNavyCraftInteraction(action, pointed))\n"
    "\t\treturn;\n",
    "construct-local client interaction hook")
client_cpp.write_text(text, encoding="utf-8")

# Integrate native construct motion directly into LocalPlayer's normal movement.
localplayer_cpp = required["localplayer_cpp"]
text = localplayer_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    "\tv3f accel_f(0, -gravity, 0);\n\tconst v3f initial_position = position;\n",
    "\tv3f accel_f(0, -gravity, 0);\n"
    "\tm_client->beginNavyCraftLocalPlayerMove(this, dtime, position, m_speed);\n"
    "\tconst v3f initial_position = position;\n",
    "LocalPlayer pre-physics construct movement")
text = replace_once(text,
    "\t/*\n"
    "\t\tSet new position but keep sneak node set\n"
    "\t*/\n"
    "\tsetPosition(position);\n",
    "\t/*\n"
    "\t\tSet new position but keep sneak node set\n"
    "\t*/\n"
    "\t// Finalize moving-hull collision only after stock Luanti has finished\n"
    "\t// sneak/ledge corrections. The transmitted rider state and the local\n"
    "\t// position now describe the same final pose for this frame.\n"
    "\tm_client->finishNavyCraftLocalPlayerMove(this, dtime, position, m_speed,\n"
    "\t\ttouching_ground);\n"
    "\tsetPosition(position);\n",
    "LocalPlayer final-pose moving-hull collision")
localplayer_cpp.write_text(text, encoding="utf-8")

# Expose transparent triangle indices when a MapBlockMesh is attached to a
# normal Irrlicht scene node instead of being rendered through ClientMap.
mapblock_mesh_h = required["mapblock_mesh_h"]
text = mapblock_mesh_h.read_text(encoding="utf-8")
text = replace_once(text,
    "\tvoid consolidateTransparentBuffers();\n",
    "\tvoid consolidateTransparentBuffers();\n"
    "\tvoid materializeTransparentBuffersForSceneNode();\n",
    "MapBlockMesh scene-node transparency declaration")
mapblock_mesh_h.write_text(text, encoding="utf-8")

mapblock_mesh_cpp = required["mapblock_mesh_cpp"]
text = mapblock_mesh_cpp.read_text(encoding="utf-8")
transparent_anchor = "void MapBlockMesh::consolidateTransparentBuffers()\n{\n"
transparent_method = """void MapBlockMesh::materializeTransparentBuffersForSceneNode()
{
\tstd::vector<scene::SMeshBuffer *> buffers;
\tfor (const auto &triangle : m_transparent_triangles) {
\t\tauto *buffer = triangle.buffer;
\t\tif (std::find(buffers.begin(), buffers.end(), buffer) == buffers.end())
\t\t\tbuffers.push_back(buffer);
\t}
\tfor (auto *buffer : buffers)
\t\tbuffer->Indices->Data.clear();
\tfor (const auto &triangle : m_transparent_triangles) {
\t\ttriangle.buffer->Indices->Data.push_back(triangle.p1);
\t\ttriangle.buffer->Indices->Data.push_back(triangle.p2);
\t\ttriangle.buffer->Indices->Data.push_back(triangle.p3);
\t}
}

"""
if transparent_method not in text:
    if transparent_anchor not in text:
        raise SystemExit("Luanti layout changed: MapBlockMesh transparency anchor missing")
    text = text.replace(transparent_anchor, transparent_method + transparent_anchor, 1)
mapblock_mesh_cpp.write_text(text, encoding="utf-8")

# Server opcode dispatch and outgoing command factories.
serveropcodes = required["serveropcodes"]
text = serveropcodes.read_text(encoding="utf-8")
server_handler_old = '''	{ "TOSERVER_UPDATE_CLIENT_INFO",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53
};
'''
server_handler_new = '''	{ "TOSERVER_UPDATE_CLIENT_INFO",       TOSERVER_STATE_INGAME, &Server::handleCommand_UpdateClientInfo }, // 0x53
	{ "TOSERVER_NAVYCRAFT_RIDER_STATE",   TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftRiderState }, // 0x54
	{ "TOSERVER_NAVYCRAFT_INTERACTION",    TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftInteraction }, // 0x55
	{ "TOSERVER_NAVYCRAFT_HANDSHAKE",      TOSERVER_STATE_INGAME, &Server::handleCommand_NavyCraftHandshake }, // 0x56
};
'''
text = replace_once(text, server_handler_old, server_handler_new, "server rider handler table")
server_factory_old = '''	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     0, true }, // 0x64
};
'''
server_factory_new = '''	{ "TOCLIENT_SPAWN_PARTICLE_BATCH",     0, true }, // 0x64
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_SECTION", 2, true }, // 0x65
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_TRANSFORM", 1, false }, // 0x66
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_REMOVE", 0, true }, // 0x67
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_RESET", 0, true }, // 0x68
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_EFFECT", 1, true }, // 0x69
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_PROJECTILE", 1, false }, // 0x6A
	{ "TOCLIENT_NAVYCRAFT_CONSTRUCT_ARTICULATION", 1, false }, // 0x6B
	{ "TOCLIENT_NAVYCRAFT_HANDSHAKE", 0, true }, // 0x6C
};
'''
text = replace_once(text, server_factory_old, server_factory_new, "server construct command factories")
serveropcodes.write_text(text, encoding="utf-8")

# Late-join synchronisation and authoritative rider reconciliation.
serverpackethandler = required["serverpackethandler"]
text = serverpackethandler.read_text(encoding="utf-8")
text = replace_once(text,
    "	session_t peer_id = pkt->getPeerId();\n	RemoteClient *client = getClient(peer_id, CS_Created);\n",
    "	session_t peer_id = pkt->getPeerId();\n"
    "	ClearNavyCraftHandshakeState(peer_id);\n"
    "	ClearNavyCraftRiderState(peer_id);\n"
    "	ClearNavyCraftInteractionState(peer_id);\n"
    "	RemoteClient *client = getClient(peer_id, CS_Created);\n",
    "client-ready rider reset")
# Full construct state is sent only after the native protocol handshake succeeds.
text = replace_once(text,
    "	if (!playersao->isAttached()) {\n",
    "	const bool navycraft_rider = ReconcileNavyCraftRider(pkt->getPeerId(), position, speed);\n\n"
    "	if (!playersao->isAttached()) {\n",
    "server rider reconciliation")
text = replace_once(text,
    "	if (playersao->checkMovementCheat()) {\n",
    "	if (!navycraft_rider && playersao->checkMovementCheat()) {\n",
    "rider movement cheat exemption")
serverpackethandler.write_text(text, encoding="utf-8")

# Advance authoritative constructs before normal object physics, then carry
# remote riders and other supported objects after the environment step.
server_cpp = required["server_cpp"]
text = server_cpp.read_text(encoding="utf-8")
text = replace_once(text,
    "\t\t// Step environment\n\t\tm_env->step(dtime);\n",
    "\t\t// Advance NavyCraft's authoritative fixed-step simulation before normal object physics.\n"
    "\t\tStepNavyCraftNativeSimulation(dtime);\n"
    "\t\t// Step environment\n\t\tm_env->step(dtime);\n"
    "\t\tStepNavyCraftCarriedObjects(dtime);\n"
    "\t\tStepNavyCraftConstructTimers(dtime);\n",
    "server native simulation and carried-object step")
server_cpp.write_text(text, encoding="utf-8")

print(f"Applied NavyCraft native Gate 9 engine overlay to {luanti}")
