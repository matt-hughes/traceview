#ifndef TRACELANEDATA_H
#define TRACELANEDATA_H

#include <QFile>
#include <QList>
#include <QProgressDialog>
#include <QVariant>

class Trace
{
public:
    Trace() : _idx(-1) { }
    virtual ~Trace() {}

    virtual void findEvents(double t, int* evIdxLeftOf, int* evIdxRightOf);
    virtual int findNearestEvent(double t);
    virtual int eventsInRange(double begin, double end, int* idx);

    virtual int numEvents() = 0;
    virtual double getEventTime(int idx) = 0;
    virtual const char* getEventText(int idx, bool full) = 0;

    void setIndex(int idx) { _idx = idx; }
    int getIndex() { return _idx; }

protected:
    int _idx;
};

template<typename T> class ValueTrace : public Trace
{
public:
    virtual int getValueRange(double begin, double end, T* pMin, T* pMax) = 0;
    virtual T getEventValue(int idx) = 0;
};

class RegionTrace : public Trace
{
public:
    virtual double getCoverage(double begin, double end) = 0;
};

class TraceFile : public Trace
{
public:
    typedef struct {
        double timestamp;
        qint64 filePos;
    } EvData;

    TraceFile();
    virtual ~TraceFile();

    bool openText(const QString& fileName, QProgressDialog* progDlg = NULL);
    void close();

    virtual int numEvents();
    virtual double getEventTime(int idx);
    virtual const char* getEventText(int idx, bool full);

protected:
    QFile* _file;
    QByteArray* _fileData;
    QList<EvData> _data;
};

class SubTrace : public Trace
{
public:

    SubTrace(Trace* parent);
    virtual ~SubTrace();

    void addEvent(int masterIdx) { _parentIndices.push_back(masterIdx); }
    void clear() { _parentIndices.clear(); }

    virtual int numEvents() { return _parentIndices.size(); }
    virtual double getEventTime(int idx);
    virtual const char* getEventText(int idx, bool full);

protected:
    QList<int> _parentIndices;
    Trace* _parent;
};

class FilteredTrace : public SubTrace
{
public:
    FilteredTrace(Trace* parent) : SubTrace(parent) {}

    void processRegEx(const QString& regEx, QProgressDialog* progDlg = NULL);
};

#endif // TRACELANEDATA_H
