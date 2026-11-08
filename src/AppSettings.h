#pragma once
#include <QString>

enum class RecordingFormat { Wav, Opus };
struct AppSettings {
    QString username;
    QString authUsername;
    QString password; // Memory only. Never written by save().
    QString identityDomain;
    QString displayName = QStringLiteral("TK App");
    QString stunServer;
    bool ice = false;
    int captureDevice = -1;
    int playbackDevice = -2;
    QString recordingDirectory;
    RecordingFormat recordingFormat = RecordingFormat::Wav;
    bool autoRecord = false;
    bool rejectIncomingOnDnd = true;
    int dndAnswerSeconds = 2;
    static AppSettings load();
    void save() const;
};
QString sipServer();
constexpr int sipPort() { return 5060; }
QString networkName();
QString normalizeNumber(QString number);
bool validNumber(const QString &number);
bool validSipUsername(const QString &username);
