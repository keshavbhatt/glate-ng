#pragma once

#include "Diagnostics.h"

#include <QDialog>
#include <QString>
#include <QStringList>
#include <QUrl>

class QBoxLayout;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QToolButton;

namespace glate::shell {

// Report a bug: the user writes a title and what happened; Glate opens a
// GitHub issue or an email with them filled in and puts the details (system
// information and, if ticked, recent log lines) on the clipboard. The app
// itself sends nothing.
class BugReportDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(BugReportDialog)

public:
    struct Targets
    {
        QUrl newIssueUrl;
        QString mailAddress;
    };

    BugReportDialog(QString diagnostics, QStringList logLines, Targets targets, QWidget* parent = nullptr);
    ~BugReportDialog() override = default;

    // Prefills the description, e.g. with an error message.
    void setDescription(const QString& text);

    [[nodiscard]] BugReport report() const;
    // What goes on the clipboard: diagnostics, then the log lines when ticked.
    [[nodiscard]] QString details() const;

private:
    void buildForm(QBoxLayout* body);
    void buildFooter(QBoxLayout* footer);
    void copyDetails() const;
    void openIssue();
    void openMail();
    void refreshDetails();
    void restyleDisclosure();

    QString m_diagnostics;
    QStringList m_logLines;
    Targets m_targets;
    QLineEdit* m_title = nullptr;
    QPlainTextEdit* m_description = nullptr;
    QCheckBox* m_includeLog = nullptr;
    QToolButton* m_disclosure = nullptr;
    QPlainTextEdit* m_detailsView = nullptr;
};

} // namespace glate::shell
