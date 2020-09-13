#include <QtWidgets/QGesture>
#include <QtWidgets/QGestureEvent>
#include <QtCore/QDebug>
#include <QtWidgets>
#include <QVector>
#include <QQueue>
#include <QPair>
#include <iostream>
#include <cmath>
#include <QMap>
#include "scribblearea.h"
#include <opencv2/opencv.hpp>
#include <opencv2\imgproc\types_c.h>
#include <vector>
#include "gui_setting.h"

static void warpTriangle(cv::Mat& img1, cv::Mat& img2, std::vector<cv::Point2f> tri1, std::vector<cv::Point2f> tri2)
{
	cv::Rect r1 = boundingRect(tri1);
	cv::Rect r2 = boundingRect(tri2);

	std::vector<cv::Point2f> tri1Cropped, tri2Cropped;
	std::vector<cv::Point> tri2CroppedInt;
	for (int i = 0; i < 3; i++)
	{
		tri1Cropped.push_back(cv::Point2f(tri1[i].x - r1.x, tri1[i].y - r1.y));
		tri2Cropped.push_back(cv::Point2f(tri2[i].x - r2.x, tri2[i].y - r2.y));
		tri2CroppedInt.push_back(cv::Point((int)(tri2[i].x - r2.x), (int)(tri2[i].y - r2.y)));
	}

	cv::Mat img1Cropped;
	img1(r1).copyTo(img1Cropped);

	cv::Mat warpMat = getAffineTransform(tri1Cropped, tri2Cropped);


	cv::Mat img2Cropped = cv::Mat::zeros(r2.height, r2.width, img1Cropped.type());
	cv::warpAffine(img1Cropped, img2Cropped, warpMat, img2Cropped.size(), cv::INTER_LINEAR, cv::BORDER_REFLECT_101);

	cv::Mat mask = cv::Mat::zeros(r2.height, r2.width, CV_32FC3);
	cv::fillConvexPoly(mask, tri2CroppedInt, cv::Scalar(1.0, 1.0, 1.0), 16, 0);

	cv::multiply(img2Cropped, mask, img2Cropped);

	cv::multiply(img2(r2), cv::Scalar(1.0, 1.0, 1.0) - mask, img2(r2));
	img2(r2) = img2(r2) + img2Cropped;
}

cv::Mat QImage2cvMat(QImage image)
{
	cv::Mat mat;
	qDebug() << image.format();
	switch (image.format())
	{
	case QImage::Format_ARGB32:
	case QImage::Format_RGB32:
	case QImage::Format_ARGB32_Premultiplied:
		mat = cv::Mat(image.height(), image.width(), CV_8UC4, (void*)image.constBits(), image.bytesPerLine());
		break;
	case QImage::Format_RGB888:
		mat = cv::Mat(image.height(), image.width(), CV_8UC3, (void*)image.constBits(), image.bytesPerLine());
		cv::cvtColor(mat, mat, CV_BGR2RGB);
		break;
	case QImage::Format_Indexed8:
		mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
		break;
	case QImage::Format_Grayscale8:
		mat = cv::Mat(image.height(), image.width(), CV_8UC1, (void*)image.constBits(), image.bytesPerLine());
		break;
	}
	return mat;
}


QImage cvMat2QImage(const cv::Mat& mat)
{
	// 8-bits unsigned, NO. OF CHANNELS = 1
	if (mat.type() == CV_8UC1)
	{
		QImage image(mat.cols, mat.rows, QImage::Format_Indexed8);
		// Set the color table (used to translate colour indexes to qRgb values)
		image.setColorCount(256);
		for (int i = 0; i < 256; i++)
		{
			image.setColor(i, qRgb(i, i, i));
		}
		// Copy input Mat
		uchar* pSrc = mat.data;
		for (int row = 0; row < mat.rows; row++)
		{
			uchar* pDest = image.scanLine(row);
			memcpy(pDest, pSrc, mat.cols);
			pSrc += mat.step;
		}
		return image;
	}
	// 8-bits unsigned, NO. OF CHANNELS = 3
	else if (mat.type() == CV_8UC3)
	{
		// Copy input Mat
		const uchar* pSrc = (const uchar*)mat.data;
		// Create QImage with same dimensions as input Mat
		QImage image(pSrc, mat.cols, mat.rows, mat.step, QImage::Format_RGB888);
		return image.rgbSwapped();
	}
	else if (mat.type() == CV_8UC4)
	{
		qDebug() << "CV_8UC4";
		//QImage image;

		QImage image(mat.data,
			mat.cols, mat.rows,
			static_cast<int>(mat.step),
			QImage::Format_ARGB32);
		//image.fill(qRgba(254, 254, 254, 0));
		return image.copy();
	}
	else
	{
		qDebug() << "ERROR: Mat could not be converted to QImage.";
		return QImage();
	}
}


