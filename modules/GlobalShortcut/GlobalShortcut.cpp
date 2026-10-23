#include "GlobalShortcut.h"

#include "KeyGrab.h"
#include "KeyNames.h"
#include "ShortcutLogging.h"

#ifdef GLATE_SHORTCUT_PORTAL
#include "PortalShortcuts.h"
#endif

#include <QGuiApplication>

#include <utility>

using namespace Qt::StringLiterals;

namespace glate::shortcut {

Q_LOGGING_CATEGORY(lcShortcut, "glate.shortcut", QtInfoMsg)

GlobalShortcut::GlobalShortcut(QString appId, QString shortcutId, QString description, QObject* parent)
    : QObject(parent)
    , m_appId(std::move(appId))
    , m_shortcutId(std::move(shortcutId))
    , m_description(std::move(description))
    , m_method(platformMethod())
{
    if (m_method == Method::Grab) {
        m_grab = new KeyGrab(this);
        connect(m_grab, &KeyGrab::activated, this, [this] { Q_EMIT activated({}); });
    }
#ifdef GLATE_SHORTCUT_PORTAL
    if (m_method == Method::Portal) {
        m_portal = new PortalShortcuts(m_appId, m_shortcutId, m_description, this);
        connect(m_portal, &PortalShortcuts::activated, this, &GlobalShortcut::activated);
        connect(m_portal, &PortalShortcuts::stateChanged, this, &GlobalShortcut::takePortalState);
    }
#endif
}

GlobalShortcut::~GlobalShortcut() = default;

GlobalShortcut::Method GlobalShortcut::platformMethod()
{
    if (KeyGrab::isSupported()) {
        return Method::Grab;
    }
#ifdef GLATE_SHORTCUT_PORTAL
    const QString platform = QGuiApplication::platformName();
    if (platform.startsWith(u"wayland"_s) || platform == u"xcb"_s) { // xcb here: Xwayland
        return Method::Portal;
    }
#endif
    return Method::None;
}

QString GlobalShortcut::trigger() const
{
#ifdef GLATE_SHORTCUT_PORTAL
    if (m_portal != nullptr) {
        return m_portal->trigger();
    }
#endif
    return m_state == State::Active ? m_keys.toString(QKeySequence::NativeText) : QString();
}

bool GlobalShortcut::canConfigure() const
{
#ifdef GLATE_SHORTCUT_PORTAL
    return m_enabled && m_portal != nullptr && m_portal->canConfigure();
#else
    return false;
#endif
}

void GlobalShortcut::configure()
{
#ifdef GLATE_SHORTCUT_PORTAL
    if (canConfigure()) {
        m_portal->configure();
    }
#endif
}

void GlobalShortcut::setKeys(const QKeySequence& keys)
{
    if (keys == m_keys) {
        return;
    }
    m_keys = keys;
    // The desktop owns a portal key: new keys only matter for a later first bind.
    if (m_method == Method::Grab) {
        apply();
    }
}

void GlobalShortcut::setEnabled(bool enabled)
{
    if (enabled == m_enabled) {
        return;
    }
    m_enabled = enabled;
    apply();
}

void GlobalShortcut::retry()
{
    apply();
}

void GlobalShortcut::apply()
{
    if (!m_enabled) {
        if (m_grab != nullptr) {
            m_grab->release();
        }
#ifdef GLATE_SHORTCUT_PORTAL
        if (m_portal != nullptr) {
            m_portal->stop();
        }
#endif
        setState(State::Off);
        return;
    }
    switch (m_method) {
    case Method::Grab:
        if (m_grab->grab(m_keys)) {
            setState(State::Active);
        } else {
            qCInfo(lcShortcut) << "the shortcut keys could not be grabbed";
            setState(State::Taken);
        }
        return;
    case Method::Portal:
#ifdef GLATE_SHORTCUT_PORTAL
        m_portal->setPreferredTrigger(portalTrigger(m_keys));
        setState(State::Checking);
        m_portal->start();
#endif
        return;
    case Method::None:
        setState(State::Unavailable);
        return;
    }
}

void GlobalShortcut::takePortalState()
{
#ifdef GLATE_SHORTCUT_PORTAL
    if (!m_enabled) {
        return;
    }
    switch (m_portal->state()) {
    case PortalShortcuts::State::Checking:
        setState(State::Checking);
        break;
    case PortalShortcuts::State::Unavailable:
        setState(State::Unavailable);
        break;
    case PortalShortcuts::State::Active:
        setState(State::Active);
        break;
    case PortalShortcuts::State::NoKey:
        setState(State::NoKey);
        break;
    case PortalShortcuts::State::Declined:
        setState(State::Declined);
        break;
    }
    Q_EMIT stateChanged(); // the key may change while the state stays Active
#endif
}

void GlobalShortcut::setState(State state)
{
    if (state == m_state) {
        return;
    }
    m_state = state;
    Q_EMIT stateChanged();
}

} // namespace glate::shortcut
