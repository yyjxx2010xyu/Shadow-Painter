#pragma once
#include <iostream>
#include <QPoint>
#include <vector>
#include <cassert>
#include <opencv2/opencv.hpp>

class MovablePolyContour
{
private:
	double SelDis;
	int GrayDegree;
	double CalcDis(const QPoint& A, const QPoint& B);
public:
	std::vector<QPoint> Poly;
	std::vector<cv::Point> cvPoly;

	MovablePolyContour(const std::vector<cv::Point>& CVPoly, int Degree, double Dis = 20.0);
	int SelContour(const QPoint& curPoint);
	std::pair<QPoint, QPoint> FindPointBrother(int pos);
	void ChangePoint(int I, QPoint Pos);
	int GetGrayDegree();
	bool InContour(const QPoint& curPoint);
	double DisContour(const  QPoint& curPoint);
};

