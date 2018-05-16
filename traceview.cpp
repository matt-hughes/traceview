#include "traceview.h"
#include <stdlib.h>
#include <math.h>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QToolTip>
#include <QInputDialog>
#include <QGestureEvent>

#define NO_SELECTION    0
#define TIME_SELECTION  1
#define EVENT_SELECTION 2

#define MOUSE_ZOOM_FACTOR   4.5
#define WHEEL_ZOOM_FACTOR   1.5

#define MIN_GRID_SIZE       5

#define LANE_Y_BEGIN        20

#define DEFAULT_LANE_HEIGHT     50
#define COLLAPSED_LANE_HEIGHT   20
#define LANE_LABEL_H        18
#define LANE_LABEL_INSET_X    6
#define LANE_LABEL_INSET_Y    1
#define EVT_INSET_Y         6

#define INFO_TEXT_W         350
#define INFO_TEXT_H         20
#define INFO_TEXT_INSET_Y   3
#define INFO_TEXT_INSET_X   3
#define INFO_TEXT_FONT_SZ   10

#define EVENT_HOVER_DIST    10

#define SELECT_RANGE_COLOR      QColor(255,255,255,50)
#define SELECT_CURSOR_COLOR     QColor(255,255,255,90)
#define CURSOR_COLOR            QColor(220,220,255,70)
#define GRID_COLOR              QColor(100,100,255,70)
#define DEFAULT_EVENT_COLOR     Qt::white
#define BG_COLOR                Qt::black
#define LANE_BG_COLOR           QColor(0,15,30)
#define LANE_BG_ALT_COLOR       QColor(20,35,40)
#define LANE_SEPARATOR_COLOR    QColor(40,50,60)
#define LANE_LABEL_BG_COLOR     QColor(0,0,0,180)

static int laneHeight(Lane const& lane)
{
    return lane.isCollapsed() ? DEFAULT_LANE_HEIGHT : COLLAPSED_LANE_HEIGHT;
}

QString timeToString(double t, bool full)
{
    QString str;
    double sec = t;
    double msec = (sec - (int)sec) * 1000;
    double usec = (msec - (int)msec) * 1000;
    double nsec = (usec - (int)usec) * 1000;

    if(full)
        return str.sprintf("%ds.%03dms.%03dus.%03dns", ((int)sec), ((int)msec), ((int)usec), ((int)nsec));
    else if(sec >= 1)
        return str.sprintf("%.3fs", sec);
    else if(msec >= 1)
        return str.sprintf("%.3fms", msec);
    else if(usec >= 1)
        return str.sprintf("%.3fus", usec);
    else if(nsec >= 1)
        return str.sprintf("%.3fns", nsec);
    return str.sprintf("%fs", t);
}

static int indexRangeToCount(int left, int right)
{
    if(left == -1 || right == -1)
        return 0;
    else if(left > right)
        return 0;
    else
        return right - left + 1;
}

static QColor colorForNumEvents(const QColor baseColor, int numEv, double intensity)
{
    double a = intensity*numEv*0.5+0.5;
    if(a < 0.5) a = 0.5;
    a *= 255;
    if(a > 255) a = 255;
    if(a < 0) a = 0;
    return QColor(baseColor.red(), baseColor.green(), baseColor.blue(), (int)a);
}

static void drawEvents(QPainter& p,
                       const Lane& lane,
                       int x, int y, int w, int h,
                       double timeLeft,
                       double timeRight,
                       int evtIdxLeft,
                       int evtIdxRight,
                       double intensityScale)
{
    int numEvents = indexRangeToCount(evtIdxLeft, evtIdxRight);

    if(numEvents <= 0)
        return;

    p.setPen(Qt::NoPen);

    //if(numEvents <= w)
    if(numEvents == 1)
    {
        double pxPerTime = (double)w / (timeRight-timeLeft);
        for(int n = 0; n < numEvents; n++)
        {
            double t = lane.data->getEventTime(evtIdxLeft + n);
            int ex = x + (int)((t - timeLeft) * pxPerTime);
            p.fillRect(ex, y, 1, h, colorForNumEvents(lane.color, 1, intensityScale) );
        }
    }
    else
    {
        double timePerPx = (timeRight-timeLeft)/w;
        if(w >= 2)
        {
            int mid = x + w/2;
            int leftWidth = mid - x;
            int rightWidth = (x+w)-mid;
            double timeMid = timeLeft + (leftWidth * timePerPx);
            int evtIdxLeftOfMid, evtIdxRightOfMid;
            lane.data->findEvents(timeMid, &evtIdxLeftOfMid, &evtIdxRightOfMid);
            drawEvents(p, lane, x,   y, leftWidth,  h, timeLeft, timeMid,   evtIdxLeft,       evtIdxLeftOfMid,   intensityScale);
            drawEvents(p, lane, mid, y, rightWidth, h, timeMid,  timeRight, evtIdxRightOfMid, evtIdxRight,       intensityScale);
        }
        else
        {
            p.fillRect(x, y, w, h, colorForNumEvents(lane.color, numEvents, intensityScale) );
        }
    }
}


