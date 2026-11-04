#include "settings.h"

#include "licenseservice.h"
#include "utils.h"

#include <AppTheme.h>
#include <ShellStyle.h>
#include <ThemeController.h>
#include <WindowChrome.h>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

namespace {
// Fusion 2 is the only theme; main.cpp moves the old appearance/mode here.
constexpr auto kKeyMode = "theme/mode";        // system | light | dark
constexpr auto kKeyCsd = "appearance/csd";     // bool
constexpr auto kKeyUiLanguage = "ui_language"; // system | locale code

const QStringList kModes = {QStringLiteral("system"), QStringLiteral("light"),
                            QStringLiteral("dark")};

bool isDark(const QWidget *widget) {
  return widget->palette().color(QPalette::Window).lightness() < 128;
}
} // namespace

Settings::Settings(QWidget *parent, glate::shortcut::GlobalShortcut *shortcut,
                   ThemeController *theme)
    : QWidget(parent), m_shortcut(shortcut), m_theme(theme) {
  setWindowTitle(tr("Glate | Settings"));
  setMinimumWidth(460);
  resize(540, 660);
  auto *esc = new QShortcut(Qt::Key_Escape, this);
  connect(esc, &QShortcut::activated, this, [this] { close(); });

  // The page scrolls when the window is shorter than it.
  auto *scroll = new QScrollArea(this);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidgetResizable(true);
  scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  auto *pageWidget = new QWidget(scroll);
  pageWidget->setObjectName(QStringLiteral("settingsPage"));
  auto *page = new QVBoxLayout(pageWidget);
  page->setContentsMargins(20, 16, 20, 20);
  page->setSpacing(6);
  buildTranslation(page);
  buildQuickTranslate(page);
  buildAppearance(page);
  buildGeneral(page);
  page->addStretch(1);
  scroll->setWidget(pageWidget);

  auto *outer = new QVBoxLayout(this);
  outer->setContentsMargins(0, 0, 0, 0);
  outer->addWidget(scroll);

  applyStyles();
  updateShortcutRow();
}

// ── Building blocks ─────────────────────────────────────────────────────────

QVBoxLayout *Settings::addSection(QVBoxLayout *page, const QString &title) {
  auto *heading = new QLabel(title.toUpper(), this);
  glate::shell::setRelativeFont(heading, 0.85, true);
  glate::shell::makeMuted(heading);
  heading->setContentsMargins(2, page->count() == 0 ? 0 : 14, 0, 4);
  page->addWidget(heading);

  auto *card = new QFrame(this);
  card->setObjectName(QStringLiteral("settingsCard"));
  m_cards.append(card);
  auto *rows = new QVBoxLayout(card);
  rows->setContentsMargins(0, 0, 0, 0);
  rows->setSpacing(0);
  page->addWidget(card);
  return rows;
}

QWidget *Settings::addRow(QVBoxLayout *card, const QString &title,
                          const QString &hint, QWidget *control,
                          QLabel **hintOut) {
  auto *row = new QWidget(this);
  row->setObjectName(QStringLiteral("settingsRow"));
  // A line above every row but the first of its card.
  if (card->count() > 0)
    row->setProperty("separated", true);
  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(14, 10, 14, 10);
  layout->setSpacing(12);

  auto *text = new QVBoxLayout;
  text->setSpacing(2);
  auto *titleLabel = new QLabel(title, row);
  titleLabel->setWordWrap(true);
  text->addWidget(titleLabel);
  QLabel *hintLabel = nullptr;
  if (!hint.isEmpty() || hintOut != nullptr) {
    hintLabel = new QLabel(hint, row);
    hintLabel->setWordWrap(true);
    glate::shell::setRelativeFont(hintLabel, 0.9);
    glate::shell::makeMuted(hintLabel);
    text->addWidget(hintLabel);
  }
  layout->addLayout(text, 1);
  if (control != nullptr)
    layout->addWidget(control, 0, Qt::AlignVCenter);
  card->addWidget(row);
  if (hintOut != nullptr)
    *hintOut = hintLabel;
  return row;
}

