// SPDX-FileCopyrightText: 2026 Keshav Bhatt (Ktechpit)
// SPDX-License-Identifier: LicenseRef-Ktechpit-Licensing-Module

#ifndef PLANSCOMPARISIONDIALOG_H
#define PLANSCOMPARISIONDIALOG_H

#include <QDialog>

namespace Ui {
class PlansComparisionDialog;
}

class PlansComparisionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlansComparisionDialog(QWidget *parent = nullptr);
    ~PlansComparisionDialog();

private:
    Ui::PlansComparisionDialog *ui;
};

#endif // PLANSCOMPARISIONDIALOG_H
