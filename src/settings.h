#ifndef SETTINGS_H
#define SETTINGS_H

#include <GlobalShortcut.h>
#include <QList>
#include <QSettings>
#include <QWidget>

class ThemeController;
class QAbstractButton;
class QBoxLayout;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QKeySequenceEdit;
class QLabel;
class QPushButton;
class QVBoxLayout;

// The Settings window (DOCS/mocks/settings.html): one scrolling page with four
// sections of cards, Translation, Quick Translate, Appearance and General.
class Settings : public QWidget {
  Q_OBJECT

public:
  explicit Settings(QWidget *parent = nullptr,
                    glate::shortcut::GlobalShortcut *shortcut = nullptr,
                    ThemeController *theme = nullptr);
  ~Settings() override = default;
signals:
  // Offline translation (Pro): the switch changed, or "Manage languages" was clicked.
  void offlineModeChanged(bool enabled);
  void offlineLanguagesRequested();

public:
  // Settings key for offline mode; read by MainWindow.
  static constexpr const char *kKeyOfflineMode = "offlineMode";
  // Hides the offline rows when this build has no offline engine.
  void setOfflineAvailable(bool available);
  // "3 languages downloaded" and the like, under "Offline languages".
  void setOfflineSummary(const QString &summary);
  void setOfflineModeChecked(bool checked);
  [[nodiscard]] bool closeToTrayEnabled() const;

protected:
  void changeEvent(QEvent *event) override;

private:
  // A titled section and its card; rows are added to the returned layout.
  QVBoxLayout *addSection(QVBoxLayout *page, const QString &title);
  // One row: title and an optional muted line on the left, the control on
  // the right. Returns the row; *hintOut gets the muted line when asked for.
  QWidget *addRow(QVBoxLayout *card, const QString &title, const QString &hint,
                  QWidget *control, QLabel **hintOut = nullptr);
  // Buttons side by side that act as one choice (Theme, Closing the window).
  QWidget *segmented(const QStringList &labels, QButtonGroup *group);
  void buildTranslation(QVBoxLayout *page);
  void buildQuickTranslate(QVBoxLayout *page);
  void buildAppearance(QVBoxLayout *page);
  void buildGeneral(QVBoxLayout *page);
  void applyStyles();
  void promptRestartRequiredDialog();
  void setShortcut(const QKeySequence &sequence);
  // The line under the keys and the Change button follow the global
  // shortcut's state (DOCS/mocks/settings-shortcut.html).
  void updateShortcutRow();

  QSettings settings;
  glate::shortcut::GlobalShortcut *m_shortcut = nullptr;
  ThemeController *m_theme = nullptr;
  bool m_restartPromptOpen = false;

  QList<QFrame *> m_cards;
  QList<QWidget *> m_segments;
  QList<QLabel *> m_badges;

  // Translation
  QCheckBox *m_offlineCheckBox = nullptr;
  QLabel *m_offlineSummary = nullptr;
  QList<QWidget *> m_offlineRows;
  QComboBox *m_voiceCombo = nullptr;

  // Quick Translate
  QCheckBox *m_quickCheckBox = nullptr;
  QKeySequenceEdit *m_keyEdit = nullptr;
  QPushButton *m_shortcutButton = nullptr;
  QLabel *m_shortcutHint = nullptr;

  // Appearance and General
  QButtonGroup *m_modeGroup = nullptr;
  QCheckBox *m_csdCheckBox = nullptr;
  QComboBox *m_languageCombo = nullptr;
  QButtonGroup *m_closeGroup = nullptr;
};

#endif // SETTINGS_H