QWidget *Settings::segmented(const QStringList &labels, QButtonGroup *group) {
  auto *holder = new QWidget(this);
  holder->setObjectName(QStringLiteral("segmented"));
  m_segments.append(holder);
  auto *layout = new QHBoxLayout(holder);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  for (int i = 0; i < labels.size(); ++i) {
    auto *button = new QPushButton(labels.at(i), holder);
    button->setCheckable(true);
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("segment", i == 0                   ? "first"
                                   : i == labels.size() - 1 ? "last"
                                                            : "middle");
    group->addButton(button, i);
    layout->addWidget(button);
  }
  group->setExclusive(true);
  return holder;
}

// ── Sections ────────────────────────────────────────────────────────────────

void Settings::buildTranslation(QVBoxLayout *page) {
  QVBoxLayout *card = addSection(page, tr("Translation"));

  // Translate offline, with a PRO tag after the title.
  m_offlineCheckBox = new QCheckBox(this);
  m_offlineCheckBox->setToolTip(tr("Pro feature"));
  QWidget *offlineRow = addRow(
      card, tr("Translate offline"),
      tr("On this computer, with downloaded languages. Your text never "
         "leaves your device."),
      m_offlineCheckBox);
  auto *textColumn =
      qobject_cast<QVBoxLayout *>(offlineRow->layout()->itemAt(0)->layout());
  if (textColumn != nullptr) {
    QWidget *title = textColumn->itemAt(0)->widget();
    textColumn->removeWidget(title);
    auto *titleLine = new QHBoxLayout;
    titleLine->setSpacing(6);
    auto *badge = new QLabel(tr("PRO"), offlineRow);
    glate::shell::setRelativeFont(badge, 0.75, true);
    m_badges.append(badge);
    titleLine->addWidget(title);
    titleLine->addWidget(badge);
    titleLine->addStretch(1);
    textColumn->insertLayout(0, titleLine);
  }
  m_offlineCheckBox->setChecked(settings.value(kKeyOfflineMode, false).toBool());
  connect(m_offlineCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
    if (checked &&
        !LicenseService::instance()->checkAccess(tr("Offline translation"))) {
      const QSignalBlocker block(m_offlineCheckBox);
      m_offlineCheckBox->setChecked(false);
      return;
    }
    settings.setValue(kKeyOfflineMode, checked);
    emit offlineModeChanged(checked);
  });

  auto *manage = new QPushButton(tr("Manage languages..."), this);
  connect(manage, &QPushButton::clicked, this,
          &Settings::offlineLanguagesRequested);
  QWidget *languagesRow =
      addRow(card, tr("Offline languages"), QString(), manage, &m_offlineSummary);
  m_offlineSummary->hide();
  m_offlineRows = {offlineRow, languagesRow};

  m_voiceCombo = new QComboBox(this);
  m_voiceCombo->setFixedWidth(200);
  for (const VoiceOption &voice : utils::availableVoices())
    m_voiceCombo->addItem(voice.displayName);
  m_voiceCombo->setCurrentIndex(settings.value("voiceGender", 0).toInt());
  connect(m_voiceCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) { settings.setValue("voiceGender", index); });
  addRow(card, tr("Reading voice"), tr("Used when Glate reads text aloud."),
         m_voiceCombo);
}

