#include "simple_fst_reader.h"

#include <QFile>
#include <QTextStream>

#include <algorithm>
#include <stack>

namespace fst
{
namespace
{
QString joinPath(const QString &parent, const QString &name)
{
    if (parent.isEmpty())
    {
        return name;
    }
    return parent + "." + name;
}
} // namespace

SimpleFstReader::SimpleFstReader() = default;
SimpleFstReader::~SimpleFstReader() = default;

bool SimpleFstReader::load(const QString &filePath)
{
    clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        m_lastError = QObject::tr("Unable to open %1").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    struct ScopeFrame
    {
        Scope scope;
    };

    std::stack<ScopeFrame> scopeStack;
    Scope root;
    root.name = "root";
    root.path = QString();
    root.type = "root";
    scopeStack.push({root});

    QMap<QString, int> nameToHandle;
    int nextHandle = 1;

    int lineNumber = 0;
    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        ++lineNumber;
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#'))
        {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            continue;
        }

        const QString keyword = parts.first().toLower();
        if (keyword == "scope")
        {
            if (parts.size() < 3)
            {
                m_lastError = QObject::tr("Invalid scope declaration on line %1").arg(lineNumber);
                return false;
            }

            Scope scope;
            scope.type = parts.at(1);
            scope.name = parts.at(2);
            scope.path = joinPath(scopeStack.top().scope.path, scope.name);

            scopeStack.push({scope});
        }
        else if (keyword == "endscope")
        {
            if (scopeStack.size() <= 1)
            {
                m_lastError = QObject::tr("Unexpected endscope on line %1").arg(lineNumber);
                return false;
            }

            Scope completed = scopeStack.top().scope;
            scopeStack.pop();
            scopeStack.top().scope.children.append(completed);
        }
        else if (keyword == "signal")
        {
            if (parts.size() < 5)
            {
                m_lastError = QObject::tr("Invalid signal declaration on line %1").arg(lineNumber);
                return false;
            }

            Signal signal;
            signal.handle = nextHandle++;
            signal.name = parts.at(1);
            signal.type = parts.at(2);
            signal.direction = parts.at(3);
            signal.bitWidth = parts.at(4).toInt();
            signal.path = joinPath(scopeStack.top().scope.path, signal.name);

            nameToHandle.insert(signal.path, signal.handle);
            if (!nameToHandle.contains(signal.name))
            {
                nameToHandle.insert(signal.name, signal.handle);
            }

            scopeStack.top().scope.signalHandles.append(signal.handle);
            m_signals.insert(signal.handle, signal);
        }
        else if (keyword == "value")
        {
            if (parts.size() < 4)
            {
                m_lastError = QObject::tr("Invalid value change on line %1").arg(lineNumber);
                return false;
            }

            const QString id = parts.at(1);
            bool ok = false;
            qint64 time = parts.at(2).toLongLong(&ok);
            if (!ok)
            {
                m_lastError = QObject::tr("Invalid time in value change on line %1").arg(lineNumber);
                return false;
            }
            const QString value = parts.at(3);

            int handle = nameToHandle.value(joinPath(scopeStack.top().scope.path, id), -1);
            if (handle < 0)
            {
                handle = nameToHandle.value(id, -1);
            }
            if (handle < 0)
            {
                m_lastError = QObject::tr("Unknown signal '%1' on line %2").arg(id).arg(lineNumber);
                return false;
            }

            SignalValue change;
            change.time = time;
            change.value = value;
            m_signals[handle].values.append(change);
            if (time > m_timeEnd)
            {
                m_timeEnd = time;
            }
        }
    }

    while (scopeStack.size() > 1)
    {
        Scope completed = scopeStack.top().scope;
        scopeStack.pop();
        scopeStack.top().scope.children.append(completed);
    }

    m_rootScope = scopeStack.top().scope;

    for (auto it = m_signals.begin(); it != m_signals.end(); ++it)
    {
        auto &values = it.value().values;
        std::sort(values.begin(), values.end(), [](const SignalValue &a, const SignalValue &b) {
            return a.time < b.time;
        });
    }

    return true;
}

const Scope &SimpleFstReader::rootScope() const
{
    return m_rootScope;
}

const QMap<int, Signal> &SimpleFstReader::signals() const
{
    return m_signals;
}

QString SimpleFstReader::lastError() const
{
    return m_lastError;
}

qint64 SimpleFstReader::maxTime() const
{
    return m_timeEnd;
}

void SimpleFstReader::clear()
{
    m_rootScope = Scope();
    m_signals.clear();
    m_lastError.clear();
    m_timeEnd = 0;
}

} // namespace fst
