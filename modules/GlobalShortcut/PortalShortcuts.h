#pragma once

#include <QDBusArgument>
#include <QDBusConnection>
#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

#include <functional>

class QDBusMessage;

namespace glate::shortcut {

/// One entry of the portal's `a(sa{sv})` shortcut lists: an id and its properties
/// (`description`, `preferred_trigger`, `trigger_description`).
struct PortalShortcut
{
    QString id;
    QVariantMap properties;
};

QDBusArgument& operator<<(QDBusArgument& argument, const PortalShortcut& shortcut);
const QDBusArgument& operator>>(const QDBusArgument& argument, PortalShortcut& shortcut);

/// One shortcut registered with the desktop through the xdg-desktop-portal
/// GlobalShortcuts portal: the only way a Wayland app, or a sandboxed one (Flatpak,
/// snap), can have a global shortcut. The desktop owns the key: it asks the user to
/// accept it once, and its own settings change it. Works while the app runs. Where
/// there is no portal (GNOME 46 and 47, wlroots desktops) the state says so.
/// Adapted from the owner's Omnidict (services::GlobalShortcuts).
class PortalShortcuts : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PortalShortcuts)

public:
    enum class State
    {
        Checking,    ///< asking the portal
        Unavailable, ///< no GlobalShortcuts portal on this desktop
        Active,      ///< bound; trigger() says which key
        NoKey,       ///< accepted, but the desktop bound no key: the user has to choose one
        Declined,    ///< the user or the desktop refused the shortcut
    };

    PortalShortcuts(QString appId, QString shortcutId, QString description, QObject* parent = nullptr);
    ~PortalShortcuts() override;

    /// The key suggested to the desktop on the first bind ("CTRL+SHIFT+space");
    /// the desktop may choose another.
    void setPreferredTrigger(const QString& trigger) { m_preferredTrigger = trigger; }

    /// Registers the shortcut, asynchronously; stateChanged() reports the outcome.
    /// Calling it again starts over (after Declined, for example).
    void start();
    /// Closes the session: the shortcut stops reaching the app.
    void stop();

    [[nodiscard]] State state() const { return m_state; }
    /// The key as the desktop describes it ("Ctrl+Alt+D"); empty unless Active.
    [[nodiscard]] QString trigger() const { return m_trigger; }
    /// Whether the desktop can open its own dialog to change the key (portal version 2).
    [[nodiscard]] bool canConfigure() const
    {
        return (m_state == State::Active || m_state == State::NoKey) && m_version >= 2;
    }
    /// Opens the desktop's dialog to change the key; `parentWindow` per the portal
    /// spec ("x11:<xid>", "wayland:<handle>" or empty).
    void configure(const QString& parentWindow = {});

    /// The object path the portal answers a request on, known before the call so the
    /// answer cannot be missed: /org/freedesktop/portal/desktop/request/<sender>/<token>.
    [[nodiscard]] static QString requestPath(const QString& uniqueName, const QString& token);

Q_SIGNALS:
    /// The shortcut was pressed; `activationToken` lets the window come to the front
    /// on Wayland (empty when the desktop gives none).
    void activated(const QByteArray& activationToken);
    void stateChanged();

private Q_SLOTS:
    void onResponse(const QDBusMessage& message);
    void onActivated(const QDBusMessage& message);
    void onShortcutsChanged(const QDBusMessage& message);

private:
    using ResponseHandler = std::function<void(uint response, const QVariantMap& results)>;

    void registerHostApp();
    void readVersion();
    void createSession();
    void listShortcuts();
    void bindShortcut();
    void setState(State state);
    /// Takes the key from the portal's list and becomes Active, or NoKey without one.
    void takeTrigger(const QList<PortalShortcut>& shortcuts);
    [[nodiscard]] QDBusConnection bus() const;
    /// Calls a portal method that answers through a Request object.
    void request(const QString& method, QList<QVariant> arguments, QVariantMap options,
                 ResponseHandler onResponse);

    QString m_appId;
    QString m_shortcutId;
    QString m_description;
    QString m_preferredTrigger;
    /// A connection of its own: the portal ties an app id to a connection on its first
    /// call, and Qt's own portal calls (colour scheme, file dialogs) come first on the
    /// shared one, so registering there fails and KDE files the shortcut under whatever
    /// started the app (issue #1).
    QString m_busName;
    State m_state = State::Checking;
    QString m_session;
    QString m_trigger;
    uint m_version = 0;
    bool m_listening = false; ///< Activated and ShortcutsChanged are connected
    quint64 m_nextToken = 0;
    QHash<QString, ResponseHandler> m_pending; ///< request path -> what to do with the answer
};

} // namespace glate::shortcut

Q_DECLARE_METATYPE(glate::shortcut::PortalShortcut)
