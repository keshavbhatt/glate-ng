#ifndef SHARE_H
#define SHARE_H

#include "ttsdownloader.h"

#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QProcess>
#include <QSettings>
#include <QPushButton>

namespace Ui {
class Share;
}

class Share : public QWidget {
  Q_OBJECT

public:
  explicit Share(QWidget *parent = nullptr);
  ~Share();

public slots:
  void setTranslation(const QString &translation, const QString &uuid,
                      const QString &langCode = QString());

  /// Called by MainWindow whenever the licence status changes.
  void setHasPaidAccess(bool hasPaidAccess);

signals:
  /// Emitted when the user clicks "View Plans" inside a Pro-feature prompt.
  void upgradeRequested();

private slots:
  void on_text_clicked();
  void on_email_clicked();
  void on_pastebin_clicked();
  void on_download_clicked();
  void ffmpeg_finished(int k);

  static QString getFileNameFromString(const QString &string);
  void showStatus(const QString &message);
  bool saveFile(const QString &filename);
  void concat(const QStringList &audioFiles);

private:
  /// Shows a polite upgrade prompt for @p featureName.
  /// Returns true if the user has paid access (feature may proceed).
  /// Returns false and, optionally, emits upgradeRequested() if they do not.
  bool checkPaidAccess(const QString &featureName);

  Ui::Share *ui;
  QSettings settings;
  QProcess *ffmpeg = nullptr;
  QNetworkAccessManager *m_networkManager = nullptr;
  TtsDownloader *m_ttsDownloader = nullptr;
  QString translationUUID;
  QString translationLangCode;
  QStringList m_downloadedAudioFiles;
  bool m_hasPaidAccess = false;
};

#endif // SHARE_H
