#pragma once
#include <vector>
#include "CGL/CGL.h"

using namespace std;
using namespace CGL;

namespace Balle
{

    struct Triangle
    {
    public:
        Vector3D a, b, c;

        Triangle(Vector3D &a, Vector3D &b, Vector3D &c) : a(a), b(b), c(c){};
        Triangle() : a(Vector3D()), b(Vector3D()), c(Vector3D()){};
        ~Triangle() = default;
    };

    struct Quadrangle
    {
    public:
        Vector3D a, b, c, d;

        Quadrangle(Vector3D &a, Vector3D &b, Vector3D &c, Vector3D &d) : a(a), b(b), c(c), d(d) {}
        Quadrangle() : a(Vector3D()), b(Vector3D()), c(Vector3D()), d(Vector3D()) {}
        ~Quadrangle() = default;
    };

    struct Limb
    {
    public:
        size_t n_layer;
        vector<Quadrangle> quadrangles;
        vector<Vector3D> points;

        Limb() : n_layer(0U){};
        ~Limb() = default;

        size_t add_layer(Vector3D &a, Vector3D &b, Vector3D &c, Vector3D &d)
        {
            if (n_layer > 0)
            {
                Vector3D pa = points[(n_layer - 1) * 4];
                Vector3D pb = points[(n_layer - 1) * 4 + 1];
                Vector3D pc = points[(n_layer - 1) * 4 + 2];
                Vector3D pd = points[(n_layer - 1) * 4 + 3];

                /*quadrangles.push_back({ pb, pa, a, b });
                quadrangles.push_back({ pc, pb, b, c });
                quadrangles.push_back({ pd, pc, c, d });
                quadrangles.push_back({ pa, pd, d, a });*/

                quadrangles.push_back({ pa, pb, b, a });
                quadrangles.push_back({ pb, pc, c, b });
                quadrangles.push_back({ pc, pd, d, c });
                quadrangles.push_back({ pd, pa, a, d });
            }
            points.emplace_back(a);
            points.emplace_back(b);
            points.emplace_back(c);
            points.emplace_back(d);
            n_layer++;
            return n_layer;
        }

        void seal()
        {
            Vector3D pa = points[(n_layer - 1) * 4];
            Vector3D pb = points[(n_layer - 1) * 4 + 1];
            Vector3D pc = points[(n_layer - 1) * 4 + 2];
            Vector3D pd = points[(n_layer - 1) * 4 + 3];
            quadrangles.push_back({pa, pb, pc, pd});
        }

        vector<Vector3D> get_last_four_points()
        {
            if (n_layer == 0)
            {
                throw runtime_error("Trying to get points in a Limb with zero layer.");
            }
            return {points[(n_layer - 1) * 4], points[(n_layer - 1) * 4 + 1], points[(n_layer - 1) * 4 + 2], points[(n_layer - 1) * 4 + 3]};
        }

        vector<Vector3D> get_first_four_points()
        {
            if (n_layer == 0)
            {
                throw runtime_error("Trying to get points in a Limb with zero layer.");
            }
            return {points[0], points[1], points[2], points[3]};
        }
    };

};