#include "simple_fst_reader.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QIODevice>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryFile>
#include <QTextStream>

#include <algorithm>

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
    return parent + QLatin1Char('.') + name;
}

QString normalizeScalarValue(const QString &value)
{
    if (value.isEmpty())
    {
        return QStringLiteral("0");
    }

    const QChar c = value.at(0);
    if (c == QLatin1Char('0') || c == QLatin1Char('1'))
    {
        return QString(c);
    }

    if (c == QLatin1Char('x') || c == QLatin1Char('X'))
    {
        return QStringLiteral("X");
    }
    if (c == QLatin1Char('z') || c == QLatin1Char('Z'))
    {
        return QStringLiteral("Z");
    }

    return value;
}
} // namespace

SimpleFstReader::SimpleFstReader() = default;
SimpleFstReader::~SimpleFstReader() = default;

bool SimpleFstReader::load(const QString &filePath)
{
    clear();

    QFileInfo info(filePath);
    if (!info.exists() || !info.isFile())
    {
        m_lastError = QObject::tr("File not found: %1").arg(filePath);
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_lastError = QObject::tr("Unable to open %1").arg(filePath);
        return false;
    }

    const QByteArray header = file.peek(32);

    if (header.startsWith("# Pseudo FST"))
    {
        const bool ok = loadFromPseudoText(file);
        file.close();
        return ok;
    }

    const QString suffix = info.suffix().toLower();

    if (suffix == QLatin1String("vcd"))
    {
        const bool ok = loadFromVcd(file);
        file.close();
        return ok;
    }

    if (suffix == QLatin1String("fst"))
    {
        file.close();
        if (loadFromFstBinary(filePath))
        {
            return true;
        }

        if (file.open(QIODevice::ReadOnly))
        {
            const bool fallbackOk = loadFromVcd(file);
            file.close();
            if (fallbackOk)
            {
                return true;
            }
        }

        if (m_lastError.isEmpty())
        {
            m_lastError = QObject::tr("Unsupported or corrupt FST file: %1").arg(filePath);
        }
        return false;
    }

    const bool ok = loadFromVcd(file);
    file.close();
    if (!ok && m_lastError.isEmpty())
    {
        m_lastError = QObject::tr("Unsupported file format: %1").arg(filePath);
    }
    return ok;
}

const Scope &SimpleFstReader::rootScope() const
{
    return m_rootScope;
}

const QMap<int, Signal> &SimpleFstReader::signalMap() const
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

bool SimpleFstReader::loadFromPseudoText(QIODevice &device)
{
    QTextStream stream(&device);
    stream.setCodec("UTF-8");

    QVector<Scope> scopeStack;
    Scope root;
    root.name = QStringLiteral("root");
    root.type = QStringLiteral("root");
    root.path = QString();
    scopeStack.append(root);

    QMap<QString, int> nameToHandle;
    int nextHandle = 1;
    int lineNumber = 0;

    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        ++lineNumber;
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
        {
            continue;
        }

        const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty())
        {
            continue;
        }

        const QString keyword = parts.first().toLower();
        if (keyword == QLatin1String("scope"))
        {
            if (parts.size() < 3)
            {
                m_lastError = QObject::tr("Invalid scope declaration on line %1").arg(lineNumber);
                return false;
            }

            Scope scope;
            scope.type = parts.at(1);
            scope.name = parts.at(2);
            scope.path = joinPath(scopeStack.last().path, scope.name);
            scopeStack.append(scope);
        }
        else if (keyword == QLatin1String("endscope"))
        {
            if (scopeStack.size() <= 1)
            {
                m_lastError = QObject::tr("Unexpected endscope on line %1").arg(lineNumber);
                return false;
            }

            Scope completed = scopeStack.takeLast();
            scopeStack.last().children.append(completed);
        }
        else if (keyword == QLatin1String("signal"))
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
            signal.path = joinPath(scopeStack.last().path, signal.name);

            nameToHandle.insert(signal.path, signal.handle);
            if (!nameToHandle.contains(signal.name))
            {
                nameToHandle.insert(signal.name, signal.handle);
            }

            scopeStack.last().signalHandles.append(signal.handle);
            m_signals.insert(signal.handle, signal);
        }
        else if (keyword == QLatin1String("value"))
        {
            if (parts.size() < 4)
            {
                m_lastError = QObject::tr("Invalid value change on line %1").arg(lineNumber);
                return false;
            }

            const QString id = parts.at(1);
            bool ok = false;
            const qint64 time = parts.at(2).toLongLong(&ok);
            if (!ok)
            {
                m_lastError = QObject::tr("Invalid time in value change on line %1").arg(lineNumber);
                return false;
            }
            const QString value = parts.at(3);

            int handle = nameToHandle.value(joinPath(scopeStack.last().path, id), -1);
            if (handle < 0)
            {
                handle = nameToHandle.value(id, -1);
            }
            if (handle < 0)
            {
                m_lastError = QObject::tr("Unknown signal '%1' on line %2").arg(id).arg(lineNumber);
                return false;
            }

            appendSignalValue(handle, time, value);
        }
    }

    if (!finalizeHierarchy(scopeStack))
    {
        m_lastError = QObject::tr("Malformed scope hierarchy");
        return false;
    }

    for (auto it = m_signals.begin(); it != m_signals.end(); ++it)
    {
        auto &values = it.value().values;
        std::sort(values.begin(), values.end(), [](const SignalValue &a, const SignalValue &b) {
            return a.time < b.time;
        });
    }

    return true;
}

