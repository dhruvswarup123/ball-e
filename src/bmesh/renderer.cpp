#include "renderer.h"
namespace Balle
{
    void Renderer::draw_skeleton(GLShader &shader, SkeletalNode *root)
    {
        // enable sphere rendering
        int numlinks = __get_num_bones(root);

        MatrixXf positions(4, numlinks * 2);
        MatrixXf normals(4, numlinks * 2);

        for (int i = 0; i < numlinks; i++)
        {
            normals.col(i * 2) << 0., 0., 0., 0.0;
            normals.col(i * 2 + 1) << 0., 0., 0., 0.0;
        }

        int si = 0;
        __fill_position(positions, root, si);

        shader.setUniform("u_balls", true, false);
        shader.uploadAttrib("in_position", positions, false);
        shader.uploadAttrib("in_normal", normals, false);

        shader.drawArray(GL_LINES, 0, numlinks * 2);

        Misc::SphereMesh msm;
        __draw_skeletal_spheres(shader, msm, root);
    }

    void Renderer::draw_polygon_faces(GLShader &shader, vector<Quadrangle> &quadrangles, vector<Triangle> &triangles, vector<vector<size_t>> &polygons, vector<Vector3D> &vertices)
    {
        MatrixXf mesh_positions(3, triangles.size() * 3 + quadrangles.size() * 6);
        MatrixXf mesh_normals(3, triangles.size() * 3 + quadrangles.size() * 6);

        int ind = 0;
        for (const vector<size_t> &polygon : polygons)
        {
            if (polygon.size() == 3)
            {
                // continue;
                Vector3D vertex0 = vertices[polygon[0]];
                Vector3D vertex1 = vertices[polygon[1]];
                Vector3D vertex2 = vertices[polygon[2]];

                Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex0)).unit();

                mesh_positions.col(ind * 3) << vertex0.x, vertex0.y, vertex0.z;
                mesh_positions.col(ind * 3 + 1) << vertex1.x, vertex1.y, vertex1.z;
                mesh_positions.col(ind * 3 + 2) << vertex2.x, vertex2.y, vertex2.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;
            }
            else if (polygon.size() == 4)
            {
                // continue;
                Vector3D vertex0 = vertices[polygon[0]];
                Vector3D vertex1 = vertices[polygon[1]];
                Vector3D vertex2 = vertices[polygon[2]];
                Vector3D vertex3 = vertices[polygon[3]];
                Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex3) + cross(vertex3, vertex0)).unit();

                mesh_positions.col(ind * 3) << vertex0.x, vertex0.y, vertex0.z;
                mesh_positions.col(ind * 3 + 1) << vertex1.x, vertex1.y, vertex1.z;
                mesh_positions.col(ind * 3 + 2) << vertex2.x, vertex2.y, vertex2.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;

                mesh_positions.col(ind * 3) << vertex2.x, vertex2.y, vertex2.z;
                mesh_positions.col(ind * 3 + 1) << vertex3.x, vertex3.y, vertex3.z;
                mesh_positions.col(ind * 3 + 2) << vertex0.x, vertex0.y, vertex0.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;
            }
        }
        size_t actual_triangles_to_draw = ind * 3;
        ind = 0;

        shader.setUniform("u_balls", false, false);
        shader.uploadAttrib("in_normal", mesh_normals);
        shader.uploadAttrib("in_position", mesh_positions);
        shader.drawArray(GL_TRIANGLES, 0, actual_triangles_to_draw);
    }

    void Renderer::draw_mesh_faces(GLShader &shader, HalfedgeMesh *mesh)
    {
        MatrixXf mesh_positions(3, mesh->nFaces() * 6);
        MatrixXf mesh_normals(3, mesh->nFaces() * 6);

        int ind = 0;
        for (auto face = mesh->facesBegin(); face != mesh->facesEnd(); face++)
        {
            Vector3D normal = face->normal();

            int deg = face->degree();
            if (deg == 3)
            {
                Vector3D v0 = face->halfedge()->vertex()->position;
                Vector3D v1 = face->halfedge()->next()->vertex()->position;
                Vector3D v2 = face->halfedge()->next()->next()->vertex()->position;

                mesh_positions.col(ind * 3) << v0.x, v0.y, v0.z;
                mesh_positions.col(ind * 3 + 1) << v1.x, v1.y, v1.z;
                mesh_positions.col(ind * 3 + 2) << v2.x, v2.y, v2.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;
            }
            else if (deg == 4)
            {
                Vector3D v0 = face->halfedge()->vertex()->position;
                Vector3D v1 = face->halfedge()->next()->vertex()->position;
                Vector3D v2 = face->halfedge()->next()->next()->vertex()->position;
                Vector3D v3 = face->halfedge()->next()->next()->next()->vertex()->position;

                mesh_positions.col(ind * 3) << v0.x, v0.y, v0.z;
                mesh_positions.col(ind * 3 + 1) << v1.x, v1.y, v1.z;
                mesh_positions.col(ind * 3 + 2) << v2.x, v2.y, v2.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;

                mesh_positions.col(ind * 3) << v2.x, v2.y, v2.z;
                mesh_positions.col(ind * 3 + 1) << v3.x, v3.y, v3.z;
                mesh_positions.col(ind * 3 + 2) << v0.x, v0.y, v0.z;

                mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
                mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

                ind += 1;
            }
        }

        shader.setUniform("u_balls", false, false);
        shader.uploadAttrib("in_normal", mesh_normals);
        shader.uploadAttrib("in_position", mesh_positions);
        shader.drawArray(GL_TRIANGLES, 0, ind * 3);
    }

    void Renderer::draw_polygon_wireframe(GLShader &shader, vector<Quadrangle> &quadrangles, vector<Triangle> &triangles, SkeletalNode *root, vector<Vector3D> &vertices)
    {
        MatrixXf mesh_positions(3, triangles.size() * 6 + quadrangles.size() * 8);
        MatrixXf mesh_normals(3, triangles.size() * 6 + quadrangles.size() * 8);

        int ind = 0;
        for (const Balle::Triangle &triangle : triangles)
        {
            Vector3D vertex1 = triangle.a;
            Vector3D vertex2 = triangle.b;
            Vector3D vertex3 = triangle.c;

            mesh_positions.col(ind) << vertex1.x, vertex1.y, vertex1.z;
            mesh_positions.col(ind + 1) << vertex2.x, vertex2.y, vertex2.z;
            mesh_positions.col(ind + 2) << vertex2.x, vertex2.y, vertex2.z;
            mesh_positions.col(ind + 3) << vertex3.x, vertex3.y, vertex3.z;
            mesh_positions.col(ind + 4) << vertex3.x, vertex3.y, vertex3.z;
            mesh_positions.col(ind + 5) << vertex1.x, vertex1.y, vertex1.z;

            mesh_normals.col(ind) << 0., 0., 0.;
            mesh_normals.col(ind + 1) << 0., 0., 0.;
            mesh_normals.col(ind + 2) << 0., 0., 0.;
            mesh_normals.col(ind + 3)
                << 0.,
                0., 0.;
            mesh_normals.col(ind + 4) << 0., 0., 0.;
            mesh_normals.col(ind + 5) << 0., 0., 0.;

            ind += 6;
        }

        for (const Balle::Quadrangle &quadrangle : quadrangles)
        {
            Vector3D vertex1 = quadrangle.a;
            Vector3D vertex2 = quadrangle.b;
            Vector3D vertex3 = quadrangle.c;
            Vector3D vertex4 = quadrangle.d;

            mesh_positions.col(ind) << vertex1.x, vertex1.y, vertex1.z;
            mesh_positions.col(ind + 1) << vertex2.x, vertex2.y, vertex2.z;
            mesh_positions.col(ind + 2) << vertex2.x, vertex2.y, vertex2.z;
            mesh_positions.col(ind + 3) << vertex3.x, vertex3.y, vertex3.z;
            mesh_positions.col(ind + 4) << vertex3.x, vertex3.y, vertex3.z;
            mesh_positions.col(ind + 5) << vertex4.x, vertex4.y, vertex4.z;
            mesh_positions.col(ind + 6) << vertex4.x, vertex4.y, vertex4.z;
            mesh_positions.col(ind + 7) << vertex1.x, vertex1.y, vertex1.z;

            mesh_normals.col(ind) << 0., 0., 0.;
            mesh_normals.col(ind + 1) << 0., 0., 0.;
            mesh_normals.col(ind + 2) << 0., 0., 0.;
            mesh_normals.col(ind + 3) << 0., 0., 0.;
            mesh_normals.col(ind + 4) << 0., 0., 0.;
            mesh_normals.col(ind + 5) << 0., 0., 0.;
            mesh_normals.col(ind + 6) << 0., 0., 0.;
            mesh_normals.col(ind + 7) << 0., 0., 0.;

            ind += 8;
        }

        shader.uploadAttrib("in_position", mesh_positions, false);
        shader.uploadAttrib("in_normal", mesh_normals, false);
        shader.setUniform("u_balls", false, false);
        shader.drawArray(GL_LINES, 0, triangles.size() * 6 + quadrangles.size() * 8);

        int numlinks = __get_num_bones(root);

        MatrixXf positions(4, numlinks * 2);
        MatrixXf normals(4, numlinks * 2);

        for (int i = 0; i < numlinks; i++)
        {
            normals.col(i * 2) << 0., 0., 0., 0.0;
            normals.col(i * 2 + 1) << 0., 0., 0., 0.0;
        }

        int si = 0;
        __fill_position(positions, root, si);

        shader.uploadAttrib("in_position", positions, false);
        shader.uploadAttrib("in_normal", normals, false);

        shader.drawArray(GL_LINES, 0, numlinks * 2);
        Misc::SphereMesh msm;
        __draw_mesh_vertices(shader, msm, vertices);
    }

    void Renderer::draw_mesh_wireframe(GLShader &shader, HalfedgeMesh *mesh, SkeletalNode *root)
    {
        MatrixXf mesh_positions(3, mesh->nEdges() * 2);
        MatrixXf mesh_normals(3, mesh->nEdges() * 2);

        int ind = 0;
        for (EdgeIter i = mesh->edgesBegin(); i != mesh->edgesEnd(); i++)
        {
            // TODO: Fix this for faces
            Vector3D vertex1 = i->halfedge()->vertex()->position;
            Vector3D vertex2 = i->halfedge()->next()->vertex()->position;

            // Vector3D normal = i->face()->normal();

            mesh_positions.col(ind) << vertex1.x, vertex1.y, vertex1.z;
            mesh_positions.col(ind + 1) << vertex2.x, vertex2.y, vertex2.z;

            mesh_normals.col(ind) << 0., 0., 0.;
            mesh_normals.col(ind + 1) << 0., 0., 0.;

            ind += 2;
        }

        shader.uploadAttrib("in_position", mesh_positions, false);
        shader.uploadAttrib("in_normal", mesh_normals, false);
        shader.setUniform("u_balls", false, false);
        shader.drawArray(GL_LINES, 0, mesh->nEdges() * 2);

        int numlinks = __get_num_bones(root);

        MatrixXf positions(4, numlinks * 2);
        MatrixXf normals(4, numlinks * 2);

        for (int i = 0; i < numlinks; i++)
        {
            normals.col(i * 2) << 0., 0., 0., 0.0;
            normals.col(i * 2 + 1) << 0., 0., 0., 0.0;
        }
        int si = 0;
        __fill_position(positions, root, si);

        shader.uploadAttrib("in_position", positions, false);
        shader.uploadAttrib("in_normal", normals, false);

        shader.drawArray(GL_LINES, 0, numlinks * 2);

        Misc::SphereMesh msm;
        __draw_skeletal_spheres(shader, msm, root);
    }

    void Renderer::__draw_skeletal_spheres(GLShader &shader, Misc::SphereMesh &msm, SkeletalNode *root)
    {
        if (root == nullptr)
            return;

        nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);

        if (root->selected && root->interpolated)
        {
            color = nanogui::Color(0.8f, 0.5f, 0.5f, 1.0f);
        }
        else if (root->selected)
        {
            color = nanogui::Color(1.0f, 0.0f, 0.0f, 1.0f);
        }
        else if (root->interpolated)
        {
            color = nanogui::Color(0.6f, 0.6f, 0.6f, 1.0f);
        }

        msm.draw_sphere(shader, root->pos, root->radius, color);

        for (int i = 0; i < root->children->size(); i++)
        {
            SkeletalNode *child = root->children->at(i);
            __draw_skeletal_spheres(shader, msm, child);
        }

        return;
    }

    void Renderer::__draw_mesh_vertices(GLShader &shader, Misc::SphereMesh &msm, vector<Vector3D> &vertices)
    {
        nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);
        for (const Vector3D &vertex : vertices)
        {
            msm.draw_sphere(shader, vertex, 0.0005, color);
        }
    }

    void Renderer::__fill_position(MatrixXf &positions, SkeletalNode *root, int &si)
    {
        if (root == nullptr)
            return;

        if (root->children->size() == 0)
            return;

        for (int i = 0; i < root->children->size(); i++)
        {
            SkeletalNode *child = root->children->at(i);

            positions.col(si) << root->pos.x, root->pos.y, root->pos.z, 1.0;
            positions.col(si + 1) << child->pos.x, child->pos.y, child->pos.z, 1.0;
            si += 2;

            __fill_position(positions, child, si);
        }
    }

    int Renderer::__get_num_bones(SkeletalNode *root)
    {
        if (root == nullptr)
            return 0;

        int size = root->children->size();

        for (int i = 0; i < root->children->size(); i++)
        {
            SkeletalNode *child = root->children->at(i);
            size += __get_num_bones(child);
        }

        return size;
    }
}