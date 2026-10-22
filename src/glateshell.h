#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

#include <functional>

class QDialog;
class QMenu;
class QWidget;

// Glate's side of the app shell (modules/AppShell): what About shows, the
// release notes, bug reports and the Help menu. MainWindow owns one.
class GlateShell : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GlateShell)

public:
    // appLines: extra diagnostics lines (offline engine state, theme), asked
    // for each time diagnostics are shown, so they are current.
    GlateShell(QWidget* window, std::function<QStringList()> appLines);
    ~GlateShell() override = default;

    [[nodiscard]] static QString logDirectory();
    [[nodiscard]] static QString logFilePath();
    [[nodiscard]] QString diagnostics() const;

    void showAbout();
    void showWhatsNew();
    void showBugReport(const QString& description = {});
    void showShortcuts();
    // Opens What's new once after an update (never on a first install).
    void showWhatsNewIfUpdated();

    // The Help button's menu; tour opens the welcome slides.
    [[nodiscard]] QMenu* createHelpMenu(QWidget* parent, const std::function<void()>& tour);

    // The open dialogs, for GLATE_DEBUG_GRAB.
    [[nodiscard]] QDialog* aboutDialog() const { return m_about; }
    [[nodiscard]] QDialog* whatsNewDialog() const { return m_whatsNew; }
    [[nodiscard]] QDialog* bugReportDialog() const { return m_bugReport; }
    [[nodiscard]] QDialog* shortcutsDialog() const { return m_shortcuts; }

private:
    void present(QDialog* dialog);

    QWidget* m_window;
    std::function<QStringList()> m_appLines;
    QPointer<QDialog> m_about;
    QPointer<QDialog> m_whatsNew;
    QPointer<QDialog> m_bugReport;
    QPointer<QDialog> m_shortcuts;
};
