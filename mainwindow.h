#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QMainWindow>
#include <QtWidgets>
#include "mainwindow.h"
#include "scribblearea.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void open();
    void save();

private:
    void createActions();
    void createMenus();
    bool saveFile(const QByteArray &fileFormat);
    ScribbleArea *scribbleArea;

    QMenu *saveAsMenu{};
    QMenu *fileMenu{};
    QMenu *optionMenu{};

    QAction *openAct{};
    QList<QAction *> saveAsActs;
    QAction *exitAct{};
    QAction *clearScreenAct{};
};

#endif // MAINWINDOW_H
