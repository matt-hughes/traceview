#include "tracedata.h"
#include <math.h>
#include <stdlib.h>
#include <QRegExp>
#include <QMessageBox>
#include <QtAlgorithms>

#define DEFAULT_CAPACITY    4096

#define MAX_IN_MEMORY_SZ    (1024*1024*512)

#define MAX_LINE_SZ         256

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void Trace::findEvents(double t, int* evIdxLeftOf, int* evIdxRightOf)
{
    int left = 0;
    int count = numEvents();
    int right = count;
    int mid;

    while(left < right)
    {
        mid = (right+left)/2;
        if(getEventTime(mid) < t)
            left = mid + 1;
        else
            right = mid;
    }

    *evIdxLeftOf = (left > 0) ? (left-1) : -1;
    *evIdxRightOf = (left < count) ? left : -1;
}

int Trace::findNearestEvent(double t)
{
    int left, right;
    findEvents(t, &left, &right);
    if(left == -1) return right;
    if(right == -1) return left;
    double mid = (getEventTime(left) + getEventTime(right)) / 2;
    return (t < mid) ? left : right;
}

int Trace::eventsInRange(double begin, double end, int* idx)
{
    if(begin > end)
    {
        double dtmp = begin;
        begin = end;
        end = dtmp;
    }
    int evtIdxLeft, evtIdxRight, tmp;
    findEvents(begin, &tmp, &evtIdxLeft);
    findEvents(end, &evtIdxRight, &tmp);
    if(evtIdxLeft == -1) { *idx = -1; return 0; }
    if(evtIdxRight == -1) { *idx = -1; return 0; }
    if(evtIdxLeft > evtIdxRight) { *idx = -1; return 0; }
    *idx = evtIdxLeft;
    return evtIdxRight - evtIdxLeft + 1;
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

TraceFile::TraceFile()
    : _file(NULL)
{
}

TraceFile::~TraceFile()
{
    close();
}

int TraceFile::numEvents()
{
    return _data.size();
}

double TraceFile::getEventTime(int idx)
{
    return _data.at(idx).timestamp;
}

const char* TraceFile::getEventText(int idx, bool full)
{
    QByteArray line;
    const char* lineData;

    if(idx < 0 || idx >= _data.size())
        return NULL;

    quint64 filePos = _data[idx].filePos;

    static char txt[MAX_LINE_SZ]; //bleh
    double timestamp;

    if(_file)
    {
        _file->seek(filePos);
        line = _file->readLine();
        lineData = line.data();
    }
    else if(_fileData)
    {
        lineData = _fileData->data() + filePos;
    }
    else
        return NULL;

    if(full)
    {
        sscanf(lineData, "%[^\n]", txt);
        return txt;
    }
    else if(sscanf(lineData, "%lf %[^\n]", &timestamp, txt) == 2)
        return txt;
    else
        return NULL;
}

static bool eventLessThan(const TraceFile::EvData &e1, const TraceFile::EvData &e2)
{
    return e1.timestamp < e2.timestamp;
}

bool TraceFile::openText(const QString& fileName, QProgressDialog* progDlg)
{
    bool isMonotonic = true;
    unsigned int idx = 0;
    double lastTime = 0;

    if(_file)
        delete _file;
    _file = new QFile(fileName);

    if(_fileData)
    {
        delete _fileData;
        _fileData = NULL;
    }

    _data.clear();

    if(_file->size() <= MAX_IN_MEMORY_SZ)
    {
        _fileData = new QByteArray();
    }

    if (!_file->open(QIODevice::ReadOnly | QIODevice::Text))
    {
        delete _file;
        _file = NULL;
        return false;
    }

    double timestamp;

    if(progDlg)
    {
        progDlg->reset();
        progDlg->setRange(0, _file->size());
    }

    if(_fileData)
    {
        *_fileData = _file->readAll();
        _file->close();
        delete _file;
        _file = NULL;
    }

    qint64 curFilePos = 0;
    bool eof = false;
    while (!eof)
    {
        QByteArray line;
        const char* lineData;

        qint64 evFilePos;

        if(_file)
        {
            evFilePos = curFilePos = _file->pos();
            line = _file->readLine();
            eof = _file->atEnd();
            lineData = line.data();
        }
        else
        {
            lineData = _fileData->data() + curFilePos;
            char* ptr = (char*)lineData;
            evFilePos = curFilePos;
            char ch;

            while((ch = *ptr) != '\0')
            {
                if(ch == '\0') eof = true;
                else if(ch == '\r') ++ptr;
                else if(ch == '\n') { *ptr++ = '\0'; break; }
                else ++ptr;
            }
            curFilePos += ptr - lineData;
            if(curFilePos >= _fileData->length())
                eof = true;
        }

        if(progDlg)
            progDlg->setValue(curFilePos);

        if(sscanf(lineData, "%lf", &timestamp) == 1)
        {
            EvData ev;

            if((timestamp < lastTime) && (idx != 0))
                isMonotonic = false;
            lastTime = timestamp;

            ev.timestamp = timestamp;
            ev.filePos = evFilePos;
            _data.push_back(ev);
            ++idx;
        }
    }

    if(!isMonotonic)
    {
        //QMessageBox::warning(NULL, "Warning", "Timestamps are not monotonic!");
        qStableSort(_data.begin(), _data.end(), eventLessThan);
    }

    return true;
}

void TraceFile::close()
{
    if(_file)
        _file->close();
    _file = NULL;
    _data.clear();
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

SubTrace::SubTrace(Trace* parent)
        : _parent(parent)
{
}

SubTrace::~SubTrace()
{
}


double SubTrace::getEventTime(int idx)
{
    if(idx < 0 || idx >= _parentIndices.size())
        return 0;
    return _parent->getEventTime(_parentIndices[idx]);
}


const char* SubTrace::getEventText(int idx, bool full)
{
    if(idx < 0 || idx >= _parentIndices.size())
        return NULL;
    return _parent->getEventText(_parentIndices[idx], full);
}


//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

void FilteredTrace::processRegEx(const QString& regEx, QProgressDialog* progDlg)
{
    QRegExp regex(regEx);
    int numEvents = _parent->numEvents();

    clear();

    if(progDlg)
    {
        progDlg->reset();
        progDlg->setRange(0, numEvents);
    }

    for(int n = 0; n < numEvents; n++)
    {
        const char* msg = _parent->getEventText(n, false);
        if(regex.indexIn(msg) != -1)
            addEvent(n);

        if(progDlg)
            progDlg->setValue(n);
    }
}

