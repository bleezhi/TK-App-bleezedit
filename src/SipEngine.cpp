#include "SipEngine.h"
#include <QDateTime>
#include <QDir>
#include <QMetaObject>
#include <QRegularExpression>
#include <QUrl>
#include <QUuid>
#include <algorithm>
#include <array>

std::atomic<SipEngine *> SipEngine::s_instance{nullptr};
namespace {
QString pjText(const pj_str_t &s) { return QString::fromUtf8(s.ptr, static_cast<int>(s.slen)); }
pj_str_t pjString(QByteArray &s) { return pj_str(s.data()); }
QString statusText(pj_status_t status) {
    char buf[PJ_ERR_MSG_SIZE]; pj_strerror(status, buf, sizeof(buf)); return QString::fromUtf8(buf);
}
}

struct SipEngine::RecorderPort {
    pjmedia_port port{};
    pj_pool_t *pool = nullptr;
    std::shared_ptr<AudioRecorder> recorder;
    static pj_status_t put(pjmedia_port *port, pjmedia_frame *frame) {
        auto *self = static_cast<RecorderPort *>(port->port_data.pdata);
        if(frame->type == PJMEDIA_FRAME_TYPE_AUDIO && frame->buf && frame->size)
            self->recorder->enqueue(static_cast<const int16_t *>(frame->buf), frame->size / sizeof(int16_t));
        else {
            static const std::array<int16_t, AudioRecorder::FrameSamples> silence{};
            self->recorder->enqueue(silence.data(), silence.size());
        }
        return PJ_SUCCESS;
    }
    static pj_status_t get(pjmedia_port *, pjmedia_frame *frame) {
        frame->type = PJMEDIA_FRAME_TYPE_NONE; frame->size = 0; return PJ_SUCCESS;
    }
    static pj_status_t destroy(pjmedia_port *port) {
        auto *self = static_cast<RecorderPort *>(port->port_data.pdata);
        pj_pool_t *pool = self->pool;
        delete self;
        pj_pool_release(pool);
        return PJ_SUCCESS;
    }
};

