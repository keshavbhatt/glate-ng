#include "settings.h"

#include <QDesktopServices>

#include "ui_settings.h"
#include "utils.h"

#include <QGuiApplication>
#include <QShortcut>
#include <QSystemTrayIcon>

#include <AppTheme.h>
#include <ThemeController.h>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

Settings::Settings(QWidget *parent, QHotkey *hotKey, ThemeController *theme)
    : QWidget(parent), ui(new Ui::Settings), m_theme(theme) {
  ui->setupUi(this);
  { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }
  this->nativeHotkey = hotKey;

  setWindowTitle(tr("Glate | Settings"));
  buildAppearanceSection();

  // Populate voice combo from central voice list
  ui->voiceGenderGlobal->clear();
  for (const VoiceOption &v : utils::availableVoices())
    ui->voiceGenderGlobal->addItem(v.displayName);

  const QString platform = QGuiApplication::platformName().toLower();
  m_hotkeySupported = !platform.contains("wayland");

  readSettings();

  connect(ui->voiceGenderGlobal, &QComboBox::currentIndexChanged, this,
          [=](int index) { settings.setValue("voiceGender", index); });

  connect(ui->closeToTrayCheckBox, &QCheckBox::toggled, this, [=](bool checked) {
    settings.setValue("closeToTray", checked);
  });

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    ui->closeToTrayCheckBox->setChecked(false);
    ui->closeToTrayCheckBox->setEnabled(false);
    ui->closeToTrayCheckBox->setToolTip(
        tr("System tray is not available in this session."));
    settings.setValue("closeToTray", false);
  }

  if (m_hotkeySupported && this->nativeHotkey != nullptr) {
    connect(this->nativeHotkey, &QHotkey::activated, this,
            &Settings::get_selected_word_fromX11);

    connect(ui->quickResultCheckBox, &QCheckBox::toggled, this->nativeHotkey,
            &QHotkey::setRegistered);

    connect(ui->keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this,
            &Settings::setShortcut);
  }

  if (!m_hotkeySupported) {
    const QString unsupportedTooltip =
        tr("Global shortcuts are unavailable on native Wayland sessions.");
    ui->quickResultCheckBox->setChecked(false);
    ui->quickResultCheckBox->setEnabled(false);
    ui->quickResultCheckBox->setText(tr("Quick Translate (unavailable)"));
    ui->quickResultCheckBox->setToolTip(unsupportedTooltip);
    ui->keySequenceEdit->setEnabled(false);
    ui->keySequenceEdit->setToolTip(unsupportedTooltip);
    settings.setValue("quicktrans", false);
    if (this->nativeHotkey != nullptr)
      this->nativeHotkey->setRegistered(false);
  }

  ui->dark->setChecked(settings.value("theme", "dark").toString() == "dark");
  ui->light->setChecked(settings.value("theme").toString() == "light");

  const QString hotkeyNote =
      m_hotkeySupported
          ? QString()
          : tr("<p align=\"center\"><span><b>Note:</b> Global shortcuts are "
               "not available on native Wayland sessions.</span></p>");

  QString aboutTxt =
      R"(<p align="center"> <span style="font-weight: bold;">Glate</span></p>
         <p align="center"><span>Designed and Developed</span></p>
         <p align="center"><span>by </span><span style="font-weight:bold;">Keshav Bhatt</span>
         <span> &lt;keshavnrj@gmail.com&gt;</span></p>
         <p align="center"><span>Website: https://ktechpit.com</span></p>
         <p align="center"><span>Runtime: %1</span></p>
         <p align="center"><span>Version: %2</span></p>
         %3)";

  ui->aboutTextBrowser->setText(
      aboutTxt.arg(qVersion(), qApp->applicationVersion(), hotkeyNote));
}

Settings::~Settings() { delete ui; }

