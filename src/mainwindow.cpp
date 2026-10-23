#include "mainwindow.h"

#include <QFile>
#include <QHBoxLayout>
#include <QShortcut>
#include <QSplitter>
#include <QUrlQuery>

#include "glateshell.h"
#include "GlateLicenseConfig.h"
#include "accountdialog.h"
#include "licenseservice.h"
#include "offlinemodelsdialog.h"
#include "ui_mainwindow.h"
#include "utils.h"

#include "Languages.h"
#include "ModelManager.h"
#include "ModelStore.h"
#include "OfflineTranslator.h"
#include "TranslationEngine.h"

#include "Diagnostics.h"
#include "Logging.h"
#include "StatusPill.h"
#include "ShellStyle.h"
#include "WelcomeCard.h"

#include <QJsonArray>
#include <QLabel>
#include <QBuffer>
#include <QMenu>
#include <QPainter>
#include <QJsonDocument>
#include <QMessageBox>
#include <QTabWidget>
#include <QTimer>
#include <QClipboard>

#include <memory>

#include <utility>

#include <AppTheme.h>
#include <ThemeController.h>
#include <WindowChrome.h>

namespace {
constexpr auto kPlay1IdleTooltip = "Play source text";
constexpr auto kPlay2IdleTooltip = "Play translation";
constexpr auto kPreparingTooltip = "Preparing audio...";
constexpr auto kBufferingTooltip = "Buffering audio...";
constexpr auto kStopTooltip = "Stop playback";
// First-run card: shown while both panes are empty, until this many translations.
constexpr int kWelcomeTranslations = 3;
constexpr auto kTranslationCountKey = "shell/translationCount";
}

