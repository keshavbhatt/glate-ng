#ifndef HISTORY_H
#define HISTORY_H

#include <QJsonDocument>
#include <QDateTime>
#include <QListWidgetItem>
#include <QSettings>
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
  void save(const QStringList &meta, const QString &translationId);
  void insertItem(const QStringList &meta, bool fromHistory, const QString &translationId);
  void loadHistoryItem(const QString &itemPath);

private slots:
  void on_clearall_clicked();
  void onSearchChanged(const QString &text);
  void onSortChanged(int index);
  void onItemDoubleClicked(QListWidgetItem *item);
  void onDeleteSelected();
  void onLoadSelected();
  static QJsonDocument loadJson(const QString &fileName);
  static void saveJson(const QJsonDocument &document, const QString &fileName);

private:
  void refreshList();
  void addRecordItem(const HistoryRecord &record);
  bool matchesSearch(const HistoryRecord &record) const;
  void updateUiState();
  static QString elideText(const QString &text, int maxChars);

protected:
  void closeEvent(QCloseEvent *event) override;
  void showEvent(QShowEvent *event) override;

  Ui::History *ui;
  QList<HistoryRecord> m_records;
  QString m_searchText;
};

#endif // HISTORY_H
