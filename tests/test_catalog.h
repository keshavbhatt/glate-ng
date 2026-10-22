#pragma once

// Builds catalogue JSON for the offline translation tests.

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

namespace glate::test {

inline QJsonObject fileJson(const QString& kind, const QString& name, const QByteArray& content)
{
    return QJsonObject{
        {QStringLiteral("kind"), kind},
        {QStringLiteral("name"), name},
        {QStringLiteral("asset"), QStringLiteral("x-") + name},
        {QStringLiteral("size"), content.size()},
        {
            QStringLiteral("sha256"),
            QString::fromLatin1(QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex()),
        },
    };
}

inline QJsonObject entryJson(const QString& from, const QString& to, const QString& version,
                             const QJsonArray& files)
{
    return QJsonObject{
        {QStringLiteral("from"), from},
        {QStringLiteral("to"), to},
        {QStringLiteral("version"), version},
        {QStringLiteral("gemm_precision"), QStringLiteral("int8shiftAlphaAll")},
        {QStringLiteral("files"), files},
    };
}

// en-de with three small files whose content is their own name.
inline QJsonObject sampleEntry(const QString& version = QStringLiteral("2.1"))
{
    return entryJson(
        QStringLiteral("en"), QStringLiteral("de"), version,
        QJsonArray{
            fileJson(QStringLiteral("model"), QStringLiteral("model.ende.bin"), "model.ende.bin"),
            fileJson(QStringLiteral("lex"), QStringLiteral("lex.ende.bin"), "lex.ende.bin"),
            fileJson(QStringLiteral("vocab"), QStringLiteral("vocab.ende.spm"), "vocab.ende.spm"),
        });
}

inline QByteArray catalogJson(const QJsonArray& models,
                              const QString& baseUrl = QStringLiteral("https://example.org/models/"))
{
    return QJsonDocument(QJsonObject{
                             {QStringLiteral("schema_version"), 1},
                             {QStringLiteral("base_url"), baseUrl},
                             {QStringLiteral("models"), models},
                         })
        .toJson();
}

} // namespace glate::test