MainWindow::MainWindow(ThemeController *theme, QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_theme(theme) {
  ui->setupUi(this);

  m_trayManager = new SystemTrayManager(this);

  connect(m_trayManager, &SystemTrayManager::showWindow, this, [=]() {
    this->showNormal();
    m_trayManager->updateMenu(this->isVisible());
  });
  connect(m_trayManager, &SystemTrayManager::hideWindow, this, [=]() {
    this->hide();
    m_trayManager->updateMenu(this->isVisible());
  });
  connect(m_trayManager, &SystemTrayManager::quitRequested, this, [=]() {
    m_forceQuit = true;
    this->close();
  });

  m_trayManager->updateMenu(true);

  this->setWindowTitle(QApplication::applicationName() + " v" +
                       QApplication::applicationVersion());
  this->setWindowIcon(QIcon(":/icons/app/icon-64.png"));

  // Theming is now driven globally by AppChrome (AppTheme + ThemeController,
  // applied in main.cpp) instead of the bundled qbreeze stylesheet. When the
  // user has client-side decoration enabled, add the app's own title bar +
  // themed window edge. Must be set before show() so the frameless flag is in
  // place before the native window is created.
  if (m_settings.value("appearance/csd", WindowChrome::defaultEnabled()).toBool()) {
    WindowChrome::Config cfg;
    cfg.title = QApplication::applicationName();
    cfg.icon = QApplication::windowIcon();
    WindowChrome::install(this, cfg);
  }

  // set layout
  m_translate = new controlButton(ui->translationWidget);
  m_translate->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
  m_translate->setFixedSize(36, 36);
  m_translate->setMouseTracking(true);
  m_translate->setToolTip("Translate (Shift + Enter)");
  // Opaque circular background so the splitter line behind it is fully masked.
  m_translate->setStyleSheet(
      "border:none;border-radius:18px;background-color:palette(window);");
  m_translate->setIconSize(QSize(28, 28));
  const auto shortcut =
      new QShortcut(QKeySequence(tr("Shift+Return", "Translate")), m_translate,
                    SLOT(click()));
  shortcut->setContext(Qt::ApplicationShortcut);
  m_translate->setIcon(QIcon(":/icons/arrow-right-circle-line.png"));
  QTimer::singleShot(100, this, SLOT(resizeFix()));
  connect(m_translate, &QAbstractButton::clicked, this,
          &MainWindow::translate_clicked);

  // Make the source/translation panes user-resizable. src1/src2 move out of the
  // translationWidget layout into a splitter; the translate + switch-language
  // buttons ride the handle (see positionPaneButtons()).
  m_splitter = new QSplitter(Qt::Horizontal, ui->translationWidget);
  m_splitter->setObjectName("paneSplitter");
  m_splitter->setHandleWidth(6);
  m_splitter->setChildrenCollapsible(false);
  m_splitter->addWidget(ui->src1);
  m_splitter->addWidget(ui->src2);
  m_splitter->setStretchFactor(0, 1);
  m_splitter->setStretchFactor(1, 1);
  if (auto *lay = qobject_cast<QHBoxLayout *>(ui->translationWidget->layout()))
    lay->addWidget(m_splitter);
  // The overlay buttons must stay above the splitter (created after them).
  m_translate->raise();
  ui->switch_lang->raise();
  connect(m_splitter, &QSplitter::splitterMoved, this,
          [this](int, int) { positionPaneButtons(); });

  // The panes are QTextEdits, scroll areas whose minimumSizeHint is ~0, so on
  // their own they let the window collapse and the layout can't derive a real
  // floor. Give them a sensible minimum so the window's minimumSizeHint() (which
  // also accounts for the toolbars) becomes meaningful, no magic number needed;
  // resizeEvent enforces it (see below).
  ui->src1->setMinimumSize(170, 120);
  ui->src2->setMinimumSize(170, 120);
  // Breathing room between the text and the (outer-edge) scrollbar so the two
  // don't visually collide and are each easy to interact with. documentMargin
  // is side-agnostic, which matters here since src1 is RTL (scrollbar on the
  // left) and src2 is LTR (scrollbar on the right).
  ui->src1->document()->setDocumentMargin(10);
  ui->src2->document()->setDocumentMargin(10);

  // Thin themed separators between the top/bottom toolbars and the panes.
  auto makeToolbarSeparator = [this]() {
    auto *line = new QFrame(centralWidget());
    line->setObjectName(QStringLiteral("toolbarSeparator"));
    line->setFrameShape(QFrame::NoFrame);
    line->setFixedHeight(1);
    line->setAttribute(Qt::WA_StyledBackground, true); // paint the QSS background
    return line;
  };
  ui->verticalLayout->insertWidget(1, makeToolbarSeparator()); // under top toolbar
  ui->verticalLayout->insertWidget(3, makeToolbarSeparator()); // over bottom toolbar

  // Drive the window's minimum from the layout's real minimum (derived from the
  // widget size hints, no magic number) and push it to the window manager once
  // the layout is realised, so an interactive resize can't squash the toolbars.
  ui->verticalLayout->setSizeConstraint(QLayout::SetMinimumSize);
  QTimer::singleShot(0, this, [this]() { setMinimumSize(resizeFloor()); });

  ui->src1->installEventFilter(this);

  // read language codes and initialize vars
  readLangCode();

  // populate lang combos
  ui->lang1->blockSignals(true);
  ui->lang2->blockSignals(true);

  ui->lang1->addItem(QIcon(":/icons/translate-2.png"), "Auto Detect");
  ui->lang1->addItems(m_langName);
  ui->lang2->addItems(m_langName);
  // Cap the (very long) language popups so they scroll instead of spanning the
  // whole screen height (needs `combobox-popup: 0` in the Fusion 2 QSS).
  ui->lang1->setMaxVisibleItems(14);
  ui->lang2->setMaxVisibleItems(14);
  for (int i = 0; i < ui->lang1->count(); i++) {
    if (i == 0) {
      ui->lang1->setItemIcon(i, QIcon(":/icons/magic-line.png"));
    } else {
      ui->lang1->setItemIcon(i, QIcon(":/icons/translate-2.png"));
    }
  }
  for (int i = 0; i < ui->lang2->count(); i++) {
    ui->lang2->setItemIcon(i, QIcon(":/icons/translate-2.png"));
  }

  // load settings
  ui->lang1->setCurrentIndex(m_settings.value("lang1", 0).toInt());
  ui->lang2->setCurrentIndex(m_settings.value("lang2", 20).toInt());

  ui->switch_lang->setEnabled(ui->lang1->currentIndex() != 0);

  ui->lang1->blockSignals(false);
  ui->lang2->blockSignals(false);

  ui->lang1->setMinimumWidth(150);
  ui->lang2->setMinimumWidth(150);

  // loader is the child of wall_view
  m_loader = new WaitingSpinnerWidget(m_translate, true, true);
  m_loader->setRoundness(70.0);
  m_loader->setMinimumTrailOpacity(15.0);
  m_loader->setTrailFadePercentage(70.0);
  m_loader->setNumberOfLines(30);
  m_loader->setLineLength(14);
  m_loader->setLineWidth(2);
  m_loader->setInnerRadius(2);
  m_loader->setRevolutionsPerSecond(3);
  m_loader->setColor(QColor(67, 124, 198));

  if (m_settings.value("geometry").isValid()) {
    restoreGeometry(m_settings.value("geometry").toByteArray());
    if (m_settings.value("windowState").isValid()) {
      restoreState(m_settings.value("windowState").toByteArray());
    } else {
      const QScreen *pScreen = QGuiApplication::screenAt(
          this->mapToGlobal(QPoint(this->width() / 2, 0)));
      if (pScreen) {
        this->move(pScreen->availableGeometry().center() - this->rect().center());
      }
    }
  }

  // init media player
  m_player = new QMediaPlayer(this);
  m_ttsDownloader = new TtsDownloader(this);
  const auto audioOutput = new QAudioOutput(this);
  audioOutput->setVolume(1.0);
  m_player->setAudioOutput(audioOutput);

  connect(m_ttsDownloader, &TtsDownloader::progress, this,
          [=](const QString &status) {
            const QString activeButton = activePlayButtonName();
            if (!activeButton.isEmpty()) {
              setPlayButtonState(activeButton,
                                 QIcon(":/icons/loader-2-fill.png"), status);
            }
          });
  connect(m_ttsDownloader, &TtsDownloader::finished, this,
          [=](const QStringList &audioFiles) {
            if (!m_ttsSessionActive) {
              return;
            }
            m_ttsLocalFiles = audioFiles;
            m_ttsCurrentIndex = 0;
            if (m_ttsLocalFiles.isEmpty()) {
              showError(tr("Could not play the audio"),
                        tr("The speech service returned no audio."), QString());
              stopTtsSession(false);
              return;
            }
            playCurrentTtsChunk();
          });
  connect(m_ttsDownloader, &TtsDownloader::failed, this,
          [=](const QString &error) {
            stopTtsSession(false);
            qCWarning(glate::shell::lcSpeech).noquote()
                << "speech download failed:" << glate::shell::redactUrlQueries(error);
            showError(tr("Could not play the audio"), QString(),
                      glate::shell::redactUrlQueries(error));
          });
  connect(m_player, &QMediaPlayer::playbackStateChanged, this,
          [=](QMediaPlayer::PlaybackState state) {
            const QString activeButton = activePlayButtonName();
            if (!activeButton.isEmpty() &&
                state == QMediaPlayer::StoppedState) {
              setPlayButtonState(activeButton, QIcon(":/icons/volume-up-line.png"),
                                 activeButton == "play1"
                                     ? kPlay1IdleTooltip
                                     : kPlay2IdleTooltip);
              m_playSelected = false;
              m_selectedText.clear();
            }
            if (!activeButton.isEmpty() &&
                state == QMediaPlayer::PlayingState) {
              setPlayButtonState(activeButton, QIcon(":/icons/stop-line.png"),
                                 kStopTooltip);
            }
          });

  connect(m_player, &QMediaPlayer::errorOccurred, this,
          [=](QMediaPlayer::Error error, const QString &errorString) {
            Q_UNUSED(error);
            stopTtsSession(false);
            const QString message =
                errorString.isEmpty() ? m_player->errorString() : errorString;
            qCWarning(glate::shell::lcSpeech).noquote() << "playback failed:" << message;
            showError(tr("Could not play the audio"), QString(), message);
          });

  connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
          [=](QMediaPlayer::MediaStatus mediastate) {
            if (mediastate == QMediaPlayer::EndOfMedia) {
              if (m_ttsSessionActive) {
                ++m_ttsCurrentIndex;
                if (m_ttsCurrentIndex < m_ttsLocalFiles.size()) {
                  playCurrentTtsChunk();
                } else {
                  stopTtsSession(false);
                }
                return;
              }
            }

            const QString activeButton = activePlayButtonName();
            if (!activeButton.isEmpty() &&
                (mediastate == QMediaPlayer::LoadingMedia ||
                 mediastate == QMediaPlayer::BufferingMedia)) {
              setPlayButtonState(activeButton,
                                 QIcon(":/icons/loader-2-fill.png"),
                                 kBufferingTooltip);
            }
            if (!activeButton.isEmpty() &&
                (mediastate == QMediaPlayer::BufferedMedia)) {
              setPlayButtonState(activeButton, QIcon(":/icons/stop-line.png"),
                                 kStopTooltip);
            }
          });

  resetPlayIcons();

  // Disable TTS buttons initially, re-enabled by on_src*_textChanged
  // as soon as the user types or text is loaded.
  ui->play1->setEnabled(!ui->src1->toPlainText().isEmpty());
  ui->play2->setEnabled(!ui->src2->toPlainText().isEmpty());

  // load src1
  ui->counter->setText("5000/5000");
  const int minWid =
      ui->counter->fontMetrics().boundingRect(ui->counter->text()).width();
  ui->counter->setText(QString::number(ui->src1->toPlainText().length()) +
                       "/5000");

  ui->counter->setMinimumWidth(minWid);
  ui->counter_2->setMinimumWidth(minWid);

  m_clipboard = QApplication::clipboard();

  // init share
  m_share = new Share(this);
  m_share->setWindowTitle(QApplication::applicationName() +
                          QStringLiteral(" | Share translation"));
  m_share->setWindowFlag(Qt::Dialog);
  m_share->setWindowModality(Qt::NonModal);

  // LicenseService is the single point that any class calls for Pro gating.
  // MainWindow connects its upgradeRequested() to the Account dialog here.
  connect(LicenseService::instance(), &LicenseService::upgradeRequested,
          this, [this]() {
              on_account_clicked();
              m_accountDialog->highlightPurchaseButton();
          });

  m_lineByLine = new LineByLine(this, m_clipboard);
  m_lineByLine->setWindowTitle(QApplication::applicationName() +
                               " | LineByLine translation");
  m_lineByLine->setWindowFlag(Qt::Dialog);
  m_lineByLine->setWindowModality(Qt::NonModal);
  connect(m_lineByLine, &LineByLine::readSourceRequested, this,
          [this](const QString &text) {
            toggleOrStartTts("line_read_src", getSourceLang(),
                             getLangName(getSourceLang()), text);
          });
  connect(m_lineByLine, &LineByLine::readTranslationRequested, this,
          [this](const QString &text) {
            toggleOrStartTts("line_read_tr", getTransLang(),
                             getLangName(getTransLang()), text);
          });

  // init settings
  init_settings();
  initOffline();

  // init history
  init_history();

  if (m_settings.value("lastTranslation").isValid()) {
    const QFileInfo check(QDir(utils::returnPath("history"))
                              .filePath(m_settings.value("lastTranslation").toString()
                                        + ".json"));
    if (check.exists()) {
      m_translationId = m_settings.value("lastTranslation").toString();
      m_historyWidget->loadHistoryItem(
          QDir(utils::returnPath("history"))
              .filePath(m_translationId + ".json"));
    }
  }

  // ── Account & licence ──────────────────────────────────────────────────────
  m_licenseManager = new AccountLicenseManager(glateAccountLicenseConfig(), this);
  LicenseService::instance()->setAccountManager(m_licenseManager);
  connect(m_licenseManager, &AccountLicenseManager::licenseStatusUpdated,
          this, &MainWindow::onLicenseStatusUpdated);

  // Account dialog, created once, shown on demand via the toolbar button
  m_accountDialog = new AccountDialog(this);
  m_accountDialog->setWindowFlag(Qt::Dialog);
  m_accountDialog->setWindowModality(Qt::NonModal);
  m_accountDialog->setAccountManager(m_licenseManager);

  // Restore / generate the serial and kick off a background licence check.
  m_licenseManager->ensureSerial();
  m_accountDialog->setSerial(m_licenseManager->serial());
  m_licenseManager->checkPurchase(m_licenseManager->serial());

  initShell();
  // What's new reads "firstRun" to tell an update from a new install.
  m_shell->showWhatsNewIfUpdated();
  m_settings.setValue("firstRun", false);
}

