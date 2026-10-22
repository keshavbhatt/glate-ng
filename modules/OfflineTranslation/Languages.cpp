#include "Languages.h"

#include <array>
#include <utility>

namespace glate::offline {

namespace {

struct Rename
{
    QStringView glate;
    QStringView model;
};

// Google codes that the Firefox models spell differently.
constexpr std::array kRenames{
    Rename{.glate = u"zh-CN", .model = u"zh-Hans"},
    Rename{.glate = u"zh-TW", .model = u"zh-Hant"},
    Rename{.glate = u"iw", .model = u"he"},
    Rename{.glate = u"no", .model = u"nb"},
};

} // namespace

QString Direction::id() const
{
    return from + u'-' + to;
}

QString toModelCode(const QString& glateCode)
{
    for (const auto& rename : kRenames) {
        if (glateCode == rename.glate) {
            return rename.model.toString();
        }
    }
    return glateCode;
}

QString toGlateCode(const QString& modelCode)
{
    for (const auto& rename : kRenames) {
        if (modelCode == rename.model) {
            return rename.glate.toString();
        }
    }
    return modelCode;
}

QList<Direction> route(const QString& from, const QString& to)
{
    if (from.isEmpty() || to.isEmpty() || from == to) {
        return {};
    }
    if (from == kPivotLanguage || to == kPivotLanguage) {
        return {Direction{.from = from, .to = to}};
    }
    const QString pivot = kPivotLanguage.toString();
    return {Direction{.from = from, .to = pivot}, Direction{.from = pivot, .to = to}};
}

} // namespace glate::offline
