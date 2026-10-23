#include "KeyNames.h"

#include <QStringList>

#include <array>
#include <utility>

using namespace Qt::StringLiterals;

namespace glate::shortcut {

namespace {

struct NamedKey
{
    Qt::Key key;
    std::uint32_t keysym;
    std::uint32_t virtualKey;
    const char* portalName;
};

// Keys outside the letter, digit and function-key ranges.
constexpr std::array kNamedKeys{
    NamedKey{.key = Qt::Key_Space, .keysym = 0x20, .virtualKey = 0x20, .portalName = "space"},
    NamedKey{.key = Qt::Key_Return, .keysym = 0xff0d, .virtualKey = 0x0d, .portalName = "Return"},
    NamedKey{.key = Qt::Key_Tab, .keysym = 0xff09, .virtualKey = 0x09, .portalName = "Tab"},
    NamedKey{.key = Qt::Key_Insert, .keysym = 0xff63, .virtualKey = 0x2d, .portalName = "Insert"},
    NamedKey{.key = Qt::Key_Delete, .keysym = 0xffff, .virtualKey = 0x2e, .portalName = "Delete"},
    NamedKey{.key = Qt::Key_Home, .keysym = 0xff50, .virtualKey = 0x24, .portalName = "Home"},
    NamedKey{.key = Qt::Key_End, .keysym = 0xff57, .virtualKey = 0x23, .portalName = "End"},
    NamedKey{.key = Qt::Key_PageUp, .keysym = 0xff55, .virtualKey = 0x21, .portalName = "Page_Up"},
    NamedKey{.key = Qt::Key_PageDown, .keysym = 0xff56, .virtualKey = 0x22, .portalName = "Page_Down"},
    NamedKey{.key = Qt::Key_Left, .keysym = 0xff51, .virtualKey = 0x25, .portalName = "Left"},
    NamedKey{.key = Qt::Key_Up, .keysym = 0xff52, .virtualKey = 0x26, .portalName = "Up"},
    NamedKey{.key = Qt::Key_Right, .keysym = 0xff53, .virtualKey = 0x27, .portalName = "Right"},
    NamedKey{.key = Qt::Key_Down, .keysym = 0xff54, .virtualKey = 0x28, .portalName = "Down"},
    NamedKey{.key = Qt::Key_Pause, .keysym = 0xff13, .virtualKey = 0x13, .portalName = "Pause"},
    NamedKey{.key = Qt::Key_Print, .keysym = 0xff61, .virtualKey = 0x2c, .portalName = "Print"},
};

constexpr std::uint32_t kKeysymF1 = 0xffbe;
constexpr std::uint32_t kVirtualKeyF1 = 0x70;
constexpr int kFunctionKeys = 24;

const NamedKey* namedKey(Qt::Key key)
{
    for (const NamedKey& named : kNamedKeys) {
        if (named.key == key) {
            return &named;
        }
    }
    return nullptr;
}

bool isLetter(Qt::Key key)
{
    return key >= Qt::Key_A && key <= Qt::Key_Z;
}

bool isDigit(Qt::Key key)
{
    return key >= Qt::Key_0 && key <= Qt::Key_9;
}

std::optional<int> functionKey(Qt::Key key)
{
    const int index = static_cast<int>(key) - static_cast<int>(Qt::Key_F1);
    if (index >= 0 && index < kFunctionKeys) {
        return index;
    }
    return std::nullopt;
}

} // namespace

QString portalTrigger(const QKeySequence& keys)
{
    if (keys.isEmpty()) {
        return {};
    }
    const QKeyCombination combo = keys[0];
    QStringList parts;
    const Qt::KeyboardModifiers modifiers = combo.keyboardModifiers();
    if (modifiers.testFlag(Qt::ControlModifier)) {
        parts << u"CTRL"_s;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        parts << u"ALT"_s;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        parts << u"SHIFT"_s;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        parts << u"LOGO"_s;
    }
    const Qt::Key key = combo.key();
    if (const NamedKey* named = namedKey(key)) {
        parts << QString::fromLatin1(named->portalName);
    } else if (isLetter(key)) {
        parts << QString(QChar(u'a' + (key - Qt::Key_A)));
    } else if (isDigit(key)) {
        parts << QString(QChar(u'0' + (key - Qt::Key_0)));
    } else if (const auto index = functionKey(key)) {
        parts << u"F%1"_s.arg(*index + 1);
    } else {
        return {};
    }
    return parts.join(u'+');
}

std::optional<std::uint32_t> x11Keysym(Qt::Key key)
{
    if (isLetter(key)) {
        return static_cast<std::uint32_t>('a' + (key - Qt::Key_A));
    }
    if (isDigit(key)) {
        return static_cast<std::uint32_t>(key);
    }
    if (const auto index = functionKey(key)) {
        return kKeysymF1 + static_cast<std::uint32_t>(*index);
    }
    if (const NamedKey* named = namedKey(key)) {
        return named->keysym;
    }
    return std::nullopt;
}

std::uint16_t x11Modifiers(Qt::KeyboardModifiers modifiers)
{
    constexpr std::uint16_t kShift = 1U << 0U;
    constexpr std::uint16_t kControl = 1U << 2U;
    constexpr std::uint16_t kMod1 = 1U << 3U;
    constexpr std::uint16_t kMod4 = 1U << 6U;
    std::uint16_t out = 0;
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        out |= kShift;
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        out |= kControl;
    }
    if (modifiers.testFlag(Qt::AltModifier)) {
        out |= kMod1;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        out |= kMod4;
    }
    return out;
}

std::optional<std::uint32_t> windowsVirtualKey(Qt::Key key)
{
    if (isLetter(key) || isDigit(key)) {
        return static_cast<std::uint32_t>(key); // 'A' to 'Z' and '0' to '9', as Qt's
    }
    if (const auto index = functionKey(key)) {
        return kVirtualKeyF1 + static_cast<std::uint32_t>(*index);
    }
    if (const NamedKey* named = namedKey(key)) {
        return named->virtualKey;
    }
    return std::nullopt;
}

std::uint32_t windowsModifiers(Qt::KeyboardModifiers modifiers)
{
    std::uint32_t out = 0;
    if (modifiers.testFlag(Qt::AltModifier)) {
        out |= 1U;
    }
    if (modifiers.testFlag(Qt::ControlModifier)) {
        out |= 2U;
    }
    if (modifiers.testFlag(Qt::ShiftModifier)) {
        out |= 4U;
    }
    if (modifiers.testFlag(Qt::MetaModifier)) {
        out |= 8U;
    }
    return out;
}

} // namespace glate::shortcut