TraceView::TraceView(QWidget* parent)
        : QWidget(parent)
{
    _selectLane.set(-1, -1);
    _haveSelection = false;
    _hoverLaneIdx = -1;
    _hoverEvtIdx = -1;

    _selectTime.set(0, 0);
    _viewTime.set(0, 1);

    _scrollYOfs = 0;

    setMouseTracking(true);
    setFocusPolicy(Qt::ClickFocus);
}

bool TraceView::event(QEvent *event)
{
//    if (event->type() == QEvent::Gesture)
//    {
//        if (QGesture *pan = event->gesture(Qt::PanGesture))
//        {
//            panTriggered(static_cast<QPanGesture *>(pan));
//        }
//        if (QGesture *pinch = event->gesture(Qt::PinchGesture))
//        {
//            pinchTriggered(static_cast<QPinchGesture *>(pinch));
//        }
//        return true;
//    }
    return QWidget::event(event);
}

void TraceView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), BG_COLOR);

    int viewWidth = width();
    int viewHeight = height();

    int yOfs = -_scrollYOfs;

    int laneIdx = 0;
    int laneY = LANE_Y_BEGIN;

    QList<Lane>::const_iterator laneIter;

    // draw lane backgrounds
    for(laneIter = _lanes.begin(); laneIter != _lanes.end(); ++laneIter)
    {
        const Lane& lane = *laneIter;
        p.fillRect(0, laneY+yOfs, viewWidth, 1, LANE_SEPARATOR_COLOR);
        p.fillRect(0, laneY+1+yOfs, viewWidth, laneHeight(lane)-1, (laneIdx&1) ? LANE_BG_ALT_COLOR : LANE_BG_COLOR);
        ++laneIdx;
        laneY += laneHeight(lane);
    }
    if(laneIdx > 0)
        p.fillRect(0, laneY+yOfs, viewWidth, 1, LANE_SEPARATOR_COLOR);

    // draw time grid
    {
        double visibleTime = _viewTime.delta();
        double gridScale = visibleTime / ((double)viewWidth/MIN_GRID_SIZE);
        gridScale = pow(10, ceil(log10(gridScale)));

        QColor minorColor = GRID_COLOR;
        {
            double minorFactor = ((gridScale*viewWidth/visibleTime) - MIN_GRID_SIZE) / (MIN_GRID_SIZE*10 - MIN_GRID_SIZE);
            if(minorFactor < 0) minorFactor = 0;
            else if(minorFactor > 1) minorFactor = 1;
            minorFactor = pow(minorFactor, 0.5); // adjust alpha curve
            minorColor.setAlphaF(minorColor.alphaF() * minorFactor);
        }

        int64_t gridIdx = (int64_t)(_viewTime.begin / gridScale);
        double gridBegin = gridIdx * gridScale;
        double gridEnd = (int64_t)(_viewTime.end / gridScale) * gridScale;
        double gridSpan = gridEnd - gridBegin;
        for(double gridOfs = 0; gridOfs < gridSpan; gridOfs += gridScale)
        {
            double gridTime = gridBegin + gridOfs;
            int gridX = (int)absTimeToCoord(gridTime);
            bool isMajor = (gridIdx % 10) == 0;
            p.fillRect(gridX, 0, 1, viewHeight, isMajor ? GRID_COLOR : minorColor);
            ++gridIdx;
        }
    }


    // draw selection range
    if(_haveSelection)
    {
        Range<double> range = _selectTime.fix();
        int selX = (int)absTimeToCoord(range.begin);
        int selX2 = (int)absTimeToCoord(range.end);
        p.setPen(QPen(SELECT_CURSOR_COLOR, 0, Qt::DotLine));
        p.drawLine(selX, 0, selX, viewHeight);
        p.drawLine(selX2, 0, selX2, viewHeight);
        p.setPen(Qt::NoPen);

        int selY = 0;
        int selH = viewHeight;

        if(_selectLane.begin != -1 && _selectLane.end != -1)
        {
            Range<int> sel = _selectLane.fix();
            int endH, endY;
            selY = getLaneCoords(sel.begin, NULL);
            endY = getLaneCoords(sel.end, &endH);
            selH = (endY + endH) - selY;
        }
        p.fillRect(selX, selY, selX2-selX+1, selH, SELECT_RANGE_COLOR);
    }


    // draw cursor
    int cursorX = (int)absTimeToCoord(_cursorTime);
    p.fillRect(cursorX, 0, 1, viewHeight, CURSOR_COLOR);


    // draw lane data
    laneIdx = 0; laneY = LANE_Y_BEGIN;
    for(laneIter = _lanes.begin(); laneIter != _lanes.end(); ++laneIter)
    {
        int evtInsetY = EVT_INSET_Y;
        const Lane& lane = *laneIter;

        int evtIdxLeft, evtIdxRight, tmp;

        lane.data->findEvents(_viewTime.begin, &tmp, &evtIdxLeft);
        lane.data->findEvents(_viewTime.end, &evtIdxRight, &tmp);

        int numEventsVisible = indexRangeToCount(evtIdxLeft, evtIdxRight);
        double intensityScale = (numEventsVisible ? ((double)viewWidth/numEventsVisible) : 1) * 0.3;

        drawEvents(p, lane, 0, laneY+evtInsetY+yOfs, width(), laneHeight(lane)-(evtInsetY*2), _viewTime.begin, _viewTime.end, evtIdxLeft, evtIdxRight, intensityScale);

        if(laneIdx == _hoverLaneIdx && _hoverEvtIdx != -1)
        {
            double hoverEvtTime = lane.data->getEventTime(_hoverEvtIdx);
            int hoverEvtX = (int)absTimeToCoord(hoverEvtTime);
            int hoverEvtY = laneY+evtInsetY/2+yOfs;
            int hoverEvtH = laneHeight(lane)-evtInsetY;
            QColor outlineColor = lane.color.darker(170);
            outlineColor.setAlphaF(outlineColor.alphaF() * 0.5);
            const int outset = 2;
            p.fillRect(hoverEvtX-outset, hoverEvtY-outset, 1+outset*2, hoverEvtH+outset*2, outlineColor);
            p.fillRect(hoverEvtX,hoverEvtY, 1, hoverEvtH, lane.color);
        }

        // draw lane label overlay
        QString labelTxt = lane.name;
        if(!labelTxt.isNull() && !labelTxt.isEmpty())
        {
            int labelW = p.fontMetrics().width(labelTxt);
            int labelH = LANE_LABEL_H;
            QRect labelRect(LANE_LABEL_INSET_X,LANE_LABEL_INSET_Y+laneY+yOfs,labelW+10,labelH);
            QPainter::RenderHints tmpHints = p.renderHints();
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setPen(Qt::NoPen);
            p.setBrush(LANE_LABEL_BG_COLOR);
            p.drawRoundedRect(labelRect,7,7);
            p.setPen(lane.color);
            p.drawText(labelRect, Qt::AlignCenter, labelTxt);
            p.setPen(Qt::NoPen);
            p.setRenderHints(tmpHints);
        }

        ++laneIdx;
        laneY += laneHeight(lane);
    }
    

    QString infoTxt;

    infoTxt = timeToString(_cursorTime, true);

    if(_haveSelection)
    {
        Range<double> range = _selectTime.fix();
        QString selTxt = QString(" - selected %1").arg(timeToString(range.end - range.begin,false));
        infoTxt.append(selTxt);
    }

    if(!infoTxt.isNull())
    {
        QRect rect(width() - INFO_TEXT_W - INFO_TEXT_INSET_X, INFO_TEXT_INSET_Y, INFO_TEXT_W, INFO_TEXT_H);
        p.setFont(QFont("Monospace", INFO_TEXT_FONT_SZ));
        p.setPen(Qt::red);
        p.drawText(rect, Qt::AlignRight|Qt::AlignTop, infoTxt);
    }
}

