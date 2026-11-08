#pragma once
#include "AppSettings.h"
#include <QDialog>
#include <QVariantList>
class QLineEdit; class QSpinBox; class QComboBox; class QCheckBox;
class SettingsDialog final : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(const AppSettings &settings, const QVariantList &devices, QWidget *parent=nullptr);
    AppSettings settings() const;
protected:
    void accept() override;
private:
    AppSettings m_original;
    QLineEdit *m_user, *m_authUser, *m_password, *m_domain, *m_name, *m_stun, *m_directory;
    QSpinBox *m_dndSeconds;
    QComboBox *m_capture, *m_playback, *m_format;
    QCheckBox *m_ice, *m_auto, *m_rejectDnd;
};
