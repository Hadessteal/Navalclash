// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once

#include <cstdint>

namespace navycraft {

enum class ChatAnchor {
	TopLeft,
	BottomLeft,
};

enum class ChatFontMode {
	Default,
	Standard,
	Mono,
};

struct ChatColor {
	bool enabled = false;
	int alpha = 255;
	int red = 255;
	int green = 255;
	int blue = 255;
};

struct ChatStyle {
	ChatAnchor recent_anchor = ChatAnchor::TopLeft;
	ChatAnchor console_anchor = ChatAnchor::TopLeft;
	int recent_margin_left = 10;
	int recent_margin_right = 20;
	int recent_margin_top = 5;
	int recent_margin_bottom = 62;
	int console_margin_left = 0;
	int console_margin_right = 0;
	int console_margin_top = 0;
	int console_margin_bottom = 0;
	ChatFontMode recent_font_mode = ChatFontMode::Default;
	ChatFontMode console_font_mode = ChatFontMode::Default;
	int recent_font_size = 0;
	int console_font_size = 0;
	int recent_line_spacing = 0;
	int console_line_spacing = 0;
	float console_height_speed = 5.0f;
	ChatColor recent_text_color;
	ChatColor console_text_color;
	ChatColor prompt_text_color;
	ChatColor console_background_color;
	std::uint64_t revision = 1;
};

ChatStyle currentChatStyle();
void setChatStyle(ChatStyle style);

} // namespace navycraft
