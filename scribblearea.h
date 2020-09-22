#ifndef SCRIBBLEAREA_H
#define SCRIBBLEAREA_H

#include <QColor>
#include <QImage>
#include <QPoint>
#include <QBrush>
#include <QPen>
#include <QWidget>
#include <QtWidgets>
#include <QTabletEvent>
#include "MovablePolyContour.h"

class ScribbleArea : public QWidget
{
	Q_OBJECT

public:
	ScribbleArea(QWidget* parent = nullptr);

	bool openImage(const QString& fileName);
	bool openBorderImage(const QString& fileName);
	bool saveImage(const QString& fileName, const char* fileFormat);

	QLabel* gestureLabel;

public slots:
	void clearImage();

protected:
	void tabletEvent(QTabletEvent* event) override;
	void paintEvent(QPaintEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	bool event(QEvent* event) override;


private:
	void resizeImage(QImage* image, const QSize& newSize);
	void updateBrush(const QTabletEvent* event);
	void updateCanvas(QTabletEvent* event);
	void updateCursor(const QTabletEvent* event, const qreal width);
	void selectBoundary(QImage* image, QImage* front);

	void countGray(QImage* image);
	void countPoly(QImage* image);
	void drawColorPalette();
	void drawPolyContour();
	void constructGrayRect();
	void blurImage();
	bool checkInPolyContour();

	QColor grayDegreeToColor(int grayDegree);
	void initColorList();
	void updatePoly();

	void touchedTwo(const QList<QTouchEvent::TouchPoint>& touchPoints);
	void touchedOne(const QList<QTouchEvent::TouchPoint>& touchPoints);


	QPointF _Brother1;
	QPointF _Brother2;
	QColor _curGrayDegree;

	QVector <QColor> colorList;
	QVector <QRgb> grayDegree;
	QVector <QRect> grayPaletteRect;
	/*
		_background ����ͼ�� ��ͼƬ
		_canvas     ����ͼ�� ����Ӱ
		_front      ����ͼ�� ������Ӱ��Χ������
	 */
	QRect   PaintRect;

	QImage  _background;
	QImage  _canvas;
	QImage  _front;
	QImage  _poly;
	QImage  _polyContour;
	QImage  _outPut;
	QImage  _border;
	QVector<MovablePolyContour> _PolyContours;
	
	/*
		_leftTop        ��ǰͼ������Ͻ�
		_originLeftop   ԭʼͼ������Ͻ�
		_scale          ��ǰ�����ű���
		_originScale    ԭʼ�����ű�����������˫ָ���ŵ�ʱ��ı������ν�
	*/

	QPointF _leftTop;
	QPointF _originLeftTop;
	QPoint beginSelPoint;
	QPoint endSelPoint;

	qreal _scale;
	qreal _originScale;

	QBrush _brush;
	QPen _pen;
	QColor _color;

	QTimer* touchTimer;

	QList<QTouchEvent::TouchPoint> preTouchPoints;
	/*
		_twoTouch   true:   ��ǰΪ˫ָ
	*/
	bool _twoTouch;
	bool _penDown;
	bool _eraseMode;
	bool colorSelected;
	bool touched;
	bool touchupdated;
	int colorSel;
	int _contourI;
	int _displayContourId;
	bool _polyUpdated;
	int _choosePoly;
	bool _selPoly;
	int _touchNum;
	struct Point
	{
		QPointF pos;
		qreal pressure;
		qreal rotation;
	} lastPoint;
};
#endif // SCRIBBLEAREA_H
