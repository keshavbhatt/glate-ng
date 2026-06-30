#include "error.h"
#include "ui_error.h"
#include <QShortcut>

Error::Error(QWidget *parent) : QWidget(parent), ui(new Ui::Error) {
  ui->setupUi(this);
  setWindowTitle(tr("Glate | Error"));
  { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }
}

void Error::setError(QString shortMessage, QString detail) {
  ui->detail->setText(detail);
  ui->message->setText(shortMessage);
}

Error::~Error() { delete ui; }

void Error::on_ok_clicked() { this->close(); }
