#pragma once

#include <QByteArray>
#include <QList>
#include <QString>
#include <QUrl>

#include <optional>

namespace glate::offline {

// Version of catalog.json this build reads. A catalogue with another version is
// refused rather than half understood.
inline constexpr int kCatalogSchemaVersion = 1;

// One file of a model direction.
struct ModelFile
{
    QString kind;      // "model", "lex", "vocab", "srcvocab" or "trgvocab"
    QString name;      // file name on disk, as the engine expects it
    QString asset;     // release asset name, unique across the catalogue
    qint64 size = 0;   // bytes
    QByteArray sha256; // lowercase hex
};

// One direction (en -> de) at one version.
struct ModelEntry
{
    QString from;
    QString to;
    QString version;
    QString gemmPrecision; // engine setting for the model's int8 format
    QList<ModelFile> files;

    [[nodiscard]] QString id() const;
    [[nodiscard]] qint64 totalSize() const;
    [[nodiscard]] const ModelFile* file(QStringView kind) const;
};

struct Catalog
{
    int schemaVersion = 0;
    QUrl baseUrl; // release download URL; asset names are appended
    QList<ModelEntry> models;

    [[nodiscard]] const ModelEntry* find(const QString& from, const QString& to) const;
    [[nodiscard]] const ModelEntry* find(const QString& id) const;
    [[nodiscard]] QUrl urlFor(const ModelFile& file) const;
};

// Parses and validates catalog.json. On failure returns nothing and, when
// `error` is given, says why.
[[nodiscard]] std::optional<Catalog> parseCatalog(const QByteArray& json, QString* error = nullptr);

} // namespace glate::offline
