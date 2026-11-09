#include "MainWindow.h"
#include "SipEngine.h"
#include "SettingsDialog.h"
#include "Icons.h"
#include "BrandWidgets.h"
#include <QtWidgets>
#include <QDesktopServices>
#include <QUrl>
#include <QScreen>

namespace {
QFrame *card(const char *name="card") { auto *f=new QFrame;f->setObjectName(name);return f; }
QLabel *label(const QString &text,const char *name="") {
    auto *l=new QLabel(text);l->setTextFormat(Qt::PlainText);l->setObjectName(name);return l;
}
QString durationText(qint64 seconds) { return QString("%1:%2").arg(seconds/60,2,10,QChar('0')).arg(seconds%60,2,10,QChar('0')); }
QFrame *separator(const char *name="separator") { auto *f=new QFrame;f->setObjectName(name);f->setFixedHeight(1);return f; }
}

MainWindow::MainWindow(bool headless,QWidget *parent) : QMainWindow(parent),m_settings(AppSettings::load()) {
    setWindowTitle("TK App — Telekomunikacja Kaliska");setWindowIcon(appIcon());resize(1120,790);setMinimumSize(960,620);
    buildUi();
    if(!headless) {
        if(auto *screen=QGuiApplication::primaryScreen()) {
            auto area=screen->availableGeometry();
            resize(qMax(960,qMin(width(),area.width()-40)),qMax(620,qMin(height(),area.height()-60)));
        }
    }
    m_engine=new SipEngine;m_engine->moveToThread(&m_sipThread);
    connect(&m_sipThread,&QThread::finished,m_engine,&QObject::deleteLater);
    connect(m_engine,&SipEngine::ready,this,[this](bool ready){m_ready=ready;updateControls();});
    connect(m_engine,&SipEngine::devicesReady,this,[this](const QVariantList &devices){m_devices=devices;});
    connect(m_engine,&SipEngine::registrationChanged,this,[this](bool registered,int code,const QString &text){
        m_registered=registered;m_connecting=!registered&&text=="Rejestracja…";
        m_connectionLabel->setText(registered?"●  Połączono":m_connecting?"●  Rejestracja…":"●  Rozłączono");
        m_connectionLabel->setStyleSheet(registered?"color:#377b59;background:#e9f3ed;":m_connecting?"color:#9e792a;background:#faf1db;":"color:#708494;background:#e9eef2;");
        m_accountLabel->setText(m_settings.username.isEmpty()?"Konto nie jest skonfigurowane":"Konto SIP: "+m_settings.username);
        if(code>=400) appendEvent(QString("SIP %1: %2").arg(code).arg(text));
        updateControls();
    });
    connect(m_engine,&SipEngine::callChanged,this,[this](const QString &phase,const QString &peer,bool active,bool confirmed,bool incoming){
        bool wasConfirmed=m_confirmed,wasActive=m_callActive;
        m_callActive=active;m_confirmed=confirmed;m_incoming=incoming;
        QString text=phase=="idle"?"Linia gotowa":phase=="incoming"?"Połączenie przychodzące":phase=="connected"?"W trakcie rozmowy":phase=="ringing"?"Dzwonienie…":"Łączenie…";
        m_callPhase->setText(text);m_callPeer->setText(active?peer:"Czekamy na rozmowę");
        if(confirmed&&!wasConfirmed){m_callElapsed.start();m_callTimer.start();}
        if(!active){m_callTimer.stop();m_callDuration->setText("00:00");}
        if(active&&!wasActive) showPage(0);
        if(incoming&&!confirmed) {
            if(!m_ringTimer.isActive()) {
                QApplication::beep();m_ringTimer.start();QApplication::alert(this);
                if(m_tray&&(isMinimized()||!isActiveWindow())) m_tray->showMessage("Telekomunikacja Kaliska — połączenie",peer,QSystemTrayIcon::Information,10000);
            }
        } else m_ringTimer.stop();
        if(active) m_number->setFocus();
        updateControls();
    });
    connect(m_engine,&SipEngine::callFinished,this,[this](const QString &peer,int code,int seconds,bool incoming){
        QString clean=peer;clean.replace('\n',' ');clean=clean.left(100);
        QString row=QTime::currentTime().toString("HH:mm")+"    "+(incoming?"Przychodzące":"Wychodzące")+"    "+clean+"    ·    "+durationText(seconds)+"    ·    SIP "+QString::number(code);
        m_history->insertItem(0,new QListWidgetItem(glyph(incoming?Glyph::Headphones:Glyph::Phone),row));
        while(m_history->count()>100) delete m_history->takeItem(m_history->count()-1);
        m_historyEmpty->hide();m_history->show();
    });
    connect(m_engine,&SipEngine::dndChanged,this,[this](bool enabled,bool pending,const QString &text){
        m_dndEnabled=enabled;m_dndPending=pending;QSignalBlocker blocker(m_dndSwitch);m_dndSwitch->setChecked(enabled);
        m_dndLabel->setText(pending?"Przełączanie w tle…":enabled?"Włączone":"Wyłączone");
        m_dndLabel->setToolTip(text);updateControls();
    });
    connect(m_engine,&SipEngine::mutedChanged,this,[this](bool muted){
        QSignalBlocker blocker(m_muteButton);m_muteButton->setChecked(muted);
        m_muteButton->setText(muted?"Mikrofon wyciszony":"Wycisz mikrofon");
    });
    connect(m_engine,&SipEngine::recordingChanged,this,[this](bool active,const QString &path){
        m_recording=active;m_recordLabel->setText(active?"●  Nagrywanie 00:00":"Zapisano: "+QFileInfo(path).fileName());
        m_recordLabel->setToolTip(path);m_recordLabel->setStyleSheet(active?"color:#b52f2a;":"color:#78909f;");
        if(!active) refreshRecordings();
        updateControls();
    });
    connect(m_engine,&SipEngine::recordingProgress,this,[this](qint64 seconds){if(m_recording)m_recordLabel->setText("●  Nagrywanie "+durationText(seconds));});
    connect(m_engine,&SipEngine::error,this,[this](const QString &text){m_errorBanner->setText(text);m_errorBanner->show();appendEvent("Błąd: "+text);if(m_connecting&&!m_registered){m_connecting=false;updateControls();}});
    connect(m_engine,&SipEngine::log,this,&MainWindow::appendEvent);
    m_callTimer.setInterval(1000);connect(&m_callTimer,&QTimer::timeout,this,[this]{m_callDuration->setText(durationText(m_callElapsed.elapsed()/1000));});
    m_ringTimer.setInterval(2500);connect(&m_ringTimer,&QTimer::timeout,this,[]{QApplication::beep();});
    m_sipThread.start();AppSettings settings=m_settings;
    command([settings,headless](SipEngine *engine){engine->initialize(settings,headless);});
    appendEvent("Profil Telekomunikacji Kaliskiej: 15.235.62.162:5060 / UDP. Brak automatycznej rejestracji.");
    updateControls();
}
MainWindow::~MainWindow() {
    if(m_engine&&m_sipThread.isRunning()) {
        QMetaObject::invokeMethod(m_engine,[engine=m_engine]{engine->shutdown();},Qt::BlockingQueuedConnection);
        m_sipThread.quit();m_sipThread.wait();
    }
}
void MainWindow::command(std::function<void(SipEngine *)> fn) {
    if(!m_engine) return;
    QMetaObject::invokeMethod(m_engine,[engine=m_engine,fn=std::move(fn)]{fn(engine);},Qt::QueuedConnection);
}

