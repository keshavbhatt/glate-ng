#include "history.h"
#include "ui_history.h"
#include "utils.h"
#include "elidedlabel.h"

#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>

#include "ShellStyle.h"
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSettings>
#include <QCloseEvent>
#include <QShowEvent>

#include <algorithm>

namespace {
QString singleLinePreview(const QString &text) { return text.simplified(); }
} // namespace

History::History(QWidget *parent) : QWidget(parent), ui(new Ui::History) {
  ui->setupUi(this);
  {
    auto *_esc = new QShortcut(Qt::Key_Escape, this);
    connect(_esc, &QShortcut::activated, this, [this] { close(); });
  }
  {
    auto *loadShortcut = new QShortcut(QKeySequence(Qt::Key_Return), this);
    connect(loadShortcut, &QShortcut::activated, this, &History::onLoadSelected);
    auto *enterShortcut = new QShortcut(QKeySequence(Qt::Key_Enter), this);
    connect(enterShortcut, &QShortcut::activated, this, &History::onLoadSelected);
    auto *deleteShortcut = new QShortcut(QKeySequence::Delete, this);
    connect(deleteShortcut, &QShortcut::activated, this, &History::onDeleteSelected);
  }

  ui->historyList->setSelectionRectVisible(true);
  ui->historyList->setAlternatingRowColors(false);
  ui->historyList->setDragEnabled(false);
  ui->historyList->setDropIndicatorShown(false);
  ui->historyList->setDragDropMode(QAbstractItemView::NoDragDrop);
  ui->historyList->setUniformItemSizes(false);
  ui->historyList->setSpacing(8);

  connect(ui->searchEdit, &QLineEdit::textChanged,
          this, &History::onSearchChanged);
  connect(ui->sortBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &History::onSortChanged);
  connect(ui->historyList, &QListWidget::itemDoubleClicked,
          this, &History::onItemDoubleClicked);

  loadHistory();
}

History::~History() { delete ui; }

void History::on_clearall_clicked() {
  if (m_records.isEmpty()) {
    return;
  }

  const int answer = QMessageBox::question(
      this, tr("Clear translation history"),
      tr("This removes all saved history items. This action cannot be undone."),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer != QMessageBox::Yes) {
    return;
  }

  QDir dir(utils::returnPath("history"));
  if (dir.removeRecursively()) {
    utils::returnPath("history");
    m_records.clear();
    refreshList();
  }
}

void History::insertItem(const QStringList &meta, bool fromHistory,
                         const QString &translationId) {
  if (meta.count() < 5 || translationId.isEmpty()) {
    return;
  }

  if (!fromHistory) {
    save(meta, translationId);
    loadHistory();

    for (int i = 0; i < ui->historyList->count(); ++i) {
      QListWidgetItem *item = ui->historyList->item(i);
      if (item->data(Qt::UserRole).toString() == translationId) {
        ui->historyList->setCurrentItem(item);
        ui->historyList->scrollToItem(item, QAbstractItemView::PositionAtTop);
        break;
      }
    }
  }
}

void History::loadHistoryItem(const QString &itemPath) {
  QFile file(itemPath);
  if (file.exists()) {
    QJsonDocument doc = loadJson(itemPath);
    QJsonObject jsonObject = doc.object();
    QString from, to, src1, src2;
    from = jsonObject.value("from").toString();
    to = jsonObject.value("to").toString();
    src1 = jsonObject.value("src1").toString();
    src2 = jsonObject.value("src2").toString();
    QStringList meta = QStringList() << from << to << src1 << src2;
    if (jsonObject.value("offline").toBool())
      meta << QStringLiteral("offline");
    emit historyItemMeta(meta);
  }
}

void History::save(const QStringList &meta, const QString &translationId) {
  if (meta.count() >= 5) {
    QString from, to, src1, src2, date;
    from = meta.at(0);
    to = meta.at(1);
    src1 = meta.at(2);
    src2 = meta.at(3);
    date = meta.at(4);
    QString history_file_path =
        QDir(utils::returnPath("history")).filePath(translationId + ".json");
    QVariantMap map;
    map.insert("date", date);
    map.insert("from", from);
    map.insert("to", to);
    map.insert("src1", src1);
    map.insert("src2", src2);
    if (meta.value(5) == QLatin1String("offline"))
      map.insert("offline", true);
    QJsonObject object = QJsonObject::fromVariantMap(map);
    QJsonDocument document;
    document.setObject(object);
    saveJson(document, history_file_path);
  }
}

