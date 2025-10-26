#pragma once

#include <QMap>
#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

namespace fst
{
struct SignalValue
{
    qint64 time = 0;
    QString value;
};

struct Signal
{
    int handle = -1;
    QString name;
    QString path;
    QString type;
    QString direction;
    int bitWidth = 1;
    bool isEnum = false;
    QVector<SignalValue> values;
};

struct Scope
{
    QString name;
    QString path;
    QString type;
    QVector<Scope> children;
    QVector<int> signalHandles;
};

class SimpleFstReader
{
public:
    SimpleFstReader();
    ~SimpleFstReader();

    bool load(const QString &filePath);

    const Scope &rootScope() const;
    const QMap<int, Signal> &signalMap() const;

    QString lastError() const;
    qint64 maxTime() const;

private:
    void clear();

    Scope m_rootScope;
    QMap<int, Signal> m_signals;
    QString m_lastError;
    qint64 m_timeEnd = 0;
};

} // namespace fst
