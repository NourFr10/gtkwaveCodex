#pragma once

#include <QColor>
#include <QMap>
#include <QVector>
#include <QWidget>

#include "simple_fst_reader.h"

class WaveformView : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformView(QWidget *parent = nullptr);

    void addSignal(const fst::Signal &signal);
    void removeSignal(int handle);
    void clearSignals();

    void setTimeRange(qint64 start, qint64 end);
    qint64 primaryCursor() const { return m_primaryCursor; }
    qint64 referenceCursor() const { return m_referenceCursor; }

signals:
    void cursorMoved(qint64 primaryTime, qint64 deltaTime);

public slots:
    void zoomIn();
    void zoomOut();
    void resetView();

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    struct RenderSignal
    {
        fst::Signal signal;
        int height = 24;
    };

    QRectF signalRect(int index) const;
    QRectF timeAxisRect() const;
    QRectF waveformRect() const;
    void updateCursorFromPosition(const QPoint &pos);
    void drawGrid(QPainter &painter, const QRectF &rect);
    void drawSignal(QPainter &painter, const RenderSignal &sig, const QRectF &rect, bool alternateRow);
    void drawSignalBackground(QPainter &painter, const QRectF &rect, const RenderSignal &sig, bool alternateRow) const;
    void drawSignalWave(QPainter &painter, const RenderSignal &sig, const QRectF &rect);
    void drawTimeAxis(QPainter &painter, const QRectF &rect);
    void drawCursors(QPainter &painter, const QRectF &rect);
    qreal pixelsPerTime(const QRectF &rect) const;
    QString formatTime(qint64 value) const;

    QVector<RenderSignal> m_signals;
    qint64 m_timeStart = 0;
    qint64 m_timeEnd = 100;
    qint64 m_primaryCursor = -1;
    qint64 m_referenceCursor = -1;
    bool m_dragging = false;
    QPoint m_lastMousePos;
    QColor m_backgroundColor = QColor(18, 18, 18);
    QColor m_axisBackground = QColor(37, 37, 37);
    QColor m_gridColor = QColor(70, 70, 70);
    QColor m_nameBackground = QColor(30, 30, 30);
    QColor m_nameBorderColor = QColor(70, 70, 70);
    QColor m_digitalHigh = QColor(0, 200, 83);
    QColor m_digitalLow = QColor(244, 67, 54);
    QColor m_busFill = QColor(33, 150, 243, 90);
};