SipEngine::SipEngine(QObject *parent, quint16 loopbackTestPort)
    : QObject(parent), m_host(loopbackTestPort ? QStringLiteral("127.0.0.1") : sipServer()),
      m_port(loopbackTestPort ? loopbackTestPort : static_cast<quint16>(sipPort())) {
    m_recordTimer.setParent(this); // follows the engine when it moves to the SIP thread
    m_recordTimer.setInterval(1000);
    connect(&m_recordTimer, &QTimer::timeout, this, [this] {
        if(!m_recorder) return;
        QString problem = m_recorder->error();
        if(!problem.isEmpty()) { stopRecording(); return; }
        emit recordingProgress(m_recorder->sampleCount() / AudioRecorder::SampleRate);
    });
}
SipEngine::~SipEngine() { shutdown(); }
bool SipEngine::check(pj_status_t status, const QString &op) {
    if(status == PJ_SUCCESS) return true;
    emit error(op + ": " + statusText(status)); return false;
}
void SipEngine::initialize(const AppSettings &settings, bool headless) {
    if(m_initialized) return;
    ++m_epoch;
    m_settings = settings; m_headless = headless;
    if(!check(pjsua_create(), QStringLiteral("Inicjalizacja SIP"))) { emit ready(false); return; }
    m_initialized = true; s_instance.store(this);
    pjsua_config cfg; pjsua_config_default(&cfg);
    cfg.max_calls = 4; cfg.thread_cnt = 2;
    QByteArray agent("TK App/2.0 (Telekomunikacja Kaliska; UDP)"); cfg.user_agent = pjString(agent);
    cfg.cb.on_reg_state = &onRegistration;
    cfg.cb.on_incoming_call = &onIncoming;
    cfg.cb.on_call_state = &onCallState;
    cfg.cb.on_call_media_state = &onMediaState;
    cfg.cb.on_conf_op_completed = &onConferenceOperation;
    QByteArray stun = settings.stunServer.toUtf8();
    if(!stun.isEmpty()) { cfg.stun_srv_cnt = 1; cfg.stun_srv[0] = pjString(stun); cfg.stun_ignore_failure = PJ_TRUE; }
    pjsua_logging_config logging; pjsua_logging_config_default(&logging);
    // No SIP packet dumps: Digest Authorization and SDP must not enter logs.
    logging.level = 2; logging.console_level = 0; logging.msg_logging = PJ_FALSE;
    pjsua_media_config media; pjsua_media_config_default(&media);
    media.clock_rate = AudioRecorder::SampleRate; media.snd_clock_rate = AudioRecorder::SampleRate;
    media.channel_count = 1; media.audio_frame_ptime = 20; media.no_vad = PJ_TRUE;
    media.ec_tail_len = 128; media.snd_auto_close_time = 3;
    media.enable_ice = settings.ice ? PJ_TRUE : PJ_FALSE;
    if(!check(pjsua_init(&cfg, &logging, &media), QStringLiteral("Konfiguracja SIP"))) { shutdown(); return; }
    pjsua_transport_config tp; pjsua_transport_config_default(&tp); tp.port = 0;
    if(!check(pjsua_transport_create(PJSIP_TRANSPORT_UDP, &tp, &m_udp), QStringLiteral("Transport UDP")) ||
       !check(pjsua_start(), QStringLiteral("Uruchomienie SIP"))) { shutdown(); return; }
    pjsua_codec_info codecs[64]; unsigned count = 64;
    if(pjsua_enum_codecs(codecs, &count) == PJ_SUCCESS) {
        for(unsigned i=0; i<count; ++i) {
            QString name = pjText(codecs[i].codec_id).toLower();
            pj_uint8_t priority = 0;
            if(name.startsWith("opus/")) priority = 255;
            else if(name.startsWith("pcmu/")) priority = 240;
            else if(name.startsWith("pcma/")) priority = 230;
            else if(name.startsWith("g722/")) priority = 220;
            pjsua_codec_set_priority(&codecs[i].codec_id, priority);
        }
    }
    QVariantList devices;
    unsigned deviceCount = pjmedia_aud_dev_count();
    for(unsigned i=0; i<deviceCount; ++i) {
        pjmedia_aud_dev_info d{};
        if(pjmedia_aud_dev_get_info(static_cast<pjmedia_aud_dev_index>(i), &d) == PJ_SUCCESS) {
            QVariantMap map; map["id"] = static_cast<int>(i);
            map["name"] = QString::fromUtf8(d.name) + " (" + QString::fromUtf8(d.driver) + ")";
            map["input"] = d.input_count > 0; map["output"] = d.output_count > 0;
            devices.append(map);
        }
    }
    emit devicesReady(devices);
    if(headless || deviceCount == 0) {
        check(pjsua_set_null_snd_dev(), QStringLiteral("Zegar audio"));
        if(!headless) emit log(QStringLiteral("Nie znaleziono urządzeń audio. Wybierz mikrofon / głośniki w ustawieniach."));
    } else {
        pjsua_snd_dev_param sound; pjsua_snd_dev_param_default(&sound);
        sound.capture_dev = settings.captureDevice; sound.playback_dev = settings.playbackDevice;
        if(sound.capture_dev >= static_cast<int>(deviceCount)) sound.capture_dev = -1;
        if(sound.playback_dev >= static_cast<int>(deviceCount)) sound.playback_dev = -2;
        sound.mode = PJSUA_SND_DEV_NO_IMMEDIATE_OPEN;
        check(pjsua_set_snd_dev2(&sound), QStringLiteral("Wybór urządzeń audio"));
    }
    emit ready(true);
    emit registrationChanged(false, 0, QStringLiteral("Rozłączono"));
}
void SipEngine::connectAccount(const AppSettings &s) {
    if(m_mainCall != PJSUA_INVALID_ID || m_dndPending) { emit error(QStringLiteral("Najpierw zakończ trwające połączenie.")); return; }
    if(!validSipUsername(s.username)) { emit error(QStringLiteral("Wpisz poprawny login SIP w ustawieniach.")); return; }
    if(!s.identityDomain.isEmpty() && !QRegularExpression("^[A-Za-z0-9.\\-]+$").match(s.identityDomain).hasMatch()) {
        emit error(QStringLiteral("Nieprawidłowa domena konta SIP.")); return;
    }
    bool headless = m_headless;
    // Recreate the endpoint to apply transport, STUN, ICE and audio changes.
    shutdown(); initialize(s, headless);
    if(!m_initialized) return;
    m_settings = s;
    QString domain = s.identityDomain.isEmpty() ? m_host : s.identityDomain;
    QString display = s.displayName;
    display.replace("\\", "\\\\"); display.replace("\"", "\\\"");
    QByteArray id = (display.isEmpty() ? "sip:" + s.username + "@" + domain : "\"" + display + "\" <sip:" + s.username + "@" + domain + ">").toUtf8();
    QByteArray registrar = ("sip:" + m_host + ":" + QString::number(m_port) + ";transport=udp").toUtf8();
    QByteArray authUser = (s.authUsername.isEmpty() ? s.username : s.authUsername).toUtf8();
    QByteArray password = s.password.toUtf8();
    QByteArray realm("*"), scheme("digest");
    pjsua_acc_config acc; pjsua_acc_config_default(&acc);
    acc.id = pjString(id); acc.reg_uri = pjString(registrar);
    acc.transport_id = m_udp;
    acc.reg_timeout = 300; acc.reg_retry_interval = 30; acc.reg_first_retry_interval = 5;
    acc.cred_count = 1; acc.cred_info[0].realm = pjString(realm); acc.cred_info[0].scheme = pjString(scheme);
    acc.cred_info[0].username = pjString(authUser); acc.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    acc.cred_info[0].data = pjString(password);
    acc.allow_contact_rewrite = PJ_TRUE; acc.allow_via_rewrite = PJ_TRUE; acc.ka_interval = 20;
    acc.ipv6_sip_use = PJSUA_IPV6_DISABLED; acc.ipv6_media_use = PJSUA_IPV6_DISABLED;
    acc.rtp_cfg.port = 40000; acc.rtp_cfg.port_range = 200;
    acc.use_srtp = PJMEDIA_SRTP_DISABLED;
    emit registrationChanged(false, 0, QStringLiteral("Rejestracja…"));
    if(check(pjsua_acc_add(&acc, PJ_TRUE, &m_account), QStringLiteral("Rejestracja konta")))
        emit log(QStringLiteral("Telekomunikacja Kaliska: %1:%2 / UDP").arg(m_host).arg(m_port));
    // Credentials are cloned internally by PJSIP. This temporary byte buffer is cleared.
    password.fill('\0');
}
void SipEngine::disconnectAccount() {
    if(!m_initialized) return;
    if(m_mainCall != PJSUA_INVALID_ID || m_dndPending) { emit error(QStringLiteral("Najpierw zakończ połączenie.")); return; }
    if(m_account != PJSUA_INVALID_ID) { pjsua_acc_del(m_account); m_account = PJSUA_INVALID_ID; }
    m_registered = false;
    emit registrationChanged(false, 0, QStringLiteral("Rozłączono"));
}
void SipEngine::shutdown() {
    if(!m_initialized) return;
    m_recordTimer.stop(); stopRecording();
    s_instance.store(nullptr); ++m_epoch;
    pjsua_call_hangup_all();
    pjsua_destroy(); // joins callback / media threads before call-token memory is released
    m_initialized = false; m_registered = false;
    m_account = m_udp = PJSUA_INVALID_ID;
    m_serviceCall = PJSUA_INVALID_ID; m_serviceToken = nullptr;
    m_dndPending = false;
    m_tokens.clear(); resetMainCall();
    emit dndChanged(m_dnd, false, QStringLiteral("DND: stan lokalny"));
    emit registrationChanged(false, 0, QStringLiteral("Rozłączono"));
    emit ready(false);
}
SipEngine::CallToken *SipEngine::newToken(bool service) {
    auto token = std::make_unique<CallToken>(); token->serial = m_nextSerial++; token->service = service;
    CallToken *ptr = token.get(); m_tokens.push_back(std::move(token)); return ptr;
}
QString SipEngine::makeUri(const QString &number) const {
    QString encoded = QString::fromLatin1(QUrl::toPercentEncoding(number, "*+"));
    return "sip:" + encoded + "@" + m_host + ":" + QString::number(m_port) + ";transport=udp";
}
void SipEngine::dial(const QString &raw) {
    QString number = normalizeNumber(raw);
    if(!m_registered) { emit error(QStringLiteral("Najpierw połącz konto SIP.")); return; }
    if(!validNumber(number)) { emit error(QStringLiteral("Numer może zawierać cyfry, *, # oraz + na początku.")); return; }
    if(m_mainCall != PJSUA_INVALID_ID) { emit error(QStringLiteral("Trwa już rozmowa.")); return; }
    m_mainToken = newToken(false); m_peer = number; m_incoming = false; m_confirmed = false; m_muted = false; m_autoRecordAttempted = false;
    QByteArray uri = makeUri(number).toUtf8(); pj_str_t dst = pjString(uri);
    pjsua_call_setting options; pjsua_call_setting_default(&options); options.aud_cnt = 1; options.vid_cnt = 0;
    pj_status_t status = pjsua_call_make_call(m_account, &dst, &options, m_mainToken, nullptr, &m_mainCall);
    if(!check(status, QStringLiteral("Wybieranie numeru"))) { resetMainCall(); return; }
    emit callChanged("connecting", m_peer, true, false, false);
    const quint64 serial = m_mainToken->serial;
    QTimer::singleShot(60000, this, [this, serial] {
        if(m_mainToken && m_mainToken->serial == serial && !m_confirmed && m_mainCall != PJSUA_INVALID_ID) {
            pjsua_call_hangup(m_mainCall, 408, nullptr, nullptr);
            emit log(QStringLiteral("Upłynął czas oczekiwania na odebranie (60 s)."));
        }
    });
}
void SipEngine::answer() {
    if(m_initialized && m_mainCall != PJSUA_INVALID_ID && m_incoming && !m_confirmed)
        check(pjsua_call_answer(m_mainCall, 200, nullptr, nullptr), QStringLiteral("Odbieranie"));
}
void SipEngine::hangup() {
    if(m_initialized && m_mainCall != PJSUA_INVALID_ID)
        check(pjsua_call_hangup(m_mainCall, m_incoming && !m_confirmed ? 486 : 0, nullptr, nullptr), QStringLiteral("Rozłączanie"));
}
void SipEngine::sendDtmf(const QString &digits) {
    if(!m_initialized || !m_confirmed || m_mainCall == PJSUA_INVALID_ID) return;
    if(!QRegularExpression("^[0-9*#]+$").match(digits).hasMatch()) return;
    QByteArray bytes = digits.toLatin1(); pj_str_t text = pjString(bytes);
    check(pjsua_call_dial_dtmf(m_mainCall, &text), QStringLiteral("DTMF (RFC 4733)"));
}
void SipEngine::setMuted(bool muted) {
    m_muted = muted;
    if(m_initialized && m_mainSlot != PJSUA_INVALID_ID) {
        if(muted) pjsua_conf_disconnect(0, m_mainSlot);
        else pjsua_conf_connect(0, m_mainSlot);
    }
    if(m_initialized && m_recordSlot != PJSUA_INVALID_ID) {
        if(muted) pjsua_conf_disconnect(0, m_recordSlot);
        else pjsua_conf_connect(0, m_recordSlot);
    }
    emit mutedChanged(muted);
}
void SipEngine::toggleDnd() {
    if(!m_registered) {
        emit error(QStringLiteral("DND wymaga zarejestrowanego konta SIP."));
        emit dndChanged(m_dnd, false, QStringLiteral("Konto SIP nie jest zarejestrowane"));
        return;
    }
    if(m_dndPending) return;
    m_dndTarget = !m_dnd; m_serviceAnswered = false; m_dndPending = true; m_serviceToken = newToken(true);
    QByteArray uri = makeUri("13").toUtf8(); pj_str_t dst = pjString(uri);
    pjsua_call_setting options; pjsua_call_setting_default(&options); options.aud_cnt = 1; options.vid_cnt = 0;
    emit dndChanged(m_dnd, true, QStringLiteral("Połączenie w tle → 13"));
    pj_status_t status = pjsua_call_make_call(m_account, &dst, &options, m_serviceToken, nullptr, &m_serviceCall);
    if(!check(status, QStringLiteral("Usługa DND 13"))) {
        m_serviceCall = PJSUA_INVALID_ID; m_serviceToken = nullptr; m_dndPending = false;
        emit dndChanged(m_dnd, false, QStringLiteral("Nie udało się przełączyć DND")); return;
    }
    quint64 serial = m_serviceToken->serial;
    QTimer::singleShot(15000, this, [this, serial] {
        if(m_serviceToken && m_serviceToken->serial == serial && !m_serviceAnswered && m_serviceCall != PJSUA_INVALID_ID)
            pjsua_call_hangup(m_serviceCall, 408, nullptr, nullptr);
    });
}
void SipEngine::startRecording(RecordingFormat format, const QString &directory) {
    if(m_recorder) return;
    if(!m_initialized || !m_confirmed || m_mainSlot == PJSUA_INVALID_ID) { emit error(QStringLiteral("Nagrywanie jest dostępne po nawiązaniu rozmowy audio.")); return; }
    if(directory.isEmpty() || !QDir().mkpath(directory)) { emit error(QStringLiteral("Nie można utworzyć katalogu nagrań.")); return; }
    QString peer = m_peer; peer.replace(QRegularExpression("[^0-9A-Za-z_+\\-]"), "_"); peer = peer.left(48);
    QString filename = QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss-zzz") + "_" + peer + "_" + QUuid::createUuid().toString(QUuid::Id128).left(6) + (format == RecordingFormat::Wav ? ".wav" : ".opus");
    auto recorder = std::make_shared<AudioRecorder>(); QString problem;
    if(!recorder->start(QDir(directory).filePath(filename), format, &problem)) { emit error(problem); return; }
    auto *sink = new RecorderPort(); sink->recorder = recorder;
    sink->pool = pjsua_pool_create("tk-recorder", 2048, 2048);
    if(!sink->pool) { recorder->stop(); delete sink; emit error(QStringLiteral("Brak pamięci dla nagrania.")); return; }
    pj_str_t name = pj_str(const_cast<char *>("TK recorder"));
    pjmedia_port_info_init(&sink->port.info, &name, PJMEDIA_SIG_CLASS_PORT_AUD('T','K'), AudioRecorder::SampleRate, 1, 16, AudioRecorder::FrameSamples);
    sink->port.port_data.pdata = sink;
    sink->port.put_frame = &RecorderPort::put; sink->port.get_frame = &RecorderPort::get; sink->port.on_destroy = &RecorderPort::destroy;
    pj_status_t status = pjmedia_port_init_grp_lock(&sink->port, sink->pool, nullptr);
    if(status != PJ_SUCCESS) { recorder->stop(); RecorderPort::destroy(&sink->port); check(status, QStringLiteral("Blokada portu nagrania")); return; }
    status = pjsua_conf_add_port(sink->pool, &sink->port, &m_recordSlot);
    if(status != PJ_SUCCESS) { recorder->stop(); pjmedia_port_destroy(&sink->port); check(status, QStringLiteral("Port nagrania")); m_recordSlot = PJSUA_INVALID_ID; return; }
    m_recorder = recorder; m_recorderPort = sink;
    if(!check(pjsua_conf_connect(m_mainSlot, m_recordSlot), QStringLiteral("Nagrywanie rozmówcy")) ||
       (!m_muted && !check(pjsua_conf_connect(0, m_recordSlot), QStringLiteral("Nagrywanie mikrofonu")))) { stopRecording(); return; }
    m_recordTimer.start();
    emit recordingChanged(true, recorder->path()); emit recordingProgress(0);
}
void SipEngine::stopRecording() {
    if(!m_recorder) return;
    m_recordTimer.stop();
    auto recorder = m_recorder;
    recorder->stop(); // disable capture, drain queue and finalise WAV / Ogg EOS
    if(m_initialized && m_recordSlot != PJSUA_INVALID_ID) pjsua_conf_remove_port(m_recordSlot);
    // Removal is asynchronous in recent PJSIP. The bridge holds a group-lock
    // reference; on_destroy releases the port/pool only after its last callback.
    if(m_recorderPort) pjmedia_port_destroy(&m_recorderPort->port);
    m_recordSlot = PJSUA_INVALID_ID; m_recorderPort = nullptr; m_recorder.reset();
    emit recordingChanged(false, recorder->path());
    if(!recorder->error().isEmpty()) emit error(recorder->error());
    else emit log(QStringLiteral("Nagranie zapisane: %1").arg(recorder->path()));
}
void SipEngine::updateRecordingSettings(bool autoRecord, RecordingFormat format, const QString &directory) {
    m_settings.autoRecord = autoRecord;
    m_settings.recordingFormat = format;
    m_settings.recordingDirectory = directory;
}
void SipEngine::maybeAutoRecord() {
    if(m_confirmed && m_mainSlot != PJSUA_INVALID_ID && m_settings.autoRecord && !m_autoRecordAttempted) {
        m_autoRecordAttempted = true; startRecording(m_settings.recordingFormat, m_settings.recordingDirectory);
    }
}
void SipEngine::resetMainCall() {
    m_mainCall = m_mainSlot = PJSUA_INVALID_ID; m_mainToken = nullptr;
    m_confirmed = m_incoming = m_muted = m_autoRecordAttempted = false; m_peer.clear(); m_callTime.invalidate();
    emit mutedChanged(false); emit callChanged("idle", {}, false, false, false);
}
void SipEngine::onRegistration(pjsua_acc_id id) {
    SipEngine *engine = s_instance.load(); if(!engine) return;
    pjsua_acc_info info{}; if(pjsua_acc_get_info(id, &info) != PJ_SUCCESS) return;
    bool registered = info.status == 200 && info.expires > 0; int code = info.status; QString reason = pjText(info.status_text);
    const quint64 epoch = engine->m_epoch.load();
    QMetaObject::invokeMethod(engine, [engine, id, registered, code, reason, epoch] { if(engine->m_epoch.load() == epoch) engine->handleRegistration(id, registered, code, reason); }, Qt::QueuedConnection);
}
void SipEngine::handleRegistration(pjsua_acc_id id, bool registered, int code, const QString &reason) {
    if(!m_initialized || id != m_account) return;
    m_registered = registered;
    emit registrationChanged(registered, code, registered ? QStringLiteral("Połączono") : reason);
    if(code >= 400 && code != 401 && code != 407) emit error(QStringLiteral("Rejestracja SIP: %1 %2").arg(code).arg(reason));
}
void SipEngine::onIncoming(pjsua_acc_id account, pjsua_call_id id, pjsip_rx_data *data) {
    Q_UNUSED(account); Q_UNUSED(data);
    SipEngine *engine = s_instance.load(); if(!engine) return;
    const quint64 epoch = engine->m_epoch.load();
    QMetaObject::invokeMethod(engine, [engine, id, epoch] { if(engine->m_epoch.load() == epoch) engine->handleIncoming(id); }, Qt::QueuedConnection);
}
void SipEngine::handleIncoming(pjsua_call_id id) {
    if(!m_initialized || !pjsua_call_is_active(id)) return;
    if((m_dnd && m_settings.rejectIncomingOnDnd) || m_mainCall != PJSUA_INVALID_ID) {
        pjsua_call_answer(id, 486, nullptr, nullptr);
        emit log((m_dnd && m_settings.rejectIncomingOnDnd) ? QStringLiteral("DND: odrzucono połączenie przychodzące (486).") : QStringLiteral("Odrzucono drugie połączenie: zajęte (486).")); return;
    }
    pjsua_call_info info{}; if(pjsua_call_get_info(id, &info) != PJ_SUCCESS) return;
    m_mainCall = id; m_mainToken = newToken(false); pjsua_call_set_user_data(id, m_mainToken);
    m_peer = pjText(info.remote_info); m_incoming = true; m_confirmed = false; m_muted = false; m_autoRecordAttempted = false;
    pjsua_call_answer(id, 180, nullptr, nullptr);
    emit callChanged("incoming", m_peer, true, false, true);
}
void SipEngine::onCallState(pjsua_call_id id, pjsip_event *event) {
    Q_UNUSED(event);
    SipEngine *engine = s_instance.load(); if(!engine) return;
    pjsua_call_info info{}; if(pjsua_call_get_info(id, &info) != PJ_SUCCESS) return;
    auto *token = static_cast<CallToken *>(pjsua_call_get_user_data(id));
    CallEvent snapshot; snapshot.id = id; snapshot.state = info.state; snapshot.code = info.last_status;
    snapshot.reason = pjText(info.last_status_text); snapshot.peer = pjText(info.remote_info);
    if(token) { snapshot.serial = token->serial; snapshot.service = token->service; }
    const quint64 epoch = engine->m_epoch.load();
    QMetaObject::invokeMethod(engine, [engine, snapshot, epoch] { if(engine->m_epoch.load() == epoch) engine->handleCallState(snapshot); }, Qt::QueuedConnection);
}
void SipEngine::handleCallState(const CallEvent &e) {
    if(!m_initialized) return;
    if(e.service) {
        if(!m_serviceToken || m_serviceToken->serial != e.serial) return;
        if(e.state == PJSIP_INV_STATE_CONFIRMED && !m_serviceAnswered) {
            m_serviceAnswered = true; m_dnd = m_dndTarget;
            emit dndChanged(m_dnd, true, QStringLiteral("Usługa 13 odebrała; kończenie w tle…"));
            quint64 serial = e.serial;
            QTimer::singleShot(m_settings.dndAnswerSeconds * 1000, this, [this, serial] {
                if(m_serviceToken && m_serviceToken->serial == serial && m_serviceCall != PJSUA_INVALID_ID)
                    pjsua_call_hangup(m_serviceCall, 0, nullptr, nullptr);
            });
        }
        if(e.state == PJSIP_INV_STATE_DISCONNECTED) {
            bool ok = m_serviceAnswered;
            m_serviceCall = PJSUA_INVALID_ID; m_serviceToken = nullptr; m_dndPending = false;
            emit dndChanged(m_dnd, false, ok ? QStringLiteral("DND: stan lokalny; wysłano telefon na 13") : QStringLiteral("DND bez zmiany; usługa 13 nie odebrała"));
            if(!ok) emit error(QStringLiteral("Usługa DND 13: %1 %2. Stan lokalny nie został zmieniony.").arg(e.code).arg(e.reason));
        }
        return;
    }
    if(!m_mainToken || m_mainToken->serial != e.serial) return;
    if(e.state == PJSIP_INV_STATE_DISCONNECTED) {
        stopRecording();
        emit callFinished(m_peer, e.code, m_callTime.isValid() ? static_cast<int>(m_callTime.elapsed()/1000) : 0, m_incoming);
        if(e.code >= 300 && e.code != 487) emit log(QStringLiteral("Zakończono: %1 %2").arg(e.code).arg(e.reason));
        resetMainCall(); return;
    }
    if(e.state == PJSIP_INV_STATE_CONFIRMED) {
        if(!m_confirmed) { m_confirmed = true; m_callTime.start(); }
        emit callChanged("connected", m_peer, true, true, m_incoming); maybeAutoRecord();
    } else if(!m_incoming) {
        emit callChanged(e.state == PJSIP_INV_STATE_EARLY ? "ringing" : "connecting", m_peer, true, false, false);
    }
}
void SipEngine::onMediaState(pjsua_call_id id) {
    SipEngine *engine = s_instance.load(); if(!engine) return;
    auto *token = static_cast<CallToken *>(pjsua_call_get_user_data(id)); if(!token) return;
    quint64 serial = token->serial; bool service = token->service;
    const quint64 epoch = engine->m_epoch.load();
    QMetaObject::invokeMethod(engine, [engine, id, serial, service, epoch] { if(engine->m_epoch.load() == epoch) engine->handleMediaState(id, serial, service); }, Qt::QueuedConnection);
}
void SipEngine::handleMediaState(pjsua_call_id id, quint64 serial, bool service) {
    if(!m_initialized) return;
    // The service call is NEVER bridged to speakers, microphone or a recorder.
    if(service) return;
    if(!m_mainToken || m_mainToken->serial != serial || m_mainCall != id) return;
    pjsua_call_info info{}; if(pjsua_call_get_info(id, &info) != PJ_SUCCESS) return;
    if(info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        m_mainSlot = info.conf_slot;
        if(!check(pjsua_conf_connect(m_mainSlot, 0), QStringLiteral("Dźwięk rozmówcy")) ||
           (!m_muted && !check(pjsua_conf_connect(0, m_mainSlot), QStringLiteral("Mikrofon")))) return;
        if(m_recordSlot != PJSUA_INVALID_ID) pjsua_conf_connect(m_mainSlot, m_recordSlot);
        maybeAutoRecord();
    } else if(info.media_status == PJSUA_CALL_MEDIA_ERROR) {
        emit error(QStringLiteral("Nie udało się uruchomić audio. Sprawdź mikrofon, głośniki i zgodność kodeków."));
        pjsua_call_hangup(id, 500, nullptr, nullptr);
    }
}
void SipEngine::onConferenceOperation(const pjmedia_conf_op_info *info) {
    if(info->status == PJ_SUCCESS) return;
    SipEngine *engine = s_instance.load(); if(!engine) return;
    QString message = QStringLiteral("Operacja mostka audio: %1").arg(statusText(info->status));
    const quint64 epoch = engine->m_epoch.load();
    QMetaObject::invokeMethod(engine, [engine, message, epoch] { if(engine->m_epoch.load() == epoch) emit engine->error(message); }, Qt::QueuedConnection);
}