void MainWindow::buildUi() {
    auto *central=new QWidget;setCentralWidget(central);auto *shell=new QHBoxLayout(central);shell->setContentsMargins(0,0,0,0);shell->setSpacing(0);
    auto *sidebar=new QFrame;sidebar->setObjectName("sidebar");sidebar->setFixedWidth(212);
    auto *side=new QVBoxLayout(sidebar);side->setContentsMargins(20,28,20,22);side->setSpacing(12);
    auto *brandRow=new QHBoxLayout;brandRow->setSpacing(11);
    auto *crest=new QLabel;crest->setPixmap(QPixmap(":/branding/telekomunikacja-kaliska.png").scaledToHeight(73,Qt::SmoothTransformation));
    crest->setAccessibleName("Logo Telekomunikacji Kaliskiej");brandRow->addWidget(crest,0,Qt::AlignTop);
    auto *brandText=new QVBoxLayout;brandText->setSpacing(3);brandText->addWidget(label("TK App","appBrand"));
    brandText->addWidget(label("Telekomunikacja\nKaliska","networkBrand"));brandRow->addLayout(brandText);side->addLayout(brandRow);
    side->addSpacing(25);
    auto nav=[this,side](const QString &text,Glyph icon,int index) {
        auto *button=new QPushButton(text);button->setObjectName("nav");button->setCheckable(true);button->setIcon(glyph(icon,	QColor("#000000")));button->setIconSize(QSize(19,19));button->setCursor(Qt::PointingHandCursor);
        side->addWidget(button);connect(button,&QPushButton::clicked,this,[this,index]{showPage(index);});return button;
    };
    m_navPhone=nav("Telefon",Glyph::Phone,0);m_navHistory=nav("Historia połączeń",Glyph::History,1);m_navRecordings=nav("Nagrania",Glyph::Folder,2);
    auto *navGroup=new QButtonGroup(this);navGroup->setExclusive(true);navGroup->addButton(m_navPhone);navGroup->addButton(m_navHistory);navGroup->addButton(m_navRecordings);m_navPhone->setChecked(true);
    side->addStretch();
    /* side->addWidget(label("PROFIL SIECI TK","sideEyebrow"));side->addWidget(label("15.235.62.162","sideServer"));side->addWidget(label("UDP / 5060 · profil stały","sideProtocol")); */
    side->addSpacing(8);side->addWidget(separator("sideSeparator"));side->addSpacing(5);
    m_settingsButton=new QPushButton("Ustawienia");m_settingsButton->setObjectName("sidebarSettings");m_settingsButton->setIcon(glyph(Glyph::Settings,QColor("#000000")));m_settingsButton->setIconSize(QSize(19,19));side->addWidget(m_settingsButton);
    connect(m_settingsButton,&QPushButton::clicked,this,&MainWindow::showSettings);
    side->addWidget(label("TK APP  0.0.2 (B)  /  DESKTOP","sideVersion"));shell->addWidget(sidebar);

    auto *mainArea=new QWidget;mainArea->setObjectName("mainArea");auto *main=new QVBoxLayout(mainArea);main->setContentsMargins(28,25,28,18);main->setSpacing(18);
    auto *header=new QHBoxLayout;auto *titles=new QVBoxLayout;titles->setSpacing(5);
    m_heading=label("Telefon","pageTitle");m_subheading=label("Rozmawiaj w sieci Telekomunikacji Kaliskiej.","pageSubtitle");titles->addWidget(m_heading);titles->addWidget(m_subheading);header->addLayout(titles);header->addStretch();
    auto *account=new QVBoxLayout;account->setSpacing(7);auto *accountButtons=new QHBoxLayout;
    m_connectionLabel=label("●  Rozłączono","connectionStatus");accountButtons->addWidget(m_connectionLabel);
    m_connectButton=new QPushButton("Połącz konto");m_connectButton->setObjectName("primary");m_connectButton->setMinimumHeight(38);accountButtons->addWidget(m_connectButton);account->addLayout(accountButtons);
    m_accountLabel=label(m_settings.username.isEmpty()?"Konto nie jest skonfigurowane":"Konto SIP: "+m_settings.username,"hint");m_accountLabel->setAlignment(Qt::AlignRight);account->addWidget(m_accountLabel);header->addLayout(account);main->addLayout(header);
    connect(m_connectButton,&QPushButton::clicked,this,&MainWindow::connectClicked);
    m_errorBanner=label("","errorBanner");m_errorBanner->setWordWrap(true);m_errorBanner->hide();main->addWidget(m_errorBanner);
    m_pages=new QStackedWidget;m_pages->setObjectName("mainPages");main->addWidget(m_pages,1);

    // PHONE — two balanced columns inside a scrollable canvas for smaller screens.
    auto *phoneCanvas=new QWidget;phoneCanvas->setMinimumSize(714,598);auto *phone=new QHBoxLayout(phoneCanvas);phone->setContentsMargins(0,0,0,0);phone->setSpacing(18);
    auto *dial=card();auto *dl=new QVBoxLayout(dial);dl->setContentsMargins(21,20,21,18);dl->setSpacing(14);
    auto *dialTitle=new QHBoxLayout;dialTitle->addWidget(label("Wybierz numer","sectionTitle"));dialTitle->addStretch();dialTitle->addWidget(label("KLAWIATURA SIP","eyebrow"));dl->addLayout(dialTitle);
    auto *numberRow=new QHBoxLayout;m_number=new QLineEdit;m_number->setObjectName("dialNumber");m_number->setPlaceholderText("Numer telefonu");m_number->setMaxLength(80);m_number->setAlignment(Qt::AlignCenter);m_number->setFixedHeight(65);m_number->setAccessibleName("Numer telefonu lub klawiatura DTMF");m_number->installEventFilter(this);numberRow->addWidget(m_number);
    m_backButton=new QPushButton;m_backButton->setIcon(glyph(Glyph::Back));m_backButton->setFixedSize(37,44);m_backButton->setToolTip("Usuń ostatnią cyfrę");m_backButton->setAccessibleName("Usuń ostatnią cyfrę");numberRow->addWidget(m_backButton);dl->addLayout(numberRow);connect(m_backButton,&QPushButton::clicked,m_number,&QLineEdit::backspace);
    auto *grid=new QGridLayout;grid->setSpacing(8);const QString digits="123456789*0#";const QStringList letters={"","ABC","DEF","GHI","JKL","MNO","PQRS","TUV","WXYZ","","+",""};
    for(int i=0;i<12;++i){QString digit=digits.mid(i,1);auto *key=new DialKey(digit,letters[i]);grid->addWidget(key,i/3,i%3);connect(key,&QPushButton::clicked,this,[this,digit]{digitPressed(digit);});}
    dl->addLayout(grid,1);
    auto *callButtons=new QHBoxLayout;m_callButton=new QPushButton("Zadzwoń");m_callButton->setObjectName("primary");m_callButton->setIcon(glyph(Glyph::Phone,QColor("#ffffff")));m_callButton->setMinimumHeight(47);
    m_hangupButton=new QPushButton("Rozłącz");m_hangupButton->setObjectName("danger");m_hangupButton->setIcon(glyph(Glyph::Hangup,QColor("#bc4b45")));m_hangupButton->setMinimumHeight(47);callButtons->addWidget(m_callButton,1);callButtons->addWidget(m_hangupButton,1);dl->addLayout(callButtons);
    auto *keysHint=label("Enter — zadzwoń / odbierz     Esc — rozłącz","hint");keysHint->setAlignment(Qt::AlignCenter);dl->addWidget(keysHint);
    connect(m_callButton,&QPushButton::clicked,this,&MainWindow::callClicked);connect(m_hangupButton,&QPushButton::clicked,this,[this]{command([](SipEngine *e){e->hangup();});});phone->addWidget(dial,1);

    auto *right=new QVBoxLayout;right->setSpacing(12);
    auto *call=card("callCard");auto *cal=new QVBoxLayout(call);cal->setContentsMargins(18,17,18,16);cal->setSpacing(10);
    auto *stateRow=new QHBoxLayout;m_callPhase=label("Linia gotowa","status");stateRow->addWidget(m_callPhase);stateRow->addStretch();m_callDuration=label("00:00","clock");stateRow->addWidget(m_callDuration);cal->addLayout(stateRow);
    m_callPeer=label("Czekamy na rozmowę","callPeer");m_callPeer->setWordWrap(true);m_callPeer->setMinimumHeight(36);cal->addWidget(m_callPeer);
    m_muteButton=new QPushButton("Wycisz mikrofon");m_muteButton->setIcon(glyph(Glyph::Mic));m_muteButton->setCheckable(true);cal->addWidget(m_muteButton);connect(m_muteButton,&QPushButton::clicked,this,[this](bool muted){command([muted](SipEngine *e){e->setMuted(muted);});});right->addWidget(call);

    auto *services=card();auto *sl=new QVBoxLayout(services);sl->setContentsMargins(18,16,18,16);sl->setSpacing(10);
    auto *serviceTitle=new QHBoxLayout;serviceTitle->addWidget(label("Usługi Telekomunikacji Kaliskiej","sectionTitle"));serviceTitle->addStretch();sl->addLayout(serviceTitle);
    auto *dndRow=new QHBoxLayout;auto *moon=new QLabel;moon->setPixmap(glyph(Glyph::Moon,QColor("#c39b36")).pixmap(26,26));dndRow->addWidget(moon);auto *dndText=new QVBoxLayout;dndText->setSpacing(3);dndText->addWidget(label("Nie przeszkadzać","sectionTitle"));m_dndLabel=label("Wyłączone · usługa 13","muted");dndText->addWidget(m_dndLabel);dndRow->addLayout(dndText);dndRow->addStretch();m_dndSwitch=new ToggleSwitch;m_dndSwitch->setAccessibleName("DND — przełącz telefonem w tle na 13");dndRow->addWidget(m_dndSwitch);sl->addLayout(dndRow);
    connect(m_dndSwitch,&QCheckBox::clicked,this,[this]{QSignalBlocker blocker(m_dndSwitch);m_dndSwitch->setChecked(m_dndEnabled);m_dndPending=true;updateControls();command([](SipEngine *e){e->toggleDnd();});});sl->addWidget(separator());
    m_echoButton=new ServiceButton("Echo test","Sprawdź mikrofon i odsłuch","12",Glyph::Echo);m_testButton=new ServiceButton("Test połączeń","Połącz się z usługą testową","14",Glyph::Test);sl->addWidget(m_echoButton);sl->addWidget(m_testButton);
    connect(m_echoButton,&QPushButton::clicked,this,[this]{m_number->setText("12");callClicked();});connect(m_testButton,&QPushButton::clicked,this,[this]{m_number->setText("14");callClicked();});right->addWidget(services,1);

    auto *record=card();auto *rl=new QVBoxLayout(record);rl->setContentsMargins(18,15,18,15);rl->setSpacing(8);auto *recHeader=new QHBoxLayout;recHeader->addWidget(label("Nagrywanie","sectionTitle"));recHeader->addStretch();
    m_format=new QComboBox;m_format->addItems({"WAV","OPUS"});m_format->setCurrentIndex(m_settings.recordingFormat==RecordingFormat::Wav?0:1);m_format->setAccessibleName("Format nagrania");m_format->setFixedWidth(84);recHeader->addWidget(m_format);
    m_recordButton=new QPushButton("Nagraj");m_recordButton->setObjectName("recordAction");m_recordButton->setIcon(glyph(Glyph::Record,QColor("#bc302a")));recHeader->addWidget(m_recordButton);rl->addLayout(recHeader);
    m_recordLabel=label("Obie strony rozmowy · zapis lokalny","muted");m_recordLabel->setMaximumWidth(345);rl->addWidget(m_recordLabel);
    m_autoRecord=new QCheckBox("Nagrywaj automatycznie nowe rozmowy");m_autoRecord->setChecked(m_settings.autoRecord);rl->addWidget(m_autoRecord);right->addWidget(record);
    connect(m_recordButton,&QPushButton::clicked,this,&MainWindow::toggleRecording);
    connect(m_format,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int index){m_settings.recordingFormat=index==0?RecordingFormat::Wav:RecordingFormat::Opus;m_settings.save();applyRecordingSettings();});
    connect(m_autoRecord,&QCheckBox::toggled,this,[this](bool enabled){m_settings.autoRecord=enabled;m_settings.save();applyRecordingSettings();});
    phone->addLayout(right,1);auto *phoneScroll=new QScrollArea;phoneScroll->setWidgetResizable(true);phoneScroll->setFrameShape(QFrame::NoFrame);phoneScroll->setWidget(phoneCanvas);m_pages->addWidget(phoneScroll);

    // HISTORY — actual session history and the real SIP event log, no sample data.
    auto *historyPage=new QWidget;auto *hl=new QVBoxLayout(historyPage);hl->setContentsMargins(0,0,0,0);hl->setSpacing(15);
    auto *historyTabs=new QTabWidget;auto *callsPage=new QWidget;auto *callsLayout=new QVBoxLayout(callsPage);
    m_historyEmpty=label("Brak połączeń w tej sesji.\nTutaj pojawią się odebrane i wykonane rozmowy.","emptyTitle");m_historyEmpty->setAlignment(Qt::AlignCenter);callsLayout->addWidget(m_historyEmpty,1);
    m_history=new QListWidget;callsLayout->addWidget(m_history,1);m_history->hide();historyTabs->addTab(callsPage,"Historia połączeń");
    m_events=new QPlainTextEdit;m_events->setReadOnly(true);m_events->setMaximumBlockCount(250);historyTabs->addTab(m_events,"Zdarzenia SIP");hl->addWidget(historyTabs,1);
    hl->addWidget(label("Historia obejmuje bieżącą sesję. Hasła i pakiety SIP nie są zapisywane w dzienniku.","hint"));m_pages->addWidget(historyPage);

    // RECORDINGS — reads real files from the chosen directory.
    auto *recordingsPage=new QWidget;auto *files=new QVBoxLayout(recordingsPage);files->setContentsMargins(0,0,0,0);files->setSpacing(16);
    auto *dirCard=card();auto *dirRow=new QHBoxLayout(dirCard);dirRow->setContentsMargins(18,15,18,15);auto *dirText=new QVBoxLayout;dirText->setSpacing(5);dirText->addWidget(label("Twój katalog nagrań","sectionTitle"));m_directoryLabel=label(m_settings.recordingDirectory,"muted");m_directoryLabel->setWordWrap(true);dirText->addWidget(m_directoryLabel);dirRow->addLayout(dirText,1);
    auto *openDir=new QPushButton("Otwórz folder");openDir->setIcon(glyph(Glyph::Folder));dirRow->addWidget(openDir);auto *chooseDir=new QPushButton("Zmień");dirRow->addWidget(chooseDir);files->addWidget(dirCard);
    connect(openDir,&QPushButton::clicked,this,[this]{QDir().mkpath(m_settings.recordingDirectory);if(!QDesktopServices::openUrl(QUrl::fromLocalFile(m_settings.recordingDirectory)))appendEvent("Katalog nagrań: "+m_settings.recordingDirectory);});connect(chooseDir,&QPushButton::clicked,this,&MainWindow::chooseRecordingDirectory);
    auto *fileCard=card();auto *fileLayout=new QVBoxLayout(fileCard);fileLayout->setContentsMargins(8,8,8,8);
    m_recordingsEmpty=label("Jeszcze nie ma nagrań.\nWłącz nagrywanie podczas rozmowy, a pliki pojawią się tutaj.","emptyTitle");m_recordingsEmpty->setAlignment(Qt::AlignCenter);fileLayout->addWidget(m_recordingsEmpty,1);
    m_recordings=new QTreeWidget;m_recordings->setRootIsDecorated(false);m_recordings->setHeaderLabels({"Nagranie","Format","Rozmiar","Data"});m_recordings->header()->setSectionResizeMode(0,QHeaderView::Stretch);m_recordings->header()->setSectionResizeMode(1,QHeaderView::ResizeToContents);m_recordings->header()->setSectionResizeMode(2,QHeaderView::ResizeToContents);m_recordings->header()->setSectionResizeMode(3,QHeaderView::ResizeToContents);fileLayout->addWidget(m_recordings,1);m_recordings->hide();files->addWidget(fileCard,1);
    connect(m_recordings,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem *item,int){if(!QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(0,Qt::UserRole).toString())))appendEvent("Nie udało się otworzyć nagrania w odtwarzaczu systemowym.");});
    files->addWidget(label("Dwukrotne kliknięcie otwiera plik w odtwarzaczu systemowym. Nagrywaj zgodnie z prawem.","hint"));m_pages->addWidget(recordingsPage);

    auto *footer=new QHBoxLayout;footer->addWidget(label("TELEKOMUNIKACJA KALISKA  /  SIP UDP 5060","hint"));footer->addStretch();footer->addWidget(label("DND 13    ·    TEST 14    ·    ECHO 12","hint"));main->addLayout(footer);shell->addWidget(mainArea,1);
    if(QSystemTrayIcon::isSystemTrayAvailable()) {
        m_tray=new QSystemTrayIcon(appIcon(),this);m_tray->setToolTip("TK App — Telekomunikacja Kaliska");auto *menu=new QMenu(this);menu->addAction("Pokaż TK App",this,[this]{showNormal();raise();activateWindow();});menu->addSeparator();menu->addAction("Zakończ",this,&QWidget::close);m_tray->setContextMenu(menu);m_tray->show();
        connect(m_tray,&QSystemTrayIcon::activated,this,[this](QSystemTrayIcon::ActivationReason reason){if(reason==QSystemTrayIcon::Trigger){showNormal();raise();activateWindow();}});
    }
    auto *recordShortcut=new QShortcut(QKeySequence("Ctrl+R"),this);connect(recordShortcut,&QShortcut::activated,this,&MainWindow::toggleRecording);
    auto *settingsShortcut=new QShortcut(QKeySequence("Ctrl+,"),this);connect(settingsShortcut,&QShortcut::activated,this,&MainWindow::showSettings);
    auto *phoneShortcut=new QShortcut(QKeySequence("Ctrl+1"),this);connect(phoneShortcut,&QShortcut::activated,this,[this]{showPage(0);m_number->setFocus();});
    auto *historyShortcut=new QShortcut(QKeySequence("Ctrl+2"),this);connect(historyShortcut,&QShortcut::activated,this,[this]{showPage(1);});
    auto *filesShortcut=new QShortcut(QKeySequence("Ctrl+3"),this);connect(filesShortcut,&QShortcut::activated,this,[this]{showPage(2);});
}

