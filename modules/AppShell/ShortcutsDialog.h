#pragma once

#include <QDialog>
#include <QList>
#include <QString>

namespace glate::shell {

// A read-only list of keyboard shortcuts.
class ShortcutsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ShortcutsDialog)

public:
    struct Shortcut
    {
        QString keys; // as QKeySequence portable text, e.g. "Shift+Return"
        QString action;
    };

    explicit ShortcutsDialog(const QList<Shortcut>& shortcuts, QWidget* parent = nullptr);
    ~ShortcutsDialog() override = default;
};

} // namespace glate::shell
