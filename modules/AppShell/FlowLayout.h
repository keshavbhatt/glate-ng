#pragma once

#include <QLayout>
#include <QList>

namespace glate::shell {

// Lays its items out left to right and wraps to a new row when the width runs
// out, like the link buttons in the About mock.
class FlowLayout : public QLayout
{
public:
    explicit FlowLayout(QWidget* parent = nullptr, int spacing = 6);
    ~FlowLayout() override;
    Q_DISABLE_COPY_MOVE(FlowLayout)

    void addItem(QLayoutItem* item) override;
    [[nodiscard]] int count() const override;
    [[nodiscard]] QLayoutItem* itemAt(int index) const override;
    QLayoutItem* takeAt(int index) override;
    [[nodiscard]] Qt::Orientations expandingDirections() const override;
    [[nodiscard]] bool hasHeightForWidth() const override;
    [[nodiscard]] int heightForWidth(int width) const override;
    void setGeometry(const QRect& rect) override;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSize() const override;

private:
    // Places the items when apply is set; returns the height they take.
    [[nodiscard]] int arrange(const QRect& rect, bool apply) const;

    QList<QLayoutItem*> m_items;
    int m_gap;
};

} // namespace glate::shell
