#include "GlobalShortcut.h"
#include "KeyNames.h"

#ifdef GLATE_SHORTCUT_PORTAL
#include "PortalShortcuts.h"
#include "fake_portal.h"
#endif

#include <QSignalSpy>
#include <QTest>

#include <memory>

using namespace Qt::StringLiterals;
using namespace glate::shortcut;

class TestGlobalShortcut : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
#ifdef GLATE_SHORTCUT_PORTAL
        // Outside a sandbox: the client registers its app id with the portal.
        qunsetenv("SNAP");
        qunsetenv("FLATPAK_ID");
        m_bus = std::make_unique<glate::tests::PrivateBus>();
        // Without dbus-daemon, a bus that does not exist: nothing reaches the real session.
        const QString address =
            m_bus->address().isEmpty() ? u"unix:path=/nonexistent/glate-test-bus"_s : m_bus->address();
        qputenv("DBUS_SESSION_BUS_ADDRESS", address.toUtf8());
#endif
    }

    void portalTriggersUseTheXdgNames()
    {
        QCOMPARE(portalTrigger(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space)), u"CTRL+SHIFT+space"_s);
        QCOMPARE(portalTrigger(QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_T)), u"CTRL+ALT+t"_s);
        QCOMPARE(portalTrigger(QKeySequence(Qt::META | Qt::Key_F2)), u"LOGO+F2"_s);
        QCOMPARE(portalTrigger(QKeySequence(Qt::ALT | Qt::Key_PageDown)), u"ALT+Page_Down"_s);
        QVERIFY(portalTrigger(QKeySequence()).isEmpty());
        QVERIFY(portalTrigger(QKeySequence(Qt::CTRL | Qt::Key_Eacute)).isEmpty());
    }

    void nativeCodesCoverTheUsualKeys()
    {
        QCOMPARE(x11Keysym(Qt::Key_A), std::optional<std::uint32_t>(0x61));
        QCOMPARE(x11Keysym(Qt::Key_5), std::optional<std::uint32_t>(0x35));
        QCOMPARE(x11Keysym(Qt::Key_Space), std::optional<std::uint32_t>(0x20));
        QCOMPARE(x11Keysym(Qt::Key_F1), std::optional<std::uint32_t>(0xffbe));
        QVERIFY(!x11Keysym(Qt::Key_Eacute));
        QCOMPARE(x11Modifiers(Qt::ControlModifier | Qt::ShiftModifier), std::uint16_t(0x5));
        QCOMPARE(x11Modifiers(Qt::AltModifier | Qt::MetaModifier), std::uint16_t(0x48));

        QCOMPARE(windowsVirtualKey(Qt::Key_A), std::optional<std::uint32_t>(0x41));
        QCOMPARE(windowsVirtualKey(Qt::Key_F12), std::optional<std::uint32_t>(0x7b));
        QCOMPARE(windowsVirtualKey(Qt::Key_Space), std::optional<std::uint32_t>(0x20));
        QCOMPARE(windowsModifiers(Qt::ControlModifier | Qt::ShiftModifier), 6U);
        QCOMPARE(windowsModifiers(Qt::AltModifier | Qt::MetaModifier), 9U);
    }

    void withoutAWayItSaysUnavailable()
    {
        // The offscreen platform of the tests has no key grab and no desktop.
        GlobalShortcut shortcut(u"com.ktechpit.glate"_s, u"quick-translate"_s, u"Quick Translate"_s);
        QCOMPARE(shortcut.method(), GlobalShortcut::Method::None);
        QCOMPARE(shortcut.state(), GlobalShortcut::State::Off);
        shortcut.setKeys(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space));
        shortcut.setEnabled(true);
        QCOMPARE(shortcut.state(), GlobalShortcut::State::Unavailable);
        QVERIFY(shortcut.trigger().isEmpty());
        QVERIFY(!shortcut.canConfigure());
        shortcut.setEnabled(false);
        QCOMPARE(shortcut.state(), GlobalShortcut::State::Off);
    }

#ifdef GLATE_SHORTCUT_PORTAL
    void requestPathsFollowThePortalSpec()
    {
        QCOMPARE(PortalShortcuts::requestPath(u":1.42"_s, u"glate7"_s),
                 u"/org/freedesktop/portal/desktop/request/1_42/glate7"_s);
    }

    void withoutAPortalItIsUnavailable()
    {
        PortalShortcuts portal(u"com.ktechpit.glate"_s, u"quick-translate"_s, u"Quick Translate"_s);
        portal.start();
        QTRY_COMPARE(portal.state(), PortalShortcuts::State::Unavailable);
        QVERIFY(!portal.canConfigure());
    }

    void theDesktopsKeyMakesItActive()
    {
        auto fake = makePortal();
        if (!fake) {
            QSKIP("no private D-Bus daemon for the fake portal");
        }
        fake->setTrigger(u"Ctrl+Shift+Space"_s);
        PortalShortcuts portal(u"com.ktechpit.glate"_s, u"quick-translate"_s, u"Quick Translate"_s);
        portal.setPreferredTrigger(u"CTRL+SHIFT+space"_s);
        portal.start();
        QTRY_COMPARE(portal.state(), PortalShortcuts::State::Active);
        QCOMPARE(portal.trigger(), u"Ctrl+Shift+Space"_s);
        QVERIFY(portal.canConfigure());
        QCOMPARE(fake->registeredAppIds().values(), QStringList{u"com.ktechpit.glate"_s});

        // Stopped, then started again on the same connection (switched off and on).
        portal.stop();
        portal.start();
        QTRY_COMPARE(portal.state(), PortalShortcuts::State::Active);
        QCOMPARE(fake->refusedRegistrations(), 0);
    }

    void noKeyUntilTheUserChoosesOne()
    {
        auto fake = makePortal();
        if (!fake) {
            QSKIP("no private D-Bus daemon for the fake portal");
        }
        fake->setTrigger({});
        PortalShortcuts portal(u"com.ktechpit.glate"_s, u"quick-translate"_s, u"Quick Translate"_s);
        portal.start();
        QTRY_COMPARE(portal.state(), PortalShortcuts::State::NoKey);
        const QSignalSpy changed(&portal, &PortalShortcuts::stateChanged);
        fake->changeTrigger(u"Meta+T"_s);
        QTRY_COMPARE(portal.state(), PortalShortcuts::State::Active);
        QCOMPARE(portal.trigger(), u"Meta+T"_s);
    }

private:
    [[nodiscard]] std::unique_ptr<glate::tests::FakePortal> makePortal() const
    {
        if (m_bus->address().isEmpty()) {
            return {};
        }
        auto portal = std::make_unique<glate::tests::FakePortal>(m_bus->address());
        return portal->isRegistered() ? std::move(portal) : nullptr;
    }

    std::unique_ptr<glate::tests::PrivateBus> m_bus;
#endif
};

QTEST_MAIN(TestGlobalShortcut)
#include "tst_global_shortcut.moc"
