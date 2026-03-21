#include "audioexportdialog.h"
#include "ui_audioexportdialog.h"
#include <QApplication>
#include <QAudioOutput>
#include <QDesktopServices>
#include <QFile>
#include <QFileDialog>
#include <QMediaPlayer>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
// ─────────────────────────────────────────────────────────────────────────────
AudioExportDialog::AudioExportDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AudioExportDialog)
{
    ui->setupUi(this);
    setWindowTitle(QApplication::applicationName() + QStringLiteral(" | Export Audio"));
    setMinimumWidth(440);
    // ── Collapsible advanced toggle button ──────────────────────────────────
    auto *toggleBtn = new QToolButton(this);
    toggleBtn->setText(QStringLiteral("  Advanced Options"));
    toggleBtn->setCheckable(true);
    toggleBtn->setChecked(false);
    toggleBtn->setArrowType(Qt::RightArrow);
    toggleBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggleBtn->setAutoRaise(true);
    toggleBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // Suppress the accent-color highlight that themes apply to checked tool buttons
    toggleBtn->setStyleSheet(QStringLiteral(
        "QToolButton { border: none; border-radius: 3px; padding: 4px 6px; text-align: left; }"
        "QToolButton:hover { background: rgba(128,128,128,0.12); }"
        "QToolButton:checked { background: rgba(128,128,128,0.12); }"
    ));
    // Insert toggle before advancedGroup
    auto *mainLay = qobject_cast<QVBoxLayout *>(layout());
    const int advIdx = mainLay->indexOf(ui->advancedGroup);
    mainLay->insertWidget(advIdx, toggleBtn);
    connect(toggleBtn, &QToolButton::toggled, this, [this, toggleBtn](bool on) {
        toggleBtn->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        ui->advancedGroup->setVisible(on);
        ui->exportBtn->setText(on ? QStringLiteral("Advanced Export")
                                  : QStringLiteral("Export"));
        ui->exportBtn->setToolTip(on ? QStringLiteral("Export with set advanced options")
                          : QStringLiteral("Export with default settings"));
        QTimer::singleShot(0, this, [this] { adjustSize(); });
    });
    // ── Echo sub-controls: hidden until checkbox is checked ──────────────────
    ui->echoDelayLabel->setVisible(false);
    ui->echoDelayWidget->setVisible(false);
    ui->echoDecayLabel->setVisible(false);
    ui->echoDecayWidget->setVisible(false);
    connect(ui->echoCheckBox, &QCheckBox::toggled, this, [this](bool on) {
        ui->echoDelayLabel->setVisible(on);
        ui->echoDelayWidget->setVisible(on);
        ui->echoDecayLabel->setVisible(on);
        ui->echoDecayWidget->setVisible(on);
        QTimer::singleShot(0, this, [this] { adjustSize(); });
    });
    // Connect echo sliders before loadSettings so labels sync on load
    connect(ui->echoDelaySlider, &QSlider::valueChanged, this, [this](int v) {
        ui->echoDelayValueLabel->setText(QString::number(v) + QStringLiteral(" ms"));
    });
    connect(ui->echoDecaySlider, &QSlider::valueChanged, this, [this](int v) {
        ui->echoDecayValueLabel->setText(QString::number(v / 100.0, 'f', 2));
    });
    // ── Format / bitrate / sample-rate / channel population ─────────────────
    initializeFormats();
    loadSettings();
    // ── Processes ───────────────────────────────────────────────────────────
    m_exportProcess  = new QProcess(this);
    m_previewProcess = new QProcess(this);
    connect(m_exportProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AudioExportDialog::onExportFinished);
    connect(m_previewProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &AudioExportDialog::onPreviewRenderFinished);
    // ── Media player for live preview ────────────────────────────────────────
    m_previewPlayer = new QMediaPlayer(this);
    m_previewAudio  = new QAudioOutput(this);
    m_previewPlayer->setAudioOutput(m_previewAudio);
    m_previewAudio->setVolume(1.0f);
    connect(m_previewPlayer, &QMediaPlayer::playbackStateChanged,
            this, &AudioExportDialog::onPreviewPlaybackStateChanged);
    // ── UI signals ───────────────────────────────────────────────────────────
    connect(ui->formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AudioExportDialog::onFormatChanged);
    connect(ui->exportBtn,  &QPushButton::clicked, this, &AudioExportDialog::onExportClicked);
    connect(ui->previewBtn, &QPushButton::clicked, this, &AudioExportDialog::onPreviewClicked);
    connect(ui->resetBtn,   &QPushButton::clicked, this, &AudioExportDialog::onResetClicked);
    connect(ui->closeBtn,   &QPushButton::clicked, this, &QDialog::close);
    connect(ui->speedSlider,  QOverload<int>::of(&QSlider::valueChanged), this, &AudioExportDialog::onSpeedChanged);
    connect(ui->volumeSlider, QOverload<int>::of(&QSlider::valueChanged), this, &AudioExportDialog::onVolumeChanged);
}
AudioExportDialog::~AudioExportDialog()
{
    saveSettings();
    if (m_previewPlayer) m_previewPlayer->stop();
    if (m_exportProcess  && m_exportProcess->state()  != QProcess::NotRunning) m_exportProcess->kill();
    if (m_previewProcess && m_previewProcess->state() != QProcess::NotRunning) m_previewProcess->kill();
    // Clean up temp preview file
    if (!m_previewTempPath.isEmpty()) QFile::remove(m_previewTempPath);
    delete ui;
}
// ── Public setters ────────────────────────────────────────────────────────────
void AudioExportDialog::setAudioFiles(const QStringList &files)
{
    m_audioFiles = files;
    ui->statusLabel->setText(tr("Ready. Select a format and click Export."));
    ui->previewBtn->setEnabled(!files.isEmpty());
    ui->exportBtn->setEnabled(!files.isEmpty());
}
void AudioExportDialog::setExportName(const QString &name)
{
    m_exportName = name;
    const QString safe = name.trimmed()
        .replace(QRegularExpression(QStringLiteral("[/\\\\:*?\"<>|]")), QStringLiteral("_"))
        .left(60);
    ui->filenameEdit->setText(safe.isEmpty() ? QStringLiteral("export") : safe);
}
// ── Format init ───────────────────────────────────────────────────────────────
void AudioExportDialog::initializeFormats()
{
    m_formats << AudioFormat{QStringLiteral("MP3 (High Quality)"), QStringLiteral("mp3"),  QStringLiteral("libmp3lame"), true,  true, 192};
    m_formats << AudioFormat{QStringLiteral("MP3 (Standard)"),     QStringLiteral("mp3"),  QStringLiteral("libmp3lame"), true,  true, 128};
    m_formats << AudioFormat{QStringLiteral("WAV (Lossless PCM)"), QStringLiteral("wav"),  QStringLiteral("pcm_s16le"),  false, true, 0};
    m_formats << AudioFormat{QStringLiteral("FLAC (Lossless)"),    QStringLiteral("flac"), QStringLiteral("flac"),       false, true, 0};
    m_formats << AudioFormat{QStringLiteral("AAC"),                QStringLiteral("m4a"),  QStringLiteral("aac"),        true,  true, 128};
    for (const auto &f : m_formats) ui->formatCombo->addItem(f.name);
    for (int br : {32, 64, 96, 128, 160, 192, 224, 256, 320})
        ui->bitrateCombo->addItem(QString::number(br) + QStringLiteral(" kbps"));
    ui->bitrateCombo->setCurrentText(QStringLiteral("192 kbps"));
    for (int sr : {22050, 44100, 48000, 96000})
        ui->sampleRateCombo->addItem(QString::number(sr) + QStringLiteral(" Hz"));
    ui->sampleRateCombo->setCurrentText(QStringLiteral("44100 Hz"));
    ui->channelsCombo->addItem(QStringLiteral("Mono"));
    ui->channelsCombo->addItem(QStringLiteral("Stereo"));
    ui->channelsCombo->setCurrentText(QStringLiteral("Stereo"));
    onFormatChanged(0);
}
void AudioExportDialog::onFormatChanged(int index)
{
    if (index < 0 || index >= m_formats.count()) return;
    const auto &fmt = m_formats[index];
    ui->bitrateLabel->setVisible(fmt.supportsbitrate);
    ui->bitrateCombo->setVisible(fmt.supportsbitrate);
    if (fmt.supportsbitrate)
        ui->bitrateCombo->setCurrentText(QString::number(fmt.defaultBitrate) + QStringLiteral(" kbps"));
}
// ── Settings persistence ──────────────────────────────────────────────────────
void AudioExportDialog::loadSettings()
{
    QSettings s;
    const int fmt = s.value(QStringLiteral("audio_export/format"), 0).toInt();
    ui->formatCombo->setCurrentIndex(qBound(0, fmt, m_formats.count() - 1));
    ui->bitrateCombo->setCurrentText(s.value(QStringLiteral("audio_export/bitrate"), QStringLiteral("192 kbps")).toString());
    ui->sampleRateCombo->setCurrentText(s.value(QStringLiteral("audio_export/sample_rate"), QStringLiteral("44100 Hz")).toString());
    ui->channelsCombo->setCurrentText(s.value(QStringLiteral("audio_export/channels"), QStringLiteral("Stereo")).toString());
    ui->volumeSlider->setValue(static_cast<int>(s.value(QStringLiteral("audio_export/volume"), 1.0f).toFloat() * 100));
    ui->speedSlider->setValue(static_cast<int>(s.value(QStringLiteral("audio_export/speed"), 1.0f).toFloat() * 100));
    ui->fadeInSpinBox->setValue(s.value(QStringLiteral("audio_export/fade_in"), 0.0).toDouble());
    ui->fadeOutSpinBox->setValue(s.value(QStringLiteral("audio_export/fade_out"), 0.0).toDouble());
    ui->echoCheckBox->setChecked(s.value(QStringLiteral("audio_export/echo"), false).toBool());
    ui->echoDelaySlider->setValue(s.value(QStringLiteral("audio_export/echo_delay"), 500).toInt());
    ui->echoDecaySlider->setValue(s.value(QStringLiteral("audio_export/echo_decay"), 50).toInt());
}
void AudioExportDialog::saveSettings()
{
    QSettings s;
    s.setValue(QStringLiteral("audio_export/format"),      ui->formatCombo->currentIndex());
    s.setValue(QStringLiteral("audio_export/bitrate"),     ui->bitrateCombo->currentText());
    s.setValue(QStringLiteral("audio_export/sample_rate"), ui->sampleRateCombo->currentText());
    s.setValue(QStringLiteral("audio_export/channels"),    ui->channelsCombo->currentText());
    s.setValue(QStringLiteral("audio_export/volume"),      ui->volumeSlider->value() / 100.0f);
    s.setValue(QStringLiteral("audio_export/speed"),       ui->speedSlider->value() / 100.0f);
    s.setValue(QStringLiteral("audio_export/fade_in"),     ui->fadeInSpinBox->value());
    s.setValue(QStringLiteral("audio_export/fade_out"),    ui->fadeOutSpinBox->value());
    s.setValue(QStringLiteral("audio_export/echo"),        ui->echoCheckBox->isChecked());
    s.setValue(QStringLiteral("audio_export/echo_delay"),  ui->echoDelaySlider->value());
    s.setValue(QStringLiteral("audio_export/echo_decay"),  ui->echoDecaySlider->value());
}
// ── Filter chain builder ──────────────────────────────────────────────────────
QString AudioExportDialog::buildFilterComplex(QString &finalLabel) const
{
    QStringList parts;
    QString cur = QStringLiteral("a_src");
    int idx = 0;
    auto next = [&] { return QStringLiteral("af%1").arg(++idx); };
    // 1. Concat or passthrough
    if (m_audioFiles.count() > 1) {
        QString inputs;
        for (int i = 0; i < m_audioFiles.count(); ++i)
            inputs += QStringLiteral("[%1:a]").arg(i);
        parts << inputs + QStringLiteral("concat=n=%1:v=0:a=1[a_src]").arg(m_audioFiles.count());
    } else {
        parts << QStringLiteral("[0:a]aresample=resampler=swr[a_src]");
    }
    // 2. Volume
    const double vol = ui->volumeSlider->value() / 100.0;
    if (qAbs(vol - 1.0) > 0.01) {
        auto out = next();
        parts << QStringLiteral("[%1]volume=%2[%3]").arg(cur).arg(vol, 0, 'f', 2).arg(out);
        cur = out;
    }
    // 3. Speed / tempo  (atempo range is 0.5-2.0; chain two if needed)
    const double spd = ui->speedSlider->value() / 100.0;
    if (qAbs(spd - 1.0) > 0.01) {
        auto out = next();
        parts << QStringLiteral("[%1]atempo=%2[%3]").arg(cur).arg(qBound(0.5, spd, 2.0), 0, 'f', 2).arg(out);
        cur = out;
    }
    // 4. Echo
    if (ui->echoCheckBox->isChecked()) {
        auto out = next();
        const int    echoDelay = ui->echoDelaySlider->value();
        const double echoDecay = ui->echoDecaySlider->value() / 100.0;
        parts << QStringLiteral("[%1]aecho=0.8:0.9:%2:%3[%4]")
                 .arg(cur)
                 .arg(echoDelay)
                 .arg(echoDecay, 0, 'f', 2)
                 .arg(out);
        cur = out;
    }
    // 5. Fade in
    const double fi = ui->fadeInSpinBox->value();
    if (fi > 0.0) {
        auto out = next();
        parts << QStringLiteral("[%1]afade=t=in:st=0:d=%2[%3]").arg(cur).arg(fi, 0, 'f', 2).arg(out);
        cur = out;
    }
    // 6. Fade out  (areverse trick so we don't need to know total duration)
    const double fo = ui->fadeOutSpinBox->value();
    if (fo > 0.0) {
        auto r1 = next(), faded = next(), r2 = next();
        parts << QStringLiteral("[%1]areverse[%2]").arg(cur, r1);
        parts << QStringLiteral("[%1]afade=t=in:st=0:d=%2[%3]").arg(r1).arg(fo, 0, 'f', 2).arg(faded);
        parts << QStringLiteral("[%1]areverse[%2]").arg(faded, r2);
        cur = r2;
    }
    finalLabel = cur;
    return parts.join(QStringLiteral(";"));
}
QStringList AudioExportDialog::buildFfmpegArgs(const QString &outputPath,
                                               const QString &codecName,
                                               const QString &/*extension*/,
                                               bool  applyBitrate,
                                               int   bitrate) const
{
    if (m_audioFiles.isEmpty()) return {};

    QStringList args;
    args << QStringLiteral("-y");
    for (const auto &f : m_audioFiles) args << QStringLiteral("-i") << f;

    const bool advancedEnabled = ui->advancedGroup->isVisible();

    if (advancedEnabled) {
        // Full effect filter chain
        QString finalLabel;
        const QString fc = buildFilterComplex(finalLabel);
        args << QStringLiteral("-filter_complex") << fc
             << QStringLiteral("-map") << QStringLiteral("[%1]").arg(finalLabel);
    } else if (m_audioFiles.count() > 1) {
        // Multiple files: simple concat, no effects
        QString inputs;
        for (int i = 0; i < m_audioFiles.count(); ++i)
            inputs += QStringLiteral("[%1:a]").arg(i);
        const QString fc = inputs +
            QStringLiteral("concat=n=%1:v=0:a=1[aout]").arg(m_audioFiles.count());
        args << QStringLiteral("-filter_complex") << fc
             << QStringLiteral("-map") << QStringLiteral("[aout]");
    }
    // Single file without advanced: no filter_complex needed, ffmpeg re-encodes directly

    args << QStringLiteral("-c:a") << codecName;

    const int sr = ui->sampleRateCombo->currentText().split(QStringLiteral(" ")).first().toInt();
    args << QStringLiteral("-ar") << QString::number(sr > 0 ? sr : 44100);

    const int ch = (ui->channelsCombo->currentText() == QStringLiteral("Mono")) ? 1 : 2;
    args << QStringLiteral("-ac") << QString::number(ch);

    if (applyBitrate && bitrate > 0)
        args << QStringLiteral("-b:a") << QString::number(bitrate) + QStringLiteral("k");

    args << outputPath;
    return args;
}
// ── Export ────────────────────────────────────────────────────────────────────
void AudioExportDialog::onExportClicked()
{
    if (m_audioFiles.isEmpty()) {
        QMessageBox::warning(this, tr("No Audio"), tr("No audio files are ready for export."));
        return;
    }
    if (m_exportProcess->state() != QProcess::NotRunning) return;
    const int fi = ui->formatCombo->currentIndex();
    if (fi < 0 || fi >= m_formats.count()) return;
    const auto &fmt = m_formats[fi];
    const QString filename = ui->filenameEdit->text().trimmed();
    const QString defaultName = filename.isEmpty() ? QStringLiteral("export") : filename;
    const QString downloadsPath = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    const QString defaultPath = QDir(downloadsPath).filePath(defaultName + QLatin1Char('.') + fmt.extension);
    const QString outPath = QFileDialog::getSaveFileName(
        this, tr("Export Audio"), defaultPath,
        tr("Audio Files (*.%1);;All Files (*)").arg(fmt.extension));
    if (outPath.isEmpty()) return;
    const int bitrate = ui->bitrateCombo->currentText().split(QStringLiteral(" ")).first().toInt();
    const QStringList args = buildFfmpegArgs(outPath, fmt.codecName, fmt.extension, fmt.supportsbitrate, bitrate);
    if (args.isEmpty()) return;
    m_lastExportPath = outPath;
    ui->statusLabel->setText(tr("Exporting..."));
    ui->exportBtn->setEnabled(false);
    ui->previewBtn->setEnabled(false);
    m_exportProcess->start(QStringLiteral("ffmpeg"), args);
}
void AudioExportDialog::onExportFinished(int exitCode, QProcess::ExitStatus status)
{
    ui->exportBtn->setEnabled(true);
    ui->previewBtn->setEnabled(true);

    if (exitCode == 0 && status == QProcess::NormalExit) {
        const QFileInfo fi(m_lastExportPath);
        ui->statusLabel->setText(tr("Exported: %1").arg(fi.fileName()));

        QMessageBox msgBox(this);
        msgBox.setWindowTitle(tr("Export Complete"));
        msgBox.setIcon(QMessageBox::NoIcon);
        msgBox.setText(QStringLiteral("<b>") + fi.fileName() + QStringLiteral("</b>"));
        msgBox.setInformativeText(tr("Audio exported successfully.\n%1").arg(fi.absolutePath()));

        QPushButton *openFileBtn   = msgBox.addButton(tr("Play File"),       QMessageBox::AcceptRole);
        QPushButton *showFolderBtn = msgBox.addButton(tr("Show in Folder"),  QMessageBox::AcceptRole);
        /*QPushButton *okBtn       =*/ msgBox.addButton(tr("Done"),          QMessageBox::RejectRole);

        msgBox.exec();

        if (msgBox.clickedButton() == openFileBtn) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(m_lastExportPath));
        } else if (msgBox.clickedButton() == showFolderBtn) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absolutePath()));
        }
    } else {
        const QString err = QString::fromUtf8(m_exportProcess->readAllStandardError()).trimmed();
        ui->statusLabel->setText(tr("Export failed."));
        QMessageBox::critical(this, tr("Export Failed"),
            err.isEmpty() ? tr("ffmpeg returned an error.") : err);
    }
}
// ── Live Preview ──────────────────────────────────────────────────────────────
void AudioExportDialog::onPreviewClicked()
{
    if (m_audioFiles.isEmpty()) return;
    // If already playing — stop
    if (m_previewPlayer->playbackState() == QMediaPlayer::PlayingState) {
        m_previewPlayer->stop();
        return;
    }
    // If preview render already running — ignore
    if (m_previewProcess->state() != QProcess::NotRunning) return;
    // Render to a temp WAV (fast, no quality trade-offs during preview)
    m_previewTempPath = QDir::temp().filePath(QStringLiteral("glate_preview_%1.wav")
        .arg(QDateTime::currentSecsSinceEpoch()));
    const QStringList args = buildFfmpegArgs(
        m_previewTempPath,
        QStringLiteral("pcm_s16le"),  // WAV — instant encode
        QStringLiteral("wav"),
        false, 0);
    if (args.isEmpty()) return;
    ui->previewBtn->setEnabled(false);
    ui->previewBtn->setText(tr("Please wait…"));
    ui->statusLabel->setText(tr("Preparing preview…"));
    m_previewProcess->start(QStringLiteral("ffmpeg"), args);
}
void AudioExportDialog::onPreviewRenderFinished(int exitCode, QProcess::ExitStatus status)
{
    if (exitCode != 0 || status != QProcess::NormalExit) {
        const QString err = QString::fromUtf8(m_previewProcess->readAllStandardError()).trimmed();
        ui->statusLabel->setText(tr("Preview failed."));
        ui->previewBtn->setText(tr("Preview"));
        ui->previewBtn->setEnabled(true);
        QMessageBox::warning(this, tr("Preview Failed"),
            err.isEmpty() ? tr("ffmpeg returned an error.") : err);
        return;
    }
    // Playback
    m_previewPlayer->setSource(QUrl::fromLocalFile(m_previewTempPath));
    m_previewPlayer->play();
    ui->previewBtn->setText(tr("Stop"));
    ui->previewBtn->setEnabled(true);
    ui->statusLabel->setText(tr("Playing preview…"));
}
void AudioExportDialog::onPreviewPlaybackStateChanged()
{
    if (m_previewPlayer->playbackState() != QMediaPlayer::PlayingState) {
        ui->previewBtn->setText(tr("Preview"));
        ui->statusLabel->setText(tr("Preview finished."));
        // Remove temp file
        if (!m_previewTempPath.isEmpty()) {
            QFile::remove(m_previewTempPath);
            m_previewTempPath.clear();
        }
    }
}
// ── Slider label updates ──────────────────────────────────────────────────────
void AudioExportDialog::onSpeedChanged(int value)
{
    ui->speedValueLabel->setText(
        QString::number(value / 100.0, 'f', 1) + QStringLiteral("x"));
}
void AudioExportDialog::onVolumeChanged(int value)
{
    ui->volumeValueLabel->setText(QString::number(value) + QStringLiteral("%"));
}

void AudioExportDialog::onResetClicked()
{
    ui->formatCombo->setCurrentIndex(0);
    ui->bitrateCombo->setCurrentText(QStringLiteral("192 kbps"));
    ui->sampleRateCombo->setCurrentText(QStringLiteral("44100 Hz"));
    ui->channelsCombo->setCurrentText(QStringLiteral("Stereo"));
    ui->volumeSlider->setValue(100);
    ui->speedSlider->setValue(100);
    ui->fadeInSpinBox->setValue(0.0);
    ui->fadeOutSpinBox->setValue(0.0);
    ui->echoCheckBox->setChecked(false);
    ui->echoDelaySlider->setValue(500);
    ui->echoDecaySlider->setValue(50);
    ui->filenameEdit->setText(m_exportName.isEmpty() ? QStringLiteral("export") : m_exportName);
    ui->statusLabel->setText(tr("Settings reset to defaults."));
    saveSettings();
}
// ── Close ─────────────────────────────────────────────────────────────────────
void AudioExportDialog::closeEvent(QCloseEvent *event)
{
    saveSettings();
    if (m_previewPlayer) m_previewPlayer->stop();
    QDialog::closeEvent(event);
}
