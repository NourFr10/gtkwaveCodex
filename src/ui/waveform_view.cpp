#include "waveform_view.h"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <QtGlobal>

namespace
{
constexpr qreal kTimeAxisHeight = 36.0;
constexpr qreal kSignalRowHeight = 28.0;
constexpr qreal kSignalGap = 4.0;
constexpr qreal kNameColumnWidth = 260.0;

QString normalizeLogicValue(const QString &value)
{
    if (value.isEmpty())
    {
        return QStringLiteral("0");
    }
    const QChar c = value.at(0).toLower();
    if (c == QLatin1Char('x'))
    {
        return QStringLiteral("X");
    }
    if (c == QLatin1Char('z'))
    {
        return QStringLiteral("Z");
    }
    if (c == QLatin1Char('1'))
    {
        return QStringLiteral("1");
    }
    return QStringLiteral("0");
}
}

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(240);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAutoFillBackground(false);
}

void WaveformView::addSignal(const fst::Signal &signal)
{
    for (const auto &existing : m_signals)
    {
        if (existing.signal.handle == signal.handle)
        {
            return;
        }
    }

    RenderSignal renderSignal;
    renderSignal.signal = signal;
    renderSignal.height = static_cast<int>(kSignalRowHeight);
    m_signals.append(renderSignal);
    update();
}

void WaveformView::removeSignal(int handle)
{
    for (int i = 0; i < m_signals.size(); ++i)
    {
        if (m_signals[i].signal.handle == handle)
        {
            m_signals.removeAt(i);
            update();
            break;
        }
    }
}

void WaveformView::clearSignals()
{
    m_signals.clear();
    m_primaryCursor = -1;
    m_referenceCursor = -1;
    update();
}

void WaveformView::setTimeRange(qint64 start, qint64 end)
{
    if (start >= end)
    {
        return;
    }
    m_timeStart = start;
    m_timeEnd = end;
    update();
}

void WaveformView::zoomIn()
{
    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal center = m_timeStart + span / 2.0;
    const qreal newSpan = span * 0.6;
    m_timeStart = static_cast<qint64>(center - newSpan / 2.0);
    m_timeEnd = static_cast<qint64>(center + newSpan / 2.0);
    if (m_timeStart < 0)
    {
        const qint64 offset = -m_timeStart;
        m_timeStart += offset;
        m_timeEnd += offset;
    }
    update();
}

void WaveformView::zoomOut()
{
    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal center = m_timeStart + span / 2.0;
    const qreal newSpan = span * 1.6;
    m_timeStart = static_cast<qint64>(center - newSpan / 2.0);
    m_timeEnd = static_cast<qint64>(center + newSpan / 2.0);
    if (m_timeStart < 0)
    {
        const qint64 offset = -m_timeStart;
        m_timeStart += offset;
        m_timeEnd += offset;
    }
    update();
}

void WaveformView::resetView()
{
    m_timeStart = 0;
    m_timeEnd = 100;
    update();
}

