#pragma once
#include <QPushButton>
#include <QCheckBox>
#include "Icons.h"

// Native Qt controls, painted in the Telekomunikacja Kaliska visual language.
// They retain QAbstractButton keyboard handling, focus and accessibility.
class DialKey final : public QPushButton {
public:
    DialKey(const QString &number, const QString &letters, QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_number, m_letters;
};
class ServiceButton final : public QPushButton {
public:
    ServiceButton(const QString &title,const QString &description,const QString &number,Glyph icon,QWidget *parent=nullptr);
protected:
    void paintEvent(QPaintEvent *) override;
private:
    QString m_title, m_description, m_number;
    Glyph m_icon;
};
class ToggleSwitch final : public QCheckBox {
public:
    explicit ToggleSwitch(QWidget *parent=nullptr);
    QSize sizeHint() const override { return QSize(48,28); }
protected:
    void paintEvent(QPaintEvent *) override;
    bool hitButton(const QPoint &point) const override { return rect().contains(point); }
};
