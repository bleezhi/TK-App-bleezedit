#pragma once
#include "AppSettings.h"
#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantList>
#include <functional>
class SipEngine; class QLabel; class QPushButton; class QCheckBox; class QLineEdit; class QComboBox;
class QPlainTextEdit; class QListWidget; class QSystemTrayIcon; class QStackedWidget; class QTreeWidget;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(bool headless=false, QWidget *parent=nullptr);
    ~MainWindow() override;
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
private:
    void buildUi();
    void command(std::function<void(SipEngine *)> fn);
    void showSettings();
    void showPage(int index);
    void refreshRecordings();
    void chooseRecordingDirectory();
    void connectClicked();
    void callClicked();
    void digitPressed(const QString &digit);
    void updateControls();
    void appendEvent(const QString &text);
    void toggleRecording();
    void applyRecordingSettings();
    AppSettings m_settings;
    QThread m_sipThread;
    SipEngine *m_engine=nullptr;
    QVariantList m_devices;
    bool m_ready=false, m_registered=false, m_connecting=false, m_callActive=false, m_confirmed=false, m_incoming=false;
    bool m_dndEnabled=false, m_dndPending=false, m_recording=false;
    QLabel *m_connectionLabel=nullptr, *m_accountLabel=nullptr, *m_callPhase=nullptr, *m_callPeer=nullptr, *m_callDuration=nullptr;
    QLabel *m_dndLabel=nullptr, *m_recordLabel=nullptr, *m_errorBanner=nullptr, *m_heading=nullptr, *m_subheading=nullptr;
    QLabel *m_historyEmpty=nullptr, *m_recordingsEmpty=nullptr, *m_directoryLabel=nullptr;
    QPushButton *m_connectButton=nullptr, *m_settingsButton=nullptr, *m_callButton=nullptr, *m_hangupButton=nullptr;
    QPushButton *m_muteButton=nullptr, *m_recordButton=nullptr, *m_backButton=nullptr, *m_echoButton=nullptr, *m_testButton=nullptr;
    QPushButton *m_navPhone=nullptr, *m_navHistory=nullptr, *m_navRecordings=nullptr;
    QCheckBox *m_dndSwitch=nullptr, *m_autoRecord=nullptr;
    QLineEdit *m_number=nullptr;
    QComboBox *m_format=nullptr;
    QPlainTextEdit *m_events=nullptr;
    QListWidget *m_history=nullptr;
    QTreeWidget *m_recordings=nullptr;
    QStackedWidget *m_pages=nullptr;
    QSystemTrayIcon *m_tray=nullptr;
    QTimer m_callTimer, m_ringTimer;
    QElapsedTimer m_callElapsed;
};
