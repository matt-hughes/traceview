#ifndef TRACEVIEW_H
#define TRACEVIEW_H

#include <QWidget>
#include <QList>
#include "tracedata.h"

template<typename T> class Range
{
public:
    T begin, end;
    Range<T> fix(void)
    {
        Range<T> x;
        x.begin = begin < end ? begin : end;
        x.end = begin > end ? begin : end;
        return x;
    }
    T delta(void) { return end - begin; }
    void set(T b, T e) { begin = b; end = e; }
};

class Lane {
public:
    Lane(Trace* data = NULL, const QString& name = QString(), QColor color = QColor()) : data(data), name(name), color(color), collapsed(false) { }
    Trace* data;
    QString name;
    QColor color;
    bool collapsed;
public:
    void setCollapsed(bool c) { collapsed = c; }
    bool isCollapsed() const { return collapsed; }
};

class TraceView : public QWidget
{
    Q_OBJECT
public:

    TraceView(QWidget* parent = NULL);

    void setLanes(const QList<Lane>& lanes);

    void zoomBy(double scale);
    void zoomToSelection();
    void zoomAll();

    void clearSelection();

    bool hasSelection() { return _haveSelection; }
    inline Range<double> selectedTimeRange() { return _selectTime.fix(); }
    inline Range<int> selectedLaneRange() { return _selectLane.fix(); }
    Lane* getLane(int idx) { return (idx < 0 || idx >= _lanes.size()) ? NULL : (Lane*)&_lanes.at(idx); }

signals:
    void selectionChanged(bool hasSelection);

protected:
    bool event(QEvent *event);
    void paintEvent (QPaintEvent *);
    void mousePressEvent(QMouseEvent* ev);
    void mouseReleaseEvent(QMouseEvent*);
    void mouseMoveEvent(QMouseEvent* ev);
    void wheelEvent(QWheelEvent* ev);
    //bool gestureEvent(QGestureEvent *event);
    void keyPressEvent(QKeyEvent* ev);

    float absTimeToCoord(double t);
    double coordToAbsTime(int c);

    void updateSelectedEvents();

    int getLaneCoords(int idx, int* height);
    int laneForCoord(int y);

protected:
    Range<double> _viewTime;
    QPoint _lastMousePos, _mousePressPos;
    QList<Lane> _lanes;
    bool _haveSelection;
    Range<double> _selectTime;
    Range<int> _selectLane;
    double _cursorTime;
    int _hoverLaneIdx;
    int _hoverEvtIdx;
    int _scrollYOfs;
};

#endif // TRACEVIEW_H
