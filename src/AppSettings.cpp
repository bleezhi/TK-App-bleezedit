#include "AppSettings.h"
#include <QDir>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

QString sipServer() { return QStringLiteral("15.235.62.162"); }
QString networkName() { return QStringLiteral("Telekomunikacja Kaliska"); }
QString normalizeNumber(QString n) {
    n = n.trimmed();
    n.remove(QRegularExpression(QStringLiteral("[\\s()\\-]")));
    return n;
}
bool validNumber(const QString &n) {
    static const QRegularExpression re(QStringLiteral("^\\+?[0-9*#]{1,64}$"));
    return re.match(n).hasMatch();
}
bool validSipUsername(const QString &n) {
    static const QRegularExpression re(QStringLiteral("^[A-Za-z0-9_.!~*'()+%\\-]{1,128}$"));
    return re.match(n).hasMatch();
}
AppSettings AppSettings::load() {
    QSettings s;
    AppSettings a;
    a.username = s.value("sip/username").toString();
    a.authUsername = s.value("sip/authUsername").toString();
    a.identityDomain = s.value("sip/domain").toString();
    a.displayName = s.value("sip/displayName", a.displayName).toString();
    // The TK profile is fixed, including when migrating 1.x settings.
    s.remove("sip/port");
    s.remove("sip/transport");
    a.stunServer = s.value("sip/stun").toString();
    a.ice = s.value("sip/ice", false).toBool();
    a.captureDevice = s.value("audio/capture", -1).toInt();
    a.playbackDevice = s.value("audio/playback", -2).toInt();
    QString docs = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if(docs.isEmpty()) docs = QDir::homePath();
    a.recordingDirectory = s.value("recording/directory", docs + "/TK App/Nagrania").toString();
    a.recordingFormat = s.value("recording/format", "WAV").toString() == "OPUS" ? RecordingFormat::Opus : RecordingFormat::Wav;
    a.autoRecord = s.value("recording/auto", false).toBool();
    a.rejectIncomingOnDnd = s.value("dnd/rejectIncoming", true).toBool();
    a.dndAnswerSeconds = qBound(1, s.value("dnd/seconds", 2).toInt(), 10);
    // Remove a credential key from older / manually edited configurations too.
    s.remove("sip/password");
    return a;
}
void AppSettings::save() const {
    QSettings s;
    s.setValue("sip/username", username);
    s.setValue("sip/authUsername", authUsername);
    s.setValue("sip/domain", identityDomain);
    s.setValue("sip/displayName", displayName);
    s.remove("sip/port");
    s.remove("sip/transport");
    s.setValue("sip/stun", stunServer);
    s.setValue("sip/ice", ice);
    s.setValue("audio/capture", captureDevice);
    s.setValue("audio/playback", playbackDevice);
    s.setValue("recording/directory", recordingDirectory);
    s.setValue("recording/format", recordingFormat == RecordingFormat::Wav ? "WAV" : "OPUS");
    s.setValue("recording/auto", autoRecord);
    s.setValue("dnd/rejectIncoming", rejectIncomingOnDnd);
    s.setValue("dnd/seconds", dndAnswerSeconds);
    s.remove("sip/password");
}
