#pragma once

#include <QLoggingCategory>

namespace glate::shortcut {
// "glate.shortcut": which way the global shortcut works and what the desktop answered.
// Never the keys' text input, only states and errors.
Q_DECLARE_LOGGING_CATEGORY(lcShortcut)
} // namespace glate::shortcut
