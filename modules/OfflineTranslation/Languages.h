#pragma once

#include <QList>
#include <QString>

namespace glate::offline {

// Language every Firefox model translates to or from; other pairs go through it.
inline constexpr QStringView kPivotLanguage = u"en";

// One model direction, in model language codes ("en", "zh-Hans").
struct Direction
{
    QString from;
    QString to;

    // Directory and catalogue id: "en-de".
    [[nodiscard]] QString id() const;
    // Spelled out: the app that includes this header still builds as C++17.
    friend bool operator==(const Direction& a, const Direction& b)
    {
        return a.from == b.from && a.to == b.to;
    }
};

// Maps a glate language code (Google's, as in resources/lang.glate) to the code
// the Firefox models use. Codes without a rename pass through unchanged.
[[nodiscard]] QString toModelCode(const QString& glateCode);

// The inverse of toModelCode().
[[nodiscard]] QString toGlateCode(const QString& modelCode);

// Directions needed to translate between two model codes: one when either side
// is the pivot, two through the pivot otherwise, none when they are equal.
[[nodiscard]] QList<Direction> route(const QString& from, const QString& to);

} // namespace glate::offline
