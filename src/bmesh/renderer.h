#pragma once

#include <vector>
#include <nanogui/nanogui.h>
#include "../misc/sphere_drawing.h"
#include "CGL/CGL.h"
#include "../mesh/halfEdgeMesh.h"
#include "skeletalNode.h"
#include "primitives.h"

using namespace std;
using namespace nanogui;
using namespace CGL;
namespace Balle
{
    // Rendering Class
    struct Renderer
    {
    public:
        void draw_skeleton(GLShader &shader, SkeletalNode *root);
        void draw_polygon_faces(GLShader &shader, vector<Quadrangle> &quadrangles, vector<Triangle> &triangles, vector<vector<size_t>> &polygons, vector<Vector3D> &vertices);
        void draw_mesh_faces(GLShader &shader, HalfedgeMesh *mesh);
        void draw_polygon_wireframe(GLShader &shader, vector<Quadrangle> &quadrangles, vector<Triangle> &triangles, SkeletalNode *root, vector<Vector3D> &vertices);
        void draw_mesh_wireframe(GLShader &shader, HalfedgeMesh *mesh, SkeletalNode *root);

    private:
        void __fill_position(MatrixXf &positions, SkeletalNode *root, int &si);
        void __draw_skeletal_spheres(GLShader &shader, Misc::SphereMesh &msm, SkeletalNode *root);
        void __draw_mesh_vertices(GLShader &shader, Misc::SphereMesh &msm, vector<Vector3D> &vertices);
        int __get_num_bones(SkeletalNode *root);
    };

}; // END_NAMESPACE BALLE