void WaveformView::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, false);

    painter.fillRect(rect(), m_backgroundColor);

    const QRectF axisRect = timeAxisRect();
    drawTimeAxis(painter, axisRect);

    const QRectF waveRect = waveformRect();
    drawGrid(painter, waveRect);

    const qreal baseTop = axisRect.bottom();
    for (int i = 0; i < m_signals.size(); ++i)
    {
        QRectF rowRect = signalRect(i);
        rowRect.translate(0, baseTop);
        drawSignal(painter, m_signals.at(i), rowRect, i % 2 == 1);
    }

    drawCursors(painter, waveRect);
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    const QRectF waveRect = waveformRect();
    if (waveRect.width() <= 0)
    {
        return;
    }

    const qreal delta = event->angleDelta().y();
    const qreal factor = std::pow(1.0015, delta);
    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart) * factor;
    const qreal mouseRatio = qBound<qreal>(0.0, (event->position().x() - waveRect.left()) / waveRect.width(), 1.0);
    const qreal center = m_timeStart + (m_timeEnd - m_timeStart) * mouseRatio;

    m_timeStart = static_cast<qint64>(center - span * mouseRatio);
    m_timeEnd = static_cast<qint64>(center + span * (1.0 - mouseRatio));

    if (m_timeStart < 0)
    {
        const qint64 offset = -m_timeStart;
        m_timeStart += offset;
        m_timeEnd += offset;
    }

    update();
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    const QRectF waveRect = waveformRect();
    if (event->button() == Qt::LeftButton && waveRect.contains(event->pos()))
    {
        updateCursorFromPosition(event->pos());
        m_dragging = true;
        m_lastMousePos = event->pos();
    }
    else if (event->button() == Qt::RightButton && waveRect.contains(event->pos()))
    {
        const qreal ppt = pixelsPerTime(waveRect);
        if (ppt <= 0)
        {
            return;
        }
        const qreal x = qBound<qreal>(waveRect.left(), event->pos().x(), waveRect.right());
        const qint64 time = static_cast<qint64>(m_timeStart + (x - waveRect.left()) / ppt);
        m_referenceCursor = time;
        const qint64 delta = (m_primaryCursor >= 0 && m_referenceCursor >= 0) ? m_primaryCursor - m_referenceCursor : 0;
        emit cursorMoved(m_primaryCursor, delta);
        update();
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
    {
        const QRectF waveRect = waveformRect();
        const qreal ppt = pixelsPerTime(waveRect);
        if (ppt <= 0)
        {
            return;
        }

        const qreal dx = event->pos().x() - m_lastMousePos.x();
        const qreal deltaTime = -dx / ppt;
        m_timeStart += static_cast<qint64>(deltaTime);
        m_timeEnd += static_cast<qint64>(deltaTime);
        if (m_timeStart < 0)
        {
            const qint64 offset = -m_timeStart;
            m_timeStart += offset;
            m_timeEnd += offset;
        }

        m_lastMousePos = event->pos();
        updateCursorFromPosition(event->pos());
        update();
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_dragging = false;
    }
    QWidget::mouseReleaseEvent(event);
}

QRectF WaveformView::signalRect(int index) const
{
    const qreal top = index * (kSignalRowHeight + kSignalGap);
    return QRectF(0.0, top, width(), kSignalRowHeight);
}

QRectF WaveformView::timeAxisRect() const
{
    return QRectF(0.0, 0.0, width(), kTimeAxisHeight);
}

QRectF WaveformView::waveformRect() const
{
    return QRectF(kNameColumnWidth, kTimeAxisHeight, width() - kNameColumnWidth, height() - kTimeAxisHeight);
}

void WaveformView::updateCursorFromPosition(const QPoint &pos)
{
    const QRectF waveRect = waveformRect();
    if (!waveRect.contains(pos))
    {
        return;
    }

    const qreal ppt = pixelsPerTime(waveRect);
    if (ppt <= 0)
    {
        return;
    }

    const qreal x = qBound<qreal>(waveRect.left(), pos.x(), waveRect.right());
    const qint64 time = static_cast<qint64>(m_timeStart + (x - waveRect.left()) / ppt);
    m_primaryCursor = time;
    const qint64 delta = (m_primaryCursor >= 0 && m_referenceCursor >= 0) ? m_primaryCursor - m_referenceCursor : 0;
    emit cursorMoved(m_primaryCursor, delta);
    update();
}

