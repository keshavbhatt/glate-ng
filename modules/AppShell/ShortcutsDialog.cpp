#include "ShortcutsDialog.h"

#include "ShellStyle.h"

#include <QBoxLayout>
#include <QGridLayout>
#include <QKeySequence>
#include <QLabel>

using namespace Qt::StringLiterals;

namespace glate::shell {

ShortcutsDialog::ShortcutsDialog(const QList<Shortcut>& shortcuts, QWidget* parent)
    : QDialog(parent)
{
    setObjectName(u"shortcutsDialog"_s);
    QBoxLayout* body = setUpDialog(this, tr("Keyboard shortcuts"));
    auto* grid = new QGridLayout;
    grid->setHorizontalSpacing(24);
    grid->setVerticalSpacing(10);
    int row = 0;
    for (const Shortcut& shortcut : shortcuts) {
        const QString keys = QKeySequence(shortcut.keys).toString(QKeySequence::NativeText);
        auto* keyLabel = new QLabel(u"<b>%1</b>"_s.arg(keys.toHtmlEscaped()), this);
        auto* action = new QLabel(shortcut.action, this);
        grid->addWidget(action, row, 0);
        grid->addWidget(keyLabel, row, 1, Qt::AlignRight);
        ++row;
    }
    grid->setColumnStretch(0, 1);
    body->addLayout(grid);
    body->addStretch(1);
    QBoxLayout* footer = addFooter(this);
    footer->addStretch(1);
    addCloseButton(this, footer);
    resize(420, 360);
}

} // namespace glate::shell
