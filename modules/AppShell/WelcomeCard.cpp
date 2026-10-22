#include "WelcomeCard.h"

#include "ShellStyle.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

using namespace Qt::StringLiterals;

namespace glate::shell {

namespace {
constexpr int kTipIconSize = 16;
constexpr int kMaxWidth = 380;
} // namespace

WelcomeCard::WelcomeCard(const QString& title, const QString& subtitle, const QList<Tip>& tips,
                         const QString& buttonText, QWidget* parent)
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
    QFont big = heading->font();
    big.setPointSizeF(big.pointSizeF() * 1.3);
    big.setBold(true);
    heading->setFont(big);
    auto* sub = new QLabel(subtitle, m_inner);
    sub->setWordWrap(true);
    makeMuted(sub);
    column->addWidget(heading);
    column->addWidget(sub);
    column->addSpacing(4);

    for (const Tip& tip : tips) {
        auto* row = new QHBoxLayout;
        row->setSpacing(10);
        auto* icon = new QLabel(m_inner);
        icon->setFixedSize(kTipIconSize, kTipIconSize);
        auto* text = new QLabel(tip.text, m_inner);
        text->setWordWrap(true);
        text->setTextFormat(Qt::RichText);
        row->addWidget(icon, 0, Qt::AlignTop);
        row->addWidget(text, 1);
        column->addLayout(row);
        m_tipIcons.append(icon);
    }

    m_button->setObjectName(u"welcomeButton"_s);
    connect(m_button, &QPushButton::clicked, this, &WelcomeCard::buttonClicked);
    column->addSpacing(4);
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
    for (qsizetype i = 0; i < m_tipIcons.size() && i < m_tips.size(); ++i) {
        m_tipIcons.at(i)->setPixmap(
            tintedIcon(m_tips.at(i).iconPath, accent).pixmap(kTipIconSize, kTipIconSize));
    }
}

} // namespace glate::shell