static const int    SIZE_INCREMENT = 128;

ScribbleArea::ScribbleArea(QWidget* parent) : QWidget(parent)
{

	setAttribute(Qt::WA_AcceptTouchEvents);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_TabletTracking);


	QList<Qt::GestureType> gestures;
	gestures << Qt::TapGesture
		<< Qt::TapAndHoldGesture
		<< Qt::PanGesture
		<< Qt::PinchGesture
		<< Qt::SwipeGesture;

	foreach(Qt::GestureType gesture, gestures)
		grabGesture(gesture);
	gestureLabel = new QLabel;

	_scale = 1.0;
	_originScale = 1.0;

	_leftTop.setX(0);
	_leftTop.setY(0);

	_penDown = false;
	_eraseMode = false;
	_twoTouch = false;
	colorSelected = false;
	colorSel = -1;
	_touchNum = 0;
	_displayContourId = -1;
	_color = QColor(255, 255, 255, 255);

	_background = QImage(this->width(), this->height(), QImage::Format_ARGB32);
	_background.fill(qRgba(254, 254, 254, 0));
	_polyContour = QImage(this->width(), this->height(), QImage::Format_ARGB32);
	_polyContour.fill(qRgba(254, 254, 254, 0));
	_outPut = QImage(this->width(), this->height(), QImage::Format_ARGB32);
	_outPut.fill(qRgba(254, 254, 254, 0));
	_border = QImage(this->width(), this->height(), QImage::Format_ARGB32);;
	_border.fill(qRgba(254, 254, 254, 0));

	touchTimer = new QTimer;
	touchTimer->setSingleShot(false);
	touchTimer->setInterval(TOUCHINTERVAL);

	initColorList();
}

void ScribbleArea::initColorList()
{
	colorList.clear();
	colorList.push_back(QColor(0, 0, 255));
	colorList.push_back(QColor(0, 255, 0));

	colorList.push_back(QColor(0, 255, 255));
	colorList.push_back(QColor(255, 0, 255));
	colorList.push_back(QColor(255, 255, 0));
}



bool ScribbleArea::openImage(const QString& fileName)
{

	/*QString bouderName = QFileDialog::getOpenFileName(this,
		tr("Open Border"), QDir::currentPath());*/
	QString bouderName = "C:/Users/yyjxx/Desktop/contour/contours1.png";
	if (!bouderName.isEmpty())
		openBorderImage(bouderName);
	//qDebug() << bouderName;
	//openBorderImage("")

	QImage loadedImage;
	if (!loadedImage.load(fileName))
		return false;
	//QImage loadedBorder;


	_background = loadedImage.convertToFormat(QImage::Format_ARGB32).copy();
	qDebug() << "load format:" << _background.format() << _background.hasAlphaChannel();

	_background = _background.scaled(width(), height(), Qt::KeepAspectRatio, Qt::FastTransformation);
	_border = _border.scaled(_background.width(), _background.height(), Qt::KeepAspectRatio, Qt::FastTransformation);

	QImage mask = _border.createMaskFromColor(_border.pixel(0, 0), Qt::MaskOutColor);
	_border.setAlphaChannel(mask);



	QPainter backPainter(&_background);
	QRect curRect = _background.rect();
	backPainter.drawImage(QRect(0, 0, _background.width(), _background.height()), _border, curRect);
	backPainter.end();

	setFixedSize(_background.size());
	//blurImage();

	_canvas = QImage(_background.width(), _background.height(), QImage::Format_ARGB32);
	//_canvas = _canvas.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	_front = QImage(_background.width(), _background.height(), QImage::Format_ARGB32);
	_polyContour = QImage(_background.width(), _background.height(), QImage::Format_ARGB32);

	selectBoundary(&_background, &_background);

	countGray(&_background);
	countPoly(&_background);


	update();
	return true;
}

bool ScribbleArea::openBorderImage(const QString& fileName)
{
	QImage loadedImage;
	if (!loadedImage.load(fileName))
		return false;
	_border = loadedImage.convertToFormat(QImage::Format_ARGB32);
	//_border = _border.scaled(_background.width(), _background.height(), Qt::KeepAspectRatio, Qt::FastTransformation);
	return true;
}

