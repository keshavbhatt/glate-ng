#include "KeyGrab.h"
#include "KeyNames.h"

#include <QCoreApplication>
#include <QGuiApplication>

#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <utility>
#include <xcb/xcb.h>

using namespace Qt::StringLiterals;

namespace glate::shortcut {

namespace {

// Grab the key with and without Num Lock and Caps Lock, so they do not get in the way.
constexpr std::array<std::uint16_t, 4> kLockMasks = {
    0,
    XCB_MOD_MASK_2,
    XCB_MOD_MASK_LOCK,
    XCB_MOD_MASK_2 | XCB_MOD_MASK_LOCK,
};
constexpr std::uint8_t kEventTypeMask = 0x7f; // response_type without the "sent" bit
constexpr unsigned kWithoutLocks =
    0xffffU ^ (static_cast<unsigned>(XCB_MOD_MASK_2) | static_cast<unsigned>(XCB_MOD_MASK_LOCK));

template <typename T>
using Reply = std::unique_ptr<T, decltype(&std::free)>;

xcb_connection_t* connection()
{
    if (QGuiApplication::platformName() != u"xcb"_s) {
        return nullptr;
    }
    const auto* native = qGuiApp->nativeInterface<QNativeInterface::QX11Application>();
    return native != nullptr ? native->connection() : nullptr;
}

xcb_window_t rootWindow(xcb_connection_t* conn)
{
    return xcb_setup_roots_iterator(xcb_get_setup(conn)).data->root;
}

bool isXwayland(xcb_connection_t* conn)
{
    static constexpr char kName[] = "XWAYLAND";
    const Reply<xcb_query_extension_reply_t> reply(
        xcb_query_extension_reply(conn, xcb_query_extension(conn, sizeof kName - 1, kName), nullptr),
        &std::free);
    return reply && reply->present != 0;
}

std::optional<std::uint8_t> keycode(xcb_connection_t* conn, std::uint32_t keysym)
{
    const xcb_setup_t* setup = xcb_get_setup(conn);
    const auto count = static_cast<std::uint8_t>(setup->max_keycode - setup->min_keycode + 1);
    const Reply<xcb_get_keyboard_mapping_reply_t> map(
        xcb_get_keyboard_mapping_reply(conn, xcb_get_keyboard_mapping(conn, setup->min_keycode, count),
                                       nullptr),
        &std::free);
    if (!map) {
        return std::nullopt;
    }
    const xcb_keysym_t* syms = xcb_get_keyboard_mapping_keysyms(map.get());
    const int perCode = map->keysyms_per_keycode;
    for (int code = 0; std::cmp_less(code, count); ++code) {
        for (int i = 0; i < perCode; ++i) {
            if (syms[(code * perCode) + i] == keysym) {
                return static_cast<std::uint8_t>(setup->min_keycode + code);
            }
        }
    }
    return std::nullopt;
}

} // namespace

KeyGrab::KeyGrab(QObject* parent)
    : QObject(parent)
{
    if (isSupported()) {
        QCoreApplication::instance()->installNativeEventFilter(this);
    }
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
    xcb_connection_t* conn = connection();
    return conn != nullptr && !isXwayland(conn);
}

bool KeyGrab::grab(const QKeySequence& keys)
{
    release();
    xcb_connection_t* conn = connection();
    if (conn == nullptr || keys.isEmpty()) {
        return false;
    }
    const QKeyCombination combo = keys[0];
    const auto sym = x11Keysym(combo.key());
    const auto code = sym ? keycode(conn, *sym) : std::nullopt;
    if (!code) {
        return false;
    }
    m_code = *code;
    m_modifiers = x11Modifiers(combo.keyboardModifiers());
    bool grabbed = true;
    for (const std::uint16_t lock : kLockMasks) {
        const xcb_void_cookie_t cookie = xcb_grab_key_checked(
            conn, 1, rootWindow(conn), static_cast<std::uint16_t>(m_modifiers | lock),
            static_cast<xcb_keycode_t>(m_code), XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC);
        const Reply<xcb_generic_error_t> error(xcb_request_check(conn, cookie), &std::free);
        grabbed = grabbed && !error; // another app holds these keys
    }
    if (!grabbed) {
        release();
    }
    return grabbed;
}

void KeyGrab::release()
{
    xcb_connection_t* conn = connection();
    if (conn == nullptr || m_code == 0) {
        m_code = 0;
        return;
    }
    for (const std::uint16_t lock : kLockMasks) {
        xcb_ungrab_key(conn, static_cast<xcb_keycode_t>(m_code), rootWindow(conn),
                       static_cast<std::uint16_t>(m_modifiers | lock));
    }
    xcb_flush(conn);
    m_code = 0;
}

bool KeyGrab::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* /*result*/)
{
    if (m_code == 0 || eventType != "xcb_generic_event_t") {
        return false;
    }
    const auto* event = static_cast<const xcb_generic_event_t*>(message);
    if ((event->response_type & kEventTypeMask) != XCB_KEY_PRESS) {
        return false;
    }
    const auto* key = static_cast<const xcb_key_press_event_t*>(message);
    const unsigned state = static_cast<unsigned>(key->state) & kWithoutLocks;
    if (key->detail != m_code || state != m_modifiers) {
        return false;
    }
    Q_EMIT activated();
    return true;
}

} // namespace glate::shortcut
