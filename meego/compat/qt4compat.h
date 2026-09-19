// Force-included (-include) into every translation unit of the MeeGo build.
// Qt 4.7 lacks the literal macros the shared code uses everywhere.
#pragma once
#include <QtGlobal>
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QByteArray>
#include <QString>
#ifndef QStringLiteral
#define QStringLiteral(str) QString::fromUtf8(str)
#endif
#ifndef QByteArrayLiteral
#define QByteArrayLiteral(str) QByteArray(str)
#endif
#endif
