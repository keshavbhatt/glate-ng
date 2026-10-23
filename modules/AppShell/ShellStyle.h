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

// Secondary text: the text colour 60 % of the way from the window colour, as in the mocks.
[[nodiscard]] QColor mutedColor(const QWidget* widget);
// Hairline borders of boxes and lists, as in the mocks.
[[nodiscard]] QColor borderColor();
// Shows label in mutedColor(), also under the Fusion 2 style sheet and across theme changes.
void makeMuted(QLabel* label);

// widget's font scaled from its current size, optionally bold.
void setRelativeFont(QWidget* widget, double scale, bool bold = false);

// Gives dialog the width from its mock and the height its content needs.
void fitToContent(QDialog* dialog, int width);

// Root layout of a shell dialog: returns the body layout; the footer comes from addFooter().
QBoxLayout* setUpDialog(QDialog* dialog, const QString& title);
QBoxLayout* addFooter(QDialog* dialog);
QPushButton* addCloseButton(QDialog* dialog, QBoxLayout* footer);

// Green used for "on this device" in both themes, readable on the window colour.
[[nodiscard]] QColor offlineColor(const QWidget* widget);

// Red for error icons in both themes.
[[nodiscard]] QColor dangerColor(const QWidget* widget);

} // namespace glate::shell