// TODO skip duplicate history item by checking translationId

void MainWindow::init_history() {
  utils::returnPath("history");
  m_historyWidget = new History(this);
  m_historyWidget->setWindowTitle(QApplication::applicationName() +
                                  " | History");
  m_historyWidget->setWindowFlag(Qt::Dialog);
  m_historyWidget->setWindowModality(Qt::NonModal);
  connect(m_historyWidget, &History::setTranslationId, this,
          [=](QString transId) { this->m_translationId = transId; });
  connect(m_historyWidget, &History::historyItemMeta, this,
          [=](QStringList historyItemMeta) {
            if (historyItemMeta.count() >= 4) {
              m_currentTranslationData.clear();
              m_settings.setValue(
                  "lastTranslation",
                  m_translationId); // translation id is set before this functor
                                    // is called via signal
              QString from, to, src1, src2;
              from = historyItemMeta.at(0);
              to = historyItemMeta.at(1);
              src1 = historyItemMeta.at(2);
              src2 = historyItemMeta.at(3);
              ui->src1->setText(src1);
              ui->src2->setText(src2);
              m_resultOffline = historyItemMeta.value(4) == QLatin1String("offline");
              m_resultPair = tr("%1 to %2").arg(from, to);
              updateOfflineCaption();
              ui->lang1->setCurrentText(from);
              ui->lang2->setCurrentText(to);
            }
          });
}

void MainWindow::changeEvent(QEvent *e) {
  QMainWindow::changeEvent(e);
  //  if (e->type() == QEvent::WindowStateChange) {
  //    m_trayManager->updateMenu(this->isVisible());
  //  }
}

void MainWindow::init_settings() {
  m_globalShortcut = new glate::shortcut::GlobalShortcut(
      QStringLiteral("com.ktechpit.glate"), QStringLiteral("quick-translate"),
      tr("Quick Translate: translate the selected text"), this);
  connect(m_globalShortcut, &glate::shortcut::GlobalShortcut::activated, this,
          &MainWindow::quickTranslate);
  m_settingsWidget = new Settings(this, m_globalShortcut, m_theme);
  m_settingsWidget->setWindowTitle(QApplication::applicationName() +
                                   " | Settings");
  m_settingsWidget->setWindowFlag(Qt::Dialog);
  m_settingsWidget->setWindowModality(Qt::NonModal);
  // Theme is now driven by the Settings → Appearance section directly through
  // the ThemeController (the old dark/light radios are superseded), so there's
  // nothing to handle on themeToggled here.
}

void MainWindow::quickTranslate(const QByteArray &activationToken) {
  qCInfo(glate::shell::lcApp) << "quick translate";
  if (!activationToken.isEmpty()) {
    // Qt reads it once when the window asks to be activated (Wayland xdg-activation).
    qputenv("XDG_ACTIVATION_TOKEN", activationToken);
  }
  setWindowState((windowState() & ~Qt::WindowMinimized) | Qt::WindowActive);
  show();
  raise();
  activateWindow();
  // On Wayland the selection is only offered to the active window: wait for
  // that, but not forever. X11 and Windows have it at once.
  const bool wayland = QGuiApplication::platformName().startsWith("wayland");
  if (!wayland || isActiveWindow()) {
    translateSelection();
    return;
  }
  auto *wait = new QTimer(this);
  wait->setSingleShot(true);
  auto done = std::make_shared<bool>(false);
  const auto run = [this, wait, done] {
    if (*done)
      return;
    *done = true;
    wait->deleteLater();
    translateSelection();
  };
  connect(wait, &QTimer::timeout, this, run);
  connect(qApp, &QGuiApplication::applicationStateChanged, wait,
          [run](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive)
              QTimer::singleShot(50, run);
          });
  wait->start(600);
}

void MainWindow::translateSelection() {
  // The selected text (X11, Wayland); else the clipboard (Windows, or nothing
  // selected: copy, then the shortcut).
  const QClipboard *clipboard = QGuiApplication::clipboard();
  QString text;
  if (clipboard->supportsSelection())
    text = clipboard->text(QClipboard::Selection);
  if (text.trimmed().isEmpty())
    text = clipboard->text(QClipboard::Clipboard);
  if (text.trimmed().isEmpty())
    return;
  ui->src1->setText(text);
  ui->lang1->setCurrentIndex(0);
  m_translate->click();
}

void MainWindow::textSelectionChanged(QTextEdit *editor) {
  const QString selection = editor->textCursor().selectedText().trimmed();
  if (!selection.isEmpty()) {
    m_textOptionWidget->setObjectName("selection_" + editor->objectName());
    m_textOptionWidget->move(QCursor::pos().x(), QCursor::pos().y() + 10);
    m_textOptionWidget->resize(m_textOptionWidget->minimumSizeHint());
    m_textOptionWidget->adjustSize();
    m_textOptionWidget->show();

    textOptionForm.readPushButton->disconnect();
    textOptionForm.copyPushButton->disconnect();

    connect(textOptionForm.readPushButton, &QPushButton::clicked, this, [=]() {
      if (this->findChild<QTextEdit *>(m_textOptionWidget->objectName()
                                           .split("selection_")
                                           .last()
                                           .trimmed()) != nullptr) {
        m_playSelected = true;
        m_selectedText = selection;
        if (m_textOptionWidget->objectName()
                .split("selection_")
                .last()
                .trimmed() == "src1") {
          on_play1_clicked();
        } else {
          on_play2_clicked();
        }
        m_textOptionWidget->hide();
      }
    });

    connect(textOptionForm.copyPushButton, &QPushButton::clicked, this, [=]() {
      m_clipboard->setText(selection);
      m_textOptionWidget->hide();
    });
  } else {
    m_playSelected = false;
    m_selectedText.clear();
  }
}

bool MainWindow::eventFilter(QObject *o, QEvent *e) {
  if (o == ui->src2->viewport() && e->type() == QEvent::Resize)
    positionPaneOverlays();
  // simulate translate shortcut instead of going to new line when cursor is on
  // textEdit widgets.
  if (e->type() == QEvent::KeyRelease || e->type() == QEvent::KeyPress ||
      e->type() == QEvent::ShortcutOverride) {
    if (o == ui->src1 || o == ui->src2) {
      const auto keyEvent = static_cast<QKeyEvent *>(e);
      if ((keyEvent->key() == Qt::Key_Return) &&
          keyEvent->modifiers() & (Qt::ShiftModifier)) {
        m_translate->click();
        return true;
      } else {
        return false;
      }
    }
  }
  return QMainWindow::eventFilter(o, e);
}

void MainWindow::resizeFix() {
  this->resize(this->width() + 1, this->height());
  ui->left_buttons_widget->setMinimumWidth(ui->right_button_widget->width());
  ui->left_buttons_widget->resize(ui->right_button_widget->width(),
                                  ui->left_buttons_widget->height());
}

QSize MainWindow::resizeFloor() const {
  QSize floor = minimumSizeHint();
  if (ui->horizontalLayout_3) {
    floor = floor.expandedTo(ui->horizontalLayout_3->sizeHint());
  }
  if (ui->horizontalLayout_4) {
    floor = floor.expandedTo(ui->horizontalLayout_4->sizeHint());
  }
  return floor;
}

void MainWindow::closeEvent(QCloseEvent *event) {

  m_settings.setValue("geometry", saveGeometry());
  m_settings.setValue("windowState", saveState());

  // Use live UI state for this close action, then persist it.
  const bool closeToTray =
      m_settingsWidget != nullptr
          ? m_settingsWidget->closeToTrayEnabled()
          : m_settings.value("closeToTray", true).toBool();
  m_settings.setValue("closeToTray", closeToTray);

  if (!m_forceQuit && m_trayManager->isTrayAvailable() && closeToTray) {
    m_trayManager->updateMenu(false);
    this->hide();
    event->ignore();
    return;
  }
  qApp->quit();

  QMainWindow::closeEvent(event);
}

