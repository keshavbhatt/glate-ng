#include "KeyGrab.h"
#include "KeyNames.h"

#include <QCoreApplication>

#include <windows.h>

namespace glate::shortcut {

namespace {
constexpr int kHotkeyId = 0x6c61; // any id the app does not use elsewhere
} // namespace

KeyGrab::KeyGrab(QObject* parent)
    : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

KeyGrab::~KeyGrab()
{
    release();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

bool KeyGrab::isSupported()
{
    return true;
}

bool KeyGrab::grab(const QKeySequence& keys)
{
    release();
    if (keys.isEmpty()) {
        return false;
    }
    const QKeyCombination combo = keys[0];
    const auto virtualKey = windowsVirtualKey(combo.key());
    if (!virtualKey) {
        return false;
    }
    const std::uint32_t modifiers = windowsModifiers(combo.keyboardModifiers()) | MOD_NOREPEAT;
    // A thread hotkey: WM_HOTKEY arrives in this thread's message queue.
    if (RegisterHotKey(nullptr, kHotkeyId, modifiers, *virtualKey) == 0) {
        return false; // another app holds these keys
    }
    m_code = *virtualKey;
    m_modifiers = modifiers;
    return true;
}

void KeyGrab::release()
{
    if (m_code != 0) {
        UnregisterHotKey(nullptr, kHotkeyId);
    }
    m_code = 0;
}

bool KeyGrab::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/)
{
    if (m_code == 0 || eventType != "windows_generic_MSG") {
        return false;
    }
    const auto* msg = static_cast<const MSG*>(message);
    if (msg->message != WM_HOTKEY || msg->wParam != kHotkeyId) {
        return false;
    }
    Q_EMIT activated();
    return true;
}

} // namespace glate::shortcut
