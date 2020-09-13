#include "MovablePolyContour.h"

double MovablePolyContour::CalcDis(const QPoint& A, const QPoint& B)
{
	return  std::sqrt(std::pow(A.x() - B.x(), 2) + std::pow(A.y() - B.y(), 2));
}

MovablePolyContour::MovablePolyContour(const std::vector<cv::Point>& curPoly, int Degree, double Dis)
{
	Poly.clear();
	for (int i = 0; i < curPoly.size(); i++)
		Poly.push_back(QPoint(curPoly[i].x, curPoly[i].y));
	cvPoly.assign(curPoly.begin(), curPoly.end());
	GrayDegree = Degree;
	SelDis = Dis;
}

/*
Brute Force
TODO: Optimize, Binary Search
*/
int MovablePolyContour::SelContour(const QPoint& curPoint)
{
	for (size_t i = 0; i < Poly.size(); i++)
		if (CalcDis(curPoint, Poly[i]) < SelDis)
			return i;
	return -1;
}

std::pair<QPoint, QPoint> MovablePolyContour::FindPointBrother(int pos)
{
	if (pos == 0)
		return std::make_pair(Poly[Poly.size() - 1], Poly[1]);
	if (pos == Poly.size() - 1)
		return std::make_pair(Poly[Poly.size() - 2], Poly[0]);
	return  std::make_pair(Poly[pos - 1], Poly[pos + 1]);
}

void MovablePolyContour::ChangePoint(int I, QPoint Pos)
{
	Poly[I] = Pos;
	cvPoly[I] = cv::Point(Pos.x(), Pos.y());
}

int MovablePolyContour::GetGrayDegree()
{
	return GrayDegree;
}

bool MovablePolyContour::InContour(const QPoint& curPoint)
{
	return pointPolygonTest(cvPoly, cv::Point(curPoint.x(), curPoint.y()), false) == 1;
}

double MovablePolyContour::DisContour(const QPoint& curPoint)
{
	return pointPolygonTest(cvPoly, cv::Point(curPoint.x(), curPoint.y()), true);
}



