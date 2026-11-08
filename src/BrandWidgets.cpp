#include "BrandWidgets.h"
#include <QPainter>
#include <QPaintEvent>

DialKey::DialKey(const QString &number,const QString &letters,QWidget *parent)
    : QPushButton(parent),m_number(number),m_letters(letters) {
    setMinimumSize(80,61); setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    setCursor(Qt::PointingHandCursor); setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Wybierz "+number); setText(number);
}
void DialKey::paintEvent(QPaintEvent *) {
    QPainter p(this); p.setRenderHint(QPainter::Antialiasing);
    QColor background=isDown()?QColor("#e5ebf0"):underMouse()?QColor("#f4f7f9"):QColor("#ffffff");
    p.setPen(QPen(hasFocus()?QColor("#1a638a"):QColor("#dfe5eb"),hasFocus()?1.5:1));p.setBrush(background);
    p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),10,10);
    QFont n=font();n.setPixelSize(27);n.setWeight(QFont::DemiBold);p.setFont(n);p.setPen(QColor("#18374c"));
    if(m_letters.isEmpty()) p.drawText(rect(),Qt::AlignCenter,m_number);
    else {
        p.drawText(QRect(0,5,width(),height()-23),Qt::AlignCenter,m_number);
        n.setPixelSize(9);n.setWeight(QFont::Medium);n.setLetterSpacing(QFont::AbsoluteSpacing,1.6);p.setFont(n);p.setPen(QColor("#7d8b97"));
        p.drawText(QRect(0,height()-23,width(),18),Qt::AlignCenter,m_letters);
    }
}
ServiceButton::ServiceButton(const QString &title,const QString &description,const QString &number,Glyph icon,QWidget *parent)
    : QPushButton(parent),m_title(title),m_description(description),m_number(number),m_icon(icon) {
    setMinimumHeight(64);setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
    setCursor(Qt::PointingHandCursor);setFocusPolicy(Qt::StrongFocus);setAccessibleName(title+" — numer "+number);
}
void ServiceButton::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    const QColor border=hasFocus()?QColor("#1a638a"):QColor("#e0e6eb");
    p.setBrush(isDown()?QColor("#edf1f5"):underMouse()&&isEnabled()?QColor("#f8fafc"):QColor("#ffffff"));p.setPen(QPen(border,1));
    p.drawRoundedRect(QRectF(rect()).adjusted(1,1,-1,-1),10,10);
    if(!isEnabled()) p.setOpacity(.55);
    glyph(m_icon,QColor("#1a638a")).paint(&p,QRect(15,(height()-24)/2,24,24));
    QFont f=font();f.setPixelSize(13);f.setWeight(QFont::DemiBold);p.setFont(f);p.setPen(QColor("#17374d"));
    p.drawText(QRect(52,12,width()-115,20),Qt::AlignLeft|Qt::AlignVCenter,m_title);
    f.setPixelSize(10);f.setWeight(QFont::Normal);p.setFont(f);p.setPen(QColor("#788893"));
    p.drawText(QRect(52,34,width()-115,16),Qt::AlignLeft|Qt::AlignVCenter,m_description);
}
ToggleSwitch::ToggleSwitch(QWidget *parent) : QCheckBox(parent) {
    setFixedSize(48,28);setFocusPolicy(Qt::StrongFocus);setCursor(Qt::PointingHandCursor);
}
void ToggleSwitch::paintEvent(QPaintEvent *) {
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);
    if(!isEnabled()) p.setOpacity(.55);
    p.setPen(hasFocus()?QPen(QColor("#1a638a"),1.5):QPen(Qt::NoPen));
    p.setBrush(isChecked()?QColor("#c62b27"):QColor("#ccd5dd"));p.drawRoundedRect(QRectF(1,2,46,24),12,12);
    p.setPen(Qt::NoPen);p.setBrush(Qt::white);p.drawEllipse(QRectF(isChecked()?25:5,6,16,16));
}
