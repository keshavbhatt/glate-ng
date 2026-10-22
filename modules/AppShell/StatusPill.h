#pragma once

#include <QAbstractButton>
#include <QString>

namespace glate::shell {

// A rounded, clickable status badge ("Offline" in the top bar). Paints itself
// from the palette, so it follows light and dark themes live.
class StatusPill : public QAbstractButton
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(StatusPill)

public:
    StatusPill(const QString& text, QString iconPath, QWidget* parent = nullptr);
    ~StatusPill() override = default;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_iconPath;
};

} // namespace glate::shell