QJsonDocument History::loadJson(const QString &fileName) {
  QFile jsonFile(fileName);
  if (!jsonFile.open(QIODevice::ReadOnly)) {
    return QJsonDocument();
  }

  QJsonParseError parseError;
  const QByteArray data = jsonFile.readAll();
  const QJsonDocument document = QJsonDocument::fromJson(data, &parseError);

  if (parseError.error != QJsonParseError::NoError) {
    return QJsonDocument();
  }

  return document;
}

void History::saveJson(const QJsonDocument &document, const QString &fileName) {
  QFile jsonFile(fileName);
  if (!jsonFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return;
  }

  const qint64 written = jsonFile.write(document.toJson(QJsonDocument::Compact));
  if (written < 0) {
    return;
  }
}

void History::loadHistory() {
  m_records.clear();
  QString history_path = utils::returnPath("history");
  QDir dir(history_path);
  const QFileInfoList files =
      dir.entryInfoList(QStringList() << "*.json", QDir::Files, QDir::Time);

  for (const QFileInfo &fileInfo : files) {
    const QJsonDocument doc = loadJson(fileInfo.filePath());
    if (!doc.isObject()) {
      continue;
    }

    QJsonObject jsonObject = doc.object();
    HistoryRecord record;
    record.id = fileInfo.baseName();
    record.from = jsonObject.value("from").toString();
    record.to = jsonObject.value("to").toString();
    record.src1 = jsonObject.value("src1").toString();
    record.src2 = jsonObject.value("src2").toString();
    record.date = jsonObject.value("date").toString();
    record.offline = jsonObject.value("offline").toBool();
    record.modifiedAt = fileInfo.lastModified();
    if (record.date.trimmed().isEmpty()) {
      record.date = record.modifiedAt.toLocalTime().toString();
    }
    m_records.append(record);
  }

  refreshList();
}

void History::onSearchChanged(const QString &text) {
  m_searchText = text.trimmed();
  refreshList();
}

void History::onSortChanged(int index) {
  Q_UNUSED(index);
  refreshList();
}

void History::onItemDoubleClicked(QListWidgetItem *item) {
  if (!item) {
    return;
  }
  ui->historyList->setCurrentItem(item);
  onLoadSelected();
}

void History::onDeleteSelected() {
  const QListWidgetItem *current = ui->historyList->currentItem();
  if (!current) {
    return;
  }

  const QString id = current->data(Qt::UserRole).toString();
  if (id.isEmpty()) {
    return;
  }

  QFile file(QDir(utils::returnPath("history")).filePath(id + ".json"));
  if (file.remove()) {
    loadHistory();
  }
}

void History::onLoadSelected() {
  const QListWidgetItem *current = ui->historyList->currentItem();
  if (!current) {
    return;
  }

  const QString id = current->data(Qt::UserRole).toString();
  if (id.isEmpty()) {
    return;
  }

  emit setTranslationId(id);
  loadHistoryItem(QDir(utils::returnPath("history")).filePath(id + ".json"));
}

void History::refreshList() {
  ui->historyList->clear();

  QList<HistoryRecord> visible = m_records;
  std::sort(visible.begin(), visible.end(), [this](const HistoryRecord &a,
                                                   const HistoryRecord &b) {
    if (ui->sortBox->currentIndex() == 1) {
      return a.modifiedAt < b.modifiedAt;
    }
    return a.modifiedAt > b.modifiedAt;
  });

  for (const HistoryRecord &record : visible) {
    if (matchesSearch(record)) {
      addRecordItem(record);
    }
  }

  updateUiState();
}

