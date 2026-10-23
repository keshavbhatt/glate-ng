#ifndef ERROR_H
#define ERROR_H

#include <QWidget>

class QPushButton;

namespace Ui {
class Error;
}

class Error : public QWidget {
  Q_OBJECT

public:
  explicit Error(QWidget *parent = nullptr);
  ~Error();

public slots:
  // heading: what failed ("Could not translate"); explanation: what to do;
  // detail: the technical message, also passed on to a bug report.
  void setError(const QString &heading, const QString &explanation,
                const QString &detail);

signals:
  void reportRequested(const QString &detail);

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void on_ok_clicked();

private:
  Ui::Error *ui;
  QPushButton *m_report = nullptr;
  void restyle();

  QString m_heading;
  QString m_explanation;
  QString m_detail;
};

#endif // ERROR_H
