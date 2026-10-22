#include "ModelStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace glate::offline {

namespace {

constexpr QStringView kManifestName = u"model.json";
constexpr QStringView kStagingName = u".staging";

bool fail(QString* error, const QString& message)
{
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

QByteArray manifestFor(const ModelEntry& entry)
{
    QJsonArray files;
    for (const ModelFile& file : entry.files) {
        files.append(QJsonObject{
            {QStringLiteral("kind"), file.kind},
            {QStringLiteral("name"), file.name},
            {QStringLiteral("size"), file.size},
            {QStringLiteral("sha256"), QString::fromLatin1(file.sha256)},
        });
    }
    const QJsonObject root{
        {QStringLiteral("from"), entry.from},       {QStringLiteral("to"), entry.to},
        {QStringLiteral("version"), entry.version}, {QStringLiteral("gemm_precision"), entry.gemmPrecision},
        {QStringLiteral("files"), files},
    };
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::optional<InstalledModel> loadInstalled(const QString& directory)
{
    QFile manifest(QDir(directory).filePath(kManifestName.toString()));
    if (!manifest.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const QJsonObject root = QJsonDocument::fromJson(manifest.readAll()).object();

    InstalledModel model;
    model.from = root.value(u"from").toString();
    model.to = root.value(u"to").toString();
    model.version = root.value(u"version").toString();
    model.gemmPrecision = root.value(u"gemm_precision").toString();
    model.directory = directory;
    if (model.from.isEmpty() || model.to.isEmpty() || QFileInfo(directory).fileName() != model.id()) {
        return std::nullopt;
    }

    const QJsonArray files = root.value(u"files").toArray();
    for (const auto& value : files) {
        const QJsonObject file = value.toObject();
        const QString name = file.value(u"name").toString();
        const QFileInfo info(QDir(directory).filePath(name));
        // A file that went missing or changed size means a broken install.
        if (name.contains(u'/') || !info.isFile() || info.size() != file.value(u"size").toInteger(-1)) {
            return std::nullopt;
        }
        model.files.insert(file.value(u"kind").toString(), info.absoluteFilePath());
        model.size += info.size();
    }
    if (!model.files.contains(QStringLiteral("model")) || !model.files.contains(QStringLiteral("lex"))) {
        return std::nullopt;
    }
    return model;
}

} // namespace

QString InstalledModel::id() const
{
    return from + u'-' + to;
}

ModelStore::ModelStore(QString root)
    : m_root(std::move(root))
{}

QString ModelStore::directoryFor(const QString& id) const
{
    return QDir(m_root).filePath(id);
}

QString ModelStore::stagingDirectoryFor(const QString& id) const
{
    return QDir(QDir(m_root).filePath(kStagingName.toString())).filePath(id);
}

QString ModelStore::catalogCachePath() const
{
    return QDir(m_root).filePath(QStringLiteral("catalog.json"));
}

QList<InstalledModel> ModelStore::installed() const
{
    QList<InstalledModel> models;
    const QDir root(m_root);
    const QStringList names = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& name : names) {
        if (name.startsWith(u'.')) {
            continue;
        }
        if (auto model = loadInstalled(root.filePath(name))) {
            models.append(*model);
        }
    }
    return models;
}

std::optional<InstalledModel> ModelStore::find(const QString& from, const QString& to) const
{
    return find(from + u'-' + to);
}

std::optional<InstalledModel> ModelStore::find(const QString& id) const
{
    if (id.isEmpty() || id.contains(u'/') || id.contains(u'\\') || id.startsWith(u'.')) {
        return std::nullopt;
    }
    return loadInstalled(directoryFor(id));
}

bool ModelStore::commit(const ModelEntry& entry, QString* error) const
{
    const QString staging = stagingDirectoryFor(entry.id());
    const QString target = directoryFor(entry.id());

    QSaveFile manifest(QDir(staging).filePath(kManifestName.toString()));
    if (!manifest.open(QIODevice::WriteOnly) || manifest.write(manifestFor(entry)) < 0 ||
        !manifest.commit()) {
        return fail(
            error, QStringLiteral("could not write %1: %2").arg(manifest.fileName(), manifest.errorString()));
    }

    // Swap directories so a crash leaves either the old or the new model, never half of one.
    const QString backup = target + QStringLiteral(".old");
    QDir(backup).removeRecursively();
    if (QFileInfo::exists(target) && !QDir().rename(target, backup)) {
        return fail(error, QStringLiteral("could not move the old %1 aside").arg(entry.id()));
    }
    if (!QDir().rename(staging, target)) {
        QDir().rename(backup, target);
        return fail(error, QStringLiteral("could not move %1 into place").arg(entry.id()));
    }
    QDir(backup).removeRecursively();
    if (!find(entry.id())) {
        return fail(error, QStringLiteral("%1 did not verify after install").arg(entry.id()));
    }
    return true;
}

bool ModelStore::remove(const QString& id) const
{
    if (!find(id)) {
        return false;
    }
    return QDir(directoryFor(id)).removeRecursively();
}

} // namespace glate::offline
