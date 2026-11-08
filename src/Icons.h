#pragma once
#include <QIcon>
#include <QColor>
enum class Glyph { Phone, Hangup, Settings, Mic, Record, Folder, Moon, Echo, Test, Back, Server, History, Headphones };
QIcon glyph(Glyph kind, const QColor &color = QColor("#516879"));
QIcon appIcon();