void MainWindow::showPage(int index) {
    if(!m_pages||index<0||index>2) return;
    m_pages->setCurrentIndex(index);m_navPhone->setChecked(index==0);m_navHistory->setChecked(index==1);m_navRecordings->setChecked(index==2);
    const QStringList titles={"Telefon","Historia połączeń","Nagrania rozmów"};
    const QStringList descriptions={"Rozmawiaj w sieci Telekomunikacji Kaliskiej.","Twoje połączenia i dziennik zdarzeń SIP.","Wszystkie nagrania zostają na Twoim urządzeniu."};
    m_heading->setText(titles[index]);m_subheading->setText(descriptions[index]);if(index==2)refreshRecordings();
}
void MainWindow::refreshRecordings() {
    if(!m_recordings) return;
    m_recordings->clear();m_directoryLabel->setText(m_settings.recordingDirectory);
    QDir directory(m_settings.recordingDirectory);auto entries=directory.entryInfoList({"*.wav","*.opus"},QDir::Files|QDir::NoSymLinks,QDir::Time);
    for(const QFileInfo &entry:entries) {
        QString size=entry.size()<1024*1024?QString::number(entry.size()/1024.0,'f',1)+" KB":QString::number(entry.size()/(1024.0*1024.0),'f',1)+" MB";
        auto *item=new QTreeWidgetItem({entry.fileName(),entry.suffix().toUpper(),size,entry.lastModified().toString("dd.MM HH:mm")});
        item->setData(0,Qt::UserRole,entry.absoluteFilePath());item->setToolTip(0,entry.absoluteFilePath());m_recordings->addTopLevelItem(item);
    }
    m_recordingsEmpty->setVisible(entries.isEmpty());m_recordings->setVisible(!entries.isEmpty());
}
void MainWindow::chooseRecordingDirectory() {
    QString path=QFileDialog::getExistingDirectory(this,"Katalog nagrań Telekomunikacji Kaliskiej",m_settings.recordingDirectory);
    if(path.isEmpty()) return;
    m_settings.recordingDirectory=path;m_settings.save();applyRecordingSettings();refreshRecordings();
}
void MainWindow::updateControls() {
    m_connectButton->setText(m_registered?"Rozłącz konto":m_connecting?"Rejestracja…":"Połącz konto");
    m_connectButton->setEnabled(m_ready&&!m_callActive&&!m_dndPending&&!m_connecting);
    m_settingsButton->setEnabled(!m_callActive&&!m_dndPending&&!m_connecting);
    m_callButton->setText(m_incoming&&!m_confirmed?"Odbierz":"Zadzwoń");m_callButton->setEnabled((m_registered&&!m_callActive)||(m_incoming&&!m_confirmed));
    m_hangupButton->setEnabled(m_callActive);m_hangupButton->setText(m_incoming&&!m_confirmed?"Odrzuć":"Rozłącz");
    m_muteButton->setEnabled(m_confirmed);m_number->setReadOnly(m_callActive);m_backButton->setEnabled(!m_callActive);
    m_dndSwitch->setEnabled(m_registered&&!m_dndPending);m_echoButton->setEnabled(m_registered&&!m_callActive);m_testButton->setEnabled(m_registered&&!m_callActive);
    m_recordButton->setEnabled(m_confirmed||m_recording);m_recordButton->setText(m_recording?"Zatrzymaj":"Nagraj");m_format->setEnabled(!m_recording);
    m_navPhone->setText(m_callActive?"Telefon · rozmowa":"Telefon");
}
void MainWindow::showSettings() {
    if(m_callActive||m_dndPending||m_connecting) return;
    SettingsDialog dialog(m_settings,m_devices,this);
    if(dialog.exec()==QDialog::Accepted) {
        m_settings=dialog.settings();m_settings.save();
        {QSignalBlocker blocker(m_format);m_format->setCurrentIndex(m_settings.recordingFormat==RecordingFormat::Wav?0:1);}
        {QSignalBlocker blocker(m_autoRecord);m_autoRecord->setChecked(m_settings.autoRecord);}
        m_accountLabel->setText(m_settings.username.isEmpty()?"Konto nie jest skonfigurowane":"Konto SIP: "+m_settings.username);
        applyRecordingSettings();refreshRecordings();appendEvent(m_registered?"Zapisano ustawienia. Zmiany konta/audio zastosuj przez rozłączenie i ponowne połączenie konta.":"Zapisano ustawienia konta Telekomunikacji Kaliskiej.");
    }
}
void MainWindow::connectClicked() {
    m_errorBanner->hide();
    if(m_registered){command([](SipEngine *e){e->disconnectAccount();});return;}
    if(m_settings.username.isEmpty()){showSettings();if(m_settings.username.isEmpty())return;}
    if(m_settings.password.isEmpty()) {
        bool ok=false;QString password=QInputDialog::getText(this,"Telekomunikacja Kaliska — konto SIP","Hasło konta "+m_settings.username+":",QLineEdit::Password,{},&ok);
        if(!ok)return;
        m_settings.password=password;
    }
    AppSettings settings=m_settings;m_connecting=true;updateControls();command([settings](SipEngine *e){e->connectAccount(settings);});
}
void MainWindow::callClicked() {
    if(m_incoming&&!m_confirmed){command([](SipEngine *e){e->answer();});return;}
    if(m_callActive||!m_registered)return;
    m_errorBanner->hide();QString number=normalizeNumber(m_number->text());m_number->setText(number);command([number](SipEngine *e){e->dial(number);});
}
void MainWindow::digitPressed(const QString &digit) {
    if(m_confirmed)command([digit](SipEngine *e){e->sendDtmf(digit);});
    else if(!m_callActive)m_number->insert(digit);
    m_number->setFocus();
}
void MainWindow::toggleRecording() {
    if(m_recording){command([](SipEngine *e){e->stopRecording();});return;}
    if(!m_confirmed)return;
    RecordingFormat format=m_settings.recordingFormat;QString directory=m_settings.recordingDirectory;
    command([format,directory](SipEngine *e){e->startRecording(format,directory);});
}
void MainWindow::applyRecordingSettings() {
    bool automatic=m_settings.autoRecord;auto format=m_settings.recordingFormat;QString directory=m_settings.recordingDirectory;
    command([automatic,format,directory](SipEngine *e){e->updateRecordingSettings(automatic,format,directory);});
}
void MainWindow::appendEvent(const QString &text){m_events->appendPlainText(QTime::currentTime().toString("HH:mm:ss")+"  "+text);}
bool MainWindow::eventFilter(QObject *object,QEvent *event) {
    if(object==m_number&&event->type()==QEvent::KeyPress) {
        auto *key=static_cast<QKeyEvent *>(event);
        if(key->key()==Qt::Key_Return||key->key()==Qt::Key_Enter){callClicked();return true;}
        if(key->key()==Qt::Key_Escape&&m_callActive){command([](SipEngine *e){e->hangup();});return true;}
        if(m_confirmed&&key->text().size()==1&&QString("0123456789*#").contains(key->text())){digitPressed(key->text());return true;}
    }
    return QMainWindow::eventFilter(object,event);
}
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key()==Qt::Key_Return||event->key()==Qt::Key_Enter){callClicked();return;}
    if(event->key()==Qt::Key_Escape&&m_callActive){command([](SipEngine *e){e->hangup();});return;}
    if(event->modifiers()==Qt::NoModifier&&event->text().size()==1&&QString("0123456789*#").contains(event->text())){digitPressed(event->text());return;}
    QMainWindow::keyPressEvent(event);
}
void MainWindow::closeEvent(QCloseEvent *event) {
    if((m_callActive||m_dndPending)&&QMessageBox::question(this,"Zakończyć TK App?","Trwające połączenia zostaną zakończone, a nagranie zapisane.",QMessageBox::Yes|QMessageBox::No,QMessageBox::No)!=QMessageBox::Yes){event->ignore();return;}
    event->accept();
}
