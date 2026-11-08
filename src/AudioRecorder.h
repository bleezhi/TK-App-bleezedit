#pragma once
#include "AppSettings.h"
#include <QString>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

// PJMEDIA calls enqueue() from its realtime mixer. Encoding and disk I/O run
// on a separate bounded-queue worker, never inside the SIP/audio callback.
class AudioRecorder final {
public:
    static constexpr int SampleRate = 48000;
    static constexpr int FrameSamples = 960;
    AudioRecorder();
    ~AudioRecorder();
    AudioRecorder(const AudioRecorder &) = delete;
    AudioRecorder &operator=(const AudioRecorder &) = delete;
    bool start(const QString &path, RecordingFormat format, QString *error);
    void enqueue(const int16_t *samples, size_t count);
    void stop();
    QString error() const;
    QString path() const { return m_path; }
    qint64 sampleCount() const { return m_written.load(); }
    bool accepting() const { return m_accepting.load(); }
private:
    class Writer;
    void run();
    void setError(const QString &error);
    std::unique_ptr<Writer> m_writer;
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::deque<std::vector<int16_t>> m_queue;
    mutable std::mutex m_errorMutex;
    QString m_error;
    QString m_path;
    std::atomic<bool> m_accepting{false};
    bool m_stopping = false;
    std::atomic<qint64> m_written{0};
};
