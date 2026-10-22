#pragma once

#include "ModelCatalog.h"

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

namespace glate::offline {

// A direction installed on disk.
struct InstalledModel
{
    QString from;
    QString to;
    QString version;
    QString gemmPrecision;
    QString directory;
    QHash<QString, QString> files; // kind -> absolute path
    qint64 size = 0;

    [[nodiscard]] QString id() const;
};

// Installed models under one root directory:
//
//   <root>/<from>-<to>/        the model's files and model.json
//   <root>/.staging/<id>/      downloads in progress
//   <root>/catalog.json        last catalogue fetched, for listing while offline
//
// Not thread-safe; used from the GUI thread.
class ModelStore
{
public:
    explicit ModelStore(QString root);

    [[nodiscard]] const QString& root() const { return m_root; }
    [[nodiscard]] QString directoryFor(const QString& id) const;
    [[nodiscard]] QString stagingDirectoryFor(const QString& id) const;
    [[nodiscard]] QString catalogCachePath() const;

    [[nodiscard]] QList<InstalledModel> installed() const;
    [[nodiscard]] std::optional<InstalledModel> find(const QString& from, const QString& to) const;
    [[nodiscard]] std::optional<InstalledModel> find(const QString& id) const;

    // Moves a fully downloaded and verified staging directory into place,
    // replacing any older version of the same direction.
    bool commit(const ModelEntry& entry, QString* error = nullptr) const;

    [[nodiscard]] bool remove(const QString& id) const;

private:
    QString m_root;
};

} // namespace glate::offline
