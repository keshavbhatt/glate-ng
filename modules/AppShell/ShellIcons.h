#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

#include <cstdint>

class QAbstractButton;
class QAction;
class QComboBox;
class QLabel;
class QWidget;

// Lucide glyphs (DOCS/mocks/styling.html): the SVGs under :/icons/ui are drawn
// in #000000 and tinted when they are drawn, so one file serves both schemes
// and every state, as in omnidict. bind() keeps a widget's glyph in its tone
// across theme changes.
namespace glate::shell::icons {

enum class Tone : std::uint8_t
{
    Text,     // the normal text colour
    Muted,    // secondary text
    Accent,   // the accent as text (lighter on dark)
    OnAccent, // on an accent fill
    Offline,  // the "on this device" green
    Danger,   // errors
};

// ":/icons/ui/<name>.svg".
[[nodiscard]] QString path(const QString& name);
[[nodiscard]] QColor color(Tone tone, const QWidget* widget = nullptr);
// The glyph at path (a Lucide SVG) in color, size logical pixels square.
[[nodiscard]] QPixmap pixmapAt(const QString& path, const QColor& color, int size,
                               qreal devicePixelRatio = 1.0);
[[nodiscard]] QPixmap pixmap(const QString& name, const QColor& color, int size,
                             qreal devicePixelRatio = 1.0);
// Normal in color, Disabled faded, Selected (a highlighted list row) on the accent.
[[nodiscard]] QIcon icon(const QString& name, const QColor& color);
[[nodiscard]] QIcon icon(const QString& name, Tone tone = Tone::Text);

// Set the glyph and keep it in tone when the theme changes; call again to
// change the glyph or the tone.
void bind(QAbstractButton* button, const QString& name, Tone tone = Tone::Text);
void bind(QAction* action, const QString& name, Tone tone = Tone::Text);
void bind(QLabel* label, const QString& name, int size, Tone tone = Tone::Text);
void bindItem(QComboBox* box, int index, const QString& name, Tone tone = Tone::Text);

} // namespace glate::shell::icons