bool ScribbleArea::saveImage(const QString& fileName, const char* fileFormat)
{
	bool success;

	QImage visibleImage = _background;
	resizeImage(&visibleImage, size());

	if (visibleImage.save(fileName, fileFormat))
	{
		success = true;
	}
	else
	{
		success = false;
	}

	return success;
}

void ScribbleArea::clearImage()
{
	_canvas.fill(qRgba(254, 254, 254, 0));
	update();
}

void ScribbleArea::tabletEvent(QTabletEvent* event)
{
	switch (event->type())
	{
	case QEvent::TabletPress:
		if (!_penDown)
		{

			_penDown = true;
			lastPoint.pos = event->posF();
			lastPoint.pressure = event->pressure();
			lastPoint.rotation = event->rotation();
			if (checkInPolyContour())
			{
				beginSelPoint = event->pos();
				_selPoly = true;
			}
			else
				_selPoly = false;
		}
		break;
	case QEvent::TabletMove:
		if (_penDown)
		{
			if (_selPoly)
			{
				lastPoint.pos = event->posF();
				lastPoint.pressure = event->pressure();
				lastPoint.rotation = event->rotation();

				QPointF curPoint = (lastPoint.pos - _leftTop) / _scale;
				_PolyContours[_choosePoly].ChangePoint(_contourI, QPoint(curPoint.x(), curPoint.y()));
			}
			else
			{
				updateBrush(event);
				updateCanvas(event);
				lastPoint.pos = event->posF();
				lastPoint.pressure = event->pressure();
				lastPoint.rotation = event->rotation();
			}
		}
		break;
	case QEvent::TabletRelease:
		if (_penDown && event->buttons() == Qt::NoButton)
		{
			//blurImage();
			if (_selPoly)
			{
				endSelPoint = event->pos();
				updatePoly();

				QPainter backPainter(&_background);
				QRect curRect = _background.rect();
				backPainter.drawImage(PaintRect, _outPut, curRect);
				backPainter.end();

				countPoly(&_background);
			}

			_selPoly = false;
			_penDown = false;
		}
		break;
	default:

		break;
	}
	event->accept();
	update();
}

static int Min(int x, int y)
{
	return x > y ? y : x;
}
static int Max(int x, int y)
{
	return x > y ? x : y;
}
void ScribbleArea::paintEvent(QPaintEvent* event)
{
	PaintRect = event->rect();
	const QRect rect = event->rect();
	QRect curRect = _background.rect();
	QPainter painter(this);

	drawPolyContour();
	curRect.setSize(curRect.size() * 10);
	//QImage scaledbackground = _background;
	//scaledbackground = scaledbackground.scaled(_background.width() * _scale, _background.height() * _scale, Qt::KeepAspectRatio, Qt::FastTransformation);

	QImage cuttedbackground = _background.copy(-_leftTop.x() / _scale, -_leftTop.y() / _scale, _background.width() / _scale, _background.height() / _scale);
	cuttedbackground = cuttedbackground.scaled(cuttedbackground.width() * _scale, cuttedbackground.height() * _scale, Qt::KeepAspectRatio, Qt::FastTransformation);
	painter.drawImage(rect.topLeft(), cuttedbackground, curRect);

	QImage cuttedPolyContour = _polyContour.copy(-_leftTop.x() / _scale, -_leftTop.y() / _scale, _background.width() / _scale, _background.height() / _scale);
	cuttedPolyContour = cuttedPolyContour.scaled(cuttedPolyContour.width() * _scale, cuttedPolyContour.height() * _scale, Qt::KeepAspectRatio, Qt::FastTransformation);
	painter.drawImage(rect.topLeft(), cuttedPolyContour, curRect);

	//drawColorPalette();
	//painter.drawImage(rect.topLeft(), _front, curRect);

	//painter.drawImage(rect.topLeft(), _border, curRect);
	/*
 * QImage scalecanvas = _canvas;
scalecanvas = scalecanvas.scaled(_canvas.width() * _scale, _canvas.height() * _scale, Qt::KeepAspectRatio, Qt::FastTransformation);
QImage scalefront = _front;
scalefront = scalefront.scaled(_front.width() * _scale, _front.height() * _scale, Qt::KeepAspectRatio, Qt::FastTransformation);*/


//painter.drawImage(rect.topLeft(), _outPut, curRect);
//painter.setCompositionMode(QPainter::CompositionMode_Source);
//
//if (!_penDown)
//	countPoly(&_background);

// painter.drawImage(rect.topLeft(), _poly, curRect);
// painter.drawImage(_leftTop, scalecanvas, curRect);
 //
}


