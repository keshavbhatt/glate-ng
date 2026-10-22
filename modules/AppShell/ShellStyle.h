#pragma once

#include <QColor>
#include <QIcon>
#include <QString>

class QBoxLayout;
class QDialog;
class QLabel;
class QPushButton;
class QWidget;

// Small helpers shared by the shell widgets, so the dialogs look alike: a body
// with 20 px margins and a footer strip with the buttons (as in omnidict).
namespace glate::shell {

// The icon at resourcePath with every opaque pixel painted in color.
[[nodiscard]] QIcon tintedIcon(const QString& resourcePath, const QColor& color, int size = 64);

// The palette's placeholder colour, used for secondary text.
[[nodiscard]] QColor mutedColor(const QWidget* widget);
void makeMuted(QLabel* label);

// Root layout of a shell dialog: returns the body layout; the footer comes from addFooter().
QBoxLayout* setUpDialog(QDialog* dialog, const QString& title);
QBoxLayout* addFooter(QDialog* dialog);
QPushButton* addCloseButton(QDialog* dialog, QBoxLayout* footer);

// Green used for "on this device" in both themes, readable on the window colour.
[[nodiscard]] QColor offlineColor(const QWidget* widget);

} // namespace glate::shell
