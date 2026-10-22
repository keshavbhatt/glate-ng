#include "ShellStyle.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QFrame>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>

using namespace Qt::StringLiterals;

namespace glate::shell {

QIcon tintedIcon(const QString& resourcePath, const QColor& color, int size)
{
    QPixmap source(resourcePath);
    if (source.isNull()) {
        return {};
    }
    source = source.scaled(size, size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QPixmap tinted(source.size());
    tinted.fill(Qt::transparent);
    QPainter painter(&tinted);
    painter.drawPixmap(0, 0, source);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(tinted.rect(), color);
    painter.end();
    return {tinted};
}

QColor mutedColor(const QWidget* widget)
{
    const QPalette palette = widget != nullptr ? widget->palette() : QApplication::palette();
    return palette.color(QPalette::Disabled, QPalette::Text);
}

void makeMuted(QLabel* label)
{
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, mutedColor(label));
    label->setPalette(palette);
}

QBoxLayout* setUpDialog(QDialog* dialog, const QString& title)
{
    dialog->setWindowTitle(title);
    auto* root = new QVBoxLayout(dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    auto* body = new QVBoxLayout;
    body->setContentsMargins(20, 18, 20, 16);
    body->setSpacing(12);
    root->addLayout(body, 1);
    return body;
}

QBoxLayout* addFooter(QDialog* dialog)
{
    auto* root = qobject_cast<QBoxLayout*>(dialog->layout());
    auto* line = new QFrame(dialog);
    line->setObjectName(u"toolbarSeparator"_s);
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    line->setAttribute(Qt::WA_StyledBackground, true);
    root->addWidget(line);
    auto* footer = new QHBoxLayout;
    footer->setContentsMargins(16, 10, 16, 12);
    footer->setSpacing(8);
    root->addLayout(footer);
    return footer;
}

QPushButton* addCloseButton(QDialog* dialog, QBoxLayout* footer)
{
    auto* close = new QPushButton(QDialog::tr("Close"), dialog);
    close->setObjectName(u"closeButton"_s);
    close->setDefault(true);
    close->setProperty("accent", true);
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::accept);
    footer->addWidget(close);
    return close;
}

QColor offlineColor(const QWidget* widget)
{
    const QPalette palette = widget != nullptr ? widget->palette() : QApplication::palette();
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    return dark ? QColor(0x5c, 0xcb, 0x84) : QColor(0x1b, 0x7f, 0x38);
}

} // namespace glate::shell