void ScribbleArea::resizeEvent(QResizeEvent* event)
{
	_background = _background.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	_canvas = _canvas.scaled(width(), height(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	if (width() > _canvas.width() || height() > _canvas.height()) {
		int newWidth = qMax(width() + SIZE_INCREMENT, _canvas.width());
		int newHeight = qMax(height() + SIZE_INCREMENT, _canvas.height());
		resizeImage(&_canvas, QSize(newWidth, newHeight));
		update();
	}
	QWidget::resizeEvent(event);
}

void ScribbleArea::touchedTwo(const QList<QTouchEvent::TouchPoint>& touchPoints)
{
	QPointF fromPoint1 = touchPoints.at(0).startPos();
	QPointF toPoint1 = touchPoints.at(0).lastPos();
	QPointF fromPoint2 = touchPoints.at(1).startPos();
	QPointF toPoint2 = touchPoints.at(1).lastPos();

	QPointF vector1 = toPoint1 - fromPoint1;
	QPointF vector2 = toPoint2 - fromPoint2;
	qreal nsin = vector1.x() * vector2.y() - vector2.x() * vector1.y();
	qreal ncos = vector1.x() * vector2.x() + vector1.y() * vector2.y();
	qreal degree = atan2(nsin, ncos) * 180 / M_PI;


	if ((degree <= 180 && degree >= 130) || (degree >= -180 && degree <= -130))
	{
		qreal scaleFactor = QLineF(fromPoint1, fromPoint2).length()  / QLineF(toPoint1, toPoint2).length();

		_scale = _originScale * 1 / scaleFactor;
		QPointF midPoint = (toPoint1 + toPoint2) / 2.0;
		QPointF position = (midPoint - _originLeftTop) / _originScale;
		_leftTop = _originLeftTop + position * (_originScale - _scale);

		qDebug() << "scale: " << _scale;
		qDebug() << "originscale: " << _originScale;
		qDebug() << "leftTop: " << _leftTop;
		qDebug() << "midPoint: " << midPoint;
		qDebug() << "position: " << position;

	}
	else
		if (degree <= 20 && degree >= -20)
		{
			_leftTop = _originLeftTop + (toPoint1 - fromPoint1);
		}

	colorSelected = false;
}

void ScribbleArea::touchedOne(const QList<QTouchEvent::TouchPoint>& touchPoints)
{
	const QTouchEvent::TouchPoint& touchPoint = touchPoints.at(0);
	_originScale = _scale;
	_originLeftTop = _leftTop;

	QPointF tPoi = touchPoint.pos();
	QPointF curPoi = (tPoi - _leftTop) / _scale;
	_displayContourId = -1;
	double miniDis = 10000.0;

	for (size_t i = 0; i < _PolyContours.size(); i++)
		if (_PolyContours[i].InContour(QPoint(curPoi.x(), curPoi.y())))
		{
			if (_PolyContours[i].DisContour(QPoint(curPoi.x(), curPoi.y())) < miniDis)
			{
				_displayContourId = i;
				miniDis = _PolyContours[i].DisContour(QPoint(curPoi.x(), curPoi.y()));
				qDebug() << "_displayContourId :" << i;
			}
			//break;
		}

	QSizeF diams = touchPoint.ellipseDiameters();
	QRectF rect(QPointF(), diams);
	rect.moveCenter(touchPoint.pos());
	bool containFlag = false;
	for (int i = 0; i < grayRect.size(); i++)
	{
		QRectF curRect = grayRect.at(i);
		if (curRect.contains(rect))
		{
			colorSelected = true;
			containFlag = true;
			_color = grayDegree.at(i);
			colorSel = i;
		}
	}
	if (!containFlag)
	{
		colorSel = -1;
		colorSelected = false;
	}
}
bool ScribbleArea::event(QEvent* event)
{
	/*if (event->type() == QEvent::TabletEnterProximity ||
		event->type() == QEvent::TabletLeaveProximity) {
		updateCursor(static_cast<QTabletEvent *>(event));
		return true;
	}*/
	//updateCursor(static_cast<QTabletEvent *>(event));

	switch (event->type())
	{
	case QEvent::TouchBegin:
	{
		qDebug() << "TouchBegin";
		touched = true;
		touchupdated = false;
		touchTimer->start();
		const QTouchEvent* touch = dynamic_cast<QTouchEvent*>(event);
		const QList<QTouchEvent::TouchPoint> touchPoints = dynamic_cast<QTouchEvent*>(event)->touchPoints();
		preTouchPoints = touchPoints;

		_touchNum = touchPoints.count();

		break;
	}
	case QEvent::TouchUpdate:
	{
		qDebug() << "TouchUpdate";
		touchupdated = true;
		const QTouchEvent* touch = dynamic_cast<QTouchEvent*>(event);
		const QList<QTouchEvent::TouchPoint> touchPoints = dynamic_cast<QTouchEvent*>(event)->touchPoints();
		preTouchPoints = touchPoints;
		if (touchTimer->isActive() && _touchNum!=touchPoints.count())
		{
			_touchNum = touchPoints.count();
			touchTimer->start();
			break;
		}
		_touchNum = touchPoints.count();

		switch (touchPoints.count())
		{
		case 1:
			touchedOne(touchPoints);
			break;
		case 2:
			touchedTwo(touchPoints);
			break;
		default:
			break;
		}
		update();
		break;
	}

	case QEvent::TouchEnd:
	{
		qDebug() << "TouchEnd";
		qDebug() << "Remain Time:" << touchTimer->remainingTime();
		qDebug() << touchupdated << " " << touchTimer->isActive();
		if (!touchupdated)
		{
			qDebug() << "!!!!";
			switch (preTouchPoints.count())
			{
			case 1:
				touchedOne(preTouchPoints);
				break;
			case 2:
				touchedTwo(preTouchPoints);
				break;
			default:
				break;
			}
			update();
		}
		touched = false;
		touchupdated = false;
		_originScale = _scale;
		_originLeftTop = _leftTop;
		break;
	}
	default:
		return QWidget::event(event);
	}

	return true;
}

void ScribbleArea::resizeImage(QImage* image, const QSize& newSize)
{
	if (image->size() == newSize)
		return;
	QImage newImage(newSize, QImage::Format_ARGB32);
	newImage.fill(qRgba(254, 254, 254, 0));
	QPainter painter(&newImage);
	painter.drawImage(QPoint(0, 0), *image);
	*image = newImage;
}

static qreal penToWidth(qreal pressure, int xtilt, int ytilt)
{
	qreal tiltScale = (qAbs(ytilt * ytilt)) / 3600.0 + 0.5;
	// qDebug() << pressure * tiltScale * 50 + 1;
	return pressure * tiltScale * 50 + 1;
}

void ScribbleArea::updateBrush(const QTabletEvent* event)
{
	int xtiltValue = int(((event->xTilt() + 60.0) / 120.0) * 255);
	int ytiltValue = int(((event->yTilt() + 60.0) / 120.0) * 255);

	_pen.setCapStyle(Qt::RoundCap);
	_pen.setJoinStyle(Qt::RoundJoin);

	_pen.setWidth(penToWidth(event->pressure(), event->xTilt(), event->yTilt()));

	if (event->pointerType() == QTabletEvent::Eraser)
	{
		_eraseMode = true;


		_pen.setColor(QColor(254, 254, 254, 0));
		_brush.setColor(qRgba(254, 254, 254, 0));
	}
	else
	{
		updateCursor(event, penToWidth(event->pressure(), event->xTilt(), event->yTilt()));
		//qDebug() << event->pos();
		_eraseMode = false;
		QPoint curPixel = ((event->posF() - _leftTop) / _scale).toPoint();
		if (colorSelected)
			_pen.setColor(_color);
		else
			_pen.setColor(_background.pixel(curPixel));
		_brush.setColor(_color);
	}

}

void ScribbleArea::updateCanvas(QTabletEvent* event)
{
	QPainter painter(&_background);

	painter.setRenderHint(QPainter::Antialiasing);
	if (_eraseMode)
		painter.setCompositionMode(QPainter::CompositionMode_Clear);
	else
		if (!_eraseMode)
			painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	painter.setPen(_pen);
	painter.drawLine((lastPoint.pos - _leftTop) / _scale, (event->posF() - _leftTop) / _scale);
	//  fillTinyGap(&_canvas);
}

void ScribbleArea::updateCursor(const QTabletEvent* event, const qreal width)
{
	QPixmap pixmap(QSize(200, 200));
	pixmap.fill(Qt::transparent);
	auto pixmapCenter = QPoint(pixmap.width() / 2, pixmap.height() / 2);
	QPainter painter(&pixmap);
	painter.setBrush(_color);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen("white");
	auto radius = (width + painter.pen().width()) * _scale / 2;
	painter.drawEllipse(pixmapCenter, (int)radius, (int)radius);

	qDebug() << pixmap;

	QCursor cursor(pixmap, -1, -1);

	//setCursor(Qt::PointingHandCursor);
	this->setCursor(cursor);
}

void ScribbleArea::fillTinyGap(QImage* image)
{
	QVector<QVector<int>> V;
}

void ScribbleArea::selectBoundary(QImage* image, QImage* front)
{
	const int Dx[] = { 1,-1,0,0 };
	const int Dy[] = { 0,0,1,-1 };
	bool** V = new bool* [image->width()];
	for (int i = 0; i < image->width(); i++)
		V[i] = new bool[image->height()];
	for (int i = 0; i < image->width(); i++)
		for (int j = 0; j < image->height(); j++)
			V[i][j] = false;

	int cnt = 0;
	V[0][0] = true;
	QQueue<QPair<int, int> > Q;
	Q.push_back(qMakePair(0, 0));

	while (!Q.empty())
	{
		QPair<int, int> T = Q.front();
		Q.pop_front();
		int curx = T.first;
		int cury = T.second;
		for (int i = 0; i < 4; i++)
		{
			int nextx = curx + Dx[i];
			int nexty = cury + Dy[i];
			if (nextx < 0 || nexty < 0 || nextx >= image->width() || nexty >= image->height())
				continue;
			if (V[nextx][nexty])
				continue;
			if (image->pixelColor(nextx, nexty) != image->pixelColor(curx, cury))
				continue;
			V[nextx][nexty] = true;
			Q.push_back(qMakePair(nextx, nexty));
		}
	}

	QColor Cur = qRgb(254, 254, 254);
	for (int i = 0; i < image->width(); i++)
		for (int j = 0; j < image->height(); j++)
			if (V[i][j])
				image->setPixelColor(i, j, Cur);

	for (int i = 0; i < image->width(); i++)
		delete[]V[i];
	delete[]V;
}

void ScribbleArea::countGray(QImage* image)
{
	QMap<QRgb, long long> C;
	C.clear();
	qDebug() << "count" << C.size();
	for (int i = 0; i < image->width(); i++)
		for (int j = 0; j < image->height(); j++)
			C[image->pixel(i, j)] += 1;
	qDebug() << "count" << C.size();
	long long total = image->width() * image->height();

	grayDegree.clear();
	for (QMap<QRgb, long long>::iterator iter = C.begin(); iter != C.end(); iter++)
	{
		if (iter.key() == qRgb(254, 254, 254))
			continue;
		if (iter.value() > (total / 200))
		{
			grayDegree.push_back(iter.key());
		}
		//qDebug() << " value : " << iter.value();
	//qDebug() <<"red: " << qRed(iter.key()) << "green: " << qGreen(iter.key()) << " value: " << iter.value();
	}
	constructGrayRect();
	/*
	for (int i=0;i<image->width();i++)
		for (int j=0;j<image->height();j++)
			if (C[image->pixel(i,j)]>5000)
				image->setPixelColor(i,j, qRgba(254, 254, 254, 0));*/
}

void ScribbleArea::countPoly(QImage* image)
{
	_PolyContours.clear();

	cv::Mat srcImage, grayImage, dstImage, ployImage;
	srcImage = QImage2cvMat(*image);

	//cv::imshow("srcImage", srcImage);
	dstImage = cv::Mat(srcImage.size(), CV_8UC4);
	ployImage = cv::Mat(srcImage.size(), CV_8UC4, cv::Scalar(255, 255, 255, 0));
	//cv::Mat alphaImage = cv::Mat(srcImage.size(), srcImage.depth(), cv::Scalar(0));
	//ployImage.
	std::vector<cv::Scalar> CurColor;
	CurColor.clear();
	CurColor.push_back(cv::Scalar(rand() % 255, rand() % 255, rand() % 255, 255));
	CurColor.push_back(cv::Scalar(255, 0, 0, 255));
	CurColor.push_back(cv::Scalar(0, 255, 0, 255));
	CurColor.push_back(cv::Scalar(0, 0, 255, 255));
	CurColor.push_back(cv::Scalar(rand() % 255, rand() % 255, rand() % 255, 255));

	for (int i = 0; i < grayDegree.size(); i++)
	{

		std::vector<std::vector<cv::Point> > contours;
		std::vector<std::vector<cv::Point> > fitcontours;
		std::vector<cv::Vec4i> hierarchy;
		contours.clear();
		fitcontours.clear();
		hierarchy.clear();

		grayImage = srcImage.clone();
		cv::Scalar color = CurColor[i];
		cv::cvtColor(srcImage, grayImage, cv::COLOR_BGRA2GRAY);
		qDebug() << "color: " << qRed(grayDegree[i]);

		grayImage = grayImage == qRed(grayDegree[i]);

		findContours(grayImage, contours, hierarchy, cv::RETR_TREE, cv::CHAIN_APPROX_NONE);



		for (int j = 0; j < hierarchy.size(); j++)
		{
			//cv::Scalar color = cv::Scalar(rand() % 255, rand() % 255, rand() % 255);
			if (fabs(cv::contourArea(contours[j])) < 200)
				continue;
			fitcontours.push_back(contours[j]);
			//drawContours(dstImage, contours, i, color, cv::FILLED, 8, hierarchy);
		}

		std::vector<cv::Point> contours_ploy;
		std::vector<std::vector<cv::Point>> ployContours;
		contours_ploy.clear();
		ployContours.clear();
		for (size_t j = 0; j < fitcontours.size(); j++)
		{
			cv::approxPolyDP(fitcontours[j], contours_ploy, 10, true);
			if (contours_ploy.size() >= 3)
				ployContours.push_back(contours_ploy);
		}
		for (size_t j = 0; j < ployContours.size(); j++)
		{

			qDebug() << "size: " << ployContours[j].size();
			//drawContours(ployImage, ployContours, i, color, 1, 8, std::vector<cv::Vec4i>(), 0, cv::Point());
			_PolyContours.push_back(MovablePolyContour(ployContours[j], i));
		}
	}

	//cv::imshow("原始图", srcImage);
	//cv::imshow("轮廓图", dstImage);
	//cv::imshow("最终图", ployImage);
	_poly = cvMat2QImage(ployImage);
	//cv::waitKey(0);
}

void ScribbleArea::constructGrayRect()
{
	int width = this->width();
	int height = this->height();
	grayRect.clear();
	int x = width / 8;
	int y = height / 8;
	int palettewidth = x;
	int paletteheight = y;
	for (int i = 0; i < grayDegree.size(); i++)
	{
		grayRect.push_back(QRect(x, y + i * (paletteheight + 10), palettewidth, paletteheight));
	}
}
void ScribbleArea::blurImage()
{

	cv::Mat src, dst;
	src = QImage2cvMat(_background);
	medianBlur(src, dst, 5);
	_background = cvMat2QImage(dst);
	//imshow("origin", src);
	//imshow("blured", dst);
	//cv::waitKey(0);

}
bool ScribbleArea::checkInPolyContour()
{
	for (size_t i = 0; i < _PolyContours.size(); i++)
		if (i == _displayContourId)
		{
			QPointF lastF = (QPoint(lastPoint.pos.x(), lastPoint.pos.y()) - _leftTop) / _scale;
			QPoint last(lastF.x(), lastF.y());
			_contourI = _PolyContours[i].SelContour(last);
			if (_contourI != -1)
			{
				std::pair<QPointF, QPointF> cur = _PolyContours[i].FindPointBrother(_contourI);
				_Brother1 = cur.first;
				_Brother2 = cur.second;
				_choosePoly = i;
				_curGrayDegree = grayDegree[_PolyContours[i].GetGrayDegree()];
				return true;
			}
		}
	return false;
}

void ScribbleArea::updatePoly()
{
	if (0)
	{
		QPainter painter(&_background);
		QPointF scaledBeginSelPoint = (beginSelPoint - _leftTop) / _scale;
		QPointF scaledEndSelPoint = (endSelPoint - _leftTop) / _scale;
		QPolygon plyBegin, plyEnd;
		plyBegin << _Brother1.toPoint() << _Brother2.toPoint() << scaledBeginSelPoint.toPoint();
		plyEnd << _Brother1.toPoint() << _Brother2.toPoint() << scaledEndSelPoint.toPoint();

		QBrush brush(Qt::SolidPattern);
		QPen pen(_curGrayDegree);
		brush.setColor(_curGrayDegree);
		painter.setPen(pen);
		painter.setBrush(brush);
		painter.drawPolygon(plyEnd);

		/*
		brush.setColor(_background.pixelColor(scaledEndSelPoint.toPoint()));
		painter.setBrush(brush);
		painter.drawPolygon(plyEnd);
		*/


		return;
	}

	// Read input image and convert to float
	// 读取图像，并将图像转换为float

	cv::Mat imgIn = QImage2cvMat(_background);

	//cv::Mat imgIn2 = cv::imread("C:\\Users\\yyjxx\\Desktop\\robot.png");

	//qDebug() << imgIn.type() << " " << imgIn2.type();

	imgIn.convertTo(imgIn, CV_8UC3);

	cv::cvtColor(imgIn, imgIn, CV_BGRA2BGR);
	qDebug() << imgIn.type() << " " << imgIn.depth();
	//imshow("Input", imgIn);
	//cv::waitKey(0);

	imgIn.convertTo(imgIn, CV_32FC3, 1 / 255.0);

	// Output image is set to white
	cv::Mat imgOut = cv::Mat::ones(imgIn.size(), imgIn.type());
	//设定输出，输出为纯白色图像
	imgOut = cv::Scalar(1.0, 1.0, 1.0);

	// Input triangle 输入三角形坐标点
	std::vector<cv::Point2f> triIn;
	triIn.push_back(cv::Point2f(_Brother1.x(), _Brother1.y()));
	triIn.push_back(cv::Point2f(_Brother2.x(), _Brother2.y()));
	QPointF scaledBeginSelPoint = (beginSelPoint - _leftTop) / _scale;
	triIn.push_back(cv::Point2f(scaledBeginSelPoint.x(), scaledBeginSelPoint.y()));

	// Output triangle 输出三角形坐标点
	std::vector<cv::Point2f> triOut;
	triOut.push_back(cv::Point2f(_Brother1.x(), _Brother1.y()));
	triOut.push_back(cv::Point2f(_Brother2.x(), _Brother2.y()));
	QPointF scaledEndSelPoint = (endSelPoint - _leftTop) / _scale;
	triOut.push_back(cv::Point2f(scaledEndSelPoint.x(), scaledEndSelPoint.y()));


	// Warp all pixels inside input triangle to output triangle 仿射变换
	warpTriangle(imgIn, imgOut, triIn, triOut);

	// Draw triangle on the input and output image.

	// Convert back to uint because OpenCV antialiasing
	// does not work on image of type CV_32FC3

	//保存为INT型
	imgIn.convertTo(imgIn, CV_8UC3, 255.0);
	imgOut.convertTo(imgOut, CV_8UC3, 255.0);
	imgOut.convertTo(imgOut, CV_8UC4);
	cv::cvtColor(imgOut, imgOut, CV_BGR2BGRA);
	_outPut = cvMat2QImage(imgOut);
	QImage mask = _outPut.createMaskFromColor(_outPut.pixel(0, 0), Qt::MaskOutColor);
	_outPut.setAlphaChannel(mask);


	//QPainter painter(&_background);
	//painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
	//painter.drawImage(QRect(), _outPut);
	//imshow("Output", imgOut);
	//cv::waitKey(0);
}

void ScribbleArea::drawColorPalette()
{
	QPainter painter(&_front);
	for (int i = 0; i < grayRect.size(); i++)
	{
		if (colorSel == i)
		{
			QPen PenColor(QColor(200, 160, 230), 5);
			painter.setPen(PenColor);
		}
		else
		{
			QPen PenColor(QColor(0, 160, 230), 5);
			painter.setPen(PenColor);
		}
		QColor brushColor(grayDegree.at(i));
		Qt::BrushStyle brushStyle(Qt::SolidPattern);
		QBrush brush(brushColor, brushStyle);
		painter.setBrush(brush);
		painter.drawRect(grayRect.at(i));
	}

}

QColor ScribbleArea::grayDegreeToColor(int grayDegree)
{
	return colorList[grayDegree];
}


void ScribbleArea::drawPolyContour()
{
	_polyContour.fill(qRgba(254, 254, 254, 0));
	QPainter painter(&_polyContour);
	for (size_t i = 0; i < _PolyContours.size(); i++)
		if (i == _displayContourId)
		{
			QPen pen;
			pen.setColor(POLYPOINTCOLOR);
			pen.setWidth(POLYPOINTWIDTH);
			painter.setPen(pen);
			for (int j = 0; j < _PolyContours[i].Poly.size(); j++)
				painter.drawPoint(_PolyContours[i].Poly[j]);

			qDebug() << grayDegreeToColor(_PolyContours[i].GetGrayDegree());
			pen.setWidth(POLYLINEWIDTH);
			pen.setColor(grayDegreeToColor(_PolyContours[i].GetGrayDegree()));
			painter.setPen(pen);

			QPolygon ply;
			for (int j = 0; j < _PolyContours[i].Poly.size(); j++)
				ply << _PolyContours[i].Poly[j];
			painter.drawConvexPolygon(ply);
		}
}


