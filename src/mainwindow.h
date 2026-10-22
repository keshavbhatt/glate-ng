#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QKeyEvent>
#include <QMainWindow>
#include <QAudioOutput>
#include <QMediaPlayer>
#include <QScrollBar>
#include <QTextEdit>

#include "controlbutton.h"
#include "error.h"
#include "history.h"
#include "linebyline.h"
#include "request.h"
#include "settings.h"
#include "share.h"
#include "slider.h"
#include "systemtraymanager.h"
#include "ttsdownloader.h"
#include "ui_textoptionform.h"
#include "waitingspinnerwidget.h"
#include "AccountLicenseManager.h"
#include "accountdialog.h"

namespace Ui {
class MainWindow;
}

class ThemeController;
class QSplitter;
class AboutDialog;
class OfflineModelsDialog;

namespace glate::offline {
class ModelManager;
class ModelStore;
class OfflineTranslator;
} // namespace glate::offline

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(ThemeController *theme = nullptr,
                      QWidget *parent = nullptr);
  ~MainWindow();

  // Developer aid for headless verification and store screenshots, driven by
  // GLATE_DEBUG_* environment variables (see scripts/screenshots.sh). Does
  // nothing when none is set.
  void debugRun();

protected slots:
  void changeEvent(QEvent *e) override;
  void resizeEvent(QResizeEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *o, QEvent *e) override;
private slots:

  void on_clear_clicked();
  void on_paste_clicked() const;
  void on_switch_lang_clicked() const;
  void on_lang1_currentIndexChanged(int index);
  void on_lang2_currentIndexChanged(int index);
  void on_play2_clicked();
  void on_play1_clicked();
  void on_src1_textChanged() const;
  void on_copy_clicked() const;
  void on_src2_textChanged() const;
  void on_share_clicked();
  void on_settings_clicked() const;
  void on_history_clicked() const;
  void on_lineByline_clicked();
  void on_help_clicked();
  void on_about_clicked();
  void on_account_clicked() const;

  // Account / licence
  void onLicenseStatusUpdated(const AccountLicenseManager::LicenseStatus &status);

  void readLangCode();
  void resizeFix();
  void translate_clicked();
  void init_history();
  QString getTransLang() const;
  QString getSourceLang() const;
  void init_settings();
  void showError(const QString &message);
  void textSelectionChanged(QTextEdit *editor);
  QString getLangName(const QString &langCode) const;
  QString getLangCode(const QString &langName) const;
  static void saveByTransId(const QString &translationId, const QString &reply);
  void processTranslation(const QString &reply);
  // Offline translation (modules/OfflineTranslation, Pro).
  void initOffline();
  bool offlineModeEnabled() const;
  void translateOffline(const QString &text);
  void showOfflineLanguages(const QStringList &downloadCodes = {});
  void updateOfflineSummary();
  QSize resizeFloor() const;
  void startTtsSession(const QString &playerName, const QString &text,
                       const QString &ttsLang, const QString &voiceGender);
  void playCurrentTtsChunk();
  void stopTtsSession(bool stopPlayer);
  void resetPlayIcons();
  void setPlayButtonState(const QString &buttonName, const QIcon &icon,
                          const QString &toolTip) const;
  QString activePlayButtonName() const;
  bool resolveTtsVoiceConfig(const QString &baseLang, QString &ttsLang,
                             QString &ttsCheckLang,
                             QString &voiceGender) const;
  void toggleOrStartTts(const QString &buttonName, const QString &baseLang,
                        const QString &displayLang, const QString &text);

private:
  Ui::MainWindow *ui;
  Ui::textOptionForm textOptionForm;

  // Live theming controller (owned by main); drives the AppChrome Fusion/Fusion2
  // look and follows the OS light/dark scheme. May be null.
  ThemeController *m_theme = nullptr;

  // Resizable source/translation panes; the translate button rides its handle.
  QSplitter *m_splitter = nullptr;
  void positionPaneButtons(); // place translate + switch-lang over the handle

  controlButton *m_translate = nullptr;
  WaitingSpinnerWidget *m_loader = nullptr;
  Error *m_error = nullptr;
  LineByLine *m_lineByLine = nullptr;
  Share *m_share = nullptr;
  Request *m_request = nullptr;
  QMediaPlayer *m_player = nullptr;
  QClipboard *m_clipboard = nullptr;
  Settings *m_settingsWidget = nullptr;
  History *m_historyWidget = nullptr;
  QHotkey *m_nativeHotkey = nullptr;
  QWidget *m_textOptionWidget = nullptr;
  Slider *m_slider = nullptr;
  SystemTrayManager *m_trayManager = nullptr;

  QString m_selectedText;
  bool m_playSelected = false;
  bool m_forceQuit = false;
  QString m_translationId;
  QStringList m_langName, m_langCode, m_supportedTts;
  QSettings m_settings;
  QList<QStringList> m_currentTranslationData;

  TtsDownloader *m_ttsDownloader = nullptr;
  QStringList m_ttsLocalFiles;
  int m_ttsCurrentIndex = -1;
  bool m_ttsSessionActive = false;

  // Account / licence
  AccountLicenseManager *m_licenseManager  = nullptr;
  AccountDialog         *m_accountDialog   = nullptr;
  AboutDialog           *m_aboutDialog     = nullptr;
  bool                   m_hasPremiumAccess = false;

  // Offline translation
  glate::offline::ModelStore        *m_modelStore    = nullptr;
  glate::offline::ModelManager      *m_modelManager  = nullptr;
  glate::offline::OfflineTranslator *m_offline       = nullptr;
  OfflineModelsDialog               *m_offlineDialog = nullptr;
  quint64                            m_offlineRequest = 0;
};

#endif // MAINWINDOW_H
