#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "tracedata.h"

#define ORG_NAME "MHughes"
#define APP_NAME "TraceView"

#define KEY_LAST_FILENAME "lastFileName"
#define KEY_WINDOW_GEOMETRY "windowGeometry"

#include <QMessageBox>
#include <QFileDialog>
#include <QFile>
#include <QList>
#include <QPair>
#include <QtAlgorithms>
#include <QProgressDialog>
#include <QProgressBar>
#include <QRegExp>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QSplitter>
#include <QSettings>

TraceFile gTraceFile;

#define ZOOM_FACTOR 1.3

#define MAX_LIST_EVENTS 150

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QSplitter* split = new QSplitter(Qt::Vertical, ui->centralWidget);
    QLayout* layout = new QVBoxLayout();
    ui->centralWidget->setLayout(layout);
    layout->addWidget(split);
    split->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    view = new TraceView(split);
    //layout->addWidget(view);
    view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    eventList = new QListView(split);
    //layout->addWidget(eventList);
    eventList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    eventList->setModel(new QStringListModel());
    eventList->setFocusPolicy(Qt::NoFocus);
    QFont eventFont("Monospace", 9);
    //eventFont.setStyleHint(QFont::TypeWriter);
    eventList->setFont(eventFont);
    split->setStretchFactor(0, 1);
    split->setStretchFactor(1, 0);
    QList<int> szList;
    szList.append(4);
    szList.append(1);
    split->setSizes(szList);

    connect(view, SIGNAL(selectionChanged(bool)), this, SLOT(onSelectionChanged(bool)));

    QSettings settings(ORG_NAME, APP_NAME);
    _fileName = settings.value(KEY_LAST_FILENAME).toString();
    restoreGeometry(settings.value(KEY_WINDOW_GEOMETRY).toByteArray());
}

MainWindow::~MainWindow()
{
    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue(KEY_LAST_FILENAME, _fileName);
    delete ui;
}

void MainWindow::on_actionLoad_triggered(void)
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File",
                                                    QString(),
                                                    "Trace files (*.txt *.bin)");

    if(fileName.isNull())
        return;

    _fileName = fileName;

    on_actionReload_triggered();
}

void MainWindow::on_actionReload_triggered()
{
    if(_fileName.isNull())
    {
        on_actionLoad_triggered();
    }
    else if(_fileName.endsWith(".txt"))
    {
        QProgressDialog progDlg(this);
        progDlg.setLabelText("Loading trace file...");
        progDlg.setAutoClose(false);
        progDlg.setAutoReset(false);
        progDlg.show();

        gTraceFile.openText(_fileName, &progDlg);

        QList<Lane> lanes;

        progDlg.setLabelText("Building lanes...");

        QMap<QString,FilteredTrace*> laneMap;
        QMap<QString,QString> laneNameMap;
        progDlg.reset();
        progDlg.setRange(0, gTraceFile.numEvents());
        char tmpStrBuf[256];
        int traceIdx = 0;
        for(int n = 0; n < gTraceFile.numEvents(); n++)
        {
            int ofs = 0;
            const char* txt = gTraceFile.getEventText(n, false);
            if(txt && (sscanf(txt, "%s%n", tmpStrBuf, &ofs) >= 1))
            {
                QString laneID(tmpStrBuf);
                QMap<QString,FilteredTrace*>::const_iterator iter;
                FilteredTrace* data;

                if(sscanf(txt+ofs, " THREAD_NAME=%n%s", &ofs, tmpStrBuf) >= 1)
                {
                    laneNameMap[laneID] = QString(tmpStrBuf);
                }

                iter = laneMap.find(laneID);
                if(iter == laneMap.end())
                {
                    data = new FilteredTrace(&gTraceFile);
                    data->setIndex(traceIdx++);
                    laneMap[laneID] = data;
                    QColor color = QColor::fromHsv((data->getIndex()*35)%255,255,255);
                    lanes.push_back(Lane(data, laneID, color));
                }
                else
                {
                    data = iter.value();
                }
                data->addEvent(n);
            }
            progDlg.setValue(n);
        }

        progDlg.setLabelText("Filtering trace data...");

        //...

        for(auto& l: lanes)
        {
            auto iter = laneNameMap.find(l.name);
            if(iter != laneNameMap.end())
            {
                l.name = iter.value();
            }
        }

        view->setLanes(lanes);
        view->zoomAll();

        progDlg.hide();
    }
}

