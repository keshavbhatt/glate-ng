#include "FlowLayout.h"

#include <QWidget>

#include <algorithm>

namespace glate::shell {

FlowLayout::FlowLayout(QWidget* parent, int spacing)
    : QLayout(parent)
    , m_gap(spacing)
{
    setContentsMargins(0, 0, 0, 0);
}

FlowLayout::~FlowLayout()
{
    qDeleteAll(m_items);
}

void FlowLayout::addItem(QLayoutItem* item)
{
    m_items.append(item);
}

int FlowLayout::count() const
{
    return static_cast<int>(m_items.size());
}

QLayoutItem* FlowLayout::itemAt(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items.at(index) : nullptr;
}

QLayoutItem* FlowLayout::takeAt(int index)
{
    return index >= 0 && index < m_items.size() ? m_items.takeAt(index) : nullptr;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
    return {};
}

bool FlowLayout::hasHeightForWidth() const
{
    return true;
}

int FlowLayout::heightForWidth(int width) const
{
    return arrange(QRect(0, 0, width, 0), false);
}

void FlowLayout::setGeometry(const QRect& rect)
{
    QLayout::setGeometry(rect);
    static_cast<void>(arrange(rect, true));
}

QSize FlowLayout::sizeHint() const
{
    return minimumSize();
}

QSize FlowLayout::minimumSize() const
{
    QSize size;
    for (const QLayoutItem* item : m_items) {
        size = size.expandedTo(item->minimumSize());
    }
    return size;
}

int FlowLayout::arrange(const QRect& rect, bool apply) const
{
    int x = rect.x();
    int y = rect.y();
    int rowHeight = 0;
    for (QLayoutItem* item : m_items) {
        if (item->widget() != nullptr && item->widget()->isHidden()) {
            continue;
        }
        const QSize hint = item->sizeHint();
        if (x > rect.x() && x + hint.width() > rect.right() + 1) {
            x = rect.x();
            y += rowHeight + m_gap;
            rowHeight = 0;
        }
        if (apply) {
            item->setGeometry(QRect(QPoint(x, y), hint));
        }
        x += hint.width() + m_gap;
        rowHeight = std::max(rowHeight, hint.height());
    }
    return y + rowHeight - rect.y();
}

} // namespace glate::shell
