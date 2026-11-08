#include "Icons.h"
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <cmath>
QIcon glyph(Glyph kind, const QColor &color) {
    QPixmap pix(48,48); pix.fill(Qt::transparent);
    QPainter p(&pix); p.setRenderHint(QPainter::Antialiasing); p.scale(2,2);
    p.setPen(QPen(color, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    switch(kind) {
    case Glyph::Phone: case Glyph::Hangup: {
        QPainterPath path;
        path.moveTo(5,3); path.lineTo(8,3); path.lineTo(10,8); path.lineTo(8,10);
        path.cubicTo(9,13,11,15,14,16); path.lineTo(16,14); path.lineTo(21,16);
        path.lineTo(21,19); path.cubicTo(21,23,13,22,7,16); path.cubicTo(1,10,1,3,5,3);
        if(kind == Glyph::Hangup) { p.translate(12,12); p.rotate(135); p.translate(-12,-12); }
        p.drawPath(path); break;
    }
    case Glyph::Mic:
        p.drawRoundedRect(QRectF(9,3,6,11),3,3); p.drawArc(QRectF(6,7,12,10),180*16,180*16);
        p.drawLine(12,17,12,21); p.drawLine(8,21,16,21); break;
    case Glyph::Record: p.setBrush(color); p.setPen(Qt::NoPen); p.drawEllipse(QPointF(12,12),6,6); break;
    case Glyph::Folder: {
        QPainterPath path; path.moveTo(3,7); path.lineTo(3,19); path.lineTo(21,19); path.lineTo(21,7);
        path.lineTo(12,7); path.lineTo(10,4); path.lineTo(3,4); path.closeSubpath(); p.drawPath(path); break;
    }
    case Glyph::Moon: {
        QPainterPath path; path.moveTo(17,3); path.cubicTo(2,1,1,20,14,21); path.cubicTo(18,21,21,19,22,15);
        path.cubicTo(13,18,10,8,17,3); p.drawPath(path); break;
    }
    case Glyph::Echo:
        p.drawLine(4,9,4,15); p.drawLine(8,5,8,19); p.drawLine(12,2,12,22); p.drawLine(16,5,16,19); p.drawLine(20,9,20,15); break;
    case Glyph::Test:
        p.drawRoundedRect(QRectF(3,3,18,18),5,5); p.drawPolyline(QPolygonF({QPointF(6,12),QPointF(10,16),QPointF(18,8)})); break;
    case Glyph::Back:
        p.drawPolyline(QPolygonF({QPointF(9,5),QPointF(3,12),QPointF(9,19),QPointF(21,19),QPointF(21,5),QPointF(9,5)}));
        p.drawLine(12,9,17,15); p.drawLine(17,9,12,15); break;
    case Glyph::Server:
        p.drawRoundedRect(QRectF(3,4,18,7),2,2); p.drawRoundedRect(QRectF(3,14,18,7),2,2);
        p.drawPoint(7,7); p.drawPoint(7,17); p.drawLine(12,7,17,7); p.drawLine(12,17,17,17); break;
    case Glyph::History:
        p.drawEllipse(QPointF(12,12),8,8); p.drawLine(12,7,12,12); p.drawLine(12,12,16,15); break;
    case Glyph::Headphones:
        p.drawArc(QRectF(4,3,16,17),0,180*16); p.drawRoundedRect(QRectF(3,11,4,9),2,2); p.drawRoundedRect(QRectF(17,11,4,9),2,2); break;
    case Glyph::Settings:
        p.drawEllipse(QPointF(12,12),7,7); p.drawEllipse(QPointF(12,12),2.5,2.5);
        for(int i=0; i<8; ++i) { p.save(); p.translate(12,12); p.rotate(i*45); p.drawLine(0,7,0,10); p.restore(); } break;
    }
    return QIcon(pix);
}
QIcon appIcon() {
    return QIcon(QStringLiteral(":/tk-app.png"));
}