void Settings::buildQuickTranslate(QVBoxLayout *page) {
  QVBoxLayout *card = addSection(page, tr("Quick Translate"));

  m_quickCheckBox = new QCheckBox(this);
  addRow(card, tr("Translate selected text from any app"),
         tr("Select text anywhere, then press the keys."), m_quickCheckBox);

  auto *keys = new QWidget(this);
  keys->setObjectName(QStringLiteral("settingsControl"));
  auto *keysLayout = new QHBoxLayout(keys);
  keysLayout->setContentsMargins(0, 0, 0, 0);
  keysLayout->setSpacing(8);
  m_keyEdit = new QKeySequenceEdit(keys);
  m_keyEdit->setMinimumWidth(150);
  m_shortcutButton = new QPushButton(keys);
  keysLayout->addWidget(m_keyEdit);
  keysLayout->addWidget(m_shortcutButton);
  addRow(card, tr("Keys"), QString(), keys);

  // The state line runs the full width under the keys.
  m_shortcutHint = new QLabel(this);
  m_shortcutHint->setObjectName(QStringLiteral("settingsRow"));
  m_shortcutHint->setWordWrap(true);
  m_shortcutHint->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_shortcutHint->setContentsMargins(14, 0, 14, 10);
  glate::shell::setRelativeFont(m_shortcutHint, 0.9);
  card->addWidget(m_shortcutHint);

  // Quick Translate: on by default, Ctrl+Shift+Space.
  QKeySequence keySequence(settings.value("quicktrans_shortcut").toString());
  if (keySequence.isEmpty())
    keySequence = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space);
  const bool quick = settings.value("quicktrans", true).toBool();
  m_keyEdit->setKeySequence(keySequence);
  m_quickCheckBox->setChecked(quick);
  if (m_shortcut != nullptr) {
    m_shortcut->setKeys(keySequence);
    m_shortcut->setEnabled(quick);
    connect(m_shortcut, &glate::shortcut::GlobalShortcut::stateChanged, this,
            &Settings::updateShortcutRow);
  }

  connect(m_quickCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    settings.setValue("quicktrans", on);
    if (m_shortcut != nullptr)
      m_shortcut->setEnabled(on);
    updateShortcutRow();
  });
  connect(m_keyEdit, &QKeySequenceEdit::keySequenceChanged, this,
          &Settings::setShortcut);
  connect(m_shortcutButton, &QPushButton::clicked, this, [this] {
    if (m_shortcut == nullptr)
      return;
    if (m_shortcut->canConfigure())
      m_shortcut->configure();
    else
      m_shortcut->retry();
  });
}

void Settings::buildAppearance(QVBoxLayout *page) {
  QVBoxLayout *card = addSection(page, tr("Appearance"));

  m_modeGroup = new QButtonGroup(this);
  QWidget *modes =
      segmented({tr("System"), tr("Light"), tr("Dark")}, m_modeGroup);
  const int mode =
      kModes.indexOf(settings.value(kKeyMode, "system").toString());
  m_modeGroup->button(qMax(0, mode))->setChecked(true);
  connect(m_modeGroup, &QButtonGroup::idClicked, this, [this](int id) {
    const QString value = kModes.value(id, QStringLiteral("system"));
    settings.setValue(kKeyMode, value);
    if (m_theme != nullptr)
      m_theme->setMode(AppTheme::modeFromString(value));
  });
  addRow(card, tr("Theme"), QString(), modes);

  m_csdCheckBox = new QCheckBox(this);
  m_csdCheckBox->setChecked(
      settings.value(kKeyCsd, WindowChrome::defaultEnabled()).toBool());
  connect(m_csdCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    settings.setValue(kKeyCsd, on);
    promptRestartRequiredDialog();
  });
  addRow(card, tr("Use Glate's own title bar"), tr("Applies after a restart."),
         m_csdCheckBox);
}

void Settings::buildGeneral(QVBoxLayout *page) {
  QVBoxLayout *card = addSection(page, tr("General"));

  m_languageCombo = new QComboBox(this);
  m_languageCombo->setFixedWidth(200);
  m_languageCombo->addItem(tr("Follow system"), QStringLiteral("system"));
  m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en_IN"));
  m_languageCombo->addItem(QStringLiteral("Deutsch"), QStringLiteral("de_DE"));
  m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr_FR"));
  m_languageCombo->addItem(QStringLiteral("Português (Brasil)"),
                           QStringLiteral("pt_BR"));
  m_languageCombo->addItem(QStringLiteral("Italiano"), QStringLiteral("it_IT"));
  m_languageCombo->setCurrentIndex(qMax(
      0, m_languageCombo->findData(
             settings.value(kKeyUiLanguage, "system").toString())));
  connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int) {
    settings.setValue(kKeyUiLanguage, m_languageCombo->currentData().toString());
    promptRestartRequiredDialog();
  });
  addRow(card, tr("Language"), tr("Applies after a restart."), m_languageCombo);

  // Closing the window: quit, or keep running in the tray. The tray button's
  // state is the "closeToTray" flag.
  m_closeGroup = new QButtonGroup(this);
  QWidget *close = segmented({tr("Quits Glate"), tr("Keeps it in the tray")},
                             m_closeGroup);
  QAbstractButton *tray = m_closeGroup->button(1);
  const bool trayAvailable = QSystemTrayIcon::isSystemTrayAvailable();
  const bool closeToTray =
      trayAvailable && settings.value("closeToTray", true).toBool();
  m_closeGroup->button(closeToTray ? 1 : 0)->setChecked(true);
  if (!trayAvailable) {
    tray->setEnabled(false);
    tray->setToolTip(tr("System tray is not available in this session."));
    settings.setValue("closeToTray", false);
  }
  connect(m_closeGroup, &QButtonGroup::idClicked, this,
          [this](int id) { settings.setValue("closeToTray", id == 1); });
  addRow(card, tr("Closing the window"),
         tr("Keep Glate in the system tray so Quick Translate keeps working."),
         close);
}

