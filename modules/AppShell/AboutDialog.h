#pragma once

#include <QDialog>
#include <QIcon>
#include <QList>
#include <QString>
#include <QUrl>

class QPlainTextEdit;

namespace glate::shell {

// What the About window shows. Everything comes from the app, so the dialog
// itself knows nothing about Glate.
struct AboutInfo
{
    struct Link
    {
        QString text;
        QString iconPath;
        QUrl url; // empty: emits bugReportRequested() instead of opening a URL
    };
    struct Row
    {
        QString name;
        QString text;
    };

    QString appName;
    QString tagline;
    QString version;
    QString author;
    QIcon icon;
    QList<Link> links;
    QList<Row> credits;
    QList<Row> openSource;
    QString diagnostics;
    QString logDirectory; // "Open log folder"; hidden when empty
};

class AboutDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(AboutDialog)

public:
    explicit AboutDialog(const AboutInfo& info, QWidget* parent = nullptr);
    ~AboutDialog() override = default;

    // Updates the Diagnostics tab (offline state can change while it is open).
    void setDiagnostics(const QString& text);

Q_SIGNALS:
    void whatsNewRequested();
    void bugReportRequested();

private:
    QWidget* buildHeader(const AboutInfo& info);
    QWidget* buildLinks(const AboutInfo& info);
    QWidget* buildDiagnostics(const AboutInfo& info);

    QPlainTextEdit* m_diagnostics = nullptr;
};

// A two-column list of name/text rows in a framed box (Credits, Open source).
[[nodiscard]] QWidget* makeRowList(const QList<AboutInfo::Row>& rows, QWidget* parent);

} // namespace glate::shell
