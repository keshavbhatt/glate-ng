#include "aboutdialog.h"

#include <QApplication>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QFont>
#include <QPushButton>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {
QString buildAboutText() {
  const QString appVersion = QApplication::applicationVersion().isEmpty()
                                 ? QStringLiteral("dev")
                                 : QApplication::applicationVersion();
  const QString platform = QApplication::platformName();
  const bool isWayland = platform.contains(QStringLiteral("wayland"), Qt::CaseInsensitive);

  QString text;
  text += QStringLiteral("<h3>Glate</h3>");
  text += QStringLiteral("<p>Designed and Developed</p>");
  text += QStringLiteral("<p>by <b>Keshav Bhatt</b> &lt;keshavnrj@gmail.com&gt;</p>");
  text += QStringLiteral("<p>Website: https://ktechpit.com</p>");
  text += QStringLiteral("<p><b>Version:</b> %1<br>").arg(appVersion);
  text += QStringLiteral("<b>Qt:</b> %1<br>").arg(QString::fromLatin1(qVersion()));
  text += QStringLiteral("<b>Platform:</b> %1</p>").arg(platform);
  if (isWayland) {
    text += QStringLiteral("<p>Global shortcuts are limited on native Wayland sessions.</p>");
  }
  return text;
}

QPushButton *linkButton(const QString &text, const QIcon &icon, const QUrl &url, QWidget *parent) {
  auto *button = new QPushButton(text, parent);
  button->setIcon(icon);
  button->setCursor(Qt::PointingHandCursor);
  QObject::connect(button, &QPushButton::clicked, parent, [url]() {
    QDesktopServices::openUrl(url);
  });
  return button;
}
} // namespace

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent) {
  setWindowTitle(tr("About Glate"));
  setWindowFlag(Qt::WindowContextHelpButtonHint, false);
  setModal(false);

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(16, 16, 16, 16);
  mainLayout->setSpacing(12);

  auto *headerLayout = new QHBoxLayout;
  headerLayout->setSpacing(12);

  auto *iconLabel = new QLabel(this);
  iconLabel->setPixmap(QIcon(QStringLiteral(":/icons/app/icon-64.png")).pixmap(48, 48));
  iconLabel->setFixedSize(48, 48);

  auto *titleLayout = new QVBoxLayout;
  titleLayout->setSpacing(2);
  auto *titleLabel = new QLabel(tr("Glate"), this);
  QFont titleFont = titleLabel->font();
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  auto *subtitleLabel = new QLabel(tr("About and project links"), this);
  subtitleLabel->setProperty("class", "muted");
  titleLayout->addWidget(titleLabel);
  titleLayout->addWidget(subtitleLabel);
  titleLayout->addStretch();

  headerLayout->addWidget(iconLabel);
  headerLayout->addLayout(titleLayout, 1);
  mainLayout->addLayout(headerLayout);

  auto *aboutBrowser = new QTextBrowser(this);
  aboutBrowser->setHtml(buildAboutText());
  aboutBrowser->setOpenExternalLinks(false);
  aboutBrowser->setReadOnly(true);
  aboutBrowser->setMinimumHeight(180);
  connect(aboutBrowser, &QTextBrowser::anchorClicked, this, [](const QUrl &url) {
    QDesktopServices::openUrl(url);
  });
  mainLayout->addWidget(aboutBrowser, 1);

  auto *linksLayout = new QHBoxLayout;
  linksLayout->setSpacing(8);
  linksLayout->addWidget(linkButton(tr("Donate"), QIcon(QStringLiteral(":/icons/paypal-line.png")),
                                    QUrl(QStringLiteral("https://paypal.me/keshavnrj/9")), this));
  linksLayout->addWidget(linkButton(tr("GitHub"), QIcon(QStringLiteral(":/icons/links-line.png")),
                                    QUrl(QStringLiteral("https://github.com/keshavbhatt/glate")), this));
  linksLayout->addWidget(linkButton(tr("Rate"), QIcon(QStringLiteral(":/icons/star-line.png")),
                                    QUrl(QStringLiteral("snap://glate")), this));
  linksLayout->addStretch();
  mainLayout->addLayout(linksLayout);

  auto *buttonRow = new QHBoxLayout;
  buttonRow->addStretch();
  auto *closeButton = new QPushButton(tr("Close"), this);
  connect(closeButton, &QPushButton::clicked, this, &QDialog::close);
  buttonRow->addWidget(closeButton);
  mainLayout->addLayout(buttonRow);

  resize(460, 360);
}
