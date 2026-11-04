// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#ifndef PLANSCOMPARISIONDIALOG_H
#define PLANSCOMPARISIONDIALOG_H

#include <QDialog>

// Free, Evaluation and Pro side by side, the user's plan marked
// (DOCS/mocks/plans.html). Accepted when the user clicks "Get Pro".
class PlansComparisionDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Plan { Free, Evaluation, Pro };

    // evaluationDaysLeft < 0: unknown.
    explicit PlansComparisionDialog(Plan plan, int evaluationDaysLeft = -1,
                                    QWidget *parent = nullptr);
};

#endif // PLANSCOMPARISIONDIALOG_H
