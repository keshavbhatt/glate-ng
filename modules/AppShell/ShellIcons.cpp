#include "ShellIcons.h"

#include "ShellStyle.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QEvent>
#include <QFile>
#include <QHash>
#include <QLabel>
#include <QList>
#include <QPainter>
#include <QPointer>
#include <QSvgRenderer>

using namespace Qt::StringLiterals;

namespace glate::shell::icons {

namespace {

constexpr float kDisabledAlpha = 0.35F;

QByteArray tintedSvg(const QString& path, const QColor& color)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    QByteArray svg = file.readAll();
    svg.replace("#000000", color.name(QColor::HexRgb).toLatin1());
    return svg;
}

QPixmap render(const QByteArray& svg, int size, qreal devicePixelRatio, qreal opacity)
{
    QSvgRenderer renderer(svg);
    QPixmap pixmap(QSize(size, size) * devicePixelRatio);
    pixmap.setDevicePixelRatio(devicePixelRatio);
    pixmap.fill(Qt::transparent);
    if (renderer.isValid()) {
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setOpacity(opacity);
        renderer.render(&painter, QRectF(0, 0, size, size));
    }
    return pixmap;
}

enum class Kind : std::uint8_t
{
    Button,
    Action,
    Label,
    ComboItem,
};

struct Binding
{
    QPointer<QObject> target;
    Kind kind = Kind::Button;
    QString name;
    Tone tone = Tone::Text;
    int size = 0;  // Label: pixmap size
    int index = 0; // ComboItem: the row
};

void apply(const Binding& binding)
{
    switch (binding.kind) {
    case Kind::Button: {
        auto* button = qobject_cast<QAbstractButton*>(binding.target);
        button->setIcon(icon(binding.name, color(binding.tone, button)));
        break;
    }
    case Kind::Action:
        qobject_cast<QAction*>(binding.target)->setIcon(icon(binding.name, binding.tone));
        break;
    case Kind::Label: {
        auto* label = qobject_cast<QLabel*>(binding.target);
        label->setPixmap(
            pixmap(binding.name, color(binding.tone, label), binding.size, label->devicePixelRatioF()));
        break;
    }
    case Kind::ComboItem: {
        auto* box = qobject_cast<QComboBox*>(binding.target);
        if (binding.index < box->count()) {
            box->setItemIcon(binding.index, icon(binding.name, color(binding.tone, box)));
        }
        break;
    }
    }
}

// Redraws every bound glyph when the application palette changes (a theme switch).
class Registry : public QObject
{
public:
    static Registry& instance()
    {
        static auto* registry = new Registry(qApp);
        return *registry;
    }

    void add(const Binding& binding)
    {
        for (Binding& existing : m_bindings) {
            if (existing.target == binding.target && existing.kind == binding.kind &&
                existing.index == binding.index) {
                existing = binding;
                apply(existing);
                return;
            }
        }
        apply(binding);
        m_bindings.append(binding);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        // Qt tells the application before it updates the widgets' palettes, so
        // redraw once that has happened; several changes in a row redraw once.
        if (watched == qApp && event->type() == QEvent::ApplicationPaletteChange && !m_pending) {
            m_pending = true;
            QMetaObject::invokeMethod(this, &Registry::refresh, Qt::QueuedConnection);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    void refresh()
    {
        m_pending = false;
        m_bindings.removeIf([](const Binding& binding) { return binding.target.isNull(); });
        for (const Binding& binding : std::as_const(m_bindings)) {
            apply(binding);
        }
    }

    explicit Registry(QObject* parent)
        : QObject(parent)
    {
        qApp->installEventFilter(this);
    }

    QList<Binding> m_bindings;
    bool m_pending = false;
};

} // namespace

QString path(const QString& name)
{
    return u":/icons/ui/"_s + name + u".svg"_s;
}

QColor color(Tone tone, const QWidget* widget)
{
    // The application palette, unless the widget has one of its own: on a theme
    // switch Qt updates the widgets' palettes after the redraw runs.
    if (widget != nullptr && !widget->testAttribute(Qt::WA_SetPalette)) {
        widget = nullptr;
    }
    const QPalette palette = widget != nullptr ? widget->palette() : QApplication::palette();
    switch (tone) {
    case Tone::Text:
        return palette.color(QPalette::Active, QPalette::WindowText);
    case Tone::Muted:
        return mutedColor(widget);
    case Tone::Accent:
        return palette.color(QPalette::Active, QPalette::Link);
    case Tone::OnAccent:
        return palette.color(QPalette::Active, QPalette::HighlightedText);
    case Tone::Offline:
        return offlineColor(widget);
    case Tone::Danger:
        return dangerColor(widget);
    }
    return palette.color(QPalette::Active, QPalette::WindowText);
}

QPixmap pixmapAt(const QString& path, const QColor& color, int size, qreal devicePixelRatio)
{
    static QHash<QString, QPixmap> cache;
    const QString key = path + u'|' + color.name(QColor::HexArgb) + u'|' + QString::number(size) + u'|' +
                        QString::number(devicePixelRatio);
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) {
        return found.value();
    }
    const QPixmap result =
        render(tintedSvg(path, color), size, devicePixelRatio, static_cast<qreal>(color.alphaF()));
    cache.insert(key, result);
    return result;
}

QPixmap pixmap(const QString& name, const QColor& color, int size, qreal devicePixelRatio)
{
    return pixmapAt(path(name), color, size, devicePixelRatio);
}

QIcon icon(const QString& name, const QColor& color)
{
    // Cached whole: a language list sets the same icon on a few hundred rows.
    static QHash<QString, QIcon> cache;
    const QColor selected = QApplication::palette().color(QPalette::Active, QPalette::HighlightedText);
    const QString key = name + u'|' + color.name(QColor::HexArgb) + u'|' + selected.name(QColor::HexArgb);
    const auto found = cache.constFind(key);
    if (found != cache.constEnd()) {
        return found.value();
    }
    QColor disabled = color;
    disabled.setAlphaF(kDisabledAlpha);
    QIcon result;
    for (const int size : {16, 18, 20, 24, 28, 32, 48}) {
        for (const qreal ratio : {1.0, 2.0}) {
            result.addPixmap(pixmap(name, color, size, ratio), QIcon::Normal);
            result.addPixmap(pixmap(name, color, size, ratio), QIcon::Active);
            result.addPixmap(pixmap(name, disabled, size, ratio), QIcon::Disabled);
            result.addPixmap(pixmap(name, selected, size, ratio), QIcon::Selected);
        }
    }
    cache.insert(key, result);
    return result;
}

QIcon icon(const QString& name, Tone tone)
{
    return icon(name, color(tone));
}

void bind(QAbstractButton* button, const QString& name, Tone tone)
{
    Registry::instance().add({.target = button, .kind = Kind::Button, .name = name, .tone = tone});
}

void bind(QAction* action, const QString& name, Tone tone)
{
    Registry::instance().add({.target = action, .kind = Kind::Action, .name = name, .tone = tone});
}

void bind(QLabel* label, const QString& name, int size, Tone tone)
{
    Registry::instance().add(
        {.target = label, .kind = Kind::Label, .name = name, .tone = tone, .size = size});
}

void bindItem(QComboBox* box, int index, const QString& name, Tone tone)
{
    Registry::instance().add(
        {.target = box, .kind = Kind::ComboItem, .name = name, .tone = tone, .index = index});
}

} // namespace glate::shell::icons
