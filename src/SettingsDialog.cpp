#include "SettingsDialog.h"
#include "Icons.h"
#include <QtWidgets>

SettingsDialog::SettingsDialog(const AppSettings &s, const QVariantList &devices, QWidget *parent) : QDialog(parent), m_original(s) {
    setWindowTitle("Ustawienia TK App"); resize(610, 580); setMinimumWidth(540);
    auto *outer = new QVBoxLayout(this); outer->setContentsMargins(24,24,24,20); outer->setSpacing(18);
    auto *heading = new QLabel("Twoje konto w Telekomunikacji Kaliskiej"); heading->setObjectName("sectionTitle"); outer->addWidget(heading);
    auto *tabs = new QTabWidget; outer->addWidget(tabs);
    auto *account = new QWidget; auto *form = new QFormLayout(account); form->setSpacing(13); form->setContentsMargins(18,22,18,18);
    auto *server = new QLineEdit(sipServer()); server->setReadOnly(true);
    form->addRow("Serwer SIP", server);
    m_user = new QLineEdit(s.username); m_user->setPlaceholderText("Login / numer konta SIP"); m_user->setMaxLength(128); form->addRow("Login SIP", m_user);
    m_authUser = new QLineEdit(s.authUsername); m_authUser->setPlaceholderText("Opcjonalnie; domyślnie taki sam jak login"); form->addRow("Login autoryzacji", m_authUser);
    m_password = new QLineEdit(s.password); m_password->setEchoMode(QLineEdit::Password); m_password->setPlaceholderText("Hasło nie jest zapisywane na dysku"); form->addRow("Hasło", m_password);
    m_domain = new QLineEdit(s.identityDomain); m_domain->setPlaceholderText("Opcjonalnie; domyślnie 15.235.62.162"); form->addRow("Domena konta", m_domain);
    m_name = new QLineEdit(s.displayName); m_name->setMaxLength(64); form->addRow("Nazwa wyświetlana", m_name);
    auto *profile = new QLineEdit("UDP / 5060 — stały profil sieci TK"); profile->setReadOnly(true);
    profile->setObjectName("fixedProfile"); form->addRow("Transport i port", profile);
    auto hideFormRow = [form](QWidget *field) {
    if (auto *caption = form->labelForField(field)) {
        caption->hide();
    }
    field->hide();
    };

    hideFormRow(server);   
    hideFormRow(m_authUser);  
    hideFormRow(profile);    
    hideFormRow(m_domain);  
    auto *security = new QLabel("Telekomunikacja Kaliska obsługuje obecnie UDP na porcie 5060. Profilu nie można zmienić. SIP/RTP nie są szyfrowane — używaj zaufanej sieci lub VPN."); security->setWordWrap(true); security->setObjectName("muted"); form->addRow(security);
    tabs->addTab(account, "Konto SIP");
    auto *audio = new QWidget; auto *af = new QFormLayout(audio); af->setSpacing(14); af->setContentsMargins(18,22,18,18);
    m_capture = new QComboBox; m_capture->addItem("Domyślny mikrofon", -1);
    m_playback = new QComboBox; m_playback->addItem("Domyślne głośniki", -2);
    for(const QVariant &v : devices) {
        QVariantMap d=v.toMap(); if(d.value("input").toBool()) m_capture->addItem(d.value("name").toString(), d.value("id"));
        if(d.value("output").toBool()) m_playback->addItem(d.value("name").toString(), d.value("id"));
    }
    int cap=m_capture->findData(s.captureDevice), play=m_playback->findData(s.playbackDevice);
    m_capture->setCurrentIndex(cap>=0?cap:0); m_playback->setCurrentIndex(play>=0?play:0);
    af->addRow("Mikrofon",m_capture); af->addRow("Głośniki",m_playback);
    m_stun=new QLineEdit(s.stunServer); m_stun->setPlaceholderText("Opcjonalny host:port serwera STUN"); af->addRow("STUN (NAT)",m_stun);
    m_ice=new QCheckBox("Włącz ICE, jeśli obsługuje je serwer"); m_ice->setChecked(s.ice); af->addRow(m_ice);
    auto *nat=new QLabel("Przy jednostronnym audio sprawdź NAT, zaporę i STUN. Zmiany urządzeń i sieci są stosowane po ponownym połączeniu konta."); nat->setWordWrap(true); nat->setObjectName("muted"); af->addRow(nat);
    if (auto *caption = af->labelForField(m_stun)) {
        caption->hide();
    }
    m_stun->hide();


    m_ice->hide();
    nat->hide();
    m_rejectDnd=new QCheckBox("Przy DND odrzucaj też połączenia lokalnie (486)"); m_rejectDnd->setChecked(s.rejectIncomingOnDnd); af->addRow(m_rejectDnd);
    m_dndSeconds=new QSpinBox; m_dndSeconds->setRange(1,10); m_dndSeconds->setSuffix(" s"); m_dndSeconds->setValue(s.dndAnswerSeconds); af->addRow("DND: czas po odebraniu",m_dndSeconds);
    auto *dnd=new QLabel("DND wykonuje ciche połączenie na 13 przy każdej zmianie. Czeka maks. 15 s na odebranie; po odebraniu rozłącza się po wybranym czasie. Stan serwera nie jest odczytywany."); dnd->setWordWrap(true); dnd->setObjectName("muted"); af->addRow(dnd);
    tabs->addTab(audio,"Audio i DND");
    auto *rec=new QWidget; auto *rf=new QFormLayout(rec); rf->setSpacing(16); rf->setContentsMargins(18,22,18,18);
    m_format=new QComboBox; m_format->addItems({"WAV", "OPUS"}); m_format->setCurrentIndex(s.recordingFormat==RecordingFormat::Wav?0:1); rf->addRow("Domyślny format",m_format);
    m_directory=new QLineEdit(s.recordingDirectory);
    auto *dirrow=new QWidget; auto *dl=new QHBoxLayout(dirrow); dl->setContentsMargins(0,0,0,0); dl->addWidget(m_directory);
    auto *choose=new QPushButton("Wybierz…"); choose->setIcon(glyph(Glyph::Folder)); dl->addWidget(choose);
    connect(choose,&QPushButton::clicked,this,[this]{QString d=QFileDialog::getExistingDirectory(this,"Katalog nagrań",m_directory->text()); if(!d.isEmpty()) m_directory->setText(d);});
    rf->addRow("Katalog nagrań",dirrow);
    m_auto=new QCheckBox("Automatycznie nagrywaj po odebraniu rozmowy"); m_auto->setChecked(s.autoRecord); rf->addRow(m_auto);
    auto *desc=new QLabel("WAV: PCM 16-bit / 48 kHz / mono.\nOPUS: kontener Ogg .opus / 32 kb/s / mono.\n\nNagranie zawiera rozmówcę i wysyłany mikrofon. Wyciszony mikrofon oraz połączenie DND nie są nagrywane. Nagrania są finalizowane przy rozłączeniu."); desc->setWordWrap(true); desc->setObjectName("muted"); rf->addRow(desc);
    auto *consent=new QLabel("Nagrywaj tylko wtedy, gdy masz do tego prawo i spełniasz wymagania dotyczące poinformowania rozmówcy."); consent->setWordWrap(true); consent->setObjectName("notice"); rf->addRow(consent);
    tabs->addTab(rec,"Nagrywanie");
    auto *buttons=new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Cancel); buttons->button(QDialogButtonBox::Save)->setText("Zapisz"); buttons->button(QDialogButtonBox::Save)->setObjectName("primary"); buttons->button(QDialogButtonBox::Cancel)->setText("Anuluj");
    connect(buttons,&QDialogButtonBox::accepted,this,&SettingsDialog::accept); connect(buttons,&QDialogButtonBox::rejected,this,&QDialog::reject); outer->addWidget(buttons);
}
AppSettings SettingsDialog::settings() const {
    AppSettings s=m_original;
    s.username=m_user->text().trimmed(); s.authUsername=m_authUser->text().trimmed(); s.password=m_password->text();
    s.identityDomain=m_domain->text().trimmed(); s.displayName=m_name->text().trimmed();
    s.captureDevice=m_capture->currentData().toInt(); s.playbackDevice=m_playback->currentData().toInt(); s.stunServer=m_stun->text().trimmed(); s.ice=m_ice->isChecked();
    s.rejectIncomingOnDnd=m_rejectDnd->isChecked(); s.dndAnswerSeconds=m_dndSeconds->value();
    s.recordingDirectory=m_directory->text().trimmed(); s.recordingFormat=m_format->currentIndex()==0?RecordingFormat::Wav:RecordingFormat::Opus; s.autoRecord=m_auto->isChecked(); return s;
}
void SettingsDialog::accept() {
    AppSettings s=settings();
    if(!s.username.isEmpty() && !validSipUsername(s.username)) { QMessageBox::warning(this,"Login SIP","Login nie może zawierać spacji ani znaku @. Domenę wpisz w oddzielnym polu."); return; }
    if(s.recordingDirectory.isEmpty()) { QMessageBox::warning(this,"Nagrywanie","Wybierz katalog nagrań."); return; }
    QDialog::accept();
}
