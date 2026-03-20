#ifndef HISTORY_H
#define HISTORY_H

#include <QJsonDocument>
#include <QDateTime>
#include <QListWidgetItem>
#include <QWidget>

namespace Ui {
class History;
}

struct HistoryRecord {
  QString id;
  QString from;
  QString to;
  QString src1;
  QString src2;
  QString date;
  QDateTime modifiedAt;
};

class History : public QWidget {
  Q_OBJECT

public:
  explicit History(QWidget *parent = nullptr);
  ~History();

signals:
  void historyItemMeta(QStringList historyItemMeta);
  void setTranslationId(QString transId);
public slots:
  void loadHistory();
  void save(QStringList meta, QString translationId);
  void insertItem(QStringList meta, bool fromHistory, QString translationId);
  void loadHistoryItem(QString itemPath);

private slots:
  void on_clearall_clicked();
  void onSearchChanged(const QString &text);
  void onSortChanged(int index);
  void onItemDoubleClicked(QListWidgetItem *item);
  void onDeleteSelected();
  void onLoadSelected();
  static QJsonDocument loadJson(QString fileName);
  static void saveJson(QJsonDocument document, QString fileName);

private:
  void refreshList();
  void addRecordItem(const HistoryRecord &record);
  bool matchesSearch(const HistoryRecord &record) const;
  void updateUiState();
  static QString elideText(const QString &text, int maxChars);

  Ui::History *ui;
  QList<HistoryRecord> m_records;
  QString m_searchText;
};

#endif // HISTORY_H
