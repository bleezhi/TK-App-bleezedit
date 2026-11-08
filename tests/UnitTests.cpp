#include "AudioRecorder.h"
#include "AppSettings.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QSettings>
#include <QtEndian>
#include <ogg/ogg.h>
#include <cmath>
#include <cstring>
#include <array>

class UnitTests : public QObject {
    Q_OBJECT
private slots:
    void numbers() {
        QCOMPARE(sipServer(),QString("15.235.62.162"));
        QCOMPARE(sipPort(),5060);
        QCOMPARE(networkName(),QString::fromUtf8("Telekomunikacja Kaliska"));
        QCOMPARE(normalizeNumber(" +48 (123) 456-789 "),QString("+48123456789"));
        for(const QString &n : {QString("12"),QString("13"),QString("14"),QString("*123#"),QString("+48123")}) QVERIFY(validNumber(n));
        for(const QString &n : {QString(),QString("sip:a@b"),QString("abc"),QString("12;transport=tcp"),QString("12+3"),QString(70,'1')}) QVERIFY(!validNumber(n));
        QVERIFY(validSipUsername("101")); QVERIFY(!validSipUsername("101@example.com"));
    }
    void noPersistedPassword() {
        QTemporaryDir tmp; QVERIFY(tmp.isValid()); QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,tmp.path()); QSettings::setDefaultFormat(QSettings::IniFormat);
        QCoreApplication::setOrganizationName("TKAppTests"); QCoreApplication::setApplicationName("Settings");
        AppSettings s=AppSettings::load(); s.username="101"; s.password="SECRET-do-not-store"; s.recordingFormat=RecordingFormat::Opus; s.save();
        QSettings q; QVERIFY(!q.contains("sip/password")); QCOMPARE(AppSettings::load().password,QString()); QCOMPARE(AppSettings::load().username,QString("101"));
        QCOMPARE(AppSettings::load().recordingFormat,RecordingFormat::Opus);
    }
    void oldTcpConfigurationCannotOverrideTkProfile() {
        QSettings old;
        old.setValue("sip/transport","TCP"); old.setValue("sip/port",5090); old.sync();
        AppSettings migrated=AppSettings::load(); migrated.save();
        QSettings now;
        QVERIFY(!now.contains("sip/transport")); QVERIFY(!now.contains("sip/port"));
        QCOMPARE(sipPort(),5060);
    }
    void wavAndUnicodePaths() {
        QTemporaryDir tmp; QVERIFY(tmp.isValid()); QString path=tmp.filePath(QString::fromUtf8("rozmowa-Łódź.wav")); AudioRecorder rec; QString err; QVERIFY2(rec.start(path,RecordingFormat::Wav,&err),qPrintable(err));
        constexpr int frames=20; feed(rec,frames); rec.stop(); QVERIFY2(rec.error().isEmpty(),qPrintable(rec.error())); QCOMPARE(rec.sampleCount(),qint64(frames*960));
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); QByteArray b=f.readAll(); QCOMPARE(b.left(4),QByteArray("RIFF")); QCOMPARE(b.mid(8,4),QByteArray("WAVE"));
        QCOMPARE(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData()+24)),quint32(48000));
        QCOMPARE(qFromLittleEndian<quint16>(reinterpret_cast<const uchar *>(b.constData()+22)),quint16(1));
        QCOMPARE(b.size(),44+frames*960*2); QCOMPARE(qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(b.constData()+40)),quint32(frames*960*2));
        QVERIFY(!rec.start(path,RecordingFormat::Wav,&err)); // never overwrite an existing recording
    }
    void realOggOpusAndTrim() {
        QTemporaryDir tmp; AudioRecorder rec; QString err; QString path=tmp.filePath("test.opus"); QVERIFY2(rec.start(path,RecordingFormat::Opus,&err),qPrintable(err));
        constexpr int frames=25; feed(rec,frames); rec.stop(); QVERIFY2(rec.error().isEmpty(),qPrintable(rec.error()));
        QFile f(path); QVERIFY(f.open(QIODevice::ReadOnly)); QByteArray bytes=f.readAll(); QVERIFY(bytes.startsWith("OggS")); QVERIFY(bytes.contains("OpusHead"));
        ogg_sync_state sync{}; ogg_sync_init(&sync); char *buf=ogg_sync_buffer(&sync,bytes.size()); std::memcpy(buf,bytes.constData(),static_cast<size_t>(bytes.size())); ogg_sync_wrote(&sync,bytes.size());
        ogg_stream_state stream{}; bool streamReady=false; ogg_page page{}; ogg_packet packet{}; int packetCount=0,preSkip=0; qint64 lastGranule=-1; bool eos=false;
        while(ogg_sync_pageout(&sync,&page)==1) {
            if(!streamReady){QCOMPARE(ogg_stream_init(&stream,ogg_page_serialno(&page)),0);streamReady=true;}
            QCOMPARE(ogg_stream_pagein(&stream,&page),0);
            while(ogg_stream_packetout(&stream,&packet)==1) {
                if(packetCount==0){QVERIFY(QByteArray(reinterpret_cast<char *>(packet.packet),8)=="OpusHead");preSkip=qFromLittleEndian<quint16>(packet.packet+10);}
                if(packetCount==1) QVERIFY(QByteArray(reinterpret_cast<char *>(packet.packet),8)=="OpusTags");
                if(packet.granulepos>=0) lastGranule=packet.granulepos;
                if(packet.e_o_s) eos=true; ++packetCount;
            }
        }
        QVERIFY(eos); QVERIFY(packetCount>=frames+2); QCOMPARE(lastGranule-preSkip,qint64(frames*960));
        if(streamReady) ogg_stream_clear(&stream); ogg_sync_clear(&sync);
    }
    void unwritableDestination() { AudioRecorder rec; QString err; QVERIFY(!rec.start("/directory-that-does-not-exist/tk.wav",RecordingFormat::Wav,&err)); QVERIFY(!err.isEmpty()); }
private:
    void feed(AudioRecorder &rec,int frames) {
        std::array<int16_t,960> pcm{};
        for(int k=0;k<frames;++k){for(int i=0;i<960;++i)pcm[i]=static_cast<int16_t>(7000*std::sin(2*3.141592653589793*440*(k*960+i)/48000.0));rec.enqueue(pcm.data(),pcm.size());QTest::qWait(5);}
    }
};
QTEST_GUILESS_MAIN(UnitTests)
#include "UnitTests.moc"