void MainWindow::positionPaneButtons() {
  // Centre the translate + switch-language buttons on the splitter handle (so
  // they track the divider as the user drags the panes), falling back to the
  // translationWidget centre before the splitter is laid out.
  int cx = ui->translationWidget->width() / 2;
  if (m_splitter) {
    if (QWidget *handle = m_splitter->handle(1))
      cx = handle->mapTo(ui->translationWidget,
                         QPoint(handle->width() / 2, 0))
               .x();
  }
  const int y = ui->translationWidget->geometry().center().y() -
                (m_translate->height() + m_translate->height() / 2);
  m_translate->move(cx - m_translate->width() / 2, y);
  ui->switch_lang->move(cx - ui->switch_lang->width() / 2,
                        ui->switch_lang->y());
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  // A frameless (client-side-decorated) window isn't reliably held to its
  // minimum by the compositor during an interactive resize, so the layout could
  // be squashed below what its widgets need. Snap back up to the layout-derived
  // minimum, this is the single source of truth (widget size hints), not a
  // hard-coded number.
  const QSize floor = resizeFloor();
  if (event->size().width() < floor.width() ||
      event->size().height() < floor.height()) {
    resize(event->size().expandedTo(floor));
    return; // a fresh resizeEvent fires with the clamped size
  }
  positionPaneButtons();
  ui->left_buttons_widget->resize(ui->right_button_widget->width(),
                                  ui->left_buttons_widget->height());
  QMainWindow::resizeEvent(event);
}

void MainWindow::readLangCode() {
  QFile lang(":/resources/lang.glate");
  if (!lang.open(QIODevice::ReadOnly)) {
    qWarning("Unable to open file");
    return;
  }
  while (!lang.atEnd()) {
    auto langLine = QString(lang.readLine());
    QStringList langItem = langLine.split("<=>");
    m_langName.append(langItem.at(0).trimmed());
    m_langCode.append(langItem.at(1).trimmed());
  }
  lang.close();

  // TTS
  QFile tts(":/resources/tts.glate");
  if (!tts.open(QIODevice::ReadOnly)) {
    qWarning("Unable to open file");
    return;
  }
  while (!tts.atEnd()) {
    auto ttsCode = QString(tts.readLine());
    m_supportedTts.append(ttsCode.trimmed());
  }
  tts.close();
}

MainWindow::~MainWindow() {
  stopTtsSession(true);
  delete ui;
}

void MainWindow::translate_clicked() {
  // clear processor vars
  m_currentTranslationData.clear();
  ui->src2->clear();

  if (m_request != nullptr) {
    m_request->blockSignals(true);
    m_request->deleteLater();
    m_request->blockSignals(false);
    m_request = nullptr;
    m_loader->stop();
  }

  if (m_request == nullptr) {
    m_request = new Request(this);
    connect(m_request, &Request::requestStarted, this,
            [=]() { m_loader->start(); });
    connect(m_request, &Request::requestFinished, this, [=](QString reply) {
      // save cache for line by line use
      saveByTransId(m_translationId, reply);
      m_nextResultOffline = false;
      // load to view
      processTranslation(reply);
      m_loader->stop();
      m_settings.setValue("lastTranslation", m_translationId);
    });
    connect(m_request, &Request::downloadError, this, [=](QString errorString) {
      m_loader->stop();
      const QString detail = glate::shell::redactUrlQueries(errorString);
      qCWarning(glate::shell::lcNetwork).noquote() << "translation failed:" << detail;
      showError(tr("Could not translate"),
                tr("The translation service did not answer. Check your "
                   "connection and try again."),
                detail);
    });

    const QString src1 = ui->src1->toPlainText();

    if (offlineModeEnabled()) {
      m_request->deleteLater();
      m_request = nullptr;
      translateOffline(src1);
      return;
    }

    QUrl url("https://translate.googleapis.com/translate_a/single");
    QUrlQuery urlQuery;

    urlQuery.addQueryItem("client", "gtx");
    urlQuery.addQueryItem("sl", getSourceLang());
    urlQuery.addQueryItem("tl", getTransLang());
    urlQuery.addQueryItem("dt", "t");
    urlQuery.addQueryItem("q", src1);

    urlQuery.setQueryDelimiters('=', '&');

    url.setQuery(urlQuery);

    const QString urlStr = url.toString(QUrl::FullyEncoded);
    qCInfo(glate::shell::lcNetwork) << "online translation" << getSourceLang()
                                    << "->" << getTransLang() << src1.size()
                                    << "characters";

    if (urlStr.isEmpty()) {
      showError("Invalid Input.");
    } else {
      m_request->get(QUrl(urlStr));
      m_translationId = utils::generateRandomId(20);
    }
  }
}

void MainWindow::saveByTransId(const QString &translationId,
                               const QString &reply) {
  QFile jsonFile(QDir(utils::returnPath("cache")).filePath(translationId + ".glate"));
  if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    qWarning() << "Unable to open cache file for writing:" << jsonFile.fileName()
               << jsonFile.errorString();
    return;
  }

  const qint64 written = jsonFile.write(reply.toUtf8());
  if (written < 0) {
    qWarning() << "Unable to write cache file:" << jsonFile.fileName()
               << jsonFile.errorString();
  }

  jsonFile.close();
}

void MainWindow::processTranslation(const QString &reply) {
  m_currentTranslationData.clear();
  const QJsonDocument jsonResponse = QJsonDocument::fromJson(reply.toUtf8());
  if (jsonResponse.isEmpty()) {
    qCWarning(glate::shell::lcNetwork) << "empty translation reply";
    showError(tr("Could not translate"),
              tr("The translation service returned nothing. Try again in a "
                 "moment."),
              QString());
    return;
  }
  QJsonArray jsonArray = jsonResponse.array();
  foreach (const QJsonValue &val, jsonArray) {
    if (val.isArray() && !val.toArray().isEmpty()) {
      foreach (const QJsonValue &val2, val.toArray()) {
        if (val2.isArray() && !val2.toArray().isEmpty() &&
            val2.toArray().count() >= 2) {
          QStringList translationFregments;
          translationFregments << val2.toArray().at(0).toString();
          translationFregments << val2.toArray().at(1).toString();
          m_currentTranslationData.append(translationFregments);
        }
      }
    }
  }
  foreach (QStringList translationFregments, m_currentTranslationData) {
    ui->src2->append(translationFregments.at(0));
  }
  // scroll to top
  ui->src2->verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
  m_resultOffline = m_nextResultOffline;
  m_nextResultOffline = false;
  m_resultPair = tr("%1 to %2").arg(getLangName(getSourceLang()),
                                     getLangName(getTransLang()));
  updateOfflineCaption();
  // history save
  QStringList meta = QStringList()
                     << getLangName(getSourceLang())
                     << getLangName(getTransLang()) << ui->src1->toPlainText()
                     << ui->src2->toPlainText()
                     << QDateTime::currentDateTime().toLocalTime().toString();
  if (m_resultOffline)
    meta << QStringLiteral("offline");
  m_historyWidget->insertItem(meta, false, m_translationId);
  m_settings.setValue(kTranslationCountKey,
                      m_settings.value(kTranslationCountKey, 0).toInt() + 1);
  updateWelcomeCard();
}

QString MainWindow::getSourceLang() const {
  const int indexLang1 = ui->lang1->currentIndex();
  if (indexLang1 == 0) {
    return "auto";
  } else {
    return m_langCode.at(indexLang1 - 1).trimmed();
  }
}

QString MainWindow::getTransLang() const {
  const int indexLang2 = ui->lang2->currentIndex();
  return m_langCode.at(indexLang2).trimmed();
}

void MainWindow::showError(const QString &message) {
  showError(tr("Something went wrong"), QString(), message);
}

void MainWindow::showError(const QString &heading, const QString &explanation,
                           const QString &detail) {
  QString temp = detail;
  temp.replace("translate.googleapis.com", "host");
  qCWarning(glate::shell::lcApp).noquote()
      << "error shown:" << heading << glate::shell::redactUrlQueries(temp);
  m_error = new Error(this);
  m_error->setAttribute(Qt::WA_DeleteOnClose, true);
  m_error->setWindowTitle(QApplication::applicationName() + " | Error");
  m_error->setWindowFlag(Qt::Dialog);
  m_error->setWindowModality(Qt::NonModal);
  m_error->setError(heading, explanation, temp);
  connect(m_error, &Error::reportRequested, this, [this, heading](const QString &d) {
    m_shell->showBugReport(heading + QStringLiteral("\n\n") +
                           glate::shell::redactUrlQueries(d));
  });
  m_error->show();
}