void TraceView::mousePressEvent(QMouseEvent* ev)
{
    double timeAtCursor = coordToAbsTime(ev->x());

    _mousePressPos = ev->pos();
    _lastMousePos = ev->pos();


    if(ev->buttons() & Qt::MidButton && ev->modifiers() & Qt::ShiftModifier)
    {
        zoomToSelection();
    }
    else if(ev->buttons() & Qt::LeftButton)
    {
        int laneIdx = laneForCoord(ev->y());
        Lane* lane = getLane(laneIdx);
        if(ev->x() < 20)
        {
            if(lane)
            {
                lane->setCollapsed(!lane->isCollapsed());
                update();
            }
        }
        else if(ev->x() < 40)
        {
            if(lane)
            {
                bool ok;
                QString text = QInputDialog::getText(this, tr("Rename lane"),
                                                     tr("Name:"), QLineEdit::Normal,
                                                     lane->name, &ok);
                if(ok)
                {
                    lane->name = text;
                    update();
                }
            }
        }
        else
        {
            _selectTime.set(timeAtCursor, timeAtCursor);
            _haveSelection = false;
            if(ev->modifiers() & Qt::AltModifier)
                laneIdx = -1; // select all lanes

            _selectLane.set(laneIdx, laneIdx);

            updateSelectedEvents();
        }
    }
}

