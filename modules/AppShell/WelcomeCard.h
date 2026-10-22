#pragma once

#include <QList>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;

namespace glate::shell {

// The first-run card shown in place inside an empty pane: a title, a line of
// text, a few tips with icons and one button. Never a popup (omnidict lesson).
class WelcomeCard : public QWidget
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(WelcomeCard)

public:
    struct Tip
    {
        QString iconPath;
        QString text; // rich text allowed (<kbd> is drawn as bold)
    };

    WelcomeCard(const QString& title, const QString& subtitle, const QList<Tip>& tips,
                const QString& buttonText, QWidget* parent = nullptr);
    ~WelcomeCard() override = default;

Q_SIGNALS:
    void buttonClicked();

protected:
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void retint();

    QList<Tip> m_tips;
    QWidget* m_inner = nullptr;
    QList<QLabel*> m_tipIcons;
    QPushButton* m_button = nullptr;
};

} // namespace glate::shell
