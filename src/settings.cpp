#include "settings.h"

#include "licenseservice.h"
#include "ui_settings.h"
#include "utils.h"

#include <QGuiApplication>
#include <QShortcut>
#include <QSystemTrayIcon>

#include <AppTheme.h>
#include <ThemeController.h>
#include <WindowChrome.h>
#include <ShellStyle.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QCoreApplication>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStyle>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

Settings::Settings(QWidget *parent, glate::shortcut::GlobalShortcut *shortcut,
                   ThemeController *theme)
    : QWidget(parent), ui(new Ui::Settings), m_theme(theme) {
  ui->setupUi(this);
  { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }
  m_shortcut = shortcut;

  setWindowTitle(tr("Glate | Settings"));
  buildAppearanceSection();
  buildOfflineSection();

  // Populate voice combo from central voice list
  ui->voiceGenderGlobal->clear();
  for (const VoiceOption &v : utils::availableVoices())
    ui->voiceGenderGlobal->addItem(v.displayName);

  buildShortcutRow();
  readSettings();

  connect(ui->voiceGenderGlobal, &QComboBox::currentIndexChanged, this,
          [=](int index) { settings.setValue("voiceGender", index); });

  // The radios' holder would otherwise paint the window colour over the group box.
  ui->closeBehaviorWidget->setAttribute(Qt::WA_TranslucentBackground);
  ui->closeBehaviorWidget->setStyleSheet(
      QStringLiteral("#closeBehaviorWidget { background: transparent; }"));

  // The two radios are mutually exclusive; the tray radio's state is the single
  // source of truth for the "closeToTray" flag (checked = minimize, unchecked =
  // quit), so one connection covers both selections.
  connect(ui->closeTrayRadio, &QRadioButton::toggled, this, [=](bool checked) {
    settings.setValue("closeToTray", checked);
  });

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    ui->closeQuitRadio->setChecked(true);
    ui->closeTrayRadio->setEnabled(false);
    ui->closeTrayRadio->setToolTip(
        tr("System tray is not available in this session."));
    settings.setValue("closeToTray", false);
  }

  connect(ui->quickResultCheckBox, &QCheckBox::toggled, this, [this](bool on) {
    settings.setValue("quicktrans", on);
    if (m_shortcut != nullptr)
      m_shortcut->setEnabled(on);
    updateShortcutRow();
  });
  connect(ui->keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this,
          &Settings::setShortcut);

  ui->dark->setChecked(settings.value("theme", "dark").toString() == "dark");
  ui->light->setChecked(settings.value("theme").toString() == "light");

}

Settings::~Settings() { delete ui; }

void Settings::setShortcut(const QKeySequence &sequence) {
  settings.setValue("quicktrans_shortcut", sequence);
  if (m_shortcut != nullptr)
    m_shortcut->setKeys(sequence);
}

void Settings::readSettings() {

  // Quick Translate: on by default, Ctrl+Shift+Space.
  QKeySequence keys(settings.value("quicktrans_shortcut").toString());
  if (keys.isEmpty())
    keys = QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Space);
  const bool quick = settings.value("quicktrans", true).toBool();
  {
    const QSignalBlocker blockEdit(ui->keySequenceEdit);
    const QSignalBlocker blockBox(ui->quickResultCheckBox);
    ui->keySequenceEdit->setKeySequence(keys);
    ui->quickResultCheckBox->setChecked(quick);
  }
  if (m_shortcut != nullptr) {
    m_shortcut->setKeys(keys);
    m_shortcut->setEnabled(quick);
  }
  updateShortcutRow();

  // global TTS voice preference
  ui->voiceGenderGlobal->setCurrentIndex(
      settings.value("voiceGender", 0).toInt());

  // close-to-tray behaviour
  const bool closeToTray = settings.value("closeToTray", true).toBool();
  ui->closeTrayRadio->setChecked(closeToTray);
  ui->closeQuitRadio->setChecked(!closeToTray);
}

bool Settings::quickResultCheckBoxChecked() {
  return ui->quickResultCheckBox->isChecked();
}

bool Settings::closeToTrayEnabled() const {
  return ui->closeTrayRadio->isChecked();
}

QKeySequence Settings::keySequenceEditKeySequence() {
  return ui->keySequenceEdit->keySequence();
}

void Settings::on_dark_toggled(bool checked) {
  settings.setValue("theme", checked ? "dark" : "light");
  emit themeToggled();
}

void Settings::on_light_toggled(bool checked) {
  settings.setValue("theme", checked ? "light" : "dark");
  emit themeToggled();
}

