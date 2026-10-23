#include "KeyGrab.h"

// Platforms without a key grab of their own (macOS): the shortcut is unsupported.
namespace glate::shortcut {

KeyGrab::KeyGrab(QObject* parent)
    : QObject(parent)
{}

KeyGrab::~KeyGrab() = default;

bool KeyGrab::isSupported()
{
    return false;
}

bool KeyGrab::grab(const QKeySequence& /*keys*/)
{
    return false;
}

void KeyGrab::release()
{
    m_code = 0;
}

bool KeyGrab::nativeEventFilter(const QByteArray& /*eventType*/, void* /*message*/, qintptr* /*result*/)
{
    return false;
}

} // namespace glate::shortcut
