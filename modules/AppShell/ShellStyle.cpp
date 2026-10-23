#include "ShellStyle.h"

#include <QApplication>
#include <QBoxLayout>
#include <QDialog>
#include <QEvent>
#include <QFrame>
#include <QLabel>
#include <QLayout>
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

namespace {

// The text colour share of the way from the window colour, from the application
// palette: a muted widget's own palette already holds the muted colour.
QColor textTowardWindow(float share)
{
    const QPalette palette = QApplication::palette();
    const QColor text = palette.color(QPalette::Active, QPalette::WindowText);
    const QColor window = palette.color(QPalette::Active, QPalette::Window);
    const auto mix = [share](float from, float to) { return (from * share) + (to * (1.0F - share)); };
    return QColor::fromRgbF(mix(text.redF(), window.redF()), mix(text.greenF(), window.greenF()),
                            mix(text.blueF(), window.blueF()));
}

} // namespace

QColor mutedColor(const QWidget* /*widget*/)
{
    constexpr float kMuted = 0.6F;
    return textTowardWindow(kMuted);
}

QColor borderColor()
{
    constexpr float kBorder = 0.12F;
    return textTowardWindow(kBorder);
}

namespace {

// The app style sheet sets every widget's text colour, which wins over a palette;
// a widget style sheet wins over the app one. Kept in step with theme changes.
class MutedLabel : public QObject
{
public:
    explicit MutedLabel(QLabel* label)
        : QObject(label)
        , m_label(label)
    {
        label->installEventFilter(this);
        apply();
    }

    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == m_label &&
            (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange ||
             event->type() == QEvent::StyleChange)) {
            apply();
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void apply()
    {
        const QString sheet = u"QLabel { color: %1; }"_s.arg(mutedColor(m_label).name());
        if (m_label->styleSheet() != sheet) {
            m_label->setStyleSheet(sheet);
        }
    }

    QLabel* m_label;
};

} // namespace

void makeMuted(QLabel* label)
{
    if (label->findChild<QObject*>(u"mutedLabel"_s, Qt::FindDirectChildrenOnly) != nullptr) {
        return;
    }
    auto* muted = new MutedLabel(label);
    muted->setObjectName(u"mutedLabel"_s);
}

void setRelativeFont(QWidget* widget, double scale, bool bold)
{
    QFont font = widget->font();
    font.setPointSizeF(font.pointSizeF() * scale);
    font.setBold(bold);
    widget->setFont(font);
}

void fitToContent(QDialog* dialog, int width)
{
    dialog->setMinimumWidth(width);
    dialog->layout()->activate();
    const int height = dialog->layout()->hasHeightForWidth() ? dialog->layout()->totalHeightForWidth(width)
                                                             : dialog->sizeHint().height();
    dialog->resize(width, height);
}

QColor dangerColor(const QWidget* widget)
{
    const QPalette palette = widget != nullptr ? widget->palette() : QApplication::palette();
    const bool dark = palette.color(QPalette::Window).lightness() < 128;
    return dark ? QColor(0xff, 0x6b, 0x6b) : QColor(0xc0, 0x1c, 0x28);
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