void MainWindow::on_clear_clicked() {
  stopTtsSession(true);
  m_pendingOfflineText = QString();

  ui->src1->clear();
  ui->src2->clear();

  if (m_request != nullptr) {
    m_request->blockSignals(true);
    m_request->deleteLater();
    m_request->blockSignals(false);
    m_request = nullptr;
    m_loader->stop();
  }
  m_currentTranslationData.clear();
  m_translationId.clear();
  m_lineByLine->clearData();
  m_offlineRequest = 0; // drop a result still on its way
}

void MainWindow::on_paste_clicked() const {
  const QMimeData *mimeData = m_clipboard->mimeData();
  if (mimeData->hasText()) {
    ui->src1->setPlainText(mimeData->text());
  }
}

void MainWindow::on_switch_lang_clicked() const {
  const int index1 = ui->lang1->currentIndex();
  const int index2 = ui->lang2->currentIndex();

  ui->lang1->setCurrentIndex(index2 + 1);
  ui->lang2->setCurrentIndex(index1 - 1);

  if (ui->src2->toPlainText().trimmed().isEmpty() == false) {
    const QString src1 = ui->src1->toPlainText();
    const QString src2 = ui->src2->toPlainText();

    ui->src2->setText(src1);
    ui->src1->setText(src2);
  }
}

void MainWindow::on_lang1_currentIndexChanged(int index) {
  ui->switch_lang->setEnabled(index != 0);
  m_settings.setValue("lang1", index);
  m_translate->setToolTip(
      "Translate from " + ui->lang1->itemText(index) + " to " +
      ui->lang2->itemText(ui->lang2->currentIndex()) + "(Shift + Enter)");
}

void MainWindow::on_lang2_currentIndexChanged(int index) {
  m_settings.setValue("lang2", index);
  m_translate->setToolTip("Translate from " +
                          ui->lang1->itemText(ui->lang1->currentIndex()) +
                          " to " + ui->lang2->itemText(index));
}

QString MainWindow::activePlayButtonName() const {
  const QStringList parts = m_player->objectName().split("_");
  return parts.isEmpty() ? QString() : parts.last();
}

void MainWindow::setPlayButtonState(const QString &buttonName, const QIcon &icon,
                                    const QString &toolTip) const {
  if (buttonName == "play1") {
    ui->play1->setIcon(icon);
    ui->play1->setToolTip(toolTip);
  } else if (buttonName == "play2") {
    ui->play2->setIcon(icon);
    ui->play2->setToolTip(toolTip);
  }
}

void MainWindow::resetPlayIcons() {
  setPlayButtonState("play1", QIcon(":/icons/volume-up-line.png"),
                     kPlay1IdleTooltip);
  setPlayButtonState("play2", QIcon(":/icons/volume-up-line.png"),
                     kPlay2IdleTooltip);
}

void MainWindow::stopTtsSession(bool stopPlayer) {
  if (m_ttsDownloader != nullptr) {
    m_ttsDownloader->cancel();
  }

  if (stopPlayer && m_player->playbackState() != QMediaPlayer::StoppedState) {
    m_player->stop();
  }

  m_ttsSessionActive = false;
  m_ttsLocalFiles.clear();
  m_ttsCurrentIndex = -1;
  m_playSelected = false;
  m_selectedText.clear();
  resetPlayIcons();
}

void MainWindow::playCurrentTtsChunk() {
  if (!m_ttsSessionActive || m_ttsCurrentIndex < 0 ||
      m_ttsCurrentIndex >= m_ttsLocalFiles.size()) {
    return;
  }

  const QString filePath = m_ttsLocalFiles.at(m_ttsCurrentIndex);
  if (filePath.isEmpty() || !QFile::exists(filePath)) {
    showError("Unable to play downloaded TTS audio chunk.");
    stopTtsSession(false);
    return;
  }

  m_player->setSource(QUrl::fromLocalFile(filePath));
  m_player->play();
}

void MainWindow::startTtsSession(const QString &playerName, const QString &text,
                                 const QString &ttsLang,
                                 const QString &voiceGender) {
  stopTtsSession(true);

  m_ttsLocalFiles.clear();
  m_ttsCurrentIndex = 0;
  m_ttsSessionActive = true;
  m_player->setObjectName("player_" + playerName);

  if (playerName == "play1") {
    setPlayButtonState("play1", QIcon(":/icons/loader-2-fill.png"),
                       kPreparingTooltip);
    setPlayButtonState("play2", QIcon(":/icons/volume-up-line.png"),
                       kPlay2IdleTooltip);
  } else if (playerName == "play2") {
    setPlayButtonState("play2", QIcon(":/icons/loader-2-fill.png"),
                       kPreparingTooltip);
    setPlayButtonState("play1", QIcon(":/icons/volume-up-line.png"),
                       kPlay1IdleTooltip);
  } else {
    resetPlayIcons();
  }

  m_ttsDownloader->start(text, ttsLang, voiceGender);
}

bool MainWindow::resolveTtsVoiceConfig(const QString &baseLang, QString &ttsLang,
                                       QString &ttsCheckLang,
                                       QString &voiceGender) const {
  const int voiceIdx = m_settings.value("voiceGender", 0).toInt();
  const auto voices = utils::availableVoices();
  const VoiceOption &voiceConf =
      voiceIdx < voices.size() ? voices.at(voiceIdx) : voices.first();

  voiceGender = voiceConf.gender;
  ttsLang = voiceConf.langOverride.isEmpty() ? baseLang : voiceConf.langOverride;
  ttsCheckLang =
      voiceConf.langOverride.isEmpty() ? baseLang
                                       : voiceConf.langOverride.split('-').first();
  return true;
}

void MainWindow::toggleOrStartTts(const QString &buttonName,
                                  const QString &baseLang,
                                  const QString &displayLang,
                                  const QString &text) {
  const QString activeButton = activePlayButtonName();
  const bool isPlaying =
      m_player->playbackState() == QMediaPlayer::PlayingState;

  // Toggle off, never consumes a play credit.
  if (isPlaying && activeButton == buttonName) {
    stopTtsSession(true);
    return;
  }

  if (isPlaying && activeButton != buttonName) {
    stopTtsSession(true);
  }

  // Check free-tier daily TTS limit before doing any work.
  if (!LicenseService::instance()->checkTtsAccess())
    return;

  QString ttsLang;
  QString ttsCheckLang;
  QString voiceGender;
  resolveTtsVoiceConfig(baseLang, ttsLang, ttsCheckLang, voiceGender);

  if (!m_supportedTts.contains(ttsCheckLang, Qt::CaseInsensitive)) {
    showError("Selected language '" + displayLang +
              "' is not supported by TTS Engine.\nPlease choose different "
              "Language.");
    return;
  }

  startTtsSession(buttonName, text, ttsLang, voiceGender);
}

void MainWindow::on_play2_clicked() {
  QString text = ui->src2->toPlainText();
  if (m_playSelected)
    text = m_selectedText;

  toggleOrStartTts("play2", getTransLang(), getTransLang(), text);
}

void MainWindow::on_play1_clicked() {
  QString text = ui->src1->toPlainText();
  if (m_playSelected)
    text = m_selectedText;

  toggleOrStartTts("play1", getSourceLang(), getSourceLang(), text);
}

void MainWindow::on_src1_textChanged() const {
  ui->counter->setText(QString::number(ui->src1->toPlainText().length()) +
                       "/5000");
  if (ui->src1->toPlainText().length() > 5000) {
    const int diff = ui->src1->toPlainText().length() - 5000;
    QString newStr = ui->src1->toPlainText();
    newStr.chop(diff);
    ui->src1->setText(newStr);
    QTextCursor cursor(ui->src1->textCursor());
    cursor.movePosition(QTextCursor::End, QTextCursor::MoveAnchor);
    ui->src1->setTextCursor(cursor);
  }
  ui->play1->setEnabled(!ui->src1->toPlainText().isEmpty());
}

