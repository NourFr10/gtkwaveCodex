#pragma once

#include <QWidget>
#include <QMap>
#include <QVector>

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
    void updateCursorFromPosition(const QPoint &pos);
    void drawGrid(QPainter &painter, const QRectF &rect);
    void drawSignal(QPainter &painter, const RenderSignal &sig, const QRectF &rect);
    void drawTimeAxis(QPainter &painter, const QRectF &rect);

    QVector<RenderSignal> m_signals;
    qint64 m_timeStart = 0;
    qint64 m_timeEnd = 100;
    qint64 m_primaryCursor = -1;
    qint64 m_referenceCursor = 0;
    bool m_dragging = false;
    QPoint m_lastMousePos;
};