namespace {
// Appearance settings keys (kept in sync with main.cpp / mainwindow.cpp).
constexpr auto kKeyMode = "appearance/mode";     // system | light | dark
constexpr auto kKeySkin = "appearance/skin";     // fusion | fusion2
constexpr auto kKeyAccent = "appearance/accent"; // #rrggbb
constexpr auto kKeyCsd = "appearance/csd";       // bool
constexpr auto kKeyUiLanguage = "ui_language";   // system | locale code
QColor defaultAccent() { return QColor(0x4a, 0x78, 0xff); }
} // namespace

void Settings::updateAccentButton() {
  if (!m_accentButton)
    return;
  const QColor c = m_accentColor.isValid() ? m_accentColor : defaultAccent();
  m_accentButton->setText(QStringLiteral("  %1  ").arg(c.name().toUpper()));
  // Swatch: fill the button with the colour, readable text on top.
  const bool dark = c.lightnessF() < 0.6;
  m_accentButton->setStyleSheet(
      QStringLiteral("QPushButton{background:%1;color:%2;border:1px solid "
                     "palette(mid);border-radius:4px;padding:4px 10px;}")
          .arg(c.name(), dark ? "#ffffff" : "#000000"));
}

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

void Settings::buildAppearanceSection() {
  // The old dark/light radios (and their "Theme" label) are superseded by this
  // richer section — hide them rather than delete (they live in the .ui form).
  if (ui->dark)
    ui->dark->hide();
  if (ui->light)
    ui->light->hide();
  if (auto *lbl = findChild<QLabel *>("label"))
    lbl->hide(); // the "Theme" label next to the old radios

  auto *box = new QGroupBox(tr("Appearance"), this);
  auto *form = new QFormLayout(box);
  form->setContentsMargins(12, 8, 12, 12);
  form->setSpacing(10);
  form->setLabelAlignment(Qt::AlignLeft);
  form->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

  // Appearance — light/dark, or follow the OS scheme live.
  m_modeCombo = new QComboBox(box);
  m_modeCombo->addItem(tr("Follow system"), QStringLiteral("system"));
  m_modeCombo->addItem(tr("Light"), QStringLiteral("light"));
  m_modeCombo->addItem(tr("Dark"), QStringLiteral("dark"));
  form->addRow(tr("Appearance:"), m_modeCombo);

  // Theme (skin).
  m_skinCombo = new QComboBox(box);
  m_skinCombo->addItem(tr("Fusion"), QStringLiteral("fusion"));
  m_skinCombo->addItem(tr("Fusion 2"), QStringLiteral("fusion2"));
  m_skinCombo->setToolTip(tr("Overall look — \"Fusion\" is the clean native "
                             "style; \"Fusion 2\" is richer and branded."));
  form->addRow(tr("Theme:"), m_skinCombo);

  // UI language (applies on next launch).
  m_languageCombo = new QComboBox(box);
  m_languageCombo->addItem(tr("Follow system"), QStringLiteral("system"));
  m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en_IN"));
  m_languageCombo->addItem(QStringLiteral("Deutsch"), QStringLiteral("de_DE"));
  m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr_FR"));
  m_languageCombo->addItem(QStringLiteral("Português (Brasil)"), QStringLiteral("pt_BR"));
  m_languageCombo->addItem(QStringLiteral("Italiano"), QStringLiteral("it_IT"));
  m_languageCombo->setToolTip(
      tr("Interface language. Change takes effect after restart."));
  form->addRow(tr("Language:"), m_languageCombo);

  // Accent colour (+ reset to default).
  auto *accentRow = new QHBoxLayout;
  m_accentButton = new QPushButton(box);
  m_accentButton->setToolTip(tr("Accent colour for selections and highlights."));
  m_accentButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  auto *accentReset = new QPushButton(tr("Reset"), box);
  accentReset->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  accentRow->addWidget(m_accentButton);
  accentRow->addWidget(accentReset);
  accentRow->addStretch(1);
  form->addRow(tr("Accent colour:"), accentRow);

  // Client-side decoration (applies on restart).
  m_csdCheckBox =
      new QCheckBox(tr("Use the app's own title bar (custom decorations)"), box);
  m_csdCheckBox->setToolTip(tr("Replaces the system title bar with the app's "
                               "own. Takes effect after restart."));
  form->addRow(QString(), m_csdCheckBox);

  // Insert the section at the top of the settings layout.
  if (auto *outer = qobject_cast<QVBoxLayout *>(layout()))
    outer->insertWidget(0, box);

  // ── Load current values ─────────────────────────────────────────────────
  const QString mode = settings.value(kKeyMode, "system").toString();
  const QString skin = settings.value(kKeySkin, "fusion2").toString();
  const QString uiLanguage = settings.value(kKeyUiLanguage, "system").toString();
  m_accentColor = QColor(settings.value(kKeyAccent, defaultAccent().name()).toString());
  if (!m_accentColor.isValid())
    m_accentColor = defaultAccent();
  m_modeCombo->setCurrentIndex(qMax(0, m_modeCombo->findData(mode)));
  m_skinCombo->setCurrentIndex(qMax(0, m_skinCombo->findData(skin)));
  m_languageCombo->setCurrentIndex(qMax(0, m_languageCombo->findData(uiLanguage)));
  m_csdCheckBox->setChecked(
      settings.value(kKeyCsd, WindowChrome::defaultEnabled()).toBool());
  updateAccentButton();

  // ── Apply + persist on change (live, no restart for theme/skin/accent) ────
  auto applyTheme = [this]() {
    settings.setValue(kKeyMode, m_modeCombo->currentData().toString());
    settings.setValue(kKeySkin, m_skinCombo->currentData().toString());
    settings.setValue(kKeyAccent, m_accentColor.name());
    if (m_theme)
      m_theme->configure(
          AppTheme::skinFromString(m_skinCombo->currentData().toString()),
          AppTheme::modeFromString(m_modeCombo->currentData().toString()),
          m_accentColor.isValid() ? m_accentColor : defaultAccent());
  };

  connect(m_modeCombo, &QComboBox::currentIndexChanged, this,
          [applyTheme](int) { applyTheme(); });
  connect(m_skinCombo, &QComboBox::currentIndexChanged, this,
          [applyTheme](int) { applyTheme(); });
  connect(m_languageCombo, &QComboBox::currentIndexChanged, this, [this](int) {
    settings.setValue(kKeyUiLanguage, m_languageCombo->currentData().toString());
    promptRestartRequiredDialog();
  });
  connect(m_accentButton, &QPushButton::clicked, this, [this, applyTheme]() {
    const QColor c = QColorDialog::getColor(
        m_accentColor.isValid() ? m_accentColor : defaultAccent(), this,
        tr("Choose accent colour"));
    if (c.isValid()) {
      m_accentColor = c;
      updateAccentButton();
      applyTheme();
    }
  });
  connect(accentReset, &QPushButton::clicked, this, [this, applyTheme]() {
    m_accentColor = defaultAccent();
    updateAccentButton();
    applyTheme();
  });
  connect(m_csdCheckBox, &QCheckBox::toggled, this,
          [this](bool on) {
            settings.setValue(kKeyCsd, on);
            promptRestartRequiredDialog();
          });
}

