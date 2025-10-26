#include "waveform_view.h"

#include <QColor>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <QtMath>

#include <cmath>

namespace
{
constexpr qreal kTimeAxisHeight = 36.0;
constexpr qreal kSignalRowHeight = 28.0;
constexpr qreal kSignalGap = 6.0;
constexpr qreal kNameColumnWidth = 220.0;
}

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumHeight(200);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
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
    m_referenceCursor = 0;
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
    const qreal center = (m_timeStart + m_timeEnd) / 2.0;
    const qreal span = (m_timeEnd - m_timeStart) * 0.5;
    m_timeStart = static_cast<qint64>(center - span / 2.0);
    m_timeEnd = static_cast<qint64>(center + span / 2.0);
    update();
}

void WaveformView::zoomOut()
{
    const qreal center = (m_timeStart + m_timeEnd) / 2.0;
    const qreal span = (m_timeEnd - m_timeStart) * 2.0;
    m_timeStart = static_cast<qint64>(center - span / 2.0);
    m_timeEnd = static_cast<qint64>(center + span / 2.0);
    if (m_timeStart < 0)
    {
        m_timeStart = 0;
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
    painter.fillRect(rect(), palette().base());

    const QRectF axisRect = timeAxisRect();
    drawTimeAxis(painter, axisRect);

    QRectF gridRect(kNameColumnWidth, axisRect.bottom(), width() - kNameColumnWidth, height() - axisRect.bottom());
    drawGrid(painter, gridRect);

    const qreal firstSignalTop = axisRect.bottom();
    for (int i = 0; i < m_signals.size(); ++i)
    {
        QRectF rowRect = signalRect(i);
        rowRect.translate(0, firstSignalTop);
        drawSignal(painter, m_signals[i], rowRect);
    }

    if (m_primaryCursor >= 0)
    {
        const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
        const qreal pixelsPerTime = (width() - kNameColumnWidth) / span;
        const qreal x = kNameColumnWidth + (m_primaryCursor - m_timeStart) * pixelsPerTime;
        painter.setPen(QPen(Qt::red, 1, Qt::DashLine));
        painter.drawLine(QLineF(x, axisRect.bottom(), x, height()));
    }

    if (m_referenceCursor >= 0)
    {
        const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
        const qreal pixelsPerTime = (width() - kNameColumnWidth) / span;
        const qreal x = kNameColumnWidth + (m_referenceCursor - m_timeStart) * pixelsPerTime;
        painter.setPen(QPen(Qt::blue, 1, Qt::DashLine));
        painter.drawLine(QLineF(x, axisRect.bottom(), x, height()));
    }
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    const qreal numDegrees = event->angleDelta().y() / 8.0;
    const qreal factor = std::pow(1.0015, numDegrees * 8.0);
    const qreal span = (m_timeEnd - m_timeStart) * factor;
    const qreal mouseRatio = (event->position().x() - kNameColumnWidth) / qMax<qreal>(1.0, width() - kNameColumnWidth);
    const qreal center = m_timeStart + (m_timeEnd - m_timeStart) * mouseRatio;

    m_timeStart = static_cast<qint64>(center - span * mouseRatio);
    m_timeEnd = static_cast<qint64>(center + span * (1.0 - mouseRatio));

    if (m_timeStart < 0)
    {
        qint64 offset = -m_timeStart;
        m_timeStart += offset;
        m_timeEnd += offset;
    }

    update();
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        updateCursorFromPosition(event->pos());
        m_dragging = true;
        m_lastMousePos = event->pos();
    }
    else if (event->button() == Qt::RightButton)
    {
        const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
        const qreal pixelsPerTime = (width() - kNameColumnWidth) / span;
        if (pixelsPerTime <= 0)
        {
            return;
        }
        const qreal x = qBound<qreal>(kNameColumnWidth, event->pos().x(), width());
        const qint64 time = static_cast<qint64>(m_timeStart + (x - kNameColumnWidth) / pixelsPerTime);
        m_referenceCursor = time;
        emit cursorMoved(m_primaryCursor, m_primaryCursor >= 0 ? m_primaryCursor - m_referenceCursor : 0);
        update();
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
    {
        const qreal dx = event->pos().x() - m_lastMousePos.x();
        const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
        const qreal pixelsPerTime = (width() - kNameColumnWidth) / span;
        if (pixelsPerTime > 0)
        {
            const qreal deltaTime = -dx / pixelsPerTime;
            m_timeStart += static_cast<qint64>(deltaTime);
            m_timeEnd += static_cast<qint64>(deltaTime);
            if (m_timeStart < 0)
            {
                qint64 offset = -m_timeStart;
                m_timeStart += offset;
                m_timeEnd += offset;
            }
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
    return QRectF(kNameColumnWidth, 0.0, width() - kNameColumnWidth, kTimeAxisHeight);
}

void WaveformView::updateCursorFromPosition(const QPoint &pos)
{
    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal pixelsPerTime = (width() - kNameColumnWidth) / span;
    if (pixelsPerTime <= 0)
    {
        return;
    }

    const qreal x = qBound<qreal>(kNameColumnWidth, pos.x(), width());
    const qint64 time = static_cast<qint64>(m_timeStart + (x - kNameColumnWidth) / pixelsPerTime);
    m_primaryCursor = time;
    emit cursorMoved(m_primaryCursor, m_primaryCursor - m_referenceCursor);
    update();
}

void WaveformView::drawGrid(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.setPen(QPen(palette().mid().color(), 1, Qt::DotLine));

    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal pixelsPerTime = rect.width() / span;
    if (pixelsPerTime <= 0)
    {
        painter.restore();
        return;
    }
    const qreal step = span <= 0 ? 10 : span / 10.0;

    const qreal start = std::floor(m_timeStart / step) * step;
    for (qreal t = start; t <= m_timeEnd; t += step)
    {
        const qreal x = kNameColumnWidth + (t - m_timeStart) * pixelsPerTime;
        painter.drawLine(QLineF(x, rect.top(), x, rect.bottom()));
    }

    painter.restore();
}

void WaveformView::drawSignal(QPainter &painter, const RenderSignal &sig, const QRectF &rect)
{
    painter.save();

    QRectF nameRect(rect.left(), rect.top(), kNameColumnWidth, rect.height());
    QRectF waveRect(kNameColumnWidth, rect.top(), width() - kNameColumnWidth, rect.height());

    painter.fillRect(nameRect, palette().alternateBase());
    painter.setPen(palette().mid().color());
    painter.drawRect(nameRect);
    painter.drawText(nameRect.adjusted(4, 0, -4, 0), Qt::AlignVCenter | Qt::AlignLeft, sig.signal.name);

    painter.setPen(palette().mid().color());
    painter.drawRect(waveRect);
    painter.setBrush(Qt::NoBrush);

    if (waveRect.width() <= 0)
    {
        painter.restore();
        return;
    }

    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal pixelsPerTime = waveRect.width() / span;
    const qreal midY = waveRect.center().y();
    const qreal highY = waveRect.top() + 4;
    const qreal lowY = waveRect.bottom() - 4;

    if (sig.signal.values.isEmpty())
    {
        painter.drawLine(QLineF(waveRect.left(), midY, waveRect.right(), midY));
    }
    else
    {
        qreal lastX = waveRect.left();
        QString lastValue = sig.signal.values.first().value;
        qreal lastY = lastValue == "0" ? lowY : highY;
        painter.setPen(QPen(Qt::darkGreen, 2));

        for (const auto &value : sig.signal.values)
        {
            const qreal x = waveRect.left() + (value.time - m_timeStart) * pixelsPerTime;
            const QString displayValue = value.value;

            if (sig.signal.bitWidth > 1)
            {
                const qreal width = qMax<qreal>(4.0, x - lastX);
                QRectF busRect(lastX, waveRect.top(), width, waveRect.height());
                painter.setBrush(QColor(30, 144, 255, 60));
                painter.drawRect(busRect);
                painter.drawText(busRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, lastValue);
                painter.setBrush(Qt::NoBrush);
            }
            else
            {
                painter.drawLine(QLineF(lastX, lastY, x, lastY));
            }

            painter.drawLine(QLineF(x, lastY, x, displayValue == "0" ? lowY : highY));
            lastY = displayValue == "0" ? lowY : highY;
            lastX = x;
            lastValue = displayValue;
        }

        const qreal endX = waveRect.right();
        if (sig.signal.bitWidth > 1)
        {
            QRectF busRect(lastX, waveRect.top(), endX - lastX, waveRect.height());
            painter.setBrush(QColor(30, 144, 255, 60));
            painter.drawRect(busRect);
            painter.drawText(busRect.adjusted(4, 0, -4, 0), Qt::AlignCenter, lastValue);
            painter.setBrush(Qt::NoBrush);
        }
        else
        {
            painter.drawLine(QLineF(lastX, lastY, endX, lastY));
        }
    }

    painter.restore();
}

void WaveformView::drawTimeAxis(QPainter &painter, const QRectF &rect)
{
    painter.save();
    painter.fillRect(rect, palette().window());
    painter.drawRect(rect);

    const qreal span = qMax<qreal>(1.0, m_timeEnd - m_timeStart);
    const qreal pixelsPerTime = rect.width() / span;
    if (pixelsPerTime <= 0)
    {
        painter.restore();
        return;
    }
    const qreal step = span / 10.0;

    for (int i = 0; i <= 10; ++i)
    {
        const qreal t = m_timeStart + step * i;
        const qreal x = rect.left() + (t - m_timeStart) * pixelsPerTime;
        painter.drawLine(QLineF(x, rect.top(), x, rect.bottom()));
        painter.drawText(QPointF(x + 2, rect.bottom() - 4), QString::number(static_cast<qint64>(t)));
    }

    painter.restore();
}

