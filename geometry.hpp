#pragma once
#include <iostream>

struct point {
	long double lat;
	long double lon;
};


bool pnpoly(int nvert, float* vertx, float* verty, float testx, float testy)
{
	int i, j, c = 0;
	for (i = 0, j = nvert - 1; i < nvert; j = i++) {
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
			c = !c;
	}
	return c;
}


struct extent_poly {
	std::vector<point> poly;

	bool within(point& test)
	{
		int i, j;
		bool c = false;
		int nvert = poly.size();
		for (i = 0, j = nvert - 1; i < nvert; j = i++) {
			if (((poly[i].lat > test.lat) != (poly[j].lat > test.lat)) &&
				(test.lon < (poly[j].lon - poly[i].lon) *
					(test.lat - poly[i].lat) / (poly[j].lat - poly[i].lat) + poly[i].lon))
				c = !c;
		}
		return c;
	}
};



void test_pnpoly()
{/*
	float vx[] = { 0, 0, 100, 100 };
	float vy[] = { 0, 100, 100, 0 };
	std::vector<point>poly = { {0,0},{0,100},{100,100},{100,0} };

	int nvert = 4;

	float tx,ty;
	point p{};


	tx = 50;
	ty = 50;
	p = { tx, ty };
	std::cout << "\n" << tx << ", " << ty << " " << (pnpoly(nvert, vx, vy, tx, ty) == pnpoly(poly, p) ? "agree" : "disagree");
	*/
}
