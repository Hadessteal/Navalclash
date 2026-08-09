// SPDX-License-Identifier: LGPL-2.1-or-later
#include "chat_style.h"

#include <mutex>

namespace navycraft {
namespace {
std::mutex g_chat_style_mutex;
ChatStyle g_chat_style;
}

ChatStyle currentChatStyle()
{
	std::lock_guard<std::mutex> lock(g_chat_style_mutex);
	return g_chat_style;
}

void setChatStyle(ChatStyle style)
{
	std::lock_guard<std::mutex> lock(g_chat_style_mutex);
	style.revision = g_chat_style.revision + 1;
	g_chat_style = style;
}

} // namespace navycraft
