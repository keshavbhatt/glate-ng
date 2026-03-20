#include "history.h"
#include "ui_history.h"
#include "utils.h"

#include <QDir>
#include <QFile>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSettings>
#include <QCloseEvent>
#include <QShowEvent>

#include <algorithm>

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
    emit historyItemMeta(QStringList() << from << to << src1 << src2);
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
  // Transparent background so the QListWidget selection highlight shows through
  card->setStyleSheet(
      "QFrame#historyCard { background: transparent; border: none; }"
      "QFrame#historyCard QLabel { background: transparent; border: none; }");

  auto *root = new QVBoxLayout(card);
  root->setContentsMargins(10, 8, 10, 8);
  root->setSpacing(8);

  auto *topRow = new QHBoxLayout();
  auto *langLabel = new QLabel(record.from + "  \u2192  " + record.to, card);
  QFont langFont = langLabel->font();
  langFont.setBold(true);
  langLabel->setFont(langFont);

  auto *dateLabel = new QLabel(record.date, card);
  // Use placeholder/disabled text color — works correctly in both light and dark themes
  QPalette datePal = dateLabel->palette();
  datePal.setColor(QPalette::WindowText,
                   qApp->palette().color(QPalette::Disabled, QPalette::Text));
  dateLabel->setPalette(datePal);

  topRow->addWidget(langLabel);
  topRow->addStretch(1);
  topRow->addWidget(dateLabel);
  root->addLayout(topRow);

  auto *content = new QHBoxLayout();
  content->setSpacing(12);

  auto *textColumn = new QGridLayout();
  textColumn->setHorizontalSpacing(10);
  textColumn->setVerticalSpacing(4);

  auto *fromCaption = new QLabel(tr("Source"), card);
  auto *toCaption = new QLabel(tr("Translation"), card);
  QFont captionFont = fromCaption->font();
  captionFont.setBold(true);
  fromCaption->setFont(captionFont);
  toCaption->setFont(captionFont);

  auto *fromText = new QLabel(elideText(record.src1, 240), card);
  auto *toText = new QLabel(elideText(record.src2, 240), card);
  fromText->setWordWrap(true);
  toText->setWordWrap(true);

  textColumn->addWidget(fromCaption, 0, 0);
  textColumn->addWidget(toCaption, 0, 1);
  textColumn->addWidget(fromText, 1, 0);
  textColumn->addWidget(toText, 1, 1);
  content->addLayout(textColumn, 1);

  auto *actions = new QVBoxLayout();
  actions->setSpacing(6);
  auto *loadBtn = new QPushButton(tr("Load"), card);
  auto *removeBtn = new QPushButton(tr("Remove"), card);
  loadBtn->setToolTip(tr("Load this translation into the editor"));
  removeBtn->setToolTip(tr("Remove this history entry"));
  actions->addWidget(loadBtn);
  actions->addWidget(removeBtn);
  actions->addStretch(1);
  content->addLayout(actions);

  root->addLayout(content);

  // Subtle separator line at the bottom of each card
  auto *separator = new QFrame(card);
  separator->setFrameShape(QFrame::HLine);
  separator->setFrameShadow(QFrame::Sunken);
  root->addWidget(separator);

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
  item->setSizeHint(card->sizeHint());
  ui->historyList->setItemWidget(item, card);
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

