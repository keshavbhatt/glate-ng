#include "offlinemodelsdialog.h"

#include "Languages.h"
#include "ModelManager.h"
#include "ModelStore.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

using glate::offline::ModelManager;

namespace {

enum Column
{
    LanguageColumn,
    SizeColumn,
    StatusColumn,
    ActionColumn,
};

constexpr int kCodeRole = Qt::UserRole;

} // namespace

OfflineModelsDialog::OfflineModelsDialog(ModelManager* manager, const glate::offline::ModelStore* store,
                                         QHash<QString, QString> languageNames, QWidget* parent)
    : QDialog(parent)
    , m_manager(manager)
    , m_store(store)
    , m_languageNames(std::move(languageNames))
    , m_filter(new QLineEdit(this))
    , m_table(new QTableWidget(0, 4, this))
    , m_summary(new QLabel(this))
    , m_refresh(new QPushButton(tr("Refresh list"), this))
{
    setWindowTitle(tr("Glate | Offline languages"));
    resize(560, 520);

    auto* intro = new QLabel(tr("Download a language to translate it to and from English on this computer. "
                                "Other pairs go through English. Nothing you translate offline leaves "
                                "your device."),
                             this);
    intro->setWordWrap(true);

    m_filter->setPlaceholderText(tr("Search languages"));
    m_filter->setClearButtonEnabled(true);

    m_table->setHorizontalHeaderLabels({tr("Language"), tr("Size"), tr("Status"), QString()});
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setFocusPolicy(Qt::NoFocus);
    m_table->setShowGrid(false);
    m_table->horizontalHeader()->setSectionResizeMode(LanguageColumn, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(SizeColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(StatusColumn, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(ActionColumn, QHeaderView::ResizeToContents);

    auto* credit = new QLabel(tr("Models by Mozilla (Firefox Translations), MPL-2.0."), this);
    credit->setEnabled(false);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(m_summary, 1);
    bottom->addWidget(m_refresh);

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(intro);
    layout->addWidget(m_filter);
    layout->addWidget(m_table, 1);
    layout->addLayout(bottom);
    layout->addWidget(credit);

    connect(m_filter, &QLineEdit::textChanged, this, &OfflineModelsDialog::applyFilter);
    connect(m_refresh, &QPushButton::clicked, this, [this] {
        m_refresh->setEnabled(false);
        m_summary->setText(tr("Fetching the language list..."));
        m_manager->refreshCatalog();
    });
    connect(m_manager, &ModelManager::catalogChanged, this, [this] {
        m_refresh->setEnabled(true);
        rebuild();
    });
    connect(m_manager, &ModelManager::catalogFailed, this, [this](const QString& reason) {
        m_refresh->setEnabled(true);
        m_summary->setText(tr("Could not fetch the language list: %1").arg(reason));
    });
    connect(m_manager, &ModelManager::downloadProgress, this,
            [this](const QString& id, qint64 got, qint64 total) {
                const int row = rowFor(id);
                if (row < 0 || total <= 0) {
                    return;
                }
                const QString code = m_table->item(row, LanguageColumn)->data(kCodeRole).toString();
                // Two directions per language: the first counts for the first half.
                const QStringList ids = directionsFor(code);
                const int part = static_cast<int>(ids.indexOf(id));
                const double fraction = static_cast<double>(got) / static_cast<double>(total);
                const auto parts = static_cast<double>(std::max<qsizetype>(1, ids.size()));
                m_progress[code] = static_cast<int>(100.0 * (part + fraction) / parts);
                refreshRow(row);
            });
    auto refreshFor = [this](const QString& id) {
        const int row = rowFor(id);
        if (row >= 0) {
            m_progress.remove(m_table->item(row, LanguageColumn)->data(kCodeRole).toString());
            refreshRow(row);
        }
        updateSummary();
    };
    connect(m_manager, &ModelManager::installed, this, refreshFor);
    connect(m_manager, &ModelManager::removed, this, refreshFor);
    connect(m_manager, &ModelManager::queueChanged, this, [this] {
        for (int row = 0; row < m_table->rowCount(); ++row) {
            refreshRow(row);
        }
    });
    connect(m_manager, &ModelManager::installFailed, this,
            [this, refreshFor](const QString& id, const QString& why) {
                refreshFor(id);
                QMessageBox::warning(this, windowTitle(), tr("Could not download %1:\n%2").arg(id, why));
            });

    rebuild();
    if (!m_manager->catalog()) {
        m_refresh->click();
    }
}

QStringList OfflineModelsDialog::directionsFor(const QString& glateCode) const
{
    QStringList ids;
    const auto& catalog = m_manager->catalog();
    if (!catalog) {
        return ids;
    }
    const QString code = glate::offline::toModelCode(glateCode);
    const QString pivot = glate::offline::kPivotLanguage.toString();
    for (const auto& [from, to] : {std::pair{code, pivot}, std::pair{pivot, code}}) {
        if (catalog->find(from, to) != nullptr) {
            ids.append(from + u'-' + to);
        }
    }
    return ids;
}

bool OfflineModelsDialog::isInstalled(const QString& glateCode) const
{
    const QStringList ids = directionsFor(glateCode);
    return !ids.isEmpty() && std::all_of(ids.cbegin(), ids.cend(),
                                         [this](const QString& id) { return m_store->find(id).has_value(); });
}

void OfflineModelsDialog::downloadLanguages(const QStringList& glateCodes)
{
    for (const QString& code : glateCodes) {
        for (const QString& id : directionsFor(code)) {
            if (!m_store->find(id)) {
                m_manager->install(id);
            }
        }
    }
    show();
    raise();
    activateWindow();
}

void OfflineModelsDialog::rebuild()
{
    m_table->setRowCount(0);
    const auto& catalog = m_manager->catalog();
    if (!catalog) {
        updateSummary();
        return;
    }

    // Languages with a catalogue entry, sorted by display name.
    QStringList codes;
    for (const auto& entry : catalog->models) {
        for (const QString& side : {entry.from, entry.to}) {
            const QString code = glate::offline::toGlateCode(side);
            if (side != glate::offline::kPivotLanguage && !codes.contains(code)) {
                codes.append(code);
            }
        }
    }
    // Downloaded languages first, then by name.
    std::sort(codes.begin(), codes.end(), [this](const QString& a, const QString& b) {
        const bool aInstalled = isInstalled(a);
        if (aInstalled != isInstalled(b)) {
            return aInstalled;
        }
        return m_languageNames.value(a, a).localeAwareCompare(m_languageNames.value(b, b)) < 0;
    });

    // Rows tall enough for the theme's buttons, and one width for every button
    // whatever its label (Download, Remove, Cancel).
    QPushButton probe(m_table);
    int buttonWidth = 0;
    for (const QString& label : {tr("Download"), tr("Remove"), tr("Cancel")}) {
        probe.setText(label);
        buttonWidth = std::max(buttonWidth, probe.sizeHint().width());
    }
    m_table->verticalHeader()->setMinimumSectionSize(probe.sizeHint().height() + 6);
    m_table->verticalHeader()->setDefaultSectionSize(probe.sizeHint().height() + 6);

    m_table->setRowCount(static_cast<int>(codes.size()));
    for (int row = 0; row < codes.size(); ++row) {
        const QString& code = codes.at(row);
        auto* name = new QTableWidgetItem(m_languageNames.value(code, code));
        name->setData(kCodeRole, code);
        m_table->setItem(row, LanguageColumn, name);

        qint64 bytes = 0;
        for (const QString& id : directionsFor(code)) {
            bytes += catalog->find(id)->totalSize();
        }
        auto* size = new QTableWidgetItem(QLocale().formattedDataSize(bytes, 0));
        size->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_table->setItem(row, SizeColumn, size);
        m_table->setItem(row, StatusColumn, new QTableWidgetItem);

        auto* button = new QPushButton(m_table);
        button->setFixedWidth(buttonWidth);
        connect(button, &QPushButton::clicked, this, [this, code] {
            const QStringList ids = directionsFor(code);
            const bool busy = std::any_of(ids.cbegin(), ids.cend(),
                                          [this](const QString& id) { return m_manager->isQueued(id); });
            if (busy) {
                for (const QString& id : ids) {
                    m_manager->cancel(id);
                }
                m_progress.remove(code);
            } else if (isInstalled(code)) {
                for (const QString& id : ids) {
                    m_manager->remove(id);
                }
            } else {
                downloadLanguages({code});
            }
            const int at = rowFor(ids.value(0));
            if (at >= 0) {
                refreshRow(at);
            }
        });
        m_table->setCellWidget(row, ActionColumn, button);
        refreshRow(row);
    }
    applyFilter(m_filter->text());
    updateSummary();
}

void OfflineModelsDialog::refreshRow(int row)
{
    const QString code = m_table->item(row, LanguageColumn)->data(kCodeRole).toString();
    const QStringList ids = directionsFor(code);
    const QStringList updates = m_manager->updatesAvailable();
    const bool busy =
        std::any_of(ids.cbegin(), ids.cend(), [this](const QString& id) { return m_manager->isQueued(id); });
    const bool installed = isInstalled(code);
    const bool outdated =
        std::any_of(ids.cbegin(), ids.cend(), [&updates](const QString& id) { return updates.contains(id); });

    QString status;
    QString action;
    if (busy) {
        status =
            m_progress.contains(code) ? tr("Downloading %1%").arg(m_progress.value(code)) : tr("Waiting");
        action = tr("Cancel");
    } else if (installed && outdated) {
        status = tr("Update available");
        action = tr("Remove");
    } else if (installed) {
        status = tr("Downloaded");
        action = tr("Remove");
    } else {
        status = QString();
        action = tr("Download");
    }
    m_table->item(row, StatusColumn)->setText(status);
    if (auto* button = qobject_cast<QPushButton*>(m_table->cellWidget(row, ActionColumn))) {
        button->setText(action);
    }
}

void OfflineModelsDialog::updateSummary()
{
    if (!m_manager->catalog()) {
        m_summary->setText(tr("The language list has not been fetched yet."));
        return;
    }
    qint64 bytes = 0;
    int languages = 0;
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (isInstalled(m_table->item(row, LanguageColumn)->data(kCodeRole).toString())) {
            ++languages;
        }
    }
    for (const auto& model : m_store->installed()) {
        bytes += model.size;
    }
    const QString size = QLocale().formattedDataSize(bytes, 1);
    if (languages == 0) {
        m_summary->setText(tr("No languages downloaded"));
    } else if (languages == 1) {
        m_summary->setText(tr("1 language downloaded, %1").arg(size));
    } else {
        m_summary->setText(tr("%1 languages downloaded, %2").arg(languages).arg(size));
    }
}

void OfflineModelsDialog::applyFilter(const QString& text)
{
    for (int row = 0; row < m_table->rowCount(); ++row) {
        m_table->setRowHidden(
            row, !m_table->item(row, LanguageColumn)->text().contains(text, Qt::CaseInsensitive));
    }
}

int OfflineModelsDialog::rowFor(const QString& directionId) const
{
    const QString pivot = glate::offline::kPivotLanguage.toString();
    // The language is the side that is not the pivot ("en-zh-Hans" -> "zh-Hans").
    QString code = directionId;
    if (directionId.startsWith(pivot + u'-')) {
        code = directionId.mid(pivot.size() + 1);
    } else if (directionId.endsWith(u'-' + pivot)) {
        code = directionId.left(directionId.size() - pivot.size() - 1);
    }
    code = glate::offline::toGlateCode(code);
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (m_table->item(row, LanguageColumn)->data(kCodeRole).toString() == code) {
            return row;
        }
    }
    return -1;
}