void TraceView::mouseReleaseEvent(QMouseEvent*)
{
}

void TraceView::mouseMoveEvent(QMouseEvent* ev)
{
    double timePerPx = _viewTime.delta() / width();
    double timeAtCursor = _viewTime.begin + ev->x() * timePerPx;
    int overLaneIdx = laneForCoord(ev->y());

    _cursorTime = timeAtCursor;

    if(ev->buttons() & Qt::RightButton)
    {
        int deltaX = ev->x() - _lastMousePos.x();
        int deltaY = ev->y() - _lastMousePos.y();
        _lastMousePos = ev->pos();

        if(ev->modifiers() & Qt::ShiftModifier)
        {
            double timeZoom = deltaY * timePerPx * MOUSE_ZOOM_FACTOR;
            double timeZoomOfs = (double)_mousePressPos.x() / width();

            _viewTime.begin += -timeZoom * timeZoomOfs;
            _viewTime.end += timeZoom * (1-timeZoomOfs);
        }
        else
        {
            double timePan = -deltaX * timePerPx;

            _viewTime.begin += timePan;
            _viewTime.end += timePan;
        }
    }
    else if(ev->buttons() & Qt::LeftButton)
    {
        _haveSelection = true;
        _selectTime.end = timeAtCursor;
        if(_selectLane.begin != -1)
            _selectLane.end = overLaneIdx;
        updateSelectedEvents();
    }

    int lastHoverLane = _hoverLaneIdx;
    int lastHoverEvt = _hoverEvtIdx;

    _hoverLaneIdx = overLaneIdx;
    if(_hoverLaneIdx != -1)
    {
        Trace* data = getLane(_hoverLaneIdx)->data;
        _hoverEvtIdx = data->findNearestEvent(timeAtCursor);
        if(_hoverEvtIdx != -1)
        {
            int evPosX = (int)absTimeToCoord(data->getEventTime(_hoverEvtIdx));
            if(abs(evPosX - ev->x()) > EVENT_HOVER_DIST)
            {
                _hoverEvtIdx = -1;
                _hoverLaneIdx = -1;
            }
        }
    }
    else
        _hoverEvtIdx = -1;

    if((_hoverLaneIdx != lastHoverLane) || (_hoverEvtIdx != lastHoverEvt))
    {
        if(_hoverEvtIdx != -1)
        {
            const Lane* lane = getLane(_hoverLaneIdx);
            int hoverEvtX = (int)absTimeToCoord(lane->data->getEventTime(_hoverEvtIdx));
            QString txt = lane->data->getEventText(_hoverEvtIdx, true);
            int laneBottom = getLaneCoords(_hoverLaneIdx, NULL) + laneHeight(*lane);
            QPoint toolTipPos(hoverEvtX, laneBottom);
            QToolTip::showText(mapToGlobal(toolTipPos), txt, this);
            //setToolTip();
        }
        else
        {
            //setToolTip(QString());
            QToolTip::hideText();
        }
    }

    update();
}

void TraceView::wheelEvent(QWheelEvent* ev)
{
    if(true)
    {
        double timePerPx = _viewTime.delta() / width();
        double timeAtCursor = _viewTime.begin + ev->x() * timePerPx;

        double scale = 1;
        if(ev->modifiers() & Qt::ShiftModifier)
        {
            _scrollYOfs += ev->pixelDelta().y();
        }
        else
        {
            scale = pow(WHEEL_ZOOM_FACTOR, -(double)ev->pixelDelta().y()/120);
        }
        double shift = (double)ev->pixelDelta().x()*timePerPx;

        _viewTime.begin = timeAtCursor - (timeAtCursor - _viewTime.begin) * scale + shift;
        _viewTime.end = timeAtCursor + (_viewTime.end - timeAtCursor) * scale + shift;

        update();
    }
    else
    {
        ev->ignore();
    }
}

