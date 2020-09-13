#include "mainwindow.h"
#include <QApplication>
#include <opencv2/opencv.hpp>
#include <opencv2\imgproc\types_c.h>
using namespace cv;
using namespace std;

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.showMaximized();
    int status = QApplication::exec();
    return status;
}
