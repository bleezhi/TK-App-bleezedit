#include "AudioRecorder.h"
#include <QFile>
#include <QRandomGenerator>
#include <QtEndian>
#include <ogg/ogg.h>
#include <opus/opus.h>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace {
void le16(QByteArray &out, quint16 v) { char b[2]; qToLittleEndian(v, reinterpret_cast<uchar *>(b)); out.append(b, 2); }
void le32(QByteArray &out, quint32 v) { char b[4]; qToLittleEndian(v, reinterpret_cast<uchar *>(b)); out.append(b, 4); }
void checkedWrite(QFile &f, const char *data, qint64 n) {
    if(f.write(data, n) != n) throw std::runtime_error(("Blad zapisu: " + f.errorString()).toStdString());
}
void checkedWrite(QFile &f, const QByteArray &data) { checkedWrite(f, data.constData(), data.size()); }
}

class AudioRecorder::Writer {
public:
    Writer(const QString &path, RecordingFormat format) : m_file(path), m_format(format) {
        if(!m_file.open(QIODevice::ReadWrite | QIODevice::NewOnly))
            throw std::runtime_error(("Nie mozna utworzyc pliku: " + m_file.errorString()).toStdString());
        try {
            if(format == RecordingFormat::Wav) writeWavHeader();
            else initOpus();
        } catch(...) { cleanup(); m_file.close(); m_file.remove(); throw; }
    }
    ~Writer() { cleanup(); }
    void append(const std::vector<int16_t> &pcm) {
        if(m_format == RecordingFormat::Wav) {
            if(m_pcmBytes + pcm.size() * 2 > std::numeric_limits<quint32>::max() - 36)
                throw std::runtime_error("Osiagnieto limit rozmiaru WAV (4 GiB).");
            QByteArray bytes;
            bytes.resize(static_cast<int>(pcm.size() * 2));
            for(size_t i=0; i<pcm.size(); ++i)
                qToLittleEndian(static_cast<quint16>(pcm[i]), reinterpret_cast<uchar *>(bytes.data()) + i*2);
            checkedWrite(m_file, bytes);
            m_pcmBytes += pcm.size() * 2;
            // Make a partially written WAV recoverable after an abnormal exit.
            if(m_pcmBytes - m_lastHeaderBytes >= AudioRecorder::SampleRate * 2) {
                qint64 pos = m_file.pos();
                writeWavHeader();
                if(!m_file.seek(pos)) throw std::runtime_error("Blad seek WAV.");
                m_lastHeaderBytes = m_pcmBytes;
                if(!m_file.flush()) throw std::runtime_error("Blad flush WAV.");
            }
        } else {
            m_inputSamples += static_cast<qint64>(pcm.size());
            m_pendingPcm.insert(m_pendingPcm.end(), pcm.begin(), pcm.end());
            while(m_pendingPcm.size() >= AudioRecorder::FrameSamples) {
                encode(m_pendingPcm.data());
                m_pendingPcm.erase(m_pendingPcm.begin(), m_pendingPcm.begin() + AudioRecorder::FrameSamples);
            }
        }
    }
    void finish() {
        if(m_finished) return;
        m_finished = true;
        if(m_format == RecordingFormat::Wav) writeWavHeader();
        else {
            // Include the encoder's lookahead, then trim padding by EOS granule.
            const qint64 lastGranule = m_inputSamples + m_preSkip;
            while(m_encodedSamples < lastGranule || m_pendingPacket.isEmpty()) {
                std::array<int16_t, AudioRecorder::FrameSamples> tail{};
                auto n = std::min(m_pendingPcm.size(), tail.size());
                std::copy_n(m_pendingPcm.begin(), n, tail.begin());
                m_pendingPcm.erase(m_pendingPcm.begin(), m_pendingPcm.begin() + static_cast<ptrdiff_t>(n));
                encode(tail.data());
            }
            packet(m_pendingPacket, true, lastGranule, false);
            flushPages(true);
        }
        if(!m_file.flush()) throw std::runtime_error("Nie udalo sie zapisac nagrania na dysk.");
        m_file.close();
    }
private:
    void writeWavHeader() {
        QByteArray h("RIFF", 4); le32(h, static_cast<quint32>(m_pcmBytes + 36));
        h.append("WAVEfmt ", 8); le32(h, 16); le16(h, 1); le16(h, 1);
        le32(h, AudioRecorder::SampleRate); le32(h, AudioRecorder::SampleRate*2);
        le16(h, 2); le16(h, 16); h.append("data", 4); le32(h, static_cast<quint32>(m_pcmBytes));
        if(!m_file.seek(0)) throw std::runtime_error("Blad naglowka WAV.");
        checkedWrite(m_file, h);
    }
    void initOpus() {
        int err = 0;
        m_encoder = opus_encoder_create(AudioRecorder::SampleRate, 1, OPUS_APPLICATION_VOIP, &err);
        if(!m_encoder || err != OPUS_OK) throw std::runtime_error(opus_strerror(err));
        opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(32000));
        opus_encoder_ctl(m_encoder, OPUS_SET_VBR(1));
        opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(5));
        if(opus_encoder_ctl(m_encoder, OPUS_GET_LOOKAHEAD(&m_preSkip)) != OPUS_OK)
            throw std::runtime_error("Blad Opus lookahead.");
        if(ogg_stream_init(&m_ogg, static_cast<int>(QRandomGenerator::global()->generate())) != 0)
            throw std::runtime_error("Blad inicjalizacji Ogg.");
        m_oggReady = true;
        QByteArray head("OpusHead", 8); head.append(char(1)); head.append(char(1));
        le16(head, static_cast<quint16>(m_preSkip)); le32(head, AudioRecorder::SampleRate); le16(head, 0); head.append(char(0));
        packet(head, false, 0, true); flushPages(true);
        QByteArray tags("OpusTags", 8); const QByteArray vendor("TK App 1.0");
        le32(tags, static_cast<quint32>(vendor.size())); tags.append(vendor); le32(tags, 0);
        packet(tags, false, 0, false); flushPages(true);
    }
    void encode(const int16_t *pcm) {
        std::array<unsigned char, 4000> buf{};
        int n = opus_encode(m_encoder, pcm, AudioRecorder::FrameSamples, buf.data(), static_cast<opus_int32>(buf.size()));
        if(n < 0) throw std::runtime_error(opus_strerror(n));
        if(!m_pendingPacket.isEmpty()) {
            packet(m_pendingPacket, false, m_encodedSamples, false);
            flushPages(++m_sinceFlush >= 50);
            if(m_sinceFlush >= 50) m_sinceFlush = 0;
        }
        m_pendingPacket = QByteArray(reinterpret_cast<const char *>(buf.data()), n);
        m_encodedSamples += AudioRecorder::FrameSamples;
    }
    void packet(QByteArray &bytes, bool eos, qint64 granule, bool bos) {
        ogg_packet p{};
        p.packet = reinterpret_cast<unsigned char *>(bytes.data()); p.bytes = bytes.size();
        p.b_o_s = bos; p.e_o_s = eos; p.granulepos = granule; p.packetno = m_packetNumber++;
        if(ogg_stream_packetin(&m_ogg, &p) != 0) throw std::runtime_error("Blad pakietu Ogg.");
    }
    void flushPages(bool force) {
        ogg_page page{};
        while(force ? ogg_stream_flush(&m_ogg, &page) : ogg_stream_pageout(&m_ogg, &page)) {
            checkedWrite(m_file, reinterpret_cast<char *>(page.header), page.header_len);
            checkedWrite(m_file, reinterpret_cast<char *>(page.body), page.body_len);
        }
    }
    void cleanup() {
        if(m_encoder) { opus_encoder_destroy(m_encoder); m_encoder = nullptr; }
        if(m_oggReady) { ogg_stream_clear(&m_ogg); m_oggReady = false; }
    }
    QFile m_file;
    RecordingFormat m_format;
    quint64 m_pcmBytes = 0, m_lastHeaderBytes = 0;
    OpusEncoder *m_encoder = nullptr;
    ogg_stream_state m_ogg{};
    bool m_oggReady = false, m_finished = false;
    int m_preSkip = 0, m_sinceFlush = 0;
    qint64 m_inputSamples = 0, m_encodedSamples = 0, m_packetNumber = 0;
    QByteArray m_pendingPacket;
    std::vector<int16_t> m_pendingPcm;
};