void MainWindow::on_src2_textChanged() const {
  // enable disable play button if text is not empty
  ui->play2->setEnabled(!ui->src2->toPlainText().isEmpty());
}

void MainWindow::on_copy_clicked() const {
  const QString text = ui->src2->toPlainText();
  if (!text.isEmpty()) {
    m_clipboard->setText(text);
  }
}

void MainWindow::on_share_clicked() {
  m_share->setTranslation(ui->src2->toPlainText(), m_translationId,
                          getTransLang());
  if (m_share->isVisible() == false) {
    m_share->showNormal();
  }
}

void MainWindow::on_settings_clicked() const {
  if (m_settingsWidget->isVisible() == false) {
    m_settingsWidget->showNormal();
  }
}

void MainWindow::on_history_clicked() const {
  if (m_historyWidget->isVisible() == false) {
    m_historyWidget->loadHistory();
    m_historyWidget->showNormal();
  }
}

void MainWindow::on_lineByline_clicked() {
  if (m_translationId.isEmpty()) {
    showError("Translate something first.");
    return;
  }
  if (m_lineByLine->isVisible() == false) {
    if (m_translationId != m_lineByLine->getTId()) {
      m_lineByLine->clearData();
      if (m_currentTranslationData.isEmpty() == false) {
        m_lineByLine->setData(m_currentTranslationData,
                              getLangName(getSourceLang()),
                              getLangName(getTransLang()), m_translationId);
      } else {
        // load translation from cache;
        QFile jsonFile(QDir(utils::returnPath("cache")).filePath(m_translationId + ".glate"));
        if (!jsonFile.open(QFile::ReadOnly)) {
          showError("Unable to open cached translation file.");
          return;
        }

        m_currentTranslationData.clear();
        processTranslation(QString::fromUtf8(jsonFile.readAll()));
        jsonFile.close();

        m_lineByLine->setData(m_currentTranslationData, getLangName(getSourceLang()),
                              getLangName(getTransLang()), m_translationId);
      }
    }
    m_lineByLine->showNormal();
  }
}

// convert lang code to lang name.
QString MainWindow::getLangName(const QString &langCode) const {
  if (langCode == "auto") {
    return "Auto Detected";
  } else {
    QStringList langNameList = m_langName;
    langNameList.removeAt(0); // remove auto detect
    QString langName = langNameList.at(m_langCode.lastIndexOf(langCode) - 1);
    return langName;
  }
}

// not tested not being used yet
QString MainWindow::getLangCode(const QString &langName) const {
  if (langName.contains("auto", Qt::CaseInsensitive)) {
    return "auto";
  } else {
    const QStringList langCodeList = m_langCode;
    QString langCode = langCodeList.at(m_langName.lastIndexOf(langName) - 1);
    return langCode;
  }
}

void MainWindow::on_help_clicked() {
  // Right-aligned under the button, as in the mock.
  const int x = ui->help->width() - m_helpMenu->sizeHint().width();
  m_helpMenu->popup(ui->help->mapToGlobal(QPoint(x, ui->help->height() + 4)));
}

void MainWindow::showTour() {
  if (m_slider == nullptr) {
    m_slider = new Slider(this);
    m_slider->setWindowTitle(QApplication::applicationName() +
                             "  Introduction");
    m_slider->setWindowFlag(Qt::Dialog);
    m_slider->setMinimumSize(680, 420);
  }
  if (m_slider->isVisible() == false) {
    m_slider->showNormal();
    m_slider->start();
  }
}

void MainWindow::on_about_clicked() { m_shell->showAbout(); }

// ── Account / licence ─────────────────────────────────────────────────────────

void MainWindow::on_account_clicked() const {
    if (m_accountDialog->isVisible()) {
        m_accountDialog->raise();
        m_accountDialog->activateWindow();
        return;
    }
    m_accountDialog->showNormal();
}

void MainWindow::onLicenseStatusUpdated(
    const AccountLicenseManager::LicenseStatus &status)
{
    m_hasPremiumAccess = status.hasPaidAccess;

    // Keep the centralized gate in sync, all Pro-gated features read from here.
    LicenseService::instance()->setHasPaidAccess(status.hasPaidAccess);

    // Let the upgrade dialog know whether the user has had a trial so it can
    // show contextually appropriate copy ("eval ended" vs "try for free").
    using ES = LicenseService::EvaluationState;
    const ES evalState = status.evaluationActive
        ? ES::Active
        : (!status.active && status.evaluationEndTimestamp > 0)
              ? ES::Expired
              : ES::Unknown;
    LicenseService::instance()->setEvaluationState(evalState);
    LicenseService::instance()->setEvaluationDurationDays(m_licenseManager->config().evaluationDurationSecs / 86400);

    qCInfo(glate::shell::lcLicense)
        << "licence status:"
        << (status.active           ? "Pro"
            : status.evaluationActive ? "Evaluation"
                                      : "Free");
    m_licenseKnown = true;
    refreshOfflineIndicator();
    if (!m_pendingOfflineText.isNull()) {
        const QString pending = std::exchange(m_pendingOfflineText, QString());
        m_loader->stop();
        translateOffline(pending);
    }
}

// ── Offline translation ─────────────────────────────────────────────────────

void MainWindow::initOffline() {
  using namespace glate::offline;
  m_modelStore = new ModelStore(utils::returnPath("models"));
  m_modelManager =
      new ModelManager(m_modelStore, ModelManager::defaultCatalogUrl(), this);
  m_offline = new OfflineTranslator(m_modelStore, this);

  const bool available = TranslationEngine::isAvailable();
  m_settingsWidget->setOfflineAvailable(available);
  if (!available)
    m_settingsWidget->setOfflineModeChecked(false);

  connect(m_settingsWidget, &Settings::offlineLanguagesRequested, this,
          [this]() { showOfflineLanguages(); });
  connect(m_settingsWidget, &Settings::offlineModeChanged, this,
          [this](bool enabled) {
            qCInfo(glate::shell::lcOffline) << "offline mode" << (enabled ? "on" : "off");
            refreshOfflineIndicator();
            // Turning it on with nothing downloaded: show what can be downloaded.
            if (enabled && m_modelStore->installed().isEmpty())
              showOfflineLanguages();
          });
  auto modelsChanged = [this](const QString &id) {
    qCInfo(glate::shell::lcOffline) << "models changed:" << id;
    m_offline->reloadModels();
    updateOfflineSummary();
  };
  connect(m_modelManager, &ModelManager::installed, this, modelsChanged);
  connect(m_modelManager, &ModelManager::removed, this, modelsChanged);

  connect(m_offline, &OfflineTranslator::finished, this,
          [this](quint64 requestId, const QList<QStringList> &pairs) {
            if (requestId != m_offlineRequest)
              return;
            m_offlineRequest = 0;
            m_loader->stop();
            // Same shape as Google's reply, so display, history and the
            // line-by-line view work unchanged.
            QJsonArray sentences;
            for (const QStringList &pair : pairs)
              sentences.append(QJsonArray{pair.value(0), pair.value(1)});
            const QString reply = QString::fromUtf8(
                QJsonDocument(QJsonArray{sentences}).toJson(QJsonDocument::Compact));
            m_translationId = utils::generateRandomId(20);
            saveByTransId(m_translationId, reply);
            m_nextResultOffline = true;
            processTranslation(reply);
            m_settings.setValue("lastTranslation", m_translationId);
          });
  connect(m_offline, &OfflineTranslator::failed, this,
          [this](quint64 requestId, const QString &message) {
            if (requestId != m_offlineRequest && requestId != 0)
              return;
            m_offlineRequest = 0;
            m_loader->stop();
            qCWarning(glate::shell::lcOffline).noquote() << "translation failed:" << message;
            showError(tr("Could not translate offline"), QString(), message);
          });

  updateOfflineSummary();
}

bool MainWindow::offlineModeEnabled() const {
  return glate::offline::TranslationEngine::isAvailable() &&
         m_settings.value(Settings::kKeyOfflineMode, false).toBool();
}