bool SimpleFstReader::loadFromVcd(QIODevice &device)
{
    QTextStream stream(&device);
    stream.setCodec("UTF-8");

    QVector<Scope> scopeStack;
    Scope root;
    root.name = QStringLiteral("root");
    root.type = QStringLiteral("root");
    root.path = QString();
    scopeStack.append(root);

    QHash<QString, int> symbolToHandle;
    int nextHandle = 1;
    bool inDefinitions = true;
    bool inDumpvars = false;
    qint64 currentTime = 0;

    auto ensureScopeFinalized = [this, &scopeStack]() {
        if (!finalizeHierarchy(scopeStack))
        {
            m_lastError = QObject::tr("Malformed scope hierarchy in VCD");
            return false;
        }
        return true;
    };

    while (!stream.atEnd())
    {
        QString line = stream.readLine();
        if (line.isNull())
        {
            break;
        }
        line = line.trimmed();
        if (line.isEmpty())
        {
            continue;
        }

        if (inDefinitions)
        {
            if (line.startsWith(QLatin1String("$scope")))
            {
                QString definition = line;
                while (!definition.contains(QLatin1String("$end")) && !stream.atEnd())
                {
                    definition += QLatin1Char(' ') + stream.readLine().trimmed();
                }
                const QStringList parts = definition.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() < 4)
                {
                    m_lastError = QObject::tr("Invalid scope definition in VCD");
                    return false;
                }

                Scope scope;
                scope.type = parts.at(1);
                scope.name = parts.at(2);
                scope.path = joinPath(scopeStack.last().path, scope.name);
                scopeStack.append(scope);
                continue;
            }

            if (line.startsWith(QLatin1String("$upscope")))
            {
                if (scopeStack.size() <= 1)
                {
                    m_lastError = QObject::tr("Unexpected $upscope in VCD");
                    return false;
                }
                Scope completed = scopeStack.takeLast();
                scopeStack.last().children.append(completed);
                continue;
            }

            if (line.startsWith(QLatin1String("$var")))
            {
                QString definition = line;
                while (!definition.contains(QLatin1String("$end")) && !stream.atEnd())
                {
                    definition += QLatin1Char(' ') + stream.readLine().trimmed();
                }

                const QStringList parts = definition.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
                if (parts.size() < 6)
                {
                    m_lastError = QObject::tr("Invalid $var definition in VCD");
                    return false;
                }

                const QString type = parts.at(1);
                bool widthOk = false;
                const int width = parts.at(2).toInt(&widthOk);
                if (!widthOk)
                {
                    m_lastError = QObject::tr("Invalid bit width in VCD");
                    return false;
                }
                const QString identifier = parts.at(3);
                QString reference;
                for (int i = 4; i < parts.size() - 1; ++i)
                {
                    if (!reference.isEmpty())
                    {
                        reference += QLatin1Char(' ');
                    }
                    reference += parts.at(i);
                }

                Signal signal;
                signal.handle = nextHandle++;
                signal.name = reference;
                signal.type = type;
                signal.direction = QStringLiteral("-");
                signal.bitWidth = width;
                signal.path = joinPath(scopeStack.last().path, signal.name);

                scopeStack.last().signalHandles.append(signal.handle);
                m_signals.insert(signal.handle, signal);
                symbolToHandle.insert(identifier, signal.handle);
                continue;
            }

            if (line.startsWith(QLatin1String("$enddefinitions")))
            {
                inDefinitions = false;
                continue;
            }

            // ignore other definition commands like $timescale
            continue;
        }

        if (line.startsWith(QLatin1Char('#')))
        {
            bool ok = false;
            const qint64 time = line.mid(1).toLongLong(&ok);
            if (!ok)
            {
                m_lastError = QObject::tr("Invalid timestamp in VCD");
                return false;
            }
            currentTime = time;
            if (time > m_timeEnd)
            {
                m_timeEnd = time;
            }
            continue;
        }

        if (line.startsWith(QLatin1String("$dumpvars")))
        {
            inDumpvars = true;
            continue;
        }
        if (line.startsWith(QLatin1String("$end")))
        {
            inDumpvars = false;
            continue;
        }
        if (line.startsWith(QLatin1Char('$')))
        {
            // Ignore other runtime commands
            continue;
        }

        QString value;
        QString symbol;
        if (line.startsWith(QLatin1Char('b')) || line.startsWith(QLatin1Char('B')) || line.startsWith(QLatin1Char('r')) || line.startsWith(QLatin1Char('R')))
        {
            const int spaceIdx = line.indexOf(QLatin1Char(' '));
            if (spaceIdx <= 1)
            {
                m_lastError = QObject::tr("Malformed vector change in VCD");
                return false;
            }
            value = line.mid(1, spaceIdx - 1);
            symbol = line.mid(spaceIdx + 1);
        }
        else
        {
            if (line.size() < 2)
            {
                continue;
            }
            value = normalizeScalarValue(line.left(1));
            symbol = line.mid(1);
        }

        if (symbol.isEmpty())
        {
            continue;
        }

        const int handle = symbolToHandle.value(symbol, -1);
        if (handle < 0)
        {
            if (!inDumpvars)
            {
                m_lastError = QObject::tr("Unknown symbol '%1' in VCD").arg(symbol);
                return false;
            }
            continue;
        }

        appendSignalValue(handle, currentTime, value);
    }

    if (!ensureScopeFinalized())
    {
        return false;
    }

    for (auto it = m_signals.begin(); it != m_signals.end(); ++it)
    {
        auto &values = it.value().values;
        std::sort(values.begin(), values.end(), [](const SignalValue &a, const SignalValue &b) {
            return a.time < b.time;
        });
    }

    return true;
}

