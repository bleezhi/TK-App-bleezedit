#pragma once
#include "AppSettings.h"
#include "AudioRecorder.h"
#include <QObject>
#include <QVariantList>
#include <QElapsedTimer>
#include <QTimer>
#include <pjsua-lib/pjsua.h>
#include <atomic>
#include <memory>
#include <vector>

// All commands run on the dedicated Qt SIP thread. PJSIP callbacks only copy
// event data and queue work back to this thread; widgets are never touched.
class SipEngine final : public QObject {
    Q_OBJECT
public:
    // A nonzero loopbackTestPort is reserved for tests and ONLY targets 127.0.0.1.
    explicit SipEngine(QObject *parent = nullptr, quint16 loopbackTestPort = 0);
    ~SipEngine() override;
    void initialize(const AppSettings &settings, bool headless = false);
    void connectAccount(const AppSettings &settings);
    void disconnectAccount();
    void shutdown();
    void dial(const QString &number);
    void answer();
    void hangup();
    void sendDtmf(const QString &digits);
    void setMuted(bool muted);
    void toggleDnd();
    void startRecording(RecordingFormat format, const QString &directory);
    void stopRecording();
    void updateRecordingSettings(bool autoRecord, RecordingFormat format, const QString &directory);
signals:
    void ready(bool available);
    void registrationChanged(bool registered, int code, const QString &text);
    void callChanged(const QString &phase, const QString &peer, bool active, bool confirmed, bool incoming);
    void callFinished(const QString &peer, int code, int durationSeconds, bool incoming);
    void dndChanged(bool enabled, bool pending, const QString &text);
    void mutedChanged(bool muted);
    void recordingChanged(bool active, const QString &path);
    void recordingProgress(qint64 seconds);
    void devicesReady(const QVariantList &devices);
    void error(const QString &text);
    void log(const QString &text);
private:
    struct CallToken { quint64 serial; bool service; };
    struct RecorderPort;
    struct CallEvent {
        pjsua_call_id id = PJSUA_INVALID_ID;
        quint64 serial = 0;
        bool service = false;
        int state = 0, code = 0;
        QString reason, peer;
    };
    static std::atomic<SipEngine *> s_instance;
    static void onRegistration(pjsua_acc_id id);
    static void onIncoming(pjsua_acc_id account, pjsua_call_id id, pjsip_rx_data *data);
    static void onCallState(pjsua_call_id id, pjsip_event *event);
    static void onMediaState(pjsua_call_id id);
    static void onConferenceOperation(const pjmedia_conf_op_info *info);
    void handleRegistration(pjsua_acc_id id, bool registered, int code, const QString &reason);
    void handleIncoming(pjsua_call_id id);
    void handleCallState(const CallEvent &event);
    void handleMediaState(pjsua_call_id id, quint64 serial, bool service);
    CallToken *newToken(bool service);
    void maybeAutoRecord();
    QString makeUri(const QString &number) const;
    bool check(pj_status_t status, const QString &operation);
    void resetMainCall();
    const QString m_host;
    const quint16 m_port;
    AppSettings m_settings;
    bool m_initialized = false, m_headless = false, m_registered = false;
    pjsua_acc_id m_account = PJSUA_INVALID_ID;
    pjsua_transport_id m_udp = PJSUA_INVALID_ID;
    pjsua_call_id m_mainCall = PJSUA_INVALID_ID, m_serviceCall = PJSUA_INVALID_ID;
    pjsua_conf_port_id m_mainSlot = PJSUA_INVALID_ID, m_recordSlot = PJSUA_INVALID_ID;
    CallToken *m_mainToken = nullptr, *m_serviceToken = nullptr;
    std::vector<std::unique_ptr<CallToken>> m_tokens;
    quint64 m_nextSerial = 1;
    std::atomic<quint64> m_epoch{0};
    bool m_confirmed = false, m_incoming = false, m_muted = false;
    bool m_dnd = false, m_dndTarget = false, m_serviceAnswered = false, m_dndPending = false;
    bool m_autoRecordAttempted = false;
    QString m_peer;
    QElapsedTimer m_callTime;
    QTimer m_recordTimer;
    std::shared_ptr<AudioRecorder> m_recorder;
    RecorderPort *m_recorderPort = nullptr;
};
