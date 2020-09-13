#include "mainwindow.h"
#include <windows.h>


static const int DEFAULT_WIDTH = 500;
static const int DEFAULT_HEIGHT =500;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    scribbleArea = new ScribbleArea;
    setCentralWidget(scribbleArea);
    createActions();
    createMenus();
    setWindowTitle(tr("Shadow Paint"));
    resize(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    scribbleArea->gestureLabel->setStyleSheet(" QLabel{ color: red }");
    scribbleArea->gestureLabel->setText("No Gesture :");
    statusBar()->addWidget(scribbleArea->gestureLabel);
}

MainWindow::~MainWindow()
{

}


void MainWindow::open()
{


    /*QString fileName = QFileDialog::getOpenFileName(this,
                                   tr("Open File"), QDir::currentPath());
    */
    QString fileName = "C:/Users/yyjxx/Desktop/Toon/toon3.png";
    if (!fileName.isEmpty())
        scribbleArea->openImage(fileName);
}

void MainWindow::save()
{
    auto *action = qobject_cast<QAction *>(sender());
    QByteArray fileFormat = action->data().toByteArray();
    saveFile(fileFormat);
}


void MainWindow::createActions()
{
    openAct = new QAction(tr("&Open..."), this);
    openAct->setShortcut(tr("Ctrl+O"));
    connect(openAct, &QAction::triggered, this, &MainWindow::open);

    const QList<QByteArray> imageFormats = QImageWriter::supportedImageFormats();
    for (const QByteArray &format : imageFormats) {
        QString text = tr("%1...").arg(QString(format).toUpper());

        auto *action = new QAction(text, this);
        action->setData(format);
        connect(action, &QAction::triggered, this, &MainWindow::save);
        saveAsActs.append(action);
    }

    exitAct = new QAction(tr("E&xit"), this);
    exitAct->setShortcut(tr("Ctrl+Q"));
    connect(exitAct, &QAction::triggered, this, &QWidget::close);

    clearScreenAct = new QAction(("&Clear Screen"), this);
    clearScreenAct->setShortcut(tr("Ctrl+L"));
    connect(clearScreenAct, &QAction::triggered, scribbleArea, &ScribbleArea::clearImage);

}

void MainWindow::createMenus()
{
    saveAsMenu = new QMenu(tr("&Save As"), this);
    saveAsMenu->addActions(saveAsActs);

    fileMenu = new QMenu(tr("&File"), this);
    fileMenu->addAction(openAct);
    fileMenu->addMenu(saveAsMenu);
    fileMenu->addSeparator();
    fileMenu->addAction(exitAct);

    optionMenu = new QMenu(tr("&Options"), this);
    optionMenu->addAction(clearScreenAct);


    menuBar()->addMenu(fileMenu);
    menuBar()->addMenu(optionMenu);
}


bool MainWindow::saveFile(const QByteArray &fileFormat)
{
    bool success = true;

    QString initialPath = QDir::currentPath() + "/untitled." + fileFormat;

    QString fileName = QFileDialog::getSaveFileName(this, tr("Save As"),
                               initialPath,
                               tr("%1 Files (*.%2);;All Files (*)")
                               .arg(QString::fromLatin1(fileFormat.toUpper()))
                               .arg(QString::fromLatin1(fileFormat)));
    if (fileName.isEmpty())
    {
        success = false;
    }
    else
    {
        success = scribbleArea->saveImage(fileName, fileFormat.constData());
    }
    return success;
}