void History::addRecordItem(const HistoryRecord &record) {
  auto *card = new QFrame();
  card->setObjectName("historyCard");
  card->setFrameShape(QFrame::NoFrame);
  card->setStyleSheet(QStringLiteral(
      "QFrame#historyCard {"
      "  background-color: palette(base);"
      "  border: 1px solid palette(mid);"
      "  border-radius: 8px;"
      "}"));

  auto *root = new QVBoxLayout(card);
  root->setContentsMargins(12, 10, 12, 10);
  root->setSpacing(8);

  auto *topRow = new QHBoxLayout();
  topRow->setSpacing(10);
  auto *langLabel = new QLabel(record.from + "  \u2192  " + record.to, card);
  QFont langFont = langLabel->font();
  langFont.setBold(true);
  langLabel->setFont(langFont);
  langLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  langLabel->setFixedHeight(langLabel->fontMetrics().height() + 4);

  auto *dateLabel = new QLabel(record.date, card);
  // Use placeholder/disabled text color, works correctly in both light and dark themes
  QPalette datePal = dateLabel->palette();
  datePal.setColor(QPalette::WindowText,
                   qApp->palette().color(QPalette::Disabled, QPalette::Text));
  dateLabel->setPalette(datePal);
  dateLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
  dateLabel->setFixedHeight(dateLabel->fontMetrics().height() + 4);

  topRow->addWidget(langLabel);
  if (record.offline) {
    auto *offlineLabel = new QLabel(tr("Offline"), card);
    offlineLabel->setToolTip(tr("Translated on this device"));
    // The card's style sheet wins over a palette, so the colour goes in one too.
    offlineLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(glate::shell::offlineColor(card).name()));
    offlineLabel->setFixedHeight(langLabel->fontMetrics().height() + 4);
    offlineLabel->setAlignment(Qt::AlignVCenter);
    QFont small = offlineLabel->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    offlineLabel->setFont(small);
    topRow->addWidget(offlineLabel);
  }
  topRow->addStretch(1);
  topRow->addWidget(dateLabel);
  auto *loadBtn = new QPushButton(tr("Load"), card);
  auto *removeBtn = new QPushButton(tr("Remove"), card);
  loadBtn->setToolTip(tr("Load this translation into the editor"));
  removeBtn->setToolTip(tr("Remove this history entry"));
  loadBtn->setFixedHeight(24);
  removeBtn->setFixedHeight(24);
  topRow->addSpacing(6);
  topRow->addWidget(loadBtn);
  topRow->addWidget(removeBtn);
  root->addLayout(topRow);

  auto *textColumn = new QGridLayout();
  textColumn->setHorizontalSpacing(10);
  textColumn->setVerticalSpacing(0);

  auto *fromText = new ElidedLabel(singleLinePreview(record.src1), Qt::ElideRight, card);
  auto *toText   = new ElidedLabel(singleLinePreview(record.src2), Qt::ElideRight, card);
  fromText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  toText->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  fromText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  toText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  fromText->setFixedHeight(fromText->fontMetrics().height() + 6);
  toText->setFixedHeight(toText->fontMetrics().height() + 6);

  textColumn->addWidget(fromText, 0, 0);
  textColumn->addWidget(toText, 0, 1);
  root->addLayout(textColumn);

  connect(loadBtn, &QPushButton::clicked, this, [this, record]() {
    emit setTranslationId(record.id);
    loadHistoryItem(QDir(utils::returnPath("history"))
                        .filePath(record.id + ".json"));
  });
  connect(removeBtn, &QPushButton::clicked, this, [this, record]() {
    QFile file(
        QDir(utils::returnPath("history")).filePath(record.id + ".json"));
    if (file.remove()) {
      loadHistory();
    }
  });

  auto *item = new QListWidgetItem();
  item->setData(Qt::UserRole, record.id);
  ui->historyList->addItem(item);
  ui->historyList->setItemWidget(item, card);
  const int rowHeight = root->contentsMargins().top() +
                        topRow->sizeHint().height() +
                        root->spacing() +
                        textColumn->sizeHint().height() +
                        root->contentsMargins().bottom();
  item->setSizeHint(QSize(ui->historyList->viewport()->width(), rowHeight));
}

bool History::matchesSearch(const HistoryRecord &record) const {
  if (m_searchText.isEmpty()) {
    return true;
  }

  return record.from.contains(m_searchText, Qt::CaseInsensitive) ||
         record.to.contains(m_searchText, Qt::CaseInsensitive) ||
         record.src1.contains(m_searchText, Qt::CaseInsensitive) ||
         record.src2.contains(m_searchText, Qt::CaseInsensitive) ||
         record.date.contains(m_searchText, Qt::CaseInsensitive);
}

void History::updateUiState() {
  const int visibleCount = ui->historyList->count();
  const int totalCount = m_records.count();

  if (totalCount == 0) {
    ui->emptyLabel->setText(tr("No translations yet. Your recent results appear here automatically."));
  } else if (visibleCount == 0) {
    ui->emptyLabel->setText(tr("No history items match your search."));
  }

  ui->emptyLabel->setVisible(visibleCount == 0);
  ui->clearall->setEnabled(totalCount > 0);
  ui->label_2->setText(
      visibleCount == totalCount
          ? tr("%1 saved translations.").arg(totalCount)
          : tr("Showing %1 of %2 saved translations.").arg(visibleCount).arg(totalCount));
}


QString History::elideText(const QString &text, int maxChars) {
  QString cleaned = text.simplified();
  if (cleaned.size() <= maxChars) {
    return cleaned;
  }
  return cleaned.left(maxChars - 3) + "...";
}

void History::closeEvent(QCloseEvent *event) {
  QSettings settings;
  settings.setValue("history/geometry", saveGeometry());
  QWidget::closeEvent(event);
}

void History::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  QSettings settings;
  const QByteArray geom = settings.value("history/geometry").toByteArray();
  if (!geom.isEmpty()) {
    restoreGeometry(geom);
  }
}
