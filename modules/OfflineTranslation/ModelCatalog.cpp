#include "ModelCatalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>

namespace glate::offline {

namespace {

bool fail(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

// Names land on disk inside the model directory, so nothing that could leave it.
bool isSafeFileName(const QString& name)
{
    static const QRegularExpression kSafe(QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]*$"));
    return kSafe.match(name).hasMatch() && !name.contains(QStringLiteral(".."));
}

bool isLanguageCode(const QString& code)
{
    static const QRegularExpression kCode(QStringLiteral("^[a-z]{2,3}(-[A-Za-z]{2,4})?$"));
    return kCode.match(code).hasMatch();
}

bool isSha256(const QByteArray& hex)
{
    static const QRegularExpression kHex(QStringLiteral("^[0-9a-f]{64}$"));
    return kHex.match(QString::fromLatin1(hex)).hasMatch();
}

bool parseFile(const QJsonObject& object, ModelFile& file, QString* error)
{
    file.kind = object.value(u"kind").toString();
    file.name = object.value(u"name").toString();
    file.asset = object.value(u"asset").toString();
    file.size = object.value(u"size").toInteger(-1);
    file.sha256 = object.value(u"sha256").toString().toLatin1();

    static const QSet<QString> kKinds{
        QStringLiteral("model"),    QStringLiteral("lex"),      QStringLiteral("vocab"),
        QStringLiteral("srcvocab"), QStringLiteral("trgvocab"),
    };
    if (!kKinds.contains(file.kind)) {
        return fail(error, QStringLiteral("unknown file kind '%1'").arg(file.kind));
    }
    if (!isSafeFileName(file.name) || !isSafeFileName(file.asset)) {
        return fail(error, QStringLiteral("unsafe file name '%1'").arg(file.name));
    }
    if (file.size <= 0) {
        return fail(error, QStringLiteral("bad size for '%1'").arg(file.name));
    }
    if (!isSha256(file.sha256)) {
        return fail(error, QStringLiteral("bad sha256 for '%1'").arg(file.name));
    }
    return true;
}

bool hasRequiredFiles(const ModelEntry& entry)
{
    const bool sharedVocab = entry.file(u"vocab") != nullptr;
    const bool splitVocab = entry.file(u"srcvocab") != nullptr && entry.file(u"trgvocab") != nullptr;
    return entry.file(u"model") != nullptr && entry.file(u"lex") != nullptr && (sharedVocab || splitVocab);
}

bool parseEntry(const QJsonObject& object, ModelEntry& entry, QString* error)
{
    entry.from = object.value(u"from").toString();
    entry.to = object.value(u"to").toString();
    entry.version = object.value(u"version").toString();
    entry.gemmPrecision = object.value(u"gemm_precision").toString();

    if (!isLanguageCode(entry.from) || !isLanguageCode(entry.to) || entry.from == entry.to) {
        return fail(error, QStringLiteral("bad direction '%1-%2'").arg(entry.from, entry.to));
    }
    if (entry.version.isEmpty()) {
        return fail(error, QStringLiteral("%1 has no version").arg(entry.id()));
    }
    if (entry.gemmPrecision != u"int8shiftAlphaAll" && entry.gemmPrecision != u"int8shiftAll") {
        return fail(error, QStringLiteral("%1 has unknown gemm_precision").arg(entry.id()));
    }

    QSet<QString> kinds;
    const QJsonArray files = object.value(u"files").toArray();
    for (const auto& value : files) {
        ModelFile file;
        if (!parseFile(value.toObject(), file, error)) {
            return false;
        }
        if (kinds.contains(file.kind)) {
            return fail(error, QStringLiteral("%1 lists '%2' twice").arg(entry.id(), file.kind));
        }
        kinds.insert(file.kind);
        entry.files.append(file);
    }
    if (!hasRequiredFiles(entry)) {
        return fail(error, QStringLiteral("%1 is missing model, lex or vocabulary files").arg(entry.id()));
    }
    return true;
}

} // namespace

QString ModelEntry::id() const
{
    return from + u'-' + to;
}

qint64 ModelEntry::totalSize() const
{
    qint64 total = 0;
    for (const ModelFile& file : files) {
        total += file.size;
    }
    return total;
}

const ModelFile* ModelEntry::file(QStringView kind) const
{
    for (const ModelFile& candidate : files) {
        if (candidate.kind == kind) {
            return &candidate;
        }
    }
    return nullptr;
}

const ModelEntry* Catalog::find(const QString& from, const QString& to) const
{
    for (const ModelEntry& entry : models) {
        if (entry.from == from && entry.to == to) {
            return &entry;
        }
    }
    return nullptr;
}

const ModelEntry* Catalog::find(const QString& id) const
{
    for (const ModelEntry& entry : models) {
        if (entry.id() == id) {
            return &entry;
        }
    }
    return nullptr;
}

QUrl Catalog::urlFor(const ModelFile& file) const
{
    QString base = baseUrl.toString();
    if (!base.endsWith(u'/')) {
        base += u'/';
    }
    return {base + file.asset};
}

std::optional<Catalog> parseCatalog(const QByteArray& json, QString* error)
{
    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        fail(error, QStringLiteral("not a JSON object: %1").arg(parseError.errorString()));
        return std::nullopt;
    }
    const QJsonObject root = document.object();

    Catalog catalog;
    catalog.schemaVersion = root.value(u"schema_version").toInt();
    if (catalog.schemaVersion != kCatalogSchemaVersion) {
        fail(error, QStringLiteral("unsupported schema_version %1").arg(catalog.schemaVersion));
        return std::nullopt;
    }
    catalog.baseUrl = QUrl(root.value(u"base_url").toString());
    if (!catalog.baseUrl.isValid() ||
        (catalog.baseUrl.scheme() != u"https" && !catalog.baseUrl.isLocalFile() &&
         catalog.baseUrl.host() != u"localhost")) {
        fail(error, QStringLiteral("base_url must be https"));
        return std::nullopt;
    }

    QSet<QString> ids;
    QSet<QString> assets;
    const QJsonArray models = root.value(u"models").toArray();
    for (const auto& value : models) {
        ModelEntry entry;
        if (!parseEntry(value.toObject(), entry, error)) {
            return std::nullopt;
        }
        if (ids.contains(entry.id())) {
            fail(error, QStringLiteral("%1 is listed twice").arg(entry.id()));
            return std::nullopt;
        }
        ids.insert(entry.id());
        for (const ModelFile& file : std::as_const(entry.files)) {
            if (assets.contains(file.asset)) {
                fail(error, QStringLiteral("asset '%1' is listed twice").arg(file.asset));
                return std::nullopt;
            }
            assets.insert(file.asset);
        }
        catalog.models.append(entry);
    }
    return catalog;
}

} // namespace glate::offline
