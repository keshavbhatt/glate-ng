#pragma once

#include <QByteArray>
#include <QKeySequence>
#include <QObject>
#include <QString>

namespace glate::shortcut {

class KeyGrab;
class PortalShortcuts;

// A key that works while another app has focus. Picks the way the platform
// allows (DOCS/DECISIONS.md ADR-009):
//   - X11 (not Xwayland) and Windows: the app grabs the keys itself.
//   - Wayland, or X11 through Xwayland: the desktop owns the key, through the
//     xdg-desktop-portal GlobalShortcuts portal; the keys set here are only a
//     suggestion for the first bind.
//   - Anything else (macOS, the offscreen platform of tests): unsupported.
class GlobalShortcut : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GlobalShortcut)

public:
    enum class Method
    {
        None,   // no way on this platform
        Grab,   // the app holds the keys (X11, Windows)
        Portal, // the desktop holds the key
    };

    enum class State
    {
        Off,         // disabled
        Checking,    // waiting for the desktop
        Active,      // works; trigger() says which keys
        Taken,       // Grab: another app holds these keys, or they have no native code
        NoKey,       // Portal: accepted, but the user still has to choose a key
        Declined,    // Portal: the user or the desktop refused it
        Unavailable, // no way on this platform or desktop
    };

    // `appId` is the desktop file id; `shortcutId` and `description` name the
    // shortcut in the desktop's settings.
    GlobalShortcut(QString appId, QString shortcutId, QString description, QObject* parent = nullptr);
    ~GlobalShortcut() override;

    // The way this platform allows, decided once from the Qt platform.
    [[nodiscard]] static Method platformMethod();

    [[nodiscard]] Method method() const { return m_method; }
    [[nodiscard]] State state() const { return m_state; }
    // The keys as shown to the user ("Ctrl+Shift+Space"); empty when there are none.
    [[nodiscard]] QString trigger() const;
    // Portal: whether the desktop can open its own dialog to change the key.
    [[nodiscard]] bool canConfigure() const;
    // Portal: opens the desktop's dialog to change the key.
    void configure();

    // The keys to grab, or to suggest to the desktop.
    void setKeys(const QKeySequence& keys);
    [[nodiscard]] QKeySequence keys() const { return m_keys; }
    void setEnabled(bool enabled);
    [[nodiscard]] bool isEnabled() const { return m_enabled; }
    // Asks again (after Declined or Unavailable, for example).
    void retry();

Q_SIGNALS:
    // The keys were pressed; `activationToken` lets the window come to the front on
    // Wayland (empty when there is none).
    void activated(const QByteArray& activationToken);
    void stateChanged();

private:
    void apply();
    void setState(State state);
    void takePortalState();

    QString m_appId;
    QString m_shortcutId;
    QString m_description;
    Method m_method;
    State m_state = State::Off;
    QKeySequence m_keys;
    bool m_enabled = false;
    KeyGrab* m_grab = nullptr;
    PortalShortcuts* m_portal = nullptr;
};

} // namespace glate::shortcut
