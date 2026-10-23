#include "error.h"
#include "ui_error.h"

#include <QFontDatabase>
#include <QFrame>
#include <QPushButton>
#include <QShortcut>
#include <QTextDocument>

#include <memory>

#include "ShellStyle.h"

namespace {
constexpr int kWidth = 460;
constexpr int kIconSize = 28;
} // namespace

Error::Error(QWidget *parent) : QWidget(parent), ui(new Ui::Error) {
  ui->setupUi(this);
  setWindowTitle(tr("Glate | Error"));
  auto *esc = new QShortcut(Qt::Key_Escape, this);
  connect(esc, &QShortcut::activated, this, [this] { close(); });

  ui->ok->setText(tr("OK"));
  ui->ok->setDefault(true);
  ui->ok->setProperty("accent", true);
  ui->message->setWordWrap(true);
  ui->message->setTextFormat(Qt::RichText);
  ui->message->setAlignment(Qt::AlignTop | Qt::AlignLeft);

  // Laid out as in DOCS/mocks/error.html: body, a line, then the buttons.
  ui->verticalLayout->setContentsMargins(0, 0, 0, 0);
  ui->verticalLayout->setSpacing(0);
  ui->horizontalLayout_2->setContentsMargins(20, 16, 20, 12);
  ui->horizontalLayout_2->setSpacing(12);
  ui->icon->setMargin(0);
  ui->icon->setFixedSize(kIconSize, kIconSize);
  ui->icon->setAlignment(Qt::AlignTop);
  ui->horizontalLayout_2->setAlignment(ui->icon, Qt::AlignTop);
  auto *detailBox = new QVBoxLayout;
  detailBox->setContentsMargins(20, 0, 20, 16);
  ui->verticalLayout->removeWidget(ui->detail);
  detailBox->addWidget(ui->detail);
  ui->verticalLayout->insertLayout(1, detailBox);
  QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
  mono.setPointSizeF(font().pointSizeF() * 0.85);
  ui->detail->setFont(mono);
  ui->detail->setFrameShape(QFrame::NoFrame);
  ui->detail->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  ui->detail->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  ui->detail->document()->setDocumentMargin(10);
  auto *line = new QFrame(this);
  line->setObjectName(QStringLiteral("toolbarSeparator"));
  line->setFrameShape(QFrame::NoFrame);
  line->setFixedHeight(1);
  line->setAttribute(Qt::WA_StyledBackground, true);
  ui->verticalLayout->insertWidget(2, line);
  ui->horizontalLayout->setContentsMargins(16, 10, 16, 12);
  ui->horizontalLayout->setSpacing(8);
  restyle();
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
  m_heading = heading;
  m_explanation = explanation;
  m_detail = detail;
  ui->detail->setPlainText(detail);
  ui->detail->setVisible(!detail.isEmpty());
  restyle();
}

void Error::restyle() {
  using namespace glate::shell;
  ui->icon->setPixmap(tintedIcon(QStringLiteral(":/icons/error-warning-line.png"),
                                 dangerColor(this))
                          .pixmap(kIconSize, kIconSize));
  QString text = QStringLiteral("<b>%1</b>").arg(m_heading.toHtmlEscaped());
  if (!m_explanation.isEmpty())
    text += QStringLiteral("<div style=\"color:%1; font-size:small; margin-top:4px;\">%2</div>")
                .arg(mutedColor(this).name(), m_explanation.toHtmlEscaped());
  ui->message->setText(text);
  ui->detail->setStyleSheet(
      QStringLiteral("QTextBrowser { border: 1px solid %1; border-radius: 6px; padding: 0; }")
          .arg(borderColor().name()));

  // The box as tall as the message, up to a limit; then the window fits it.
  setMinimumWidth(kWidth);
  // Measured on a copy: the browser resets its own document to the viewport width.
  const std::unique_ptr<QTextDocument> copy(ui->detail->document()->clone());
  copy->setTextWidth(kWidth - 40 - 2);
  const int detailHeight = static_cast<int>(copy->size().height()) + 6;
  ui->detail->setFixedHeight(qMin(detailHeight, 160));
  layout()->activate();
  resize(kWidth, layout()->hasHeightForWidth()
                     ? layout()->totalHeightForWidth(kWidth)
                     : sizeHint().height());
}

void Error::changeEvent(QEvent *event) {
  if (event->type() == QEvent::PaletteChange)
    restyle();
  QWidget::changeEvent(event);
}

Error::~Error() { delete ui; }

void Error::on_ok_clicked() { this->close(); }
