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
#include "sharewidget.h"
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
class QLabel;
class QSplitter;
class GlateShell;
class OfflineModelsDialog;

namespace glate::shell {
class StatusPill;
class WelcomeCard;
} // namespace glate::shell

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

  // Quick Translate: brings the window to the front and translates the
  // selected text (or the clipboard). From the global shortcut, or from
  // "glate --quick-translate" through the running instance.
  void quickTranslate(const QByteArray &activationToken);

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
  void translateSelection();
  void showError(const QString &message);
  void showError(const QString &heading, const QString &explanation,
                 const QString &detail);
  // App shell (modules/AppShell): help menu, offline indicator, first run.
  void initShell();
  void showTour();
  QStringList diagnosticsLines() const;
  void refreshOfflineIndicator();
  void updateOfflineCaption();
  void updateWelcomeCard();
  void positionPaneOverlays();
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
  glate::shortcut::GlobalShortcut *m_globalShortcut = nullptr;
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
  bool                   m_hasPremiumAccess = false;
  bool                   m_licenseKnown     = false;

  // Offline translation
  glate::offline::ModelStore        *m_modelStore    = nullptr;
  glate::offline::ModelManager      *m_modelManager  = nullptr;
  glate::offline::OfflineTranslator *m_offline       = nullptr;
  OfflineModelsDialog               *m_offlineDialog = nullptr;
  quint64                            m_offlineRequest = 0;

  // App shell
  GlateShell                *m_shell          = nullptr;
  glate::shell::StatusPill  *m_offlinePill    = nullptr;
  glate::shell::WelcomeCard *m_welcome        = nullptr;
  QLabel                    *m_offlineCaption = nullptr;
  QMenu                     *m_helpMenu       = nullptr;
  bool                       m_nextResultOffline = false; // set before processTranslation()
  bool                       m_resultOffline  = false;    // what src2 shows now
  QString                    m_resultPair;                // "English to Spanish"
  QString                    m_pendingOfflineText;        // waits for the licence status
  QString                    m_resultPlaceholder;         // src2's, hidden under the welcome card
};

#endif // MAINWINDOW_H