void MainWindow::on_actionZoom_in_triggered(void)
{
    view->zoomBy(ZOOM_FACTOR);
}

void MainWindow::on_actionZoom_out_triggered(void)
{
    view->zoomBy(1/ZOOM_FACTOR);
}

void MainWindow::on_actionZoom_to_selection_triggered(void)
{
    view->zoomToSelection();
}

void MainWindow::on_actionZoom_all_triggered(void)
{
    view->zoomAll();
}

static bool eventLessThan(const QPair<double,QString> &e1, const QPair<double,QString> &e2)
{
    return e1.first < e2.first;
}

void MainWindow::onSelectionChanged(bool hasSelection)
{
    QStringListModel* model = (QStringListModel*)eventList->model();
    QStringList itemStrings;
    QList<QPair<double,QString> > items;
    int totalEventCount = 0;

    if(hasSelection)
    {
        Range<int> laneRange = view->selectedLaneRange();
        Range<double> timeRange = view->selectedTimeRange();
        if(laneRange.begin != -1 && laneRange.end != -1)
        {
            for(int laneIdx = laneRange.begin; laneIdx <= laneRange.end; ++laneIdx)
            {
                const Lane* lane = view->getLane(laneIdx);
                int eventIdx = 0;
                Trace* data = lane->data;
                int eventCount = data->eventsInRange(timeRange.begin, timeRange.end, &eventIdx);
                totalEventCount += eventCount;
                if(totalEventCount > MAX_LIST_EVENTS)
                {
                    break;
                }
                else if(eventCount > 0)
                {
                    for(int n = 0; n < eventCount; n++)
                    {
                        double timestamp = data->getEventTime(eventIdx+n);
                        QString str = data->getEventText(eventIdx+n, true);
                        items.append(QPair<double,QString>(timestamp,str));
                    }
                }
            }
        }
    }

    if(totalEventCount <= MAX_LIST_EVENTS)
    {
        std::stable_sort(items.begin(), items.end(), eventLessThan);

        QList<QPair<double,QString> >::const_iterator iter;
        for(iter = items.begin(); iter != items.end(); ++iter)
            itemStrings.append(iter->second);
    }
    else
    {
        itemStrings.append(QString("Selected %1 events").arg(totalEventCount));
    }

    model->setStringList(itemStrings);
}



void MainWindow::on_actionControls_triggered()
{
    QString txt =
            "Controls: \n"
            "  Left mouse + drag: Select events\n"
            "  Right mouse + drag left/right: Scroll\n"
            "  Mouse wheel: Zoom\n"
            "  Right mouse + shift + drag up/down: Fine zoom";
    QMessageBox::about(this, "Help: Controls", txt);
}

void MainWindow::on_actionFile_format_triggered()
{
    QString txt =
            "File format: \n"
            "\n"
            "  Events consist of a single line of text with the following format:\n"
            "\n"
            "      TIMESTAMP LANE DETAIL...\n"
            "\n"
            "  Where TIMESTAMP is a floating point number representing the absolute\n"
            "  time in seconds, LANE is a symbolic name (string with no spaces)\n"
            "  identifying which lane the event should be displayed in, and DETAIL\n"
            "  is the remainder of the line (which can contain spaces) representing\n"
            "  text that will be displayed when an event is selected or highlighted.";
    QMessageBox::about(this, "Help: File format", txt);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    QSettings settings(ORG_NAME, APP_NAME);
    settings.setValue(KEY_WINDOW_GEOMETRY, saveGeometry());
    QMainWindow::closeEvent(event);
}