void TraceView::keyPressEvent(QKeyEvent* ev)
{
    if(ev->key() == Qt::Key_Z)
    {
        if(ev->modifiers() & Qt::ShiftModifier)
        {
            zoomAll();
        }
        else
        {
            zoomToSelection();
        }
    }
    else if(ev->key() == Qt::Key_X)
    {
        if(_haveSelection)
        {
            if(ev->modifiers() & Qt::ShiftModifier)
            {
                _lanes = _lanes.mid(_selectLane.begin, _selectLane.end - _selectLane.begin + 1);
                _haveSelection = false;
                zoomAll();
            }
            else
            {
                auto begin = _lanes.begin() + _selectLane.begin;
                auto end = _lanes.begin() + _selectLane.end + 1;
                _lanes.erase(begin, end);
                _haveSelection = false;
                update();
            }
        }
    }
}

void TraceView::setLanes(const QList<Lane>& lanes)
{
    _lanes = lanes;
    update();
}

void TraceView::zoomBy(double scale)
{
    double mid = (_viewTime.end + _viewTime.begin)/2;
    double range = (_viewTime.end - _viewTime.begin)/2;
    _viewTime.begin = mid - range/scale;
    _viewTime.end = mid + range/scale;
    update();
}

void TraceView::zoomAll()
{

    QList<Lane>::const_iterator laneIter;

    double minTime = 0, maxTime = 0;
    bool haveMinMax = false;

    _scrollYOfs = 0;

    for(laneIter = _lanes.begin(); laneIter != _lanes.end(); ++laneIter)
    {
        Trace* data = (*laneIter).data;
        if(data->numEvents() > 0)
        {
            double laneMin = data->getEventTime(0);
            double laneMax = data->getEventTime(data->numEvents()-1);
            if(haveMinMax)
            {
                minTime = laneMin < minTime ? laneMin : minTime;
                maxTime = laneMax > maxTime ? laneMax : maxTime;
            }
            else
            {
                minTime = laneMin;
                maxTime = laneMax;
                haveMinMax = true;
            }
        }
    }

    if(haveMinMax)
    {
        double delta = maxTime - minTime;
        double scale = 0.5;
        _viewTime.set(minTime - delta*scale, maxTime + delta*scale);
    }

    update();
}


void TraceView::zoomToSelection(void)
{
    if(_haveSelection)
    {
        if(_selectTime.begin != _selectTime.end)
        {
            _viewTime = _selectTime.fix();
            zoomBy(0.9);
        }
    }
    update();
}

void TraceView::clearSelection()
{
    _selectLane.set(-1, -1);
    _haveSelection = false;
    updateSelectedEvents();
    update();
}

void TraceView::updateSelectedEvents()
{
    emit selectionChanged(_haveSelection);
}

float TraceView::absTimeToCoord(double t)
{
    return (float)(((t - _viewTime.begin) / _viewTime.delta()) * width());
}

double TraceView::coordToAbsTime(int c)
{
    double timePerPx = _viewTime.delta() / width();
    return _viewTime.begin + c * timePerPx;
}

int TraceView::getLaneCoords(int idx, int* height)
{
    int yOfs = -_scrollYOfs;
    if(idx < 0 || idx >= _lanes.size())
        return 0;
    int y = LANE_Y_BEGIN;
    QList<Lane>::const_iterator iter = _lanes.begin();
    for(int n = 0; n < idx; n++)
    {
        y += laneHeight(*iter);
        ++iter;
    }
    if(height)
        *height = laneHeight(*iter);
    return y + yOfs;
}

int TraceView::laneForCoord(int y)
{
    int yOfs = -_scrollYOfs;
    y -= yOfs;
    int test_y = LANE_Y_BEGIN;
    QList<Lane>::const_iterator iter;
    int idx = 0;
    for(iter = _lanes.begin(); iter != _lanes.end(); ++iter)
    {
        int test_y2 = test_y + laneHeight(*iter);
        if(y >= test_y && y < test_y2)
            return idx;
        test_y = test_y2;
        ++idx;
    }
    return -1;
}

