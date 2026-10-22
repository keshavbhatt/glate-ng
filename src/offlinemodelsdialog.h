#pragma once

#include <QDialog>
#include <QHash>
#include <QStringList>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

namespace glate::offline {
class ModelManager;
class ModelStore;
} // namespace glate::offline

// Lists the languages that can be translated offline and downloads or removes
// them. A language means both of its directions (to and from English), which
// is how people think of it; English itself needs no download.
class OfflineModelsDialog : public QDialog
{
    Q_OBJECT

public:
    // `languageNames` maps glate language codes to display names.
    OfflineModelsDialog(glate::offline::ModelManager* manager, const glate::offline::ModelStore* store,
                        QHash<QString, QString> languageNames, QWidget* parent = nullptr);

    // Queues the downloads for these glate language codes and shows the dialog.
    void downloadLanguages(const QStringList& glateCodes);

    // Direction ids ("de-en", "en-de") a glate language needs, as listed in the catalogue.
    [[nodiscard]] QStringList directionsFor(const QString& glateCode) const;

private:
    void rebuild();
    void refreshRow(int row);
    void updateSummary();
    void applyFilter(const QString& text);
    [[nodiscard]] int rowFor(const QString& directionId) const;
    [[nodiscard]] bool isInstalled(const QString& glateCode) const;

    glate::offline::ModelManager* m_manager;
    const glate::offline::ModelStore* m_store;
    QHash<QString, QString> m_languageNames;
    QLineEdit* m_filter;
    QTableWidget* m_table;
    QLabel* m_summary;
    QPushButton* m_refresh;
    QHash<QString, int> m_progress; // glate code -> percent while downloading
};
