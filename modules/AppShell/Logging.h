#pragma once

#include <QLoggingCategory>

// Logging categories for Glate. Enable debug output with
// QT_LOGGING_RULES="glate.*.debug=true". Never log the text being translated,
// its translation, or paths under the home directory: the log can be attached
// to a bug report (DOCS/DECISIONS.md ADR-008).
namespace glate::shell {

Q_DECLARE_LOGGING_CATEGORY(lcApp)
Q_DECLARE_LOGGING_CATEGORY(lcNetwork)
Q_DECLARE_LOGGING_CATEGORY(lcOffline)
Q_DECLARE_LOGGING_CATEGORY(lcSpeech)
Q_DECLARE_LOGGING_CATEGORY(lcLicense)

} // namespace glate::shell