AudioRecorder::AudioRecorder() = default;
AudioRecorder::~AudioRecorder() { stop(); }
bool AudioRecorder::start(const QString &path, RecordingFormat format, QString *error) {
    stop();
    { std::lock_guard<std::mutex> g(m_errorMutex); m_error.clear(); }
    m_path = path; m_written = 0;
    try { m_writer = std::make_unique<Writer>(path, format); }
    catch(const std::exception &e) { setError(QString::fromUtf8(e.what())); if(error) *error = this->error(); return false; }
    { std::lock_guard<std::mutex> g(m_mutex); m_queue.clear(); m_stopping = false; }
    m_accepting = true;
    try { m_worker = std::thread(&AudioRecorder::run, this); }
    catch(const std::exception &e) { m_accepting = false; m_writer.reset(); setError(QString::fromUtf8(e.what())); if(error) *error = this->error(); return false; }
    return true;
}
void AudioRecorder::enqueue(const int16_t *samples, size_t count) {
    if(!m_accepting.load() || !samples || count == 0) return;
    if(count > 4800) { setError(QStringLiteral("Nieprawidłowy rozmiar ramki audio.")); m_accepting = false; return; }
    // Only a tiny queue operation is protected, never encoding or disk I/O.
    // Do not abort an otherwise valid recording on a transient try_lock miss.
    std::unique_lock<std::mutex> lock(m_mutex);
    if(m_queue.size() >= 250) {
        setError(QStringLiteral("Nagrywanie przerwane: przeciążenie dysku lub kolejki audio."));
        m_accepting = false; m_cv.notify_one(); return;
    }
    // Recheck after taking the lock, so stop() cannot race a new enqueue.
    if(!m_accepting.load()) return;
    m_queue.emplace_back(samples, samples + count);
    lock.unlock(); m_cv.notify_one();
}
void AudioRecorder::stop() {
    m_accepting = false;
    { std::lock_guard<std::mutex> g(m_mutex); m_stopping = true; }
    m_cv.notify_one();
    if(m_worker.joinable()) m_worker.join();
    m_writer.reset();
}
QString AudioRecorder::error() const { std::lock_guard<std::mutex> g(m_errorMutex); return m_error; }
void AudioRecorder::setError(const QString &e) { std::lock_guard<std::mutex> g(m_errorMutex); if(m_error.isEmpty()) m_error = e; }
void AudioRecorder::run() {
    try {
        while(true) {
            std::vector<int16_t> pcm;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this]{ return m_stopping || !m_accepting.load() || !m_queue.empty(); });
                if(m_queue.empty() && (m_stopping || !m_accepting.load())) break;
                if(m_queue.empty()) continue;
                pcm = std::move(m_queue.front()); m_queue.pop_front();
            }
            m_writer->append(pcm); m_written += static_cast<qint64>(pcm.size());
        }
        m_writer->finish();
    } catch(const std::exception &e) {
        setError(QString::fromUtf8(e.what())); m_accepting = false;
        try { m_writer->finish(); } catch(...) {}
    }
}
