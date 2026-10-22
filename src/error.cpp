#include "error.h"
#include "ui_error.h"

#include <QPushButton>
#include <QShortcut>

Error::Error(QWidget *parent) : QWidget(parent), ui(new Ui::Error) {
  ui->setupUi(this);
  setWindowTitle(tr("Glate | Error"));
  auto *esc = new QShortcut(Qt::Key_Escape, this);
  connect(esc, &QShortcut::activated, this, [this] { close(); });

  ui->ok->setText(tr("OK"));
  ui->ok->setDefault(true);
  ui->message->setWordWrap(true);
  ui->message->setTextFormat(Qt::RichText);
  m_report = new QPushButton(QIcon(QStringLiteral(":/icons/bug-line.png")),
                             tr("Report a bug..."), this);
  m_report->setObjectName(QStringLiteral("reportButton"));
  ui->horizontalLayout->insertWidget(0, m_report);
  ui->horizontalLayout->insertStretch(1, 1);
  connect(m_report, &QPushButton::clicked, this, [this] {
    emit reportRequested(m_detail);
    close();
  });
}

void Error::setError(const QString &heading, const QString &explanation,
                     const QString &detail) {
  m_detail = detail;
  QString text = QStringLiteral("<b>%1</b>").arg(heading.toHtmlEscaped());
  if (!explanation.isEmpty())
    text += QStringLiteral("<br>") + explanation.toHtmlEscaped();
  ui->message->setText(text);
  ui->detail->setPlainText(detail);
  ui->detail->setVisible(!detail.isEmpty());
}

Error::~Error() { delete ui; }

void Error::on_ok_clicked() { this->close(); }