void MainWindow::translateOffline(const QString &text) {
  using glate::offline::TranslationPlan;
  // Right after start the licence status is not known yet; wait for it (up to
  // 5 s) instead of asking a Pro or evaluation user to upgrade.
  if (!m_licenseKnown) {
    const bool waiting = !m_pendingOfflineText.isNull();
    m_pendingOfflineText = text;
    m_loader->start();
    if (!waiting)
      QTimer::singleShot(5000, this, [this]() {
        if (m_pendingOfflineText.isNull())
          return;
        m_licenseKnown = true; // no answer: let checkAccess() decide from what it has
        const QString pending = std::exchange(m_pendingOfflineText, QString());
        m_loader->stop();
        translateOffline(pending);
      });
    return;
  }
  // The licence may have lapsed since offline mode was turned on. Never fall
  // back to the online service silently: the user expects text to stay local.
  if (!LicenseService::instance()->checkAccess(tr("Offline translation")))
    return;

  const QString from = getSourceLang();
  const QString to = getTransLang();
  const TranslationPlan plan = m_offline->plan(from, to);

  if (plan.status == TranslationPlan::Status::MissingModels) {
    QStringList codes;
    for (const auto &direction : plan.missing) {
      const QString side = direction.from == glate::offline::kPivotLanguage
                               ? direction.to
                               : direction.from;
      const QString code = glate::offline::toGlateCode(side);
      if (!codes.contains(code))
        codes.append(code);
    }
    QStringList names;
    for (const QString &code : codes)
      names.append(getLangName(code));
    QMessageBox box(this);
    box.setWindowTitle(tr("Offline translation"));
    box.setIcon(QMessageBox::Information);
    box.setText(tr("To translate offline, download: %1.").arg(names.join(", ")));
    box.setInformativeText(
        tr("Downloads happen once; after that this pair works without internet."));
    QPushButton *download = box.addButton(tr("Download"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();
    if (box.clickedButton() == download)
      showOfflineLanguages(codes);
    return;
  }
  if (plan.status == TranslationPlan::Status::NeedsSourceLanguage) {
    QMessageBox::information(
        this, tr("Offline translation"),
        tr("Choose the language you are translating from. Detecting it "
           "automatically needs the online service."));
    return;
  }

  qCInfo(glate::shell::lcOffline) << "offline translation" << from << "->" << to
                                  << text.size() << "characters";
  m_loader->start();
  m_offlineRequest = m_offline->translate(from, to, text);
  if (m_offlineRequest == 0)
    m_loader->stop(); // failed() follows with the reason
}

void MainWindow::showOfflineLanguages(const QStringList &downloadCodes) {
  if (m_offlineDialog == nullptr) {
    QHash<QString, QString> names;
    for (int i = 0; i < m_langCode.size(); ++i)
      names.insert(m_langCode.at(i).trimmed(), m_langName.value(i).trimmed());
    m_offlineDialog =
        new OfflineModelsDialog(m_modelManager, m_modelStore, names, this);
  }
  if (downloadCodes.isEmpty()) {
    m_offlineDialog->show();
    m_offlineDialog->raise();
    m_offlineDialog->activateWindow();
  } else {
    m_offlineDialog->downloadLanguages(downloadCodes);
  }
}

void MainWindow::updateOfflineSummary() {
  qint64 bytes = 0;
  QStringList languages;
  for (const auto &model : m_modelStore->installed()) {
    bytes += model.size;
    for (const QString &side : {model.from, model.to}) {
      if (side != glate::offline::kPivotLanguage && !languages.contains(side))
        languages.append(side);
    }
  }
  const QString size = QLocale().formattedDataSize(bytes, 1);
  QString summary;
  if (languages.isEmpty())
    summary = tr("No languages downloaded");
  else if (languages.size() == 1)
    summary = tr("1 language, %1").arg(size);
  else
    summary = tr("%1 languages, %2").arg(languages.size()).arg(size);
  m_settingsWidget->setOfflineSummary(summary);
}

// ── App shell ───────────────────────────────────────────────────────────────

void MainWindow::initShell() {
  m_shell = new GlateShell(this, [this]() { return diagnosticsLines(); });
  m_helpMenu = m_shell->createHelpMenu(this, [this]() { showTour(); });
  auto *keys = new QShortcut(QKeySequence(Qt::Key_F1), this);
  keys->setContext(Qt::ApplicationShortcut);
  connect(keys, &QShortcut::activated, m_shell, &GlateShell::showShortcuts);

  // The "Offline" pill takes the top bar's centre, where an empty logo label sat.
  m_offlinePill = new glate::shell::StatusPill(
      tr("Offline"), QStringLiteral(":/icons/macbook-line.png"), this);
  m_offlinePill->setObjectName(QStringLiteral("offlinePill"));
  m_offlinePill->setToolTip(tr("Offline mode is on: translations happen on this "
                               "computer. Click to manage languages."));
  connect(m_offlinePill, &QAbstractButton::clicked, this,
          [this]() { showOfflineLanguages(); });
  const int logoIndex = ui->horizontalLayout_3->indexOf(ui->label);
  ui->label->hide();
  ui->horizontalLayout_3->insertWidget(logoIndex, m_offlinePill);

  // "Translated on this device" under an offline result.
  m_offlineCaption = new QLabel(ui->src2->viewport());
  m_offlineCaption->setObjectName(QStringLiteral("offlineCaption"));
  m_offlineCaption->setAttribute(Qt::WA_TransparentForMouseEvents);
  m_offlineCaption->hide();

  // First run: a card in the empty result pane instead of a popup.
  m_welcome = new glate::shell::WelcomeCard(
      tr("Welcome to Glate"),
      tr("Translate text and listen to it, online or on this computer."),
      {{QStringLiteral(":/icons/text.png"),
        tr("Type or paste on the left, then press <kbd>Shift</kbd> + <kbd>Enter</kbd> or "
           "the arrow.")},
       {QStringLiteral(":/icons/arrow-left-right-line.png"),
        tr("Pick both languages below; the middle button swaps them.")},
       {QStringLiteral(":/icons/macbook-line.png"),
        tr("Turn on offline mode in Settings to translate without internet "
           "(Pro).")}},
      tr("Take the tour"), QStringLiteral(":/icons/sparkling-line.png"),
      ui->src2->viewport());
  m_resultPlaceholder = ui->src2->placeholderText();
  connect(m_welcome, &glate::shell::WelcomeCard::buttonClicked, this,
          [this]() { showTour(); });

  ui->src2->viewport()->installEventFilter(this);
  connect(ui->src1, &QTextEdit::textChanged, this, [this]() { updateWelcomeCard(); });
  connect(ui->src2, &QTextEdit::textChanged, this, [this]() {
    if (ui->src2->document()->isEmpty())
      m_resultOffline = false;
    updateOfflineCaption();
    updateWelcomeCard();
  });
  if (m_theme != nullptr)
    connect(m_theme, &ThemeController::effectiveModeChanged, this,
            [this]() { refreshOfflineIndicator(); updateOfflineCaption(); });

  refreshOfflineIndicator();
  updateOfflineCaption();
  updateWelcomeCard();
}

QStringList MainWindow::diagnosticsLines() const {
  QStringList languages;
  if (m_modelStore != nullptr) {
    for (const auto &model : m_modelStore->installed()) {
      for (const QString &side : {model.from, model.to}) {
        if (side != glate::offline::kPivotLanguage && !languages.contains(side))
          languages.append(side);
      }
    }
  }
  languages.sort();
  const bool engine = glate::offline::TranslationEngine::isAvailable();
  return {
      QStringLiteral("Offline engine: %1, mode %2, %3 language(s)%4")
          .arg(engine ? QStringLiteral("available") : QStringLiteral("not built"),
               offlineModeEnabled() ? QStringLiteral("on") : QStringLiteral("off"))
          .arg(languages.size())
          .arg(languages.isEmpty() ? QString()
                                   : QStringLiteral(" (%1)").arg(languages.join(", "))),
      QStringLiteral("UI language: %1, theme: %2 %3")
          .arg(m_settings.value("ui_language", "system").toString(),
               m_settings.value("appearance/skin", "fusion2").toString(),
               m_settings.value("appearance/mode", "system").toString()),
  };
}

void MainWindow::refreshOfflineIndicator() {
  if (m_offlinePill == nullptr)
    return;
  // Hidden once the licence is known to have lapsed: translating then asks to upgrade.
  const bool on = offlineModeEnabled() && (!m_licenseKnown || m_hasPremiumAccess);
  m_offlinePill->setVisible(on);

  QPixmap arrow = QIcon(":/icons/arrow-right-circle-line.png").pixmap(64, 64);
  if (on) {
    QPainter painter(&arrow);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(palette().color(QPalette::Window), 4));
    painter.setBrush(glate::shell::offlineColor(this));
    painter.drawEllipse(QRectF(42, 4, 18, 18));
  }
  m_translate->setIcon(QIcon(arrow));
  m_translate->setToolTip(on ? tr("Translate offline (Shift + Enter)")
                             : tr("Translate (Shift + Enter)"));
}

void MainWindow::updateOfflineCaption() {
  if (m_offlineCaption == nullptr)
    return;
  const bool show = m_resultOffline && !ui->src2->document()->isEmpty();
  if (show) {
    const QColor muted = glate::shell::mutedColor(this);
    const QColor green = glate::shell::offlineColor(this);
    const QPixmap icon = glate::shell::tintedIcon(
        QStringLiteral(":/icons/macbook-line.png"), green).pixmap(14, 14);
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    icon.save(&buffer, "PNG");
    m_offlineCaption->setText(
        QStringLiteral("<img src=\"data:image/png;base64,%1\" width=14 height=14> "
                       "<span style=\"color:%2\">%3</span>")
            .arg(QString::fromLatin1(png.toBase64()), muted.name(),
                 tr("Translated on this device, %1").arg(m_resultPair).toHtmlEscaped()));
  }
  m_offlineCaption->setVisible(show);
  positionPaneOverlays();
}

void MainWindow::updateWelcomeCard() {
  if (m_welcome == nullptr)
    return;
  const bool show = ui->src1->document()->isEmpty() &&
                    ui->src2->document()->isEmpty() &&
                    m_settings.value(kTranslationCountKey, 0).toInt() <
                        kWelcomeTranslations;
  m_welcome->setVisible(show);
  // The card sits on the pane; its placeholder would show through.
  ui->src2->setPlaceholderText(show ? QString() : m_resultPlaceholder);
  positionPaneOverlays();
}

void MainWindow::positionPaneOverlays() {
  const QRect area = ui->src2->viewport()->rect();
  // isHidden(), not isVisible(): before the window is shown nothing is visible.
  if (m_welcome != nullptr && !m_welcome->isHidden())
    m_welcome->setGeometry(area);
  if (m_offlineCaption != nullptr && !m_offlineCaption->isHidden()) {
    m_offlineCaption->adjustSize();
    m_offlineCaption->move(area.left() + 12,
                           area.bottom() - m_offlineCaption->height() - 8);
    m_offlineCaption->raise();
  }
}

// ── Developer aid ───────────────────────────────────────────────────────────
//
//   GLATE_DEBUG_WINDOW_SIZE=WxH        size of the main window
//   GLATE_DEBUG_LANGS=from:to          glate codes, "auto" allowed for from
//   GLATE_DEBUG_TEXT=<text>            source text, translated once the window is up
//   GLATE_DEBUG_OFFLINE=0|1            offline mode on or off for this run
//   GLATE_DEBUG_OPEN=settings|history|linebyline|offline|share|about|account|
//                    whatsnew|bugreport|shortcuts|helpmenu|error
//   GLATE_DEBUG_ABOUT_TAB=<n>            with OPEN=about, the tab to show
//   GLATE_DEBUG_GRAB=<png>[,<seconds>] saves the opened screen (or the main
//                                      window) after the delay, then quits

void MainWindow::debugRun() {
  const QString size = qEnvironmentVariable("GLATE_DEBUG_WINDOW_SIZE");
  if (size.contains('x'))
    resize(size.section('x', 0, 0).toInt(), size.section('x', 1, 1).toInt());

  if (qEnvironmentVariableIsSet("GLATE_DEBUG_OFFLINE")) {
    m_settingsWidget->setOfflineModeChecked(
        qEnvironmentVariable("GLATE_DEBUG_OFFLINE") == QStringLiteral("1"));
    refreshOfflineIndicator(); // setOfflineModeChecked() emits nothing
  }

  const QStringList langs = qEnvironmentVariable("GLATE_DEBUG_LANGS").split(':');
  if (langs.size() == 2) {
    const int from = langs.at(0) == QStringLiteral("auto")
                         ? 0
                         : int(m_langCode.indexOf(langs.at(0))) + 1;
    const int to = int(m_langCode.indexOf(langs.at(1)));
    if (from >= 0)
      ui->lang1->setCurrentIndex(from);
    if (to >= 0)
      ui->lang2->setCurrentIndex(to);
  }

  const QString text = qEnvironmentVariable("GLATE_DEBUG_TEXT");
  if (!text.isEmpty()) {
    ui->src1->setPlainText(text);
    QTimer::singleShot(500, this, [this]() { translate_clicked(); });
  }

  QWidget *target = this;
  const QString open = qEnvironmentVariable("GLATE_DEBUG_OPEN");
  if (open == QStringLiteral("settings"))
    target = m_settingsWidget;
  else if (open == QStringLiteral("history"))
    target = m_historyWidget;
  else if (open == QStringLiteral("linebyline"))
    target = m_lineByLine;
  else if (open == QStringLiteral("share"))
    target = m_share;
  else if (open == QStringLiteral("account"))
    target = m_accountDialog;
  if (!open.isEmpty()) {
    // After the translation has had time to land.
    QTimer::singleShot(3000, this, [this, open]() {
      if (open == QStringLiteral("settings"))
        on_settings_clicked();
      else if (open == QStringLiteral("history"))
        on_history_clicked();
      else if (open == QStringLiteral("linebyline"))
        on_lineByline_clicked();
      else if (open == QStringLiteral("share"))
        on_share_clicked();
      else if (open == QStringLiteral("about")) {
        on_about_clicked();
        if (auto *tabs = m_shell->aboutDialog()->findChild<QTabWidget *>(
                QStringLiteral("aboutTabs")))
          tabs->setCurrentIndex(
              qEnvironmentVariableIntValue("GLATE_DEBUG_ABOUT_TAB"));
      }
      else if (open == QStringLiteral("account"))
        on_account_clicked();
      else if (open == QStringLiteral("offline"))
        showOfflineLanguages();
      else if (open == QStringLiteral("whatsnew"))
        m_shell->showWhatsNew();
      else if (open == QStringLiteral("bugreport"))
        m_shell->showBugReport();
      else if (open == QStringLiteral("shortcuts"))
        m_shell->showShortcuts();
      else if (open == QStringLiteral("helpmenu"))
        on_help_clicked();
      else if (open == QStringLiteral("error"))
        showError(tr("Could not translate"),
                  tr("The translation service did not answer. Check your "
                     "connection and try again."),
                  QStringLiteral("Error transferring https://host/translate_a/single - "
                                 "server replied: Service Unavailable"));
    });
  }

  if (qEnvironmentVariableIsSet("GLATE_DEBUG_GRAB")) {
    const QStringList spec = qEnvironmentVariable("GLATE_DEBUG_GRAB").split(',');
    const int delayMs = spec.size() > 1 ? spec.at(1).toInt() * 1000 : 6000;
    QTimer::singleShot(delayMs, this, [this, spec, open, target]() {
      QWidget *shot = target;
      if (open == QStringLiteral("offline"))
        shot = m_offlineDialog;
      else if (open == QStringLiteral("about"))
        shot = m_shell->aboutDialog();
      else if (open == QStringLiteral("whatsnew"))
        shot = m_shell->whatsNewDialog();
      else if (open == QStringLiteral("bugreport"))
        shot = m_shell->bugReportDialog();
      else if (open == QStringLiteral("shortcuts"))
        shot = m_shell->shortcutsDialog();
      else if (open == QStringLiteral("error"))
        shot = m_error;
      if (shot == nullptr)
        shot = this;
      if (open == QStringLiteral("helpmenu")) {
        // A popup is its own window: paint the main window with the menu on it.
        QPixmap canvas = grab();
        QPainter painter(&canvas);
        painter.drawPixmap(mapFromGlobal(m_helpMenu->pos()), m_helpMenu->grab());
        painter.end();
        canvas.save(spec.first());
        QApplication::quit();
        return;
      }
      // The top-level window, so a custom title bar comes along.
      shot->window()->grab().save(spec.first());
      QApplication::quit();
    });
  }
}
