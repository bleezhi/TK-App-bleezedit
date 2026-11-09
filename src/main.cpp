#include "MainWindow.h"
#include <QApplication>
#include <QAbstractButton>
#include <QPushButton>
#include <QCommandLineParser>
#include <QFile>
#include <QFont>
#include <QStyleFactory>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <cstdio>
int main(int argc,char **argv) {
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc,argv);
    app.setApplicationName("TK App"); app.setApplicationVersion(TK_APP_VERSION); app.setOrganizationName("TKApp");
    app.setStyle(QStyleFactory::create("Fusion"));
#ifdef Q_OS_WIN
    app.setFont(QFont("Segoe UI",10));
#else
    app.setFont(QFont("DejaVu Sans",10));
#endif
    QFile style(":/style.qss"); if(style.open(QIODevice::ReadOnly)) app.setStyleSheet(QString::fromUtf8(style.readAll()));
    QCommandLineParser parser; parser.setApplicationDescription("TK App — Telekomunikacja Kaliska (SIP UDP/5060)"); parser.addHelpOption(); parser.addVersionOption();
    QCommandLineOption screenshot(QStringList{"screenshot"},"Save an offline UI screenshot and exit (no SIP registration).","path");
    QCommandLineOption noAudio(QStringList{"no-audio"},"Use the null sound device for local smoke tests.");
    parser.addOption(screenshot); parser.addOption(noAudio); parser.process(app);
    const bool headless = parser.isSet(screenshot) || parser.isSet(noAudio);
    if(!headless) {
        QSettings settings;
        if(!settings.value("forkNotice/accepted", false).toBool()) {
            QMessageBox notice;
            notice.setIcon(QMessageBox::Information);
            notice.setWindowTitle("Informacja o pochodzeniu aplikacji");
            notice.setText("To jest niezależny fork projektu TK App.");
            notice.setInformativeText("Ta wersja jest rozwijana i dystrybuowana niezależnie w repozytorium bleezhi/TK-App-bleezedit. Nie jest oficjalnym wydaniem oryginalnego projektu ani nie oznacza poparcia ze strony jego pierwotnych autorów.\n\nZaakceptuj, aby kontynuować, lub odrzuć, aby zamknąć aplikację.");
            auto *accept = notice.addButton(QString::fromUtf8("Akceptuję"), QMessageBox::AcceptRole);
            auto *decline = notice.addButton(QString::fromUtf8("Odrzuć"), QMessageBox::RejectRole);
            notice.setDefaultButton(accept);
            notice.setEscapeButton(decline);
            notice.exec();
            if(notice.clickedButton() != accept) return 0;
            settings.setValue("forkNotice/accepted", true);
            settings.sync();
        }
    }
    MainWindow window(headless); window.show();
    if(parser.isSet(screenshot)) {
        QString path=parser.value(screenshot);
        QTimer::singleShot(700,&window,[&window,&app,path]{bool ok=window.grab().save(path); if(!ok) std::fprintf(stderr,"Screenshot failed\n"); app.exit(ok?0:1);});
    }
    return app.exec();
}
