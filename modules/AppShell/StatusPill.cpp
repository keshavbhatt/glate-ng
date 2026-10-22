#include "StatusPill.h"

#include "ShellStyle.h"

#include <QPainter>
#include <QPainterPath>

#include <utility>

namespace glate::shell {

namespace {
constexpr int kHeight = 26;
constexpr int kIconSize = 14;
constexpr int kPadLeft = 9;
constexpr int kPadRight = 11;
constexpr int kGap = 6;
} // namespace

StatusPill::StatusPill(const QString& text, QString iconPath, QWidget* parent)
    : QAbstractButton(parent)
    , m_iconPath(std::move(iconPath))
{
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    QFont bold = font();
    bold.setBold(true);
    bold.setPointSizeF(bold.pointSizeF() * 0.92);
    setFont(bold);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize StatusPill::sizeHint() const
{
    const int textWidth = fontMetrics().horizontalAdvance(text());
    return {kPadLeft + kIconSize + kGap + textWidth + kPadRight, kHeight};
}

void StatusPill::paintEvent(QPaintEvent* /*event*/)
{
    const QColor green = offlineColor(this);
    QColor fill = green;
    fill.setAlphaF(underMouse() ? 0.24F : 0.15F);
    QColor border = green;
    border.setAlphaF(0.45F);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF box = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath path;
    path.addRoundedRect(box, box.height() / 2, box.height() / 2);
    painter.fillPath(path, fill);
    painter.setPen(QPen(hasFocus() ? palette().color(QPalette::Highlight) : border, 1));
    painter.drawPath(path);

    const int iconY = (height() - kIconSize) / 2;
    tintedIcon(m_iconPath, green).paint(&painter, QRect(kPadLeft, iconY, kIconSize, kIconSize));
    painter.setPen(green);
    const QRect textRect(kPadLeft + kIconSize + kGap, 0, width() - kPadLeft - kIconSize - kGap, height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, text());
}

} // namespace glate::shell
