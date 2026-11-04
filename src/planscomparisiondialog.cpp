// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "planscomparisiondialog.h"

#include "licenseservice.h"

#include <ShellIcons.h>
#include <ShellStyle.h>

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

namespace icons = glate::shell::icons;

constexpr int kColumns = 3; // Free, Evaluation, Pro
constexpr int kGlyph = 18;

// What one plan gives for a feature: a tick, a dash or a short text.
struct Cell {
  bool yes = false;
  QString text;
};

struct Feature {
  QString name;
  QString detail;
  Cell cells[kColumns];
};

QString rgba(QColor color, int alpha) {
  color.setAlpha(alpha);
  return QStringLiteral("rgba(%1, %2, %3, %4)")
      .arg(color.red())
      .arg(color.green())
      .arg(color.blue())
      .arg(alpha);
}

} // namespace

PlansComparisionDialog::PlansComparisionDialog(Plan plan, int evaluationDaysLeft,
                                               QWidget *parent)
    : QDialog(parent) {
  setModal(true);
  QBoxLayout *body = glate::shell::setUpDialog(this, tr("Glate | Plans"));
  body->setSpacing(10);

  auto *title = new QLabel(tr("Compare plans"), this);
  glate::shell::setRelativeFont(title, 1.15, true);
  auto *intro = new QLabel(tr("Glate stays free for online translation. Pro adds "
                              "offline translation and the extras below, with "
                              "one purchase."),
                           this);
  intro->setWordWrap(true);
  glate::shell::setRelativeFont(intro, 0.92);
  glate::shell::makeMuted(intro);
  body->addWidget(title);
  body->addWidget(intro);

  const int mine = static_cast<int>(plan);
  const bool dark = palette().color(QPalette::Window).lightness() < 128;
  const QColor accent = icons::color(icons::Tone::Accent, this);
  const QColor muted = glate::shell::mutedColor(this);

  auto *table = new QWidget(this);
  table->setObjectName(QStringLiteral("plansTable"));
  table->setStyleSheet(
      QStringLiteral(
          "#plansTable { background: transparent; }"
          "#planCell { background: transparent; border-top: 1px solid %1; }"
          "#planCell[head=\"true\"] { border-top: none; }"
          "#planCell[mine=\"true\"] { background: %2; }"
          "#planCell[mine=\"true\"][head=\"true\"] {"
          " border-top-left-radius: 10px; border-top-right-radius: 10px; }"
          "#planCell[mine=\"true\"][last=\"true\"] {"
          " border-bottom-left-radius: 10px; border-bottom-right-radius: 10px; }"
          "#planCell QLabel { background: transparent; }"
          "#planCell QLabel#planBadge { background: %3; color: %4; border-radius: 6px;"
          " padding: 1px 7px; }")
          .arg(glate::shell::borderColor().name(),
               rgba(palette().color(QPalette::Highlight), dark ? 55 : 32),
               palette().color(QPalette::Highlight).name(),
               palette().color(QPalette::HighlightedText).name()));
  auto *grid = new QGridLayout(table);
  grid->setContentsMargins(0, 0, 0, 0);
  grid->setHorizontalSpacing(0);
  grid->setVerticalSpacing(0);
  grid->setColumnStretch(0, 16);
  // Wide enough that the feature names stay on one line.
  grid->setColumnMinimumWidth(0, 250);
  for (int c = 1; c <= kColumns; ++c)
    grid->setColumnStretch(c, 10);

  const auto cell = [table](int column, bool head, bool mineColumn, bool last) {
    auto *widget = new QWidget(table);
    widget->setObjectName(QStringLiteral("planCell"));
    widget->setAttribute(Qt::WA_StyledBackground);
    widget->setProperty("head", head);
    widget->setProperty("mine", mineColumn);
    widget->setProperty("last", last);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(10, head ? 12 : 9, 10, head ? 10 : 9);
    layout->setSpacing(2);
    layout->setAlignment(column == 0 ? Qt::AlignLeft | Qt::AlignVCenter
                                     : Qt::AlignCenter);
    return widget;
  };

  // Header: each plan, how long it lasts, the user's plan badged.
  const QString names[kColumns] = {tr("Free"), tr("Evaluation"), tr("Pro")};
  const QString lasts[kColumns] = {
      tr("Always"),
      evaluationDaysLeft >= 0 && plan == Plan::Evaluation
          ? tr("15 days, %n left", nullptr, evaluationDaysLeft)
          : tr("15 days"),
      tr("One-time purchase,\nlifetime updates")};
  grid->addWidget(cell(0, true, false, false), 0, 0);
  for (int c = 0; c < kColumns; ++c) {
    QWidget *head = cell(c + 1, true, c == mine, false);
    if (c == mine) {
      auto *badge = new QLabel(tr("YOUR PLAN"), head);
      badge->setObjectName(QStringLiteral("planBadge"));
      glate::shell::setRelativeFont(badge, 0.75, true);
      head->layout()->addWidget(badge);
      head->layout()->setAlignment(badge, Qt::AlignHCenter);
    }
    auto *name = new QLabel(names[c], head);
    glate::shell::setRelativeFont(name, 1.05, true);
    name->setAlignment(Qt::AlignCenter);
    auto *span = new QLabel(lasts[c], head);
    span->setAlignment(Qt::AlignCenter);
    glate::shell::setRelativeFont(span, 0.88);
    glate::shell::makeMuted(span);
    head->layout()->addWidget(name);
    head->layout()->addWidget(span);
    grid->addWidget(head, 0, c + 1);
  }

  // What the app unlocks today (LicenseService and its callers).
  const Cell yes{true, {}};
  const Cell no{false, {}};
  const QString unlimited = tr("Unlimited");
  const QList<Feature> features = {
      {tr("Online translation"), tr("Google Translate, 100+ languages"),
       {yes, yes, yes}},
      {tr("Quick Translate from any app"), {}, {yes, yes, yes}},
      {tr("History and line by line"), {}, {yes, yes, yes}},
      {tr("Share by email, save as text"), {}, {yes, yes, yes}},
      {tr("Read aloud"),
       {},
       {{false, tr("%1 a day").arg(LicenseService::kFreeTierDailyTtsLimit)},
        {false, unlimited},
        {false, unlimited}}},
      {tr("Offline translation"),
       tr("On this computer, also when Google limits requests"),
       {no, yes, yes}},
      {tr("Export as audio (MP3)"), {}, {no, yes, yes}},
      {tr("Share links"), {}, {no, yes, yes}},
      {tr("Future Pro features"), {}, {no, yes, yes}},
  };

  for (int r = 0; r < features.size(); ++r) {
    const Feature &feature = features.at(r);
    const int row = r + 1;
    const bool last = r == features.size() - 1;
    QWidget *label = cell(0, false, false, last);
    auto *name = new QLabel(feature.name, label);
    label->layout()->addWidget(name);
    if (!feature.detail.isEmpty()) {
      auto *detail = new QLabel(feature.detail, label);
      glate::shell::setRelativeFont(detail, 0.88);
      glate::shell::makeMuted(detail);
      label->layout()->addWidget(detail);
    }
    grid->addWidget(label, row, 0);

    for (int c = 0; c < kColumns; ++c) {
      const Cell &value = feature.cells[c];
      QWidget *box = cell(c + 1, false, c == mine, last);
      auto *content = new QLabel(box);
      content->setAlignment(Qt::AlignCenter);
      if (!value.text.isEmpty()) {
        content->setText(value.text);
        if (c == 0)
          glate::shell::makeMuted(content);
      } else {
        QColor faded = muted;
        faded.setAlphaF(0.6F);
        content->setPixmap(icons::pixmap(value.yes ? QStringLiteral("check")
                                                   : QStringLiteral("minus"),
                                         value.yes ? accent : faded, kGlyph,
                                         devicePixelRatioF()));
        content->setAccessibleName(value.yes ? tr("Included")
                                             : tr("Not included"));
      }
      box->layout()->addWidget(content);
      grid->addWidget(box, row, c + 1);
    }
  }
  body->addWidget(table);

  QBoxLayout *footer = glate::shell::addFooter(this);
  footer->addStretch(1);
  QPushButton *close = glate::shell::addCloseButton(this, footer);
  if (plan != Plan::Pro) {
    auto *getPro = new QPushButton(tr("Get Pro"), this);
    getPro->setProperty("accent", true);
    getPro->setDefault(true);
    icons::bind(getPro, QStringLiteral("star"), icons::Tone::OnAccent);
    connect(getPro, &QPushButton::clicked, this, &QDialog::accept);
    footer->addWidget(getPro);
    close->setDefault(false);
    close->setProperty("accent", false);
  }
  glate::shell::fitToContent(this, 640);
}
