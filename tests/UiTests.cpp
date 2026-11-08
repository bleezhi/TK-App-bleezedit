#include "MainWindow.h"
#include "SettingsDialog.h"
#include "AppSettings.h"
#include <QtTest>
#include <QtWidgets>
#include <memory>

class UiTests : public QObject {
    Q_OBJECT
    QTemporaryDir m_directory;
    std::unique_ptr<MainWindow> m_window;
    QPushButton *button(const QString &text) {
        for(auto *b:m_window->findChildren<QPushButton *>()) if(b->text()==text)return b;
        return nullptr;
    }
private slots:
    void initTestCase() {
        QVERIFY(m_directory.isValid());
        QCoreApplication::setOrganizationName("TKAppUiTests");QCoreApplication::setApplicationName("TK App UI Tests");
        QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,m_directory.path());
        AppSettings settings;settings.recordingDirectory=m_directory.filePath("recordings");settings.save();
        QFile css(":/style.qss");QVERIFY(css.open(QIODevice::ReadOnly));qApp->setStyleSheet(QString::fromUtf8(css.readAll()));
        m_window=std::make_unique<MainWindow>(true);m_window->show();
        QTRY_VERIFY_WITH_TIMEOUT(button("Połącz konto")&&button("Połącz konto")->isEnabled(),5000);
    }
    void realBrandAssetsAreEmbedded() {
        QVERIFY(!QImage(":/branding/telekomunikacja-kaliska.png").isNull());
        QVERIFY(!QImage(":/tk-app.png").isNull());QVERIFY(!m_window->windowIcon().isNull());
        QVERIFY(m_window->windowTitle().contains("Telekomunikacja Kaliska"));
    }
    void navigationAndRealRecordings() {
        auto *pages=m_window->findChild<QStackedWidget *>("mainPages");QVERIFY(pages);QCOMPARE(pages->currentIndex(),0);
        QVERIFY(button("Historia połączeń"));QTest::mouseClick(button("Historia połączeń"),Qt::LeftButton);QCOMPARE(pages->currentIndex(),1);
        QDir().mkpath(m_directory.filePath("recordings"));QFile fixture(m_directory.filePath("recordings/test.wav"));QVERIFY(fixture.open(QIODevice::WriteOnly));fixture.write("UI test fixture, not a call recording");fixture.close();
        QVERIFY(button("Nagrania"));QTest::mouseClick(button("Nagrania"),Qt::LeftButton);QCOMPARE(pages->currentIndex(),2);
        auto *files=m_window->findChild<QTreeWidget *>();QVERIFY(files);QCOMPARE(files->topLevelItemCount(),1);QCOMPARE(files->topLevelItem(0)->text(0),QString("test.wav"));
        QTest::mouseClick(button("Telefon"),Qt::LeftButton);QCOMPARE(pages->currentIndex(),0);
    }
    void dialPadAndHardwareKeyboard() {
        auto *number=m_window->findChild<QLineEdit *>("dialNumber");QVERIFY(number);
        QTest::mouseClick(button("1"),Qt::LeftButton);QTest::mouseClick(button("2"),Qt::LeftButton);QTest::mouseClick(button("#"),Qt::LeftButton);QCOMPARE(number->text(),QString("12#"));
        number->clear();number->setFocus();QTest::keyClicks(number,"14");QCOMPARE(number->text(),QString("14"));
        // Offline Enter must not connect or place a call.
        QTest::keyClick(number,Qt::Key_Return);QVERIFY(!button("Zadzwoń")->isEnabled());
    }
    void networkProfileIsReadOnly() {
        SettingsDialog dialog(AppSettings::load(),{},m_window.get());
        auto *profile=dialog.findChild<QLineEdit *>("fixedProfile");QVERIFY(profile);QVERIFY(profile->isReadOnly());
        QVERIFY(profile->text().contains("UDP"));QVERIFY(profile->text().contains("5060"));
        for(auto *combo:dialog.findChildren<QComboBox *>()) QVERIFY(combo->findText("TCP")<0);
        for(auto *spin:dialog.findChildren<QSpinBox *>()) QVERIFY(spin->maximum()!=65535);
    }
    void cleanupTestCase() {m_window.reset();}
};
QTEST_MAIN(UiTests)
#include "UiTests.moc"