void Settings::buildShortcutRow() {
  // The key field moves into a column with the Change button beside it and a
  // line under it, in the grid cell it had.
  auto *grid = qobject_cast<QGridLayout *>(ui->gridLayout);
  auto *cell = new QWidget(this);
  // Like the On close radios: no window colour over the group box.
  cell->setObjectName(QStringLiteral("shortcutCell"));
  cell->setAttribute(Qt::WA_TranslucentBackground);
  cell->setStyleSheet(QStringLiteral(
      "#shortcutCell, #shortcutHint { background: transparent; }"));
  auto *column = new QVBoxLayout(cell);
  column->setContentsMargins(0, 0, 0, 0);
  column->setSpacing(4);
  auto *row = new QHBoxLayout;
  row->setSpacing(8);
  grid->removeWidget(ui->keySequenceEdit);
  row->addWidget(ui->keySequenceEdit, 1);
  m_shortcutButton = new QPushButton(cell);
  m_shortcutButton->setObjectName(QStringLiteral("shortcutButton"));
  row->addWidget(m_shortcutButton);
  column->addLayout(row);
  m_shortcutHint = new QLabel(cell);
  m_shortcutHint->setObjectName(QStringLiteral("shortcutHint"));
  m_shortcutHint->setWordWrap(true);
  m_shortcutHint->setTextInteractionFlags(Qt::TextSelectableByMouse);
  glate::shell::setRelativeFont(m_shortcutHint, 0.9);
  column->addWidget(m_shortcutHint);
  grid->addWidget(cell, 0, 1);
  // The switch lines up with the key field, not with the middle of the cell.
  grid->setAlignment(ui->quickResultCheckBox, Qt::AlignTop);
  ui->quickResultCheckBox->setMinimumHeight(
      ui->keySequenceEdit->sizeHint().height());
  ui->quickResultCheckBox->setToolTip(
      tr("Translate the text selected in any app with a keyboard shortcut"));

  connect(m_shortcutButton, &QPushButton::clicked, this, [this] {
    if (m_shortcut == nullptr)
      return;
    if (m_shortcut->canConfigure())
      m_shortcut->configure();
    else
      m_shortcut->retry();
  });
  if (m_shortcut != nullptr)
    connect(m_shortcut, &glate::shortcut::GlobalShortcut::stateChanged, this,
            &Settings::updateShortcutRow);
}

