#ifndef SETTINGS_H
#define SETTINGS_H

#include <QColor>
#include <GlobalShortcut.h>
#include <QSettings>
#include <QWidget>

namespace Ui {
class Settings;
}

class ThemeController;
class QComboBox;
class QPushButton;
class QCheckBox;
class QLabel;

class Settings : public QWidget {
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = nullptr,
                    glate::shortcut::GlobalShortcut *shortcut = nullptr,
                    ThemeController *theme = nullptr);
  ~Settings();
signals:
  void themeToggled();
  // Offline translation (Pro): the checkbox changed, or "Manage languages" was clicked.
  void offlineModeChanged(bool enabled);
  void offlineLanguagesRequested();
public:
  // Settings key for offline mode; read by MainWindow.
  static constexpr const char *kKeyOfflineMode = "offlineMode";
  // Hides the section when this build has no offline engine.
  void setOfflineAvailable(bool available);
  // "3 languages downloaded" and the like, shown next to the button.
  void setOfflineSummary(const QString &summary);
  void setOfflineModeChecked(bool checked);
public slots:
  QKeySequence keySequenceEditKeySequence();
  bool quickResultCheckBoxChecked();
  bool closeToTrayEnabled() const;
private slots:
  void setShortcut(const QKeySequence &sequence);

  void readSettings();
  void on_dark_toggled(bool checked);

  void on_light_toggled(bool checked);

protected:
  void changeEvent(QEvent *event) override;

private:
  // Builds the "Appearance" section (theme/skin/accent/CSD) and wires it to the
  // ThemeController; supersedes the old dark/light radios.
  void buildAppearanceSection();
  void updateAccentButton();
  void promptRestartRequiredDialog();
  // Builds the "Offline translation" section under Appearance.
  void buildOfflineSection();
  // The Quick Translate row: a Change button and a line under the key field
  // that follow the global shortcut's state (DOCS/mocks/settings-shortcut.html).
  void buildShortcutRow();
  void updateShortcutRow();

  Ui::Settings *ui;
  QSettings settings;
  glate::shortcut::GlobalShortcut *m_shortcut = nullptr;
  QPushButton *m_shortcutButton = nullptr;
  QLabel *m_shortcutHint = nullptr;

  // Appearance controls (built in code, AppChrome-driven).
  ThemeController *m_theme = nullptr;
  QComboBox *m_modeCombo = nullptr;
  QComboBox *m_skinCombo = nullptr;
  QComboBox *m_languageCombo = nullptr;
  QPushButton *m_accentButton = nullptr;
  QCheckBox *m_csdCheckBox = nullptr;
  QColor m_accentColor;
  bool m_restartPromptOpen = false;

  // Offline translation controls.
  QWidget *m_offlineBox = nullptr;
  QCheckBox *m_offlineCheckBox = nullptr;
  QLabel *m_offlineSummary = nullptr;
};

#endif // SETTINGS_H
