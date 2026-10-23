#pragma once

#include <QAbstractNativeEventFilter>
#include <QKeySequence>
#include <QObject>

#include <cstdint>

namespace glate::shortcut {

// A key the app grabs itself: XGrabKey on X11, RegisterHotKey on Windows. One
// implementation file per platform (KeyGrab_x11.cpp, KeyGrab_win.cpp,
// KeyGrab_none.cpp). Not used on Wayland, where only the desktop may own keys.
class KeyGrab : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(KeyGrab)

public:
    explicit KeyGrab(QObject* parent = nullptr);
    ~KeyGrab() override;

    // True where this platform can grab keys: an X11 server that is not Xwayland, or Windows.
    [[nodiscard]] static bool isSupported();

    // Grabs the first key of `keys`, releasing any earlier one. False when the key
    // cannot be grabbed (another app holds it, or the key has no native code).
    bool grab(const QKeySequence& keys);
    void release();
    [[nodiscard]] bool isGrabbed() const { return m_code != 0; }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

Q_SIGNALS:
    void activated();

private:
    std::uint32_t m_code = 0;      // keycode (X11) or virtual key (Windows); 0 when nothing is grabbed
    std::uint32_t m_modifiers = 0; // native modifier bits
};

} // namespace glate::shortcut