void Settings::setShortcut(const QKeySequence &sequence) {
  if (!m_hotkeySupported || this->nativeHotkey == nullptr)
    return;
  this->nativeHotkey->setShortcut(sequence,
                                  ui->quickResultCheckBox->isChecked());
  // save quick trans shortcut instanty
  settings.setValue("quicktrans_shortcut", sequence);
}

void Settings::readSettings() {

  // quick trans settings
  if (settings.value("quicktrans").isValid()) {
    ui->quickResultCheckBox->setChecked(settings.value("quicktrans").toBool());
  } else if (settings.value("quicktrans").isValid()) {
    ui->quickResultCheckBox->setChecked(true);
  } else {
    ui->quickResultCheckBox->setChecked(false);
  }
  // quick trans key sequence
  if (settings.value("quicktrans_shortcut").isValid()) {
    auto k =
        QKeySequence(settings.value("quicktrans_shortcut").toString());
    ui->keySequenceEdit->setKeySequence(k);
    if (m_hotkeySupported && this->nativeHotkey != nullptr) {
      this->nativeHotkey->setShortcut(k.toString(),
                                      settings.value("quicktrans").toBool());
    }
  } else {
    // default value if value not founds
    QKeySequence k = QKeySequence::fromString(tr("Ctrl+Shift+Space"));
    ui->keySequenceEdit->setKeySequence(k);
    if (m_hotkeySupported && this->nativeHotkey != nullptr) {
      this->nativeHotkey->setShortcut(k.toString(), true);
      ui->quickResultCheckBox->setChecked(true);
    }
  }

  // global TTS voice preference
  ui->voiceGenderGlobal->setCurrentIndex(
      settings.value("voiceGender", 0).toInt());

  // close-to-tray behaviour
  ui->closeToTrayCheckBox->setChecked(
      settings.value("closeToTray", true).toBool());
}

void Settings::get_selected_word_fromX11() {
  auto xsel = new QProcess(this);
  xsel->setObjectName("xclip");
  xsel->start("xclip", QStringList() << "-o"
                                     << "-sel");
  connect(xsel, SIGNAL(finished(int)), this, SLOT(set_x11_selection()));
}

void Settings::set_x11_selection() {
  auto xselection = this->findChild<QObject *>("xclip");
  if (xselection) {
    x11_selected = ((QProcess *)(xselection))->readAllStandardOutput();
    if (!x11_selected.trimmed().isEmpty())
      show_requested_text();

    xselection->deleteLater();
  }
}

void Settings::show_requested_text() { emit translationRequest(x11_selected); }

bool Settings::quickResultCheckBoxChecked() {
  return ui->quickResultCheckBox->isChecked();
}

bool Settings::closeToTrayEnabled() const {
  return ui->closeToTrayCheckBox->isChecked();
}

QKeySequence Settings::keySequenceEditKeySequence() {
  return ui->keySequenceEdit->keySequence();
}

void Settings::on_github_clicked() {
  QDesktopServices::openUrl(QUrl("https://github.com/keshavbhatt/glate"));
}

void Settings::on_rate_clicked() {
  QDesktopServices::openUrl(QUrl("snap://glate"));
}

void Settings::on_donate_clicked() {
  QDesktopServices::openUrl(QUrl("https://paypal.me/keshavnrj/5"));
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
QColor defaultAccent() { return QColor(0x3d, 0x6e, 0xff); }
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
  m_accentColor = QColor(settings.value(kKeyAccent, defaultAccent().name()).toString());
  if (!m_accentColor.isValid())
    m_accentColor = defaultAccent();
  m_modeCombo->setCurrentIndex(qMax(0, m_modeCombo->findData(mode)));
  m_skinCombo->setCurrentIndex(qMax(0, m_skinCombo->findData(skin)));
  m_csdCheckBox->setChecked(settings.value(kKeyCsd, true).toBool());
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
          [this](bool on) { settings.setValue(kKeyCsd, on); });
}