bool SimpleFstReader::loadFromFstBinary(const QString &filePath)
{
    QTemporaryFile tempFile(QDir::tempPath() + QLatin1String("/gtkwave_viewer_XXXXXX.vcd"));
    tempFile.setAutoRemove(false);
    if (!tempFile.open())
    {
        m_lastError = QObject::tr("Failed to create temporary file for FST conversion");
        return false;
    }
    const QString vcdPath = tempFile.fileName();
    tempFile.close();

    QProcess process;
    QString program = QStringLiteral("fst2vcd");
    process.start(program, {filePath, vcdPath});
    if (!process.waitForStarted())
    {
        QFile::remove(vcdPath);
        m_lastError = QObject::tr("Unable to start fst2vcd. Ensure GTKWave tools are installed and available in PATH.");
        return false;
    }
    process.waitForFinished(-1);

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
    {
        QFile::remove(vcdPath);
        m_lastError = QObject::tr("fst2vcd failed: %1").arg(QString::fromLocal8Bit(process.readAllStandardError()).trimmed());
        if (m_lastError.trimmed().isEmpty())
        {
            m_lastError = QObject::tr("fst2vcd failed to convert the file");
        }
        return false;
    }

    QFile vcdFile(vcdPath);
    if (!vcdFile.open(QIODevice::ReadOnly))
    {
        QFile::remove(vcdPath);
        m_lastError = QObject::tr("Unable to read converted VCD data");
        return false;
    }

    const bool ok = loadFromVcd(vcdFile);
    vcdFile.close();
    QFile::remove(vcdPath);
    return ok;
}

bool SimpleFstReader::finalizeHierarchy(QVector<Scope> &scopeStack)
{
    while (scopeStack.size() > 1)
    {
        Scope completed = scopeStack.takeLast();
        scopeStack.last().children.append(completed);
    }

    if (scopeStack.isEmpty())
    {
        return false;
    }

    m_rootScope = scopeStack.takeLast();
    return true;
}

void SimpleFstReader::appendSignalValue(int handle, qint64 time, const QString &value)
{
    auto it = m_signals.find(handle);
    if (it == m_signals.end())
    {
        return;
    }

    QVector<SignalValue> &values = it->values;
    if (!values.isEmpty())
    {
        SignalValue &last = values.last();
        if (last.time == time)
        {
            last.value = value;
        }
        else if (last.value == value)
        {
            return;
        }
        else
        {
            values.append({time, value});
        }
    }
    else
    {
        values.append({time, value});
    }

    if (time > m_timeEnd)
    {
        m_timeEnd = time;
    }
}

void SimpleFstReader::clear()
{
    m_rootScope = Scope();
    m_signals.clear();
    m_lastError.clear();
    m_timeEnd = 0;
}

} // namespace fst