void Settings::updateShortcutRow() {
  using glate::shortcut::GlobalShortcut;
  const bool on = ui->quickResultCheckBox->isChecked();
  const GlobalShortcut::State state =
      m_shortcut != nullptr && on ? m_shortcut->state()
                                  : GlobalShortcut::State::Off;
  const bool portal = m_shortcut != nullptr &&
                      m_shortcut->method() == GlobalShortcut::Method::Portal;
  const bool none = m_shortcut == nullptr ||
                    m_shortcut->method() == GlobalShortcut::Method::None;
  const QString app = QApplication::applicationName();

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
    hint = portal ? tr("Your desktop manages this shortcut. Select text in any "
                       "app, then press the keys.")
                  : tr("Select text in any app, then press the keys.");
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
  ui->keySequenceEdit->setEnabled(on && !desktopKey);
  if (desktopKey) {
    const QSignalBlocker block(ui->keySequenceEdit);
    const QString trigger = m_shortcut != nullptr ? m_shortcut->trigger() : QString();
    ui->keySequenceEdit->setKeySequence(QKeySequence(trigger));
    ui->keySequenceEdit->setToolTip(
        trigger.isEmpty() ? QString()
                          : tr("Set in your desktop's shortcut settings"));
  }
  if (auto *field = ui->keySequenceEdit->findChild<QLineEdit *>())
    field->setPlaceholderText(placeholder);
  m_shortcutButton->setVisible(!button.isEmpty());
  m_shortcutButton->setText(button);
  m_shortcutButton->setProperty("accent", primary);
  m_shortcutButton->style()->unpolish(m_shortcutButton);
  m_shortcutButton->style()->polish(m_shortcutButton);
  m_shortcutHint->setVisible(!hint.isEmpty());
  m_shortcutHint->setText(hint);
  m_shortcutHint->setProperty("bad", bad);
  const QColor colour = bad ? glate::shell::dangerColor(this)
                            : glate::shell::mutedColor(this);
  m_shortcutHint->setStyleSheet(
      QStringLiteral("color: %1;").arg(colour.name()));
}

void Settings::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);
  if (event->type() == QEvent::PaletteChange && m_shortcutHint != nullptr)
    updateShortcutRow();
}

void Settings::buildOfflineSection() {
  auto *box = new QGroupBox(tr("Offline translation"), this);
  m_offlineBox = box;
  auto *column = new QVBoxLayout(box);
  column->setContentsMargins(12, 8, 12, 12);
  column->setSpacing(8);

  m_offlineCheckBox = new QCheckBox(tr("Translate offline"), box);
  m_offlineCheckBox->setToolTip(
      tr("Translate on this computer with downloaded languages. "
         "Your text never leaves your device. Pro feature."));
  column->addWidget(m_offlineCheckBox);

  auto *row = new QHBoxLayout;
  m_offlineSummary = new QLabel(box);
  m_offlineSummary->setEnabled(false);
  auto *manage = new QPushButton(tr("Manage languages..."), box);
  manage->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
  row->addWidget(m_offlineSummary, 1);
  row->addWidget(manage);
  column->addLayout(row);

  // Directly under Appearance.
  if (auto *outer = qobject_cast<QVBoxLayout *>(layout()))
    outer->insertWidget(1, box);

  m_offlineCheckBox->setChecked(settings.value(kKeyOfflineMode, false).toBool());

  connect(m_offlineCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
    if (checked && !LicenseService::instance()->checkAccess(tr("Offline translation"))) {
      QSignalBlocker block(m_offlineCheckBox);
      m_offlineCheckBox->setChecked(false);
      return;
    }
    settings.setValue(kKeyOfflineMode, checked);
    emit offlineModeChanged(checked);
  });
  connect(manage, &QPushButton::clicked, this,
          &Settings::offlineLanguagesRequested);
}

void Settings::setOfflineAvailable(bool available) {
  if (m_offlineBox)
    m_offlineBox->setVisible(available);
}

void Settings::setOfflineSummary(const QString &summary) {
  if (m_offlineSummary)
    m_offlineSummary->setText(summary);
}

void Settings::setOfflineModeChecked(bool checked) {
  if (!m_offlineCheckBox)
    return;
  QSignalBlocker block(m_offlineCheckBox);
  m_offlineCheckBox->setChecked(checked);
  settings.setValue(kKeyOfflineMode, checked);
}
