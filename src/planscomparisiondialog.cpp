// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#include "planscomparisiondialog.h"
#include "ui_planscomparisiondialog.h"

#include <QCheckBox>
#include <QFont>
#include <QLabel>

namespace {
struct FeatureRow {
  const char *name;
  bool freeTier;
  bool evaluationTier;
  bool proTier;
};

QLabel *makeHeader(const QString &text) {
  auto *label = new QLabel(text);
  QFont f = label->font();
  f.setBold(true);
  label->setFont(f);
  label->setAlignment(Qt::AlignCenter);
  return label;
}

QCheckBox *makeAvailabilityCell(bool enabled) {
  auto *box = new QCheckBox;
  box->setChecked(enabled);
  box->setEnabled(false);
  box->setFocusPolicy(Qt::NoFocus);
  return box;
}
} // namespace

PlansComparisionDialog::PlansComparisionDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::PlansComparisionDialog) {
  ui->setupUi(this);

  setWindowTitle(QStringLiteral("Plan Comparison"));
  setModal(true);
  resize(700, 420);

  auto *grid = ui->gridLayout;
  grid->setContentsMargins(8, 8, 8, 8);
  grid->setHorizontalSpacing(16);
  grid->setVerticalSpacing(10);

  grid->addWidget(makeHeader(QStringLiteral("Feature")), 0, 0);
  grid->addWidget(makeHeader(QStringLiteral("Free")), 0, 1);
  grid->addWidget(makeHeader(QStringLiteral("Evaluation")), 0, 2);
  grid->addWidget(makeHeader(QStringLiteral("Pro")), 0, 3);

  static const FeatureRow kRows[] = {
      {"Translate text", true, true, true},
      {"Translation History", true, true, true},
      {"Unlimited TTS voice output", false, true, true},
      {"Export/share advanced options", false, true, true},
      {"Priority support", false, false, true},
  };

  for (int i = 0; i < static_cast<int>(sizeof(kRows) / sizeof(kRows[0])); ++i) {
    const int row = i + 1;
    auto *featureLabel = new QLabel(QString::fromLatin1(kRows[i].name));
    featureLabel->setWordWrap(true);

    grid->addWidget(featureLabel, row, 0);
    grid->addWidget(makeAvailabilityCell(kRows[i].freeTier), row, 1,
                    Qt::AlignCenter);
    grid->addWidget(makeAvailabilityCell(kRows[i].evaluationTier), row, 2,
                    Qt::AlignCenter);
    grid->addWidget(makeAvailabilityCell(kRows[i].proTier), row, 3,
                    Qt::AlignCenter);
  }

  grid->setColumnStretch(0, 3);
  grid->setColumnStretch(1, 1);
  grid->setColumnStretch(2, 1);
  grid->setColumnStretch(3, 1);
}

PlansComparisionDialog::~PlansComparisionDialog() { delete ui; }
