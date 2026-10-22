#include "sharewidget.h"
#include "ui_sharewidget.h"

#include "licenseservice.h"
#include "utils.h"

#include <QFileDialog>
#include <QGraphicsOpacityEffect>
#include <QMessageBox>
#include <QPropertyAnimation>
#include <QShortcut>

Share::Share(QWidget *parent) : QWidget(parent), ui(new Ui::Share) {
  ui->setupUi(this);
  setWindowTitle(tr("Glate | Share"));
  { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }

  if (settings.contains(QStringLiteral("shareDialogGeometry")))
    restoreGeometry(settings.value(QStringLiteral("shareDialogGeometry")).toByteArray());

  // Route all link clicks in the status browser through QDesktopServices
  // so "Open file" and "Show in folder" file:// links actually work.
  connect(ui->status, &QTextBrowser::anchorClicked,
          this, [](const QUrl &url) {
              QDesktopServices::openUrl(url);
          });
  m_networkManager = new QNetworkAccessManager(this);
  m_ttsDownloader = new TtsDownloader(this);

  // Populate voice combo from central voice list
  ui->voiceGender->clear();
  for (const VoiceOption &v : utils::availableVoices())
    ui->voiceGender->addItem(v.displayName);

  ui->voiceGender->setCurrentIndex(
      settings.value("shareAudioVoiceGender", 0).toInt());
  connect(ui->voiceGender, &QComboBox::currentIndexChanged, this,
          [=](int index) {
            settings.setValue("shareAudioVoiceGender", index);
          });

  connect(m_ttsDownloader, &TtsDownloader::progress, this,
          [=](const QString &status) {
            showStatus("<span style='color:green'>Share: </span>" +
                       status.toHtmlEscaped());
          });
  connect(m_ttsDownloader, &TtsDownloader::finished, this,
          [=](const QStringList &audioFiles) {
            m_downloadedAudioFiles = audioFiles;
            ui->download->setEnabled(true);
            if (m_downloadedAudioFiles.isEmpty()) {
              showStatus("<span style='color:red'>Share: </span>No audio chunks downloaded.");
              return;
            }
            showStatus("<span style='color:green'>Share: </span>Voice synthesis complete. Opening audio editor...");

            if (!m_audioExportDialog)
              m_audioExportDialog = new AudioExportDialog(this);

            const QString exportName = getFileNameFromString(ui->translation->toPlainText());
            m_audioExportDialog->setExportName(exportName);
            m_audioExportDialog->setAudioFiles(m_downloadedAudioFiles);
            m_audioExportDialog->exec();
          });
  connect(m_ttsDownloader, &TtsDownloader::failed, this,
          [=](const QString &error) {
            showStatus("<span style='color:red'>Share: </span>" +
                       error.toHtmlEscaped());
            ui->download->setEnabled(true);
          });
}

void Share::setTranslation(const QString &translation, const QString &uuid, const QString &langCode) {
  ui->translation->setPlainText(translation);
  ui->voiceGender->setCurrentIndex(
      settings.value("shareAudioVoiceGender", 0).toInt());
  translationUUID = uuid;
  translationLangCode = langCode;
}

Share::~Share() { delete ui; }

void Share::closeEvent(QCloseEvent *event)
{
  settings.setValue(QStringLiteral("shareDialogGeometry"), saveGeometry());
  QWidget::closeEvent(event);
}

QString Share::getFileNameFromString(const QString &string) {
  QString filename;
  if (string.trimmed().length() > 10) {
    filename = string.trimmed().left(10).trimmed();
  } else {
    filename = utils::generateRandomId(10);
  }
  return filename.remove("/").remove(".");
}

void Share::on_text_clicked() {
  QString translation = ui->translation->toPlainText();
  QString path = settings
                     .value("sharePath", QStandardPaths::writableLocation(
                                             QStandardPaths::DocumentsLocation))
                     .toString();
  QString fileName = QFileDialog::getSaveFileName(
      this, tr("Save Text File"),
      QDir(path).filePath(getFileNameFromString(translation) + ".txt"),
      tr("Text Files (*.txt)"));
  if (!fileName.isEmpty()) {
    QFile file(QFileInfo(fileName).absoluteFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
      showStatus("<span style='color:red'>Share: </span>error failed to save file.");
      return;
    }
    QTextStream out(&file);
    out << translation;
    settings.setValue("sharePath", QFileInfo(file).path());
    file.close();
    showStatusWithFile(
        QStringLiteral("<span style='color:green'>Share: </span>File saved."),
        QFileInfo(fileName).absoluteFilePath());
  } else {
    showStatus("<span style='color:red'>Share: </span>file save operation cancelled.");
  }
}

void Share::on_email_clicked() {
  showStatus("<span style='color:green'>Share: </span>opening mail client...");
  bool opened = QDesktopServices::openUrl(
      QUrl("mailto:?body=" + ui->translation->toPlainText()));
  if (!opened) {
    showStatus("<span style='color:red'>Share: </span>unable to open a mail client...");
  }
}

// ── Licence-gated handlers ────────────────────────────────────────────────────

