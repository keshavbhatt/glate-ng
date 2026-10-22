#include "EngineConfig.h"

#include <QDir>

namespace glate::offline {

namespace {

// A YAML double-quoted scalar. Paths use forward slashes on every platform, so
// only quotes and backslashes need escaping.
std::string quoted(const QString& path)
{
    QString text = QDir::fromNativeSeparators(path);
    text.replace(u'\\', QStringLiteral("\\\\"));
    text.replace(u'"', QStringLiteral("\\\""));
    return "\"" + text.toStdString() + "\"";
}

} // namespace

std::string engineConfig(const InstalledModel& model)
{
    const QString vocab = model.files.value(QStringLiteral("vocab"));
    const QString srcVocab = model.files.value(QStringLiteral("srcvocab"), vocab);
    const QString trgVocab = model.files.value(QStringLiteral("trgvocab"), vocab);
    const QString precision =
        model.gemmPrecision.isEmpty() ? QStringLiteral("int8shiftAlphaAll") : model.gemmPrecision;

    std::string yaml;
    yaml += "models: [" + quoted(model.files.value(QStringLiteral("model"))) + "]\n";
    yaml += "vocabs: [" + quoted(srcVocab) + ", " + quoted(trgVocab) + "]\n";
    yaml += "shortlist: [" + quoted(model.files.value(QStringLiteral("lex"))) + ", false]\n";
    yaml += "gemm-precision: " + precision.toStdString() + "\n";
    yaml += "beam-size: 1\n"
            "normalize: 1.0\n"
            "word-penalty: 0\n"
            "max-length-break: 128\n"
            "mini-batch-words: 1024\n"
            "workspace: 128\n"
            "max-length-factor: 2.0\n"
            "skip-cost: true\n"
            "cpu-threads: 0\n"
            "quiet: true\n"
            "quiet-translation: true\n"
            "alignment: soft\n"
            "ssplit-mode: paragraph\n";
    return yaml;
}

} // namespace glate::offline
