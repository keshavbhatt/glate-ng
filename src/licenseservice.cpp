#include "licenseservice.h"

#include <QApplication>
#include <QMessageBox>
#include <QPushButton>

LicenseService *LicenseService::s_instance = nullptr;

LicenseService::LicenseService(QObject *parent)
    : QObject(parent)
{
}

LicenseService *LicenseService::instance()
{
    if (!s_instance)
        s_instance = new LicenseService(qApp);
    return s_instance;
}

void LicenseService::setHasPaidAccess(bool hasPaidAccess)
{
    m_hasPaidAccess = hasPaidAccess;
}

bool LicenseService::hasPaidAccess() const
{
    return m_hasPaidAccess;
}

bool LicenseService::checkAccess(const QString &featureName)
{
    if (m_hasPaidAccess)
        return true;

    // Use the currently active window as the parent so the dialog
    // appears centred over whatever is on screen at call time.
    QWidget *parent = QApplication::activeWindow();

    QMessageBox msgBox(parent);
    msgBox.setWindowTitle(QStringLiteral("Pro Feature"));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setText(
        QStringLiteral("<b>%1</b> is a Pro feature.").arg(featureName));
    msgBox.setInformativeText(
        QStringLiteral("Upgrade to a Pro licence to unlock this feature "
                       "and support continued development of Glate."));

    QPushButton *upgradeBtn =
        msgBox.addButton(QStringLiteral("View Plans"), QMessageBox::AcceptRole);
    msgBox.addButton(QStringLiteral("Not Now"), QMessageBox::RejectRole);
    msgBox.setDefaultButton(upgradeBtn);
    msgBox.exec();

    if (msgBox.clickedButton() == upgradeBtn)
        emit upgradeRequested();

    return false;
}

