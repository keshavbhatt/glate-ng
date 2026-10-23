#pragma once

#include <QKeySequence>
#include <QString>

#include <cstdint>
#include <optional>

// Converts the first key of a QKeySequence into what each platform needs. Pure
// functions, so they are tested without a display.
namespace glate::shortcut {

// The portal's preferred_trigger, in the XDG shortcuts format: "CTRL+SHIFT+space".
[[nodiscard]] QString portalTrigger(const QKeySequence& keys);

// The X11 keysym for a key: letters, digits, space, F1 to F24 and the editing
// and navigation keys; nothing for others.
[[nodiscard]] std::optional<std::uint32_t> x11Keysym(Qt::Key key);
// X11 modifier bits: Shift, Control, Mod1 for Alt, Mod4 for Meta.
[[nodiscard]] std::uint16_t x11Modifiers(Qt::KeyboardModifiers modifiers);

// The Windows virtual-key code for a key, from the same set as x11Keysym().
[[nodiscard]] std::optional<std::uint32_t> windowsVirtualKey(Qt::Key key);
// Windows MOD_ bits: MOD_ALT 1, MOD_CONTROL 2, MOD_SHIFT 4, MOD_WIN 8.
[[nodiscard]] std::uint32_t windowsModifiers(Qt::KeyboardModifiers modifiers);

} // namespace glate::shortcut