void Share::on_pastebin_clicked() {
  if (!LicenseService::instance()->checkAccess(
          QStringLiteral("Paste Link (share via paste.rs)")))
    return;

  const QString text = ui->translation->toPlainText();
  if (text.trimmed().isEmpty()) {
    showStatus("<span style='color:red'>Share: </span>No text to share.");
    return;
  }

  ui->pastebin->setDisabled(true);
  showStatus("<span style='color:green'>Share: </span>Uploading paste please wait...");

  QNetworkRequest request(QUrl("https://paste.rs"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  // Keep the request minimal (curl-compatible) and explicitly UTF-8.
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("text/plain; charset=utf-8"));
  request.setRawHeader("Accept", "text/plain");
  request.setTransferTimeout(15000);

  QByteArray payload = text.toUtf8();
  if (!payload.endsWith('\n'))
    payload.append('\n');

  QNetworkReply *reply = m_networkManager->post(request, payload);
  connect(reply, &QNetworkReply::finished, this, [this, reply]() {
    const int httpStatus =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString body = QString::fromUtf8(reply->readAll()).trimmed();
    const bool hasShareUrl = body.startsWith("http://") || body.startsWith("https://");

    if (httpStatus == 201 || httpStatus == 206) {
      if (hasShareUrl) {
        const QString prefix =
            httpStatus == 206
                ? "<span style='color:orange'>Share: </span>Paste uploaded partially (size limit). Link - "
                : "<span style='color:green'>Share: </span>Share link - ";
        showStatus(prefix + "<a target='_blank' href='" + body + "'>" + body + "</a>");
      } else {
        showStatus("<span style='color:red'>Share: </span>Unexpected response for HTTP " +
                   QString::number(httpStatus) + ": " + body.toHtmlEscaped());
      }
    } else {
      const QString statusText =
          httpStatus > 0 ? QString::number(httpStatus) : QString("network");
      const QString err = reply->error() == QNetworkReply::NoError
                              ? body
                              : reply->errorString() +
                                    (body.isEmpty() ? "" : " | " + body);
      showStatus("<span style='color:red'>Share: </span>Upload failed (" +
                 statusText + "): " + err.toHtmlEscaped());
    }
    ui->pastebin->setDisabled(false);
    reply->deleteLater();
  });
}


// not being used as we are not saving file now to share.
bool Share::saveFile(const QString &filename) {
  QString path = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
  QFile file(QDir(path).filePath(filename));
  if (!file.open(QFile::WriteOnly | QFile::Text)) {
    QMessageBox::warning(this, tr("Application"),
                         tr("Cannot write file %1:\n%2.")
                             .arg(QDir::toNativeSeparators(file.fileName()),
                                  file.errorString()));
    return false;
  }
  QTextStream out(&file);
#ifndef QT_NO_CURSOR
  QApplication::setOverrideCursor(Qt::WaitCursor);
#endif
  out << ui->translation->toPlainText();
#ifndef QT_NO_CURSOR
  QApplication::restoreOverrideCursor();
#endif
  file.close();
  return true;
}


void Share::showStatus(const QString &message) {
  auto eff = new QGraphicsOpacityEffect(this);
  ui->status->setGraphicsEffect(eff);
  auto a = new QPropertyAnimation(eff, "opacity");
  a->setDuration(500);
  a->setStartValue(0);
  a->setEndValue(1);
  a->setEasingCurve(QEasingCurve::InCurve);
  a->start(QPropertyAnimation::DeleteWhenStopped);
  ui->status->setText(message);
  ui->status->show();
}

/// Shows @p message in the status bar and appends "Open file | Show in folder"
/// links for @p filePath.
void Share::showStatusWithFile(const QString &message, const QString &filePath)
{
  const QUrl fileUrl   = QUrl::fromLocalFile(filePath);
  const QUrl folderUrl = QUrl::fromLocalFile(
      QFileInfo(filePath).dir().absolutePath());

  const QString links =
      QStringLiteral(" &nbsp;<a href='%1'>Open file</a>"
                     " &nbsp;·&nbsp; <a href='%2'>Show in folder</a>")
          .arg(fileUrl.toString(), folderUrl.toString());

  showStatus(message + links);
}

void Share::on_download_clicked() {
  if (!LicenseService::instance()->checkAccess(
          QStringLiteral("Export as Audio")))
    return;

  QString text = ui->translation->toPlainText();
  const int voiceIdx = ui->voiceGender->currentIndex();
  const auto voices = utils::availableVoices();
  const VoiceOption &voiceConf =
      voiceIdx < voices.size() ? voices.at(voiceIdx) : voices.first();
  const QString selectedGender = voiceConf.gender;
  const QString ttsLang =
      voiceConf.langOverride.isEmpty()
          ? (translationLangCode.isEmpty() ? QString("en") : translationLangCode)
          : voiceConf.langOverride;
  showStatus("<span style='color:green'>Share: </span>Synthesising " +
             voiceConf.displayName + " voice...");
  ui->download->setEnabled(false);
  m_downloadedAudioFiles.clear();
  m_ttsDownloader->start(text, ttsLang, selectedGender);
}

