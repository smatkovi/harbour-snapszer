// Qt 4 replacement for QJsonDocument/QJsonObject, as far as LanSession uses
// them: one compact JSON object per line, built from a QVariantMap. QtScript's
// JSON object (ECMAScript 5, present in Qt 4.7) does the actual work, so no
// extra library is needed on the device.
#pragma once
#include <QByteArray>
#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptValueList>
#include <QString>
#include <QVariantMap>

class QJsonObject
{
public:
    QJsonObject() {}
    static QJsonObject fromVariantMap(const QVariantMap& map) { QJsonObject o; o.m_map = map; return o; }
    QVariantMap toVariantMap() const { return m_map; }
private:
    QVariantMap m_map;
};

class QJsonDocument
{
public:
    enum JsonFormat { Indented, Compact };

    QJsonDocument() : m_isObject(false) {}
    explicit QJsonDocument(const QJsonObject& object) : m_object(object), m_isObject(true) {}

    bool isObject() const { return m_isObject; }
    QJsonObject object() const { return m_object; }

    QByteArray toJson(JsonFormat = Compact) const
    {
        QScriptEngine& e = engine();
        QScriptValue json = e.globalObject().property(QString::fromLatin1("JSON"));
        QScriptValue text = json.property(QString::fromLatin1("stringify"))
            .call(json, QScriptValueList() << e.toScriptValue(m_object.toVariantMap()));
        if (e.hasUncaughtException()) {
            e.clearExceptions();
            return QByteArray("{}");
        }
        return text.toString().toUtf8();
    }

    static QJsonDocument fromJson(const QByteArray& text)
    {
        QJsonDocument document;
        QScriptEngine& e = engine();
        QScriptValue json = e.globalObject().property(QString::fromLatin1("JSON"));
        QScriptValue value = json.property(QString::fromLatin1("parse"))
            .call(json, QScriptValueList() << QScriptValue(QString::fromUtf8(text.constData(), text.size())));
        if (e.hasUncaughtException()) {
            e.clearExceptions();
            return document;
        }
        if (!value.isObject() || value.isArray() || value.isFunction())
            return document;
        document.m_object = QJsonObject::fromVariantMap(value.toVariant().toMap());
        document.m_isObject = true;
        return document;
    }

private:
    static QScriptEngine& engine()
    {
        static QScriptEngine instance;
        return instance;
    }

    QJsonObject m_object;
    bool m_isObject;
};
