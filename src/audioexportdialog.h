#pragma once

#include <QDialog>
#include <QProcess>
#include <QString>
#include <QStringList>

class QAudioOutput;
class QMediaPlayer;

namespace Ui { class AudioExportDialog; }

/**
 * @brief Minimal audio editor/exporter using ffmpeg.
 *
 * Allows users to export TTS audio in multiple formats (MP3, WAV, FLAC, etc.)
 * with control over bitrate, sample rate, channels, and volume.
 */
class AudioExportDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AudioExportDialog(QWidget *parent = nullptr);
    ~AudioExportDialog() override;

    /// Set the audio files to export (from TTS downloader).
    void setAudioFiles(const QStringList &files);

    /// Convenience: set display name for the export.
    void setExportName(const QString &name);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onFormatChanged(int index);
    void onExportClicked();
    void onPreviewClicked();
    void onExportFinished(int exitCode, QProcess::ExitStatus status);
    void onPreviewRenderFinished(int exitCode, QProcess::ExitStatus status);
    void onPreviewPlaybackStateChanged();
    void onSpeedChanged(int value);
    void onVolumeChanged(int value);
    void onResetClicked();

private:
    struct AudioFormat {
        QString name;           // "MP3", "WAV", etc.
        QString extension;      // "mp3", "wav", etc.
        QString codecName;      // "libmp3lame", "pcm_s16le", etc.
        bool supportsbitrate;   // false for lossless formats
        bool supportsChannels;  // true for most, false for mono-only
        int defaultBitrate;     // in kbps
    };

    /// Build ffmpeg command from UI controls + input files.
    QString buildFfmpegCommand(const QString &outputPath);

    /// Populate format dropdown with available codecs.
    void initializeFormats();

    /// Load/save user preferences for format, bitrate, etc.
    void loadSettings();
    void saveSettings();

    // Builds the filter_complex string + returns the final output label.
    QString buildFilterComplex(QString &finalLabel) const;

    // Full QStringList of ffmpeg arguments for a given output path and codec.
    QStringList buildFfmpegArgs(const QString &outputPath,
                                const QString &codecName,
                                const QString &extension,
                                bool  applyBitrate,
                                int   bitrate) const;

    Ui::AudioExportDialog *ui;
    QList<AudioFormat>     m_formats;
    QStringList            m_audioFiles;
    QString                m_exportName;
    QString                m_lastExportPath;

    QProcess      *m_exportProcess  = nullptr;
    QProcess      *m_previewProcess = nullptr;
    QMediaPlayer  *m_previewPlayer  = nullptr;
    QAudioOutput  *m_previewAudio   = nullptr;
    QString        m_previewTempPath;
};
