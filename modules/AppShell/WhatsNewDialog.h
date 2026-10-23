#pragma once

#include "Changelog.h"

#include <QDialog>
#include <QList>
#include <QString>

class QComboBox;
class QLabel;
class QTextBrowser;

namespace glate::shell {

// Release notes with a version picker. Opens on the running version, or the
// newest release when the running version has no notes yet.
class WhatsNewDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WhatsNewDialog)

public:
    WhatsNewDialog(const QString& appName, QList<Release> releases, const QString& runningVersion,
                   QWidget* parent = nullptr);
    ~WhatsNewDialog() override = default;

    void showVersion(const QString& version);
    [[nodiscard]] QString shownVersion() const;

private:
    void showIndex(int index);
    void fitNotes();

    QList<Release> m_releases;
    QComboBox* m_picker = nullptr;
    QLabel* m_date = nullptr;
    QTextBrowser* m_notes = nullptr;
};

} // namespace glate::shell