void WaveformView::drawGrid(QPainter &painter, const QRectF &rect)
{
    painter.save();

    const QColor bg = m_backgroundColor.darker(110);
    painter.fillRect(rect, bg);
    painter.setPen(QPen(m_gridColor, 1, Qt::SolidLine));

    const qreal ppt = pixelsPerTime(rect);
    if (ppt <= 0)
    {
        painter.restore();
        return;
    }

    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal targetPixels = 120.0;
    const qreal roughStep = targetPixels / ppt;
    const qreal magnitude = std::pow(10.0, std::floor(std::log10(roughStep)));
    const qreal normalized = roughStep / magnitude;
    qreal step = magnitude;
    if (normalized >= 5.0)
    {
        step = 5.0 * magnitude;
    }
    else if (normalized >= 2.0)
    {
        step = 2.0 * magnitude;
    }

    const qreal start = std::floor(m_timeStart / step) * step;
    for (qreal t = start; t <= m_timeEnd; t += step)
    {
        const qreal x = rect.left() + (t - m_timeStart) * ppt;
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    painter.setPen(QPen(m_gridColor.darker(130), 1));
    const qreal rowHeight = kSignalRowHeight + kSignalGap;
    const int rowCount = std::ceil(rect.height() / rowHeight);
    for (int i = 0; i <= rowCount; ++i)
    {
        const qreal y = rect.top() + i * rowHeight;
        painter.drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    painter.restore();
}

void WaveformView::drawSignal(QPainter &painter, const RenderSignal &sig, const QRectF &rect, bool alternateRow)
{
    painter.save();
    drawSignalBackground(painter, rect, sig, alternateRow);

    QRectF waveRect = rect;
    waveRect.setLeft(kNameColumnWidth);
    waveRect.setWidth(rect.width() - kNameColumnWidth);
    drawSignalWave(painter, sig, waveRect.adjusted(0, 2, 0, -2));
    painter.restore();
}

void WaveformView::drawSignalBackground(QPainter &painter, const QRectF &rect, const RenderSignal &sig, bool alternateRow) const
{
    QRectF nameRect(0.0, rect.top(), kNameColumnWidth, rect.height());
    QColor nameBg = alternateRow ? m_nameBackground.darker(110) : m_nameBackground;
    painter.fillRect(nameRect, nameBg);

    painter.setPen(m_nameBorderColor);
    painter.drawLine(QPointF(nameRect.right(), nameRect.top()), QPointF(nameRect.right(), nameRect.bottom()));

    painter.setPen(QColor(220, 220, 220));
    painter.drawText(nameRect.adjusted(8, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft, sig.signal.path);

    QRectF waveRect(kNameColumnWidth, rect.top(), rect.width() - kNameColumnWidth, rect.height());
    QColor waveBg = alternateRow ? m_backgroundColor.darker(120) : m_backgroundColor.darker(105);
    painter.fillRect(waveRect, waveBg);
}

void WaveformView::drawSignalWave(QPainter &painter, const RenderSignal &sig, const QRectF &rect)
{
    painter.save();
    const qreal ppt = pixelsPerTime(rect);
    if (ppt <= 0)
    {
        painter.restore();
        return;
    }

    const qreal leftX = rect.left();
    const qreal rightX = rect.right();

    if (sig.signal.bitWidth > 1)
    {
        qreal currentX = leftX;
        QString currentValue = sig.signal.values.isEmpty() ? QStringLiteral("0") : sig.signal.values.first().value;
        for (const auto &value : sig.signal.values)
        {
            const qreal x = leftX + (value.time - m_timeStart) * ppt;
            if (x <= leftX)
            {
                currentValue = value.value;
                continue;
            }
            if (currentX < rightX)
            {
                QRectF busRect(currentX, rect.top(), qMin<qreal>(rightX - currentX, qMax<qreal>(4.0, x - currentX)), rect.height());
                painter.fillRect(busRect, m_busFill);
                painter.setPen(QPen(m_gridColor.lighter(160), 1));
                painter.drawRect(busRect);
                painter.setPen(Qt::white);
                painter.drawText(busRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, currentValue);
                currentX = x;
                currentValue = value.value;
            }
        }
        if (currentX < rightX)
        {
            QRectF busRect(currentX, rect.top(), rightX - currentX, rect.height());
            painter.fillRect(busRect, m_busFill);
            painter.setPen(QPen(m_gridColor.lighter(160), 1));
            painter.drawRect(busRect);
            painter.setPen(Qt::white);
            painter.drawText(busRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, currentValue);
        }
    }
    else
    {
        const qreal highY = rect.top() + 4.0;
        const qreal lowY = rect.bottom() - 4.0;

        QString lastValue = sig.signal.values.isEmpty() ? QStringLiteral("0") : sig.signal.values.first().value;
        qreal lastX = leftX;
        qreal lastY = normalizeLogicValue(lastValue) == QLatin1String("0") ? lowY : highY;
        if (sig.signal.values.isEmpty())
        {
            painter.setPen(QPen(m_digitalLow.lighter(), 2));
            painter.drawLine(QPointF(leftX, lowY), QPointF(rightX, lowY));
        }
        else
        {
            for (const auto &value : sig.signal.values)
            {
                qreal x = leftX + (value.time - m_timeStart) * ppt;
                if (x <= leftX)
                {
                    lastValue = value.value;
                    lastY = normalizeLogicValue(lastValue) == QLatin1String("0") ? lowY : highY;
                    lastX = leftX;
                    continue;
                }
                if (x > rightX)
                {
                    x = rightX;
                }

                const QColor segmentColor = normalizeLogicValue(lastValue) == QLatin1String("0") ? m_digitalLow : m_digitalHigh;
                painter.setPen(QPen(segmentColor, 2));
                painter.drawLine(QPointF(lastX, lastY), QPointF(x, lastY));

                const QString normalized = normalizeLogicValue(value.value);
                const qreal newY = normalized == QLatin1String("0") ? lowY : highY;
                painter.drawLine(QPointF(x, lastY), QPointF(x, newY));

                lastValue = value.value;
                lastY = newY;
                lastX = x;

                if (x >= rightX)
                {
                    break;
                }
            }

            if (lastX < rightX)
            {
                const QColor segmentColor = normalizeLogicValue(lastValue) == QLatin1String("0") ? m_digitalLow : m_digitalHigh;
                painter.setPen(QPen(segmentColor, 2));
                painter.drawLine(QPointF(lastX, lastY), QPointF(rightX, lastY));
            }
        }
    }

    painter.restore();
}

void WaveformView::drawTimeAxis(QPainter &painter, const QRectF &rect)
{
    painter.save();

    QLinearGradient gradient(rect.topLeft(), rect.bottomLeft());
    gradient.setColorAt(0.0, m_axisBackground.lighter(120));
    gradient.setColorAt(1.0, m_axisBackground);
    painter.fillRect(rect, gradient);
    painter.setPen(m_gridColor);
    painter.drawRect(rect.adjusted(0, 0, -1, -1));

    const qreal ppt = pixelsPerTime(rect);
    if (ppt <= 0)
    {
        painter.restore();
        return;
    }

    const qreal targetPixels = 120.0;
    const qreal roughStep = targetPixels / ppt;
    const qreal magnitude = std::pow(10.0, std::floor(std::log10(roughStep)));
    const qreal normalized = roughStep / magnitude;
    qreal step = magnitude;
    if (normalized >= 5.0)
    {
        step = 5.0 * magnitude;
    }
    else if (normalized >= 2.0)
    {
        step = 2.0 * magnitude;
    }

    const qreal start = std::floor(m_timeStart / step) * step;
    painter.setPen(QPen(Qt::white, 1));
    QFontMetrics metrics(font());
    for (qreal t = start; t <= m_timeEnd + step; t += step)
    {
        const qreal x = rect.left() + (t - m_timeStart) * ppt;
        painter.drawLine(QPointF(x, rect.bottom()), QPointF(x, rect.bottom() - 8));
        const QString text = formatTime(static_cast<qint64>(t));
        painter.drawText(QPointF(x + 4, rect.center().y() + metrics.ascent() / 2 - 2), text);
    }

    painter.restore();
}

void WaveformView::drawCursors(QPainter &painter, const QRectF &rect)
{
    painter.save();
    const qreal ppt = pixelsPerTime(rect);
    if (ppt <= 0)
    {
        painter.restore();
        return;
    }

    if (m_referenceCursor >= 0)
    {
        const qreal x = rect.left() + (m_referenceCursor - m_timeStart) * ppt;
        painter.setPen(QPen(QColor(0, 188, 212), 1, Qt::DashLine));
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    if (m_primaryCursor >= 0)
    {
        const qreal x = rect.left() + (m_primaryCursor - m_timeStart) * ppt;
        painter.setPen(QPen(QColor(255, 82, 82), 1, Qt::DashLine));
        painter.drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }

    painter.restore();
}

qreal WaveformView::pixelsPerTime(const QRectF &rect) const
{
    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    if (rect.width() <= 0)
    {
        return 0.0;
    }
    return rect.width() / span;
}

QString WaveformView::formatTime(qint64 value) const
{
    if (value == 0)
    {
        return QStringLiteral("0");
    }
    if (std::abs(value) < 1'000)
    {
        return QString::number(value);
    }
    if (std::abs(value) < 1'000'000)
    {
        return QString::number(value / 1'000.0, 'f', 2) + QLatin1String("k");
    }
    return QString::number(value / 1'000'000.0, 'f', 2) + QLatin1String("M");
}

