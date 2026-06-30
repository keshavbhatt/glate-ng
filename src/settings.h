#ifndef SETTINGS_H
#define SETTINGS_H

#include <QColor>
#include <QHotkey>
#include <QSettings>
#include <QWidget>

namespace Ui {
class Settings;
}

class ThemeController;
class QComboBox;
class QPushButton;
class QCheckBox;

class Settings : public QWidget {
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = nullptr, QHotkey *hotKey = nullptr,
                    ThemeController *theme = nullptr);
  ~Settings();
signals:
  void translationRequest(QString selected);
  void themeToggled();
public slots:
  QKeySequence keySequenceEditKeySequence();
  bool quickResultCheckBoxChecked();
  bool closeToTrayEnabled() const;
private slots:
  void get_selected_word_fromX11();
  void set_x11_selection();
  void show_requested_text();
  void setShortcut(const QKeySequence &sequence);

  void readSettings();
  void on_dark_toggled(bool checked);

  void on_light_toggled(bool checked);

private:
  // Builds the "Appearance" section (theme/skin/accent/CSD) and wires it to the
  // ThemeController; supersedes the old dark/light radios.
  void buildAppearanceSection();
  void updateAccentButton();

  Ui::Settings *ui;
  QSettings settings;
  QHotkey *nativeHotkey = nullptr;
  bool m_hotkeySupported = true;
  QString x11_selected;

  // Appearance controls (built in code, AppChrome-driven).
  ThemeController *m_theme = nullptr;
  QComboBox *m_modeCombo = nullptr;
  QComboBox *m_skinCombo = nullptr;
  QPushButton *m_accentButton = nullptr;
  QCheckBox *m_csdCheckBox = nullptr;
  QColor m_accentColor;
};

#endif // SETTINGS_H