// ── Styling ─────────────────────────────────────────────────────────────────

// Colours come from the palette, so light and dark both follow the theme.
void Settings::applyStyles() {
  const bool dark = isDark(this);
  const QColor card = dark ? QColor(0x1c, 0x1c, 0x1c) : QColor(0xff, 0xff, 0xff);
  const QColor border = glate::shell::borderColor();
  const QColor accent = palette().color(QPalette::Highlight);
  const QColor accentText = palette().color(QPalette::HighlightedText);

  const QString cardStyle =
      QStringLiteral(
          "#settingsCard { background: %1; border: 1px solid %2; "
          "border-radius: 10px; }"
          "#settingsRow, #settingsControl { background: transparent; }"
          "#settingsRow[separated=\"true\"] { border-top: 1px solid %2; }")
          .arg(card.name(), border.name());
  for (QFrame *frame : std::as_const(m_cards))
    frame->setStyleSheet(cardStyle);

  const QString segmentStyle =
      QStringLiteral(
          "#segmented { background: transparent; }"
          "#segmented QPushButton { border-radius: 0; padding: 5px 14px; }"
          "#segmented QPushButton[segment=\"middle\"],"
          "#segmented QPushButton[segment=\"last\"] { border-left: none; }"
          "#segmented QPushButton[segment=\"first\"] {"
          " border-top-left-radius: 6px; border-bottom-left-radius: 6px; }"
          "#segmented QPushButton[segment=\"last\"] {"
          " border-top-right-radius: 6px; border-bottom-right-radius: 6px; }"
          "#segmented QPushButton:checked, #segmented QPushButton:checked:hover {"
          " background: %1; color: %2; border-color: %1; }")
          .arg(accent.name(), accentText.name());
  for (QWidget *segment : std::as_const(m_segments))
    segment->setStyleSheet(segmentStyle);

  QColor soft = accent;
  soft.setAlpha(dark ? 60 : 40);
  const QString badgeStyle =
      QStringLiteral("background: rgba(%1, %2, %3, %4); color: %5; "
                     "border-radius: 6px; padding: 1px 6px;")
          .arg(soft.red())
          .arg(soft.green())
          .arg(soft.blue())
          .arg(soft.alpha())
          .arg(dark ? accent.lighter(130).name() : accent.name());
  for (QLabel *badge : std::as_const(m_badges))
    badge->setStyleSheet(badgeStyle);
}

void Settings::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange && m_shortcutHint != nullptr) {
    applyStyles();
    updateShortcutRow();
  }
}

// ── Behaviour ───────────────────────────────────────────────────────────────

void Settings::promptRestartRequiredDialog() {
  if (m_restartPromptOpen)
    return;
  m_restartPromptOpen = true;

  QMessageBox box(this);
  box.setIcon(QMessageBox::Information);
  box.setWindowTitle(tr("Restart Required"));
  box.setText(tr("This change will take effect after restarting Glate."));
  auto *restartNow = box.addButton(tr("Restart Now"), QMessageBox::AcceptRole);
  box.addButton(tr("Restart Later"), QMessageBox::RejectRole);
  box.exec();

  m_restartPromptOpen = false;
  if (box.clickedButton() != restartNow)
    return;

  const QString program = QCoreApplication::applicationFilePath();
  const QStringList args = QCoreApplication::arguments().mid(1);
  QProcess::startDetached(program, args);
  qApp->quit();
}

bool Settings::closeToTrayEnabled() const {
  return m_closeGroup->checkedId() == 1;
}

void Settings::setShortcut(const QKeySequence &sequence) {
  settings.setValue("quicktrans_shortcut", sequence);
  if (m_shortcut != nullptr)
    m_shortcut->setKeys(sequence);
}

