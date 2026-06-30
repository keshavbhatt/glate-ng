#include "linebyline.h"
#include "ui_linebyline.h"
#include <QScrollBar>
#include <QShortcut>

LineByLine::LineByLine(QWidget *parent, QClipboard *clipborad)
    : QWidget(parent), ui(new Ui::LineByLine) {
  ui->setupUi(this);
  setWindowTitle(tr("Glate | Line by Line"));
  { auto *_esc = new QShortcut(Qt::Key_Escape, this); connect(_esc, &QShortcut::activated, this, [this]{ close(); }); }
  this->clipborad = clipborad;
  ui->readSrc1->setEnabled(false);
  ui->readSrc2->setEnabled(false);
  connect(ui->src1->verticalScrollBar(), &QScrollBar::valueChanged,
          [=](int val) { ui->src2->verticalScrollBar()->setValue(val); });
  connect(ui->src2->verticalScrollBar(), &QScrollBar::valueChanged,
          [=](int val) { ui->src1->verticalScrollBar()->setValue(val); });
}

LineByLine::~LineByLine() { delete ui; }

void LineByLine::setData(QList<QStringList> currentTranslationData,
                         QString sLang, QString tLang, QString tId) {
  translationId = tId;
  ui->sLang->setText(sLang);
  ui->tLang->setText(tLang);
  foreach (QStringList translationFregments, currentTranslationData) {
    ui->src2->addItem(translationFregments.at(0)); // translation
    ui->src1->addItem(translationFregments.at(1)); // source
  }
  if (ui->src1->count() > 0)
    ui->src1->setCurrentRow(0);
  if (ui->src2->count() > 0)
    ui->src2->setCurrentRow(0);
}

void LineByLine::clearData() {
  ui->src1->blockSignals(true);
  ui->src2->blockSignals(true);

  ui->src1->clear();
  ui->src2->clear();
  ui->src1Browser->clear();
  ui->src2Browser->clear();
  ui->readSrc1->setEnabled(false);
  ui->readSrc2->setEnabled(false);

  ui->src1->blockSignals(false);
  ui->src2->blockSignals(false);
}

void LineByLine::on_src1_currentRowChanged(int currentRow) {
  if (currentRow < 0 || currentRow >= ui->src1->count() ||
      currentRow >= ui->src2->count()) {
    ui->src1Browser->clear();
    ui->src2Browser->clear();
    ui->readSrc1->setEnabled(false);
    ui->readSrc2->setEnabled(false);
    return;
  }

  ui->src2->blockSignals(true);
  ui->src2->setCurrentRow(currentRow);
  ui->src2Browser->setText(ui->src2->item(currentRow)->text());
  ui->src1Browser->setText(ui->src1->item(currentRow)->text());
  ui->readSrc1->setEnabled(!ui->src1Browser->toPlainText().trimmed().isEmpty());
  ui->readSrc2->setEnabled(!ui->src2Browser->toPlainText().trimmed().isEmpty());
  ui->src2->blockSignals(false);
}

void LineByLine::on_src2_currentRowChanged(int currentRow) {
  if (currentRow < 0 || currentRow >= ui->src2->count() ||
      currentRow >= ui->src1->count()) {
    ui->src1Browser->clear();
    ui->src2Browser->clear();
    ui->readSrc1->setEnabled(false);
    ui->readSrc2->setEnabled(false);
    return;
  }

  ui->src1->blockSignals(true);
  ui->src1->setCurrentRow(currentRow);
  ui->src1Browser->setText(ui->src1->item(currentRow)->text());
  ui->src2Browser->setText(ui->src2->item(currentRow)->text());
  ui->readSrc1->setEnabled(!ui->src1Browser->toPlainText().trimmed().isEmpty());
  ui->readSrc2->setEnabled(!ui->src2Browser->toPlainText().trimmed().isEmpty());
  ui->src1->blockSignals(false);
}

QString LineByLine::getTId() { return translationId; }

void LineByLine::on_copySrc2_clicked() {
  clipborad->setText(ui->src2Browser->toPlainText());
}

void LineByLine::on_copySrc1_clicked() {
  clipborad->setText(ui->src1Browser->toPlainText());
}

void LineByLine::on_readSrc1_clicked() {
  const QString text = ui->src1Browser->toPlainText().trimmed();
  if (!text.isEmpty()) {
    emit readSourceRequested(text);
  }
}

void LineByLine::on_readSrc2_clicked() {
  const QString text = ui->src2Browser->toPlainText().trimmed();
  if (!text.isEmpty()) {
    emit readTranslationRequested(text);
  }
}

