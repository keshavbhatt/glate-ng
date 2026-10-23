#include "WelcomeCard.h"

#include "ShellStyle.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kTipIconSize = 16;
constexpr int kMaxWidth = 360;
} // namespace

WelcomeCard::WelcomeCard(const QString& title, const QString& subtitle, const QList<Tip>& tips,
                         const QString& buttonText, const QString& buttonIconPath, QWidget* parent)
    : QWidget(parent)
    , m_tips(tips)
    , m_inner(new QWidget(this))
    , m_button(new QPushButton(buttonText, m_inner))
{
    setObjectName(u"welcomeCard"_s);
    // The column must not take a background from the app style sheet.
    m_inner->setObjectName(u"welcomeColumn"_s);
    m_inner->setStyleSheet(u"#welcomeColumn { background: transparent; }"_s);
    auto* rows = new QVBoxLayout(this);
    auto* centre = new QHBoxLayout;
    centre->addStretch(1);
    centre->addWidget(m_inner);
    centre->addStretch(1);
    rows->addStretch(1);
    rows->addLayout(centre);
    rows->addStretch(1);

    auto* column = new QVBoxLayout(m_inner);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(10);
    auto* heading = new QLabel(title, m_inner);
    setRelativeFont(heading, 1.3, true);
    auto* sub = new QLabel(subtitle, m_inner);
    sub->setWordWrap(true);
    setRelativeFont(sub, 0.9);
    makeMuted(sub);
    column->addWidget(heading);
    column->addSpacing(-6);
    column->addWidget(sub);
    column->addSpacing(6);

    for (qsizetype i = 0; i < tips.size(); ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        auto* icon = new QLabel(m_inner);
        icon->setFixedSize(kTipIconSize, kTipIconSize);
        auto* text = new QLabel(m_inner);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        row->addWidget(icon, 0, Qt::AlignTop);
        row->addWidget(text, 1);
        column->addLayout(row);
        m_tipIcons.append(icon);
        m_tipTexts.append(text);
    }

    m_button->setObjectName(u"welcomeButton"_s);
    if (!buttonIconPath.isEmpty()) {
        m_button->setIcon(QIcon(buttonIconPath));
        m_button->setIconSize(QSize(kTipIconSize, kTipIconSize));
    }
    connect(m_button, &QPushButton::clicked, this, &WelcomeCard::buttonClicked);
    column->addSpacing(6);
    column->addWidget(m_button, 0, Qt::AlignLeft);
    m_inner->setFixedWidth(kMaxWidth);
    retint();
}

void WelcomeCard::resizeEvent(QResizeEvent* event)
{
    constexpr int kSideRoom = 48;
    m_inner->setFixedWidth(std::clamp(width() - kSideRoom, 120, kMaxWidth));
    QWidget::resizeEvent(event);
}

void WelcomeCard::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::PaletteChange) {
        retint();
    }
    QWidget::changeEvent(event);
}

void WelcomeCard::retint()
{
    const QColor accent = palette().color(QPalette::Highlight);
    // Rich text has no borders, so a key cap is a filled chip in the border colour.
    const QString keyCap = u"<span style=\"background-color:%1; font-weight:600; font-size:small;\">"
                           "&nbsp;\\1&nbsp;</span>"_s.arg(borderColor().name());
    static const QRegularExpression kbd(u"<kbd>(.*?)</kbd>"_s);
    for (qsizetype i = 0; i < m_tipIcons.size() && i < m_tips.size(); ++i) {
        m_tipIcons.at(i)->setPixmap(
            tintedIcon(m_tips.at(i).iconPath, accent).pixmap(kTipIconSize, kTipIconSize));
        m_tipTexts.at(i)->setText(QString(m_tips.at(i).text).replace(kbd, keyCap));
    }
}

} // namespace glate::shell