void Settings::updateShortcutRow() {
  using glate::shortcut::GlobalShortcut;
  const bool on = m_quickCheckBox->isChecked();
  const GlobalShortcut::State state =
      m_shortcut != nullptr && on ? m_shortcut->state()
                                  : GlobalShortcut::State::Off;
  const bool portal = m_shortcut != nullptr &&
                      m_shortcut->method() == GlobalShortcut::Method::Portal;
  const bool none = m_shortcut == nullptr ||
                    m_shortcut->method() == GlobalShortcut::Method::None;

  QString hint;
  QString placeholder = tr("Press shortcut");
  bool bad = false;
  QString button;
  bool primary = false;
  switch (state) {
  case GlobalShortcut::State::Off:
    break;
  case GlobalShortcut::State::Checking:
    hint = tr("Asking your desktop for the shortcut...");
    break;
  case GlobalShortcut::State::Active:
    // The row above already says how to use it.
    if (portal)
      hint = tr("Your desktop manages these keys.");
    if (m_shortcut->canConfigure())
      button = tr("Change...");
    break;
  case GlobalShortcut::State::Taken:
    hint = tr("Another app uses these keys. Press other keys in the field.");
    bad = true;
    break;
  case GlobalShortcut::State::NoKey:
    hint = tr("Your desktop asks you to choose the keys for Quick Translate.");
    placeholder = tr("No key yet");
    if (m_shortcut->canConfigure()) {
      button = tr("Choose keys...");
      primary = true;
    }
    break;
  case GlobalShortcut::State::Declined:
    hint = tr("The desktop did not allow the shortcut.");
    placeholder = tr("Not allowed");
    bad = true;
    button = tr("Try again");
    break;
  case GlobalShortcut::State::Unavailable: {
    QString command = QStringLiteral("glate --quick-translate");
    if (qEnvironmentVariableIsSet("FLATPAK_ID"))
      command = QStringLiteral("flatpak run com.ktechpit.glate --quick-translate");
    hint = tr("This desktop does not let apps add shortcuts. Add a custom "
              "shortcut in its keyboard settings that runs:%1")
               .arg(QStringLiteral("<br><code>%1</code>").arg(command));
    placeholder = tr("Set in your desktop");
    break;
  }
  }

  // On the portal the desktop owns the key: the field shows what it bound.
  const bool desktopKey = portal || none;
  m_keyEdit->setEnabled(on && !desktopKey);
  if (desktopKey) {
    const QSignalBlocker block(m_keyEdit);
    const QString trigger =
        m_shortcut != nullptr ? m_shortcut->trigger() : QString();
    m_keyEdit->setKeySequence(QKeySequence(trigger));
    m_keyEdit->setToolTip(trigger.isEmpty()
                              ? QString()
                              : tr("Set in your desktop's shortcut settings"));
  }
  if (auto *field = m_keyEdit->findChild<QLineEdit *>())
    field->setPlaceholderText(placeholder);
  m_shortcutButton->setVisible(!button.isEmpty());
  m_shortcutButton->setText(button);
  m_shortcutButton->setProperty("accent", primary);
  m_shortcutButton->style()->unpolish(m_shortcutButton);
  m_shortcutButton->style()->polish(m_shortcutButton);
  m_shortcutHint->setVisible(!hint.isEmpty());
  m_shortcutHint->setText(hint);
  const QColor colour = bad ? glate::shell::dangerColor(this)
                            : glate::shell::mutedColor(this);
  m_shortcutHint->setStyleSheet(
      QStringLiteral("#settingsRow { color: %1; background: transparent; }")
          .arg(colour.name()));
}

// ── Offline, from MainWindow ────────────────────────────────────────────────

void Settings::setOfflineAvailable(bool available) {
  for (QWidget *row : std::as_const(m_offlineRows))
    row->setVisible(available);
}

void Settings::setOfflineSummary(const QString &summary) {
  if (m_offlineSummary != nullptr) {
    m_offlineSummary->setText(summary);
    m_offlineSummary->setVisible(!summary.isEmpty());
  }
}

void Settings::setOfflineModeChecked(bool checked) {
  const QSignalBlocker block(m_offlineCheckBox);
  m_offlineCheckBox->setChecked(checked);
  settings.setValue(kKeyOfflineMode, checked);
}
