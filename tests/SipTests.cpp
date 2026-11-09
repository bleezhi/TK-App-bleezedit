#include "SipEngine.h"
#include <QtTest>
#include <QProcess>
#include <QThread>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QtEndian>
#include <functional>

class SipTests : public QObject {
    Q_OBJECT
private:
    QProcess m_peer;
    QThread m_thread;
    SipEngine *m_engine=nullptr;
    QTemporaryDir m_dir;
    AppSettings m_settings;
    QByteArray m_lines;
    QList<QJsonObject> m_peerEvents;
    QStringList m_errors, m_logs, m_files;
    bool m_registered=false, m_active=false, m_confirmed=false, m_incoming=false;
    bool m_dnd=false, m_dndPending=false, m_recording=false;
    QString m_currentPeer;
    int m_mainStarted=0;
    void invoke(std::function<void(SipEngine *)> fn) {QMetaObject::invokeMethod(m_engine,[this,fn]{fn(m_engine);},Qt::QueuedConnection);}
    void peerCommand(const char *cmd) {m_peer.write(QJsonDocument(QJsonObject{{"cmd",cmd}}).toJson(QJsonDocument::Compact)+'\n');m_peer.waitForBytesWritten(1000);}
    void readPeer() {
        m_lines+=m_peer.readAllStandardOutput(); int end;
        while((end=m_lines.indexOf('\n'))>=0){QJsonObject obj=QJsonDocument::fromJson(m_lines.left(end)).object();m_lines.remove(0,end+1);if(!obj.isEmpty())m_peerEvents.append(obj);}
    }
    int events(const QString &name,int code=-1) {
        readPeer();int count=0;for(const auto &e:m_peerEvents)if(e.value("event").toString()==name&&(code<0||e.value("code").toInt()==code))++count;return count;
    }
private slots:
    void initTestCase() {
        QVERIFY(m_dir.isValid());
        QString peerPath=QString::fromUtf8(TK_TEST_PEER);
#ifdef Q_OS_WIN
        // Only the Linux->MinGW cross-build uses a POSIX source path (Wine Z:).
        if(peerPath.startsWith('/')) peerPath="Z:"+peerPath;
#endif
        m_peer.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        m_peer.start(qEnvironmentVariable("TK_TEST_PYTHON","python3"),{peerPath});
        QVERIFY(m_peer.waitForStarted(3000)); QVERIFY2(m_peer.waitForReadyRead(5000),qPrintable(QString::fromUtf8(m_peer.readAllStandardError()))); readPeer(); QVERIFY(!m_peerEvents.isEmpty());
        int port=m_peerEvents.first().value("sip_port").toInt(); QVERIFY(port>0);
        m_settings.username="101";m_settings.password="test-secret";m_settings.recordingDirectory=m_dir.path();m_settings.dndAnswerSeconds=1;
        m_engine=new SipEngine(nullptr,static_cast<quint16>(port));m_engine->moveToThread(&m_thread);connect(&m_thread,&QThread::finished,m_engine,&QObject::deleteLater);
        connect(m_engine,&SipEngine::registrationChanged,this,[this](bool r,int,const QString &){m_registered=r;});
        connect(m_engine,&SipEngine::callChanged,this,[this](const QString &,const QString &peer,bool active,bool confirmed,bool incoming){if(active&&!m_active)++m_mainStarted;m_active=active;m_confirmed=confirmed;m_incoming=incoming;m_currentPeer=peer;});
        connect(m_engine,&SipEngine::dndChanged,this,[this](bool d,bool p,const QString &){m_dnd=d;m_dndPending=p;});
        connect(m_engine,&SipEngine::recordingChanged,this,[this](bool active,const QString &path){m_recording=active;if(active)m_files.append(path);});
        connect(m_engine,&SipEngine::error,this,[this](const QString &e){m_errors.append(e);qWarning().noquote()<<e;});
        connect(m_engine,&SipEngine::log,this,[this](const QString &l){m_logs.append(l);});
        m_thread.start();invoke([this](SipEngine *e){e->initialize(m_settings,true);e->connectAccount(m_settings);});
        QTRY_VERIFY_WITH_TIMEOUT(m_registered,7000);QTRY_VERIFY(events("registered")>=1);QVERIFY(m_errors.isEmpty());
    }
    void onlyUdpTransportExists() {
        unsigned count=8;
        pjsua_transport_id ids[8]{};
        bool valid=false;
        QMetaObject::invokeMethod(m_engine,[&]{
            if(pjsua_enum_transports(ids,&count)==PJ_SUCCESS && count==1) {
                pjsua_transport_info info{};
                valid=pjsua_transport_get_info(ids[0],&info)==PJ_SUCCESS && info.type==PJSIP_TRANSPORT_UDP;
            }
        },Qt::BlockingQueuedConnection);
        QCOMPARE(count,1u);
        QVERIFY2(valid,"The TK application must only create a UDP transport; no TCP listener.");
    }
    void echoAndSilentBackgroundDnd() {
        invoke([](SipEngine *e){e->dial("12");});QTRY_VERIFY_WITH_TIMEOUT(m_confirmed,5000);QCOMPARE(m_currentPeer,QString("12"));
        invoke([this](SipEngine *e){e->startRecording(RecordingFormat::Wav,m_dir.path());});QTRY_VERIFY(m_recording);QTest::qWait(400);
        int started=m_mainStarted;invoke([](SipEngine *e){e->toggleDnd();});QTRY_VERIFY_WITH_TIMEOUT(m_dnd,5000);QTRY_VERIFY_WITH_TIMEOUT(!m_dndPending,5000);
        QVERIFY(m_confirmed);QCOMPARE(m_currentPeer,QString("12"));QCOMPARE(m_mainStarted,started);QVERIFY(m_recording);
        invoke([](SipEngine *e){e->stopRecording();});QTRY_VERIFY(!m_recording);
        QVERIFY(!m_files.isEmpty());QFile wav(m_files.last());QVERIFY(wav.open(QIODevice::ReadOnly));QByteArray bytes=wav.readAll();QVERIFY(bytes.startsWith("RIFF"));QVERIFY(bytes.size()>1000);
        bool audible=false;for(int i=44;i+1<bytes.size();i+=2)if(qFromLittleEndian<qint16>(reinterpret_cast<const uchar *>(bytes.constData()+i))!=0){audible=true;break;}QVERIFY2(audible,"RTP audio must reach the recording, not just a WAV header.");
        invoke([this](SipEngine *e){e->startRecording(RecordingFormat::Opus,m_dir.path());});QTRY_VERIFY(m_recording);QTest::qWait(500);
        invoke([](SipEngine *e){e->hangup();});QTRY_VERIFY(!m_active);QTRY_VERIFY(!m_recording);
        QFile opus(m_files.last());QVERIFY(opus.open(QIODevice::ReadOnly));QByteArray b=opus.readAll();QVERIFY(b.startsWith("OggS"));QVERIFY(b.contains("OpusHead"));QVERIFY(b.size()>150);
        QTRY_VERIFY(events("bye")>=2);QVERIFY(m_errors.isEmpty());
    }
    void incomingRejectedWhenDnd() {
        QVERIFY(m_dnd);int count=events("incoming_response",486);int started=m_mainStarted;peerCommand("incoming");QTRY_VERIFY_WITH_TIMEOUT(events("incoming_response",486)>count,5000);QCOMPARE(m_mainStarted,started);QVERIFY(!m_active);
    }
    void dndOffIncomingAndDtmf() {
        invoke([](SipEngine *e){e->toggleDnd();});QTRY_VERIFY_WITH_TIMEOUT(!m_dnd,5000);QTRY_VERIFY_WITH_TIMEOUT(!m_dndPending,5000);
        peerCommand("incoming");QTRY_VERIFY_WITH_TIMEOUT(m_incoming&&m_active,5000);invoke([](SipEngine *e){e->answer();});QTRY_VERIFY(m_confirmed);
        invoke([](SipEngine *e){e->sendDtmf("7");});QTRY_VERIFY_WITH_TIMEOUT(events("dtmf")>0,5000);
        invoke([](SipEngine *e){e->hangup();});QTRY_VERIFY(!m_active);QVERIFY(m_errors.isEmpty());
    }
    void connectionTest14AndAutomaticRecording() {
        invoke([this](SipEngine *e){e->updateRecordingSettings(true,RecordingFormat::Opus,m_dir.path());e->dial("14");});
        QTRY_VERIFY(m_confirmed);QCOMPARE(m_currentPeer,QString("14"));QTRY_VERIFY(m_recording);QTest::qWait(300);invoke([](SipEngine *e){e->hangup();});QTRY_VERIFY(!m_active);QTRY_VERIFY(!m_recording);QVERIFY(m_files.last().endsWith(".opus"));QVERIFY(m_errors.isEmpty());
    }
    void failedDndDoesNotChangeState() {
        QVERIFY(!m_dnd);peerCommand("reject_next_dnd");QTRY_VERIFY(events("will_reject_dnd")>0);int errors=m_errors.size();
        invoke([](SipEngine *e){e->toggleDnd();});QTRY_VERIFY_WITH_TIMEOUT(m_errors.size()>errors,5000);QTRY_VERIFY(!m_dndPending);QVERIFY(!m_dnd);QVERIFY(!m_active);
    }
    void invalidDialAndDisconnect() {
        int errors=m_errors.size();invoke([](SipEngine *e){e->dial("sip:intruder@other-server");});QTRY_VERIFY(m_errors.size()>errors);QVERIFY(!m_active);
        invoke([](SipEngine *e){e->disconnectAccount();});QTRY_VERIFY(!m_registered);
    }
    void cleanup() {
        // A failed assertion must not leave a call active and cascade failures
        // into later SIP scenarios in the same QtTest process.
        if(m_engine && m_active) {
            invoke([](SipEngine *e){e->hangup();});
            QTRY_VERIFY_WITH_TIMEOUT(!m_active,5000);
        }
    }
    void cleanupTestCase() {
        if(m_engine&&m_thread.isRunning()){QMetaObject::invokeMethod(m_engine,[this]{m_engine->shutdown();},Qt::BlockingQueuedConnection);m_thread.quit();m_thread.wait();m_engine=nullptr;}
        m_peer.closeWriteChannel();if(!m_peer.waitForFinished(1500)){m_peer.kill();m_peer.waitForFinished(1500);}
    }
};
QTEST_GUILESS_MAIN(SipTests)
#include "SipTests.moc"
