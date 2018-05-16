#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListView>
#include "traceview.h"

namespace Ui
{
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionLoad_triggered(void);
    void on_actionZoom_in_triggered(void);
    void on_actionZoom_out_triggered(void);
    void on_actionZoom_to_selection_triggered(void);
    void on_actionZoom_all_triggered(void);

    void onSelectionChanged(bool hasSelection);


    void on_actionReload_triggered();

    void on_actionControls_triggered();

    void on_actionFile_format_triggered();

private:
    Ui::MainWindow *ui;
    TraceView *view;
    QListView* eventList;
    QString _fileName;
};

#endif // MAINWINDOW_H
