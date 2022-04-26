#include <unordered_map>
#include <unordered_set>
#include "bmesh.h"
#define QUICKHULL_IMPLEMENTATION 1
#include "quickhull.h"

using namespace std;
using namespace nanogui;
using namespace CGL;
using namespace Balle;

/******************************
 * PUBLIC                      *
 ******************************/

BMesh::BMesh()
{
	root = new SkeletalNode(Vector3D(0, 0, 0), 0.05, nullptr); // root has no parent

	// SkeletalNode *chest = new SkeletalNode(Vector3D(0, 1, 0) / 3., 0.03, root);
	// SkeletalNode *arml = new SkeletalNode(Vector3D(-1.5, 0.5, 0) / 3., 0.021, chest);
	// SkeletalNode *armr = new SkeletalNode(Vector3D(1.5, 0.5, 0) / 3., 0.022, chest);
	// SkeletalNode *head = new SkeletalNode(Vector3D(0, 1.6, 0) / 3., 0.03, chest);
	SkeletalNode *footL = new SkeletalNode(Vector3D(-0.9, -1, 0) / 3., 0.011, root);
	SkeletalNode *footR = new SkeletalNode(Vector3D(0.9, -1, 0) / 3., 0.012, root);

	// root->children->push_back(chest);
	root->children->push_back(footL);
	root->children->push_back(footR);

	// chest->children->push_back(arml);
	// chest->children->push_back(armr);
	// chest->children->push_back(head);

	all_nodes_vector.push_back(root);
	// all_nodes_vector.push_back(chest);
	// all_nodes_vector.push_back(arml);
	// all_nodes_vector.push_back(armr);
	// all_nodes_vector.push_back(head);
	all_nodes_vector.push_back(footL);
	all_nodes_vector.push_back(footR);
};

void BMesh::clear_mesh()
{
	if (mesh != nullptr)
	{
		delete mesh;
	}
	mesh = nullptr;
	triangles.clear();
	quadrangles.clear();
	polygons.clear();
	fringe_points.clear();
	all_points.clear();
	unique_extra_points.clear();

	vertices.clear();
	shader_method = Balle::Method::not_ready;
}

/******************************
 * Structural Manipulation    *
 ******************************/
void BMesh::select_next_skeletal_node(SkeletalNode *&selected)
{
	if (selected == nullptr)
	{
		selected = root;
		selected->selected = true;
	}
	else if (selected == root)
	{
		selected->selected = false;
		selected = (*root->children)[0];
		selected->selected = true;
	}
	else
	{
		selected->selected = false;

		int index = rand() % selected->parent->children->size();
		if ((*selected->parent->children)[index] == selected)
		{
			index = (index + 1) % selected->parent->children->size();
		}

		selected = (*selected->parent->children)[index];
		selected->selected = true;
	}
}

void BMesh::select_parent_skeletal_node(SkeletalNode *&selected)
{
	if (selected == nullptr)
	{
		selected = root;
		selected->selected = true;
	}
	else if (selected == root)
	{
		return;
	}
	else
	{
		selected->selected = false;

		selected = selected->parent;
		selected->selected = true;
	}
}

void BMesh::select_child_skeletal_node(SkeletalNode *&selected)
{
	if (selected == nullptr)
	{
		selected = root;
		selected->selected = true;
	}
	else
	{
		if (selected->children->size() == 0)
			return; // No children

		selected->selected = false;
		selected = (*selected->children)[0];
		selected->selected = true;
	}
}

SkeletalNode *BMesh::create_skeletal_node_after(SkeletalNode *parent)
{
	SkeletalNode *temp = new Balle::SkeletalNode(parent->pos, parent->radius, parent);
	parent->children->push_back(temp);
	all_nodes_vector.push_back(temp);
	return temp;
}

bool BMesh::delete_node(SkeletalNode *node)
{
	if (node == root)
	{
		cout << "WARNING: Cant delete root!" << endl;
		return false;
	}

	// First remove the node from the master list
	int i = 0;
	for (SkeletalNode *temp : all_nodes_vector)
	{
		if (temp == node)
		{
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE
			all_nodes_vector.erase(all_nodes_vector.begin() + i);
			break;
		}
		i += 1;
	}

	// First remove the node from the master list
	SkeletalNode *parent = node->parent;

	i = 0;
	for (SkeletalNode *temp : *(parent->children))
	{
		if (temp == node)
		{
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE . it removes the node entirely?
			parent->children->erase(parent->children->begin() + i);
			break;
		}
		i += 1;
	}

	for (SkeletalNode *temp : *(node->children))
	{
		parent->children->push_back(temp);
		temp->parent = parent;
	}

	delete node;

	return true;
}

void BMesh::interpolate_spheres()
{
	delete_interpolation(root);
	__interpspheres_helper(root, 1);
}

void BMesh::delete_interpolation(SkeletalNode *root)
{
	if (root == nullptr)
	{
		return;
	}

	// Iterate through each child node
	vector<SkeletalNode *> *original_children = new vector<SkeletalNode *>();
	for (SkeletalNode *child : *(root->children))
	{
		original_children->push_back(child);
	}

	for (SkeletalNode *child : *(original_children))
	{

		// Remove the child from the current parents list of children
		if (child->interpolated)
		{
			SkeletalNode *next = (*child->children)[0];
			delete_node(child);
			delete_interpolation(next);
		}
		else
		{
			delete_interpolation(child);
		}
	}
}

vector<SkeletalNode *> BMesh::get_all_node()
{
	return all_nodes_vector;
}

/******************************
 * RENDERER                   *
 ******************************/
void BMesh::draw_skeleton(GLShader &shader)
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

void BMesh::draw_polygon_faces(GLShader &shader)
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

void BMesh::draw_mesh_faces(GLShader &shader)
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

void BMesh::draw_polygon_wireframe(GLShader &shader)
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
		mesh_normals.col(ind + 3) << 0., 0., 0.;
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
	__draw_mesh_vertices(shader, msm);
}

void BMesh::draw_mesh_wireframe(GLShader &shader)
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

/******************************
 * Sweeping and stitching     *
 ******************************/

void BMesh::generate_bmesh()
{
	interpolate_spheres();
	__joint_iterate(root);
	__stitch_faces();
}

/******************************
 * SUBDIVISTION               *
 ******************************/

void BMesh::subdivision()
{
	__catmull_clark(*mesh);
	std::cout << "after updating the mesh by cc subdivision " << std::endl;
}

Vector3D get_face_point(const FaceIter f)
{
	Vector3D fp{Vector3D(0, 0, 0)};
	int num_vertices{0};

	HalfedgeCIter h_start = f->halfedge();
	HalfedgeCIter h = f->halfedge();

	do
	{
		num_vertices++;
		fp += h->vertex()->position;
		h = h->next();
	} while (h != h_start);
	fp = fp / num_vertices;
	return fp;
}

Vector3D get_edge_point(const EdgeIter e)
{
	Vector3D ep{Vector3D(0, 0, 0)};

	HalfedgeCIter h0 = e->halfedge();
	HalfedgeCIter h1 = h0->twin();

	// face points
	ep += h0->face()->newPosition + h1->face()->newPosition;
	// vertices
	ep += h0->vertex()->position + h1->vertex()->position;
	ep = ep / 4.0;

	return ep;
}

Vector3D get_new_vertex(const VertexIter v)
{
	Vector3D vp{Vector3D(0, 0, 0)};
	int num_edges{0};
	HalfedgeCIter h_start = v->halfedge();
	HalfedgeCIter h = v->halfedge();

	do
	{
		num_edges++;
		vp += 1.0 * h->face()->newPosition;
		vp += 2.0 * h->edge()->newPosition;
		h = h->twin()->next();
	} while (h != h_start);

	vp += num_edges * v->position;
	vp = vp / (4 * num_edges);

	return vp;
}

void generate_new_quads(const FaceIter f, vector<Quadrangle> &quadangles)
{
	Vector3D v0 = f->newPosition;
	HalfedgeIter h_start = f->halfedge();
	HalfedgeIter h = f->halfedge();

	do
	{

		Vector3D v1 = h->edge()->newPosition;
		Vector3D v2 = h->next()->vertex()->newPosition;
		Vector3D v3 = h->next()->edge()->newPosition;

		Quadrangle quad(v0, v1, v2, v3);
		quadangles.push_back(quad);

		h = h->next();
	} while (h != h_start);
}

void connect_new_mesh(vector<Quadrangle> quadrangles, vector<vector<size_t>> polygons,
					  vector<Vector3D> vertices, HalfedgeMesh &mesh)
{
	polygons.clear();
	vertices.clear();

	unordered_map<Vector3D, size_t> ids;

	// label quadrangle vertices
	for (const Quadrangle &quadrangle : quadrangles)
	{
		if (ids.count(quadrangle.a) == 0)
		{
			ids[quadrangle.a] = ids.size();
			vertices.push_back(quadrangle.a);
		}
		if (ids.count(quadrangle.b) == 0)
		{
			ids[quadrangle.b] = ids.size();
			vertices.push_back(quadrangle.b);
		}
		if (ids.count(quadrangle.c) == 0)
		{
			ids[quadrangle.c] = ids.size();
			vertices.push_back(quadrangle.c);
		}
		if (ids.count(quadrangle.d) == 0)
		{
			ids[quadrangle.d] = ids.size();
			vertices.push_back(quadrangle.d);
		}
		unordered_set<size_t> distinct_ids = {ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d]};
		if (distinct_ids.size() == 4)
		{
			polygons.push_back({ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d]});
		}
	}

	mesh.build(polygons, vertices);
}

void BMesh::__catmull_clark(HalfedgeMesh &mesh)
{
	//  1. Add new face point
	for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
	{
		f->isNew = true;
		f->newPosition = get_face_point(f);
	}

	// 2. Add new edge point
	for (EdgeIter e = mesh.edgesBegin(); e != mesh.edgesEnd(); e++)
	{
		e->isNew = true;
		e->newPosition = get_edge_point(e);
	}

	// 3. Calculate new vertex
	for (VertexIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)
	{
		v->isNew = false;
		v->newPosition = get_new_vertex(v);
	}
	// 4. generate quads, 1 face divide in to 4 faces
	quadrangles.clear();
	for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
	{
		generate_new_quads(f, quadrangles);
	}

	std::cout << "call subdivision, new quad size" << quadrangles.size() << std::endl;

	// 5. mapping the quads to the polygon and vertex
	connect_new_mesh(quadrangles, polygons, vertices, mesh);
	std::cout << "call subdivision, finish connecting new mesh" << std::endl;
}

void BMesh::__remesh(HalfedgeMesh &mesh)
{
}

/******************************
 * PRIVATE                    *
 ******************************/

void BMesh::__fill_position(MatrixXf &positions, SkeletalNode *root, int &si)
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

int BMesh::__get_num_bones(SkeletalNode *root)
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

void BMesh::__draw_skeletal_spheres(GLShader &shader, Misc::SphereMesh &msm, SkeletalNode *root)
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

void BMesh::__draw_mesh_vertices(GLShader &shader, Misc::SphereMesh &msm)
{
	nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);
	for (const Vector3D &vertex : vertices)
	{
		msm.draw_sphere(shader, vertex, 0.0005, color);
	}
}

void BMesh::__interpspheres_helper(SkeletalNode *root, int divs)
{
	if (root == nullptr)
	{
		return;
	}

	// Iterate through each child node
	vector<SkeletalNode *> *original_children = new vector<SkeletalNode *>();
	for (SkeletalNode *child : *(root->children))
	{
		original_children->push_back(child);
	}

	for (SkeletalNode *child : *(original_children))
	{
		// Remove the child from the current parents list of children
		int i = 0;
		for (SkeletalNode *temp : *(root->children))
		{
			if (temp == child)
			{
				root->children->erase(root->children->begin() + i);
				break;
			}
			i += 1;
		}

		// Between the parent and each child, create new spheres
		SkeletalNode *prev = root;

		// Distance or radius step size between the interpolated spheres
		Vector3D pos_step = (child->pos - root->pos) / (divs + 1.);
		float rad_step = (child->radius - root->radius) / (divs + 1.);

		// For each sphere to be created
		for (int i = 0; i < divs; i++)
		{
			// Final pos, rad of the interp sphere
			Vector3D new_position = root->pos + (i + 1) * pos_step;
			float new_radius = root->radius + (i + 1) * rad_step;

			// Create the interp sphere
			SkeletalNode *interp_sphere = new SkeletalNode(new_position, new_radius, prev);
			interp_sphere->interpolated = true;

			// Add the interp sphere to the struct
			prev->children->push_back(interp_sphere);
			all_nodes_vector.push_back(interp_sphere);

			// Update prev
			prev = interp_sphere;
		}

		// New re add the child node back to the end of the joint
		prev->children->push_back(child);
		child->parent = prev;

		// Now do the recursive call
		__interpspheres_helper(child, divs);
	}
}

void BMesh::__update_limb(SkeletalNode *root, SkeletalNode *child, bool add_root, Limb *limb, bool isleaf)
{
	Vector3D root_center = root->pos;
	double root_radius = root->radius;

	Vector3D child_center = child->pos;
	double child_radius = child->radius;

	Vector3D localx;
	if ((root->children->size() == 1) && (root->parent != nullptr))
	{
		localx = ((root_center - root->parent->pos) + (child_center - root_center)).unit();
	}
	else
	{
		localx = (child_center - root_center).unit();
	}

	// y = Z x x;
	Vector3D localy = cross({0, 0, 1}, localx).unit();
	Vector3D localz = cross(localx, localy).unit();

	if (add_root)
	{
		Vector3D root_rtup = root_center + root_radius * localy + root_radius * localz;
		Vector3D root_rtdn = root_center + root_radius * localy - root_radius * localz;
		Vector3D root_lfup = root_center - root_radius * localy + root_radius * localz;
		Vector3D root_lfdn = root_center - root_radius * localy - root_radius * localz;
		limb->add_layer(root_lfdn, root_rtdn, root_rtup, root_lfup);
	}
	else
	{
		Vector3D child_rtup = child_center + child_radius * localy + child_radius * localz;
		Vector3D child_rtdn = child_center + child_radius * localy - child_radius * localz;
		Vector3D child_lfup = child_center - child_radius * localy + child_radius * localz;
		Vector3D child_lfdn = child_center - child_radius * localy - child_radius * localz;
		limb->add_layer(child_lfdn, child_rtdn, child_rtup, child_lfup);
	}

	if (isleaf)
	{
		Vector3D child_rtup = child_center + child_radius * localx + child_radius / 3 * localy + child_radius / 3. * localz;
		Vector3D child_rtdn = child_center + child_radius * localx + child_radius / 3. * localy - child_radius / 3. * localz;
		Vector3D child_lfup = child_center + child_radius * localx - child_radius / 3. * localy + child_radius / 3. * localz;
		Vector3D child_lfdn = child_center + child_radius * localx - child_radius / 3. * localy - child_radius / 3. * localz;
		limb->add_layer(child_lfdn, child_rtdn, child_rtup, child_lfup);
	}
}

void BMesh::__joint_iterate(SkeletalNode *root)
{
	if (root == nullptr)
	{
		// This should not be possible cause im calling this func only on joint nodes
		throw runtime_error("ERROR: joint node is null");
	}
	// cout << "current root address = " << root << endl;
	Vector3D root_center = root->pos;
	double root_radius = root->radius;
	// Because this is a joint node, iterate through all children
	for (SkeletalNode *child : *(root->children))
	{
		// cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << endl;
		// cout << "Joint node: " << root->radius << endl;
		// Create a new limb for this child
		Limb *childlimb = new Limb(); // Add the sweeping stuff here
		bool first = true;

		if (child->children->size() == 0)
		{ // Leaf node
			// That child should only have one rectangle mesh (essentially 2D)
			__update_limb(root, child, false, childlimb, true);
			child->limb = childlimb;
			childlimb->seal();
		}
		else if (child->children->size() == 1)
		{ // limb node
			SkeletalNode *cur = child;
			SkeletalNode *last;
			while (cur->children->size() == 1)
			{
				cur->limb = childlimb;
				__update_limb(cur, (*cur->children)[0], true, childlimb, false);
				if (first)
				{
					first = false;
				}
				last = cur;
				cur = (*cur->children)[0];
			}
			if (cur->children->size() == 0)
			{ // sweeping reached the end leaf node
				__update_limb(last, cur, false, childlimb, true);
				cur->limb = childlimb;
				childlimb->seal();
			}
			else
			{ // Sweeping reached a joint node
				__joint_iterate(cur);
			}
		}
		else
		{ // Joint node
			__joint_iterate(child);
		}
	}

	__add_limb_faces(root);
}

void BMesh::__add_limb_faces(SkeletalNode *root)
{
	if (root == nullptr)
	{
		throw runtime_error("Adding faces from a null joint node.");
	}
	if (root->children->size() <= 1)
	{
		throw runtime_error("Adding faces from a limb node.");
	}

	// Add child Limb quadrangles
	// Also, the first 4 mesh vertices of child skeletal node are fringe vertices
	for (SkeletalNode *child : *(root->children))
	{
		if (child->limb)
		{
			for (const Quadrangle &quadrangle : child->limb->quadrangles)
			{
				quadrangles.push_back(quadrangle);
			}
		}
	}

	// Assume the last 4 mesh vertices of parent skeletal nodes are fringe vertices
	vector<Vector3D> local_hull_points;

	if (root->parent && root->parent->limb)
	{
		for (const Vector3D &point : root->parent->limb->get_last_four_points())
		{
			fringe_points.push_back(point);
			local_hull_points.push_back(point);
			all_points.push_back(point);
		}
	}
	// Also, the first 4 mesh vertices of child skeletal node are fringe vertices
	for (SkeletalNode *child : *(root->children))
	{
		if (child->limb)
		{
			for (const Vector3D &point : child->limb->get_first_four_points())
			{
				fringe_points.push_back(point);
				local_hull_points.push_back(point);
				all_points.push_back(point);
			}
		}
	}
	// Add something related to current joint node
	// 6 uniform nodes across the joint node sphere
	for (int phi_deg = 0; phi_deg < 180; phi_deg += 90)
	{
		for (int theta_deg = 0; theta_deg < 360; theta_deg += 90)
		{
			Vector3D extra_point = root->pos;
			double phi = phi_deg * PI / 180, theta = theta_deg * PI / 180;
			double x = root->radius * cos(phi) * sin(theta);
			double y = root->radius * sin(phi) * sin(theta);
			double z = root->radius * cos(theta);
			extra_point = extra_point + Vector3D(x, y, z);
			local_hull_points.push_back(extra_point);
		}
	}

	// QuickHull algorithm
	size_t n = local_hull_points.size();
	qh_vertex_t *vertices = (qh_vertex_t *)malloc(sizeof(qh_vertex_t) * n);
	for (size_t i = 0; i < n; i++)
	{
		vertices[i].x = local_hull_points[i].x;
		vertices[i].y = local_hull_points[i].y;
		vertices[i].z = local_hull_points[i].z;
	}

	// Build a convex hull using quickhull algorithm and add the hull triangles
	qh_mesh_t mesh = qh_quickhull3d(vertices, n);
	unordered_set<Vector3D> unique_extra_points;
	for (size_t i = 0; i < mesh.nindices; i += 3)
	{
		Vector3D a(mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z);
		Vector3D b(mesh.vertices[i + 1].x, mesh.vertices[i + 1].y, mesh.vertices[i + 1].z);
		Vector3D c(mesh.vertices[i + 2].x, mesh.vertices[i + 2].y, mesh.vertices[i + 2].z);
		triangles.push_back({a, b, c});

		if ((a - root->pos).norm() < root->radius + 0.001)
		{
			unique_extra_points.insert(a);
		}
		if ((b - root->pos).norm() < root->radius + 0.001)
		{
			unique_extra_points.insert(b);
		}
		if ((c - root->pos).norm() < root->radius + 0.001)
		{
			unique_extra_points.insert(c);
		}
	}
	qh_free_mesh(mesh);

	for (const Vector3D &unique_extra_point : unique_extra_points)
	{
		all_points.push_back(unique_extra_point);
	}
}

void BMesh::__stitch_faces()
{
	// label vertices in triangles and quadrangles
	// id starts from 0
	unordered_map<Vector3D, size_t> ids;
	unordered_map<Vector3D, size_t> fringe_ids;
	// label fringe points first in groups of 4
	// (0, 1, 2, 3), (4, 5, 6, 7), 8 etc.

	// This is just for checking fringe
	for (const Vector3D &point : fringe_points)
	{
		if (fringe_ids.count(point) == 0)
		{
			fringe_ids[point] = fringe_ids.size();
		}
	}

	// label quadrangle vertices
	for (const Quadrangle &quadrangle : quadrangles)
	{
		if (ids.count(quadrangle.a) == 0)
		{
			ids[quadrangle.a] = ids.size();
			vertices.push_back(quadrangle.a);
		}
		if (ids.count(quadrangle.b) == 0)
		{
			ids[quadrangle.b] = ids.size();
			vertices.push_back(quadrangle.b);
		}
		if (ids.count(quadrangle.c) == 0)
		{
			ids[quadrangle.c] = ids.size();
			vertices.push_back(quadrangle.c);
		}
		if (ids.count(quadrangle.d) == 0)
		{
			ids[quadrangle.d] = ids.size();
			vertices.push_back(quadrangle.d);
		}
		unordered_set<size_t> distinct_ids = {ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d]};
		if (distinct_ids.size() == 4)
		{
			polygons.push_back({ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d]});
		}
	}
	// label triangle vertices
	for (Triangle &triangle : triangles)
	{
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D &vert : all_points)
			{
				if ((vert - triangle.a).norm() < closest_dist)
				{
					closest_dist = (vert - triangle.a).norm();
					closest = vert;
				}
			}
			triangle.a = closest;
		}
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D &vert : all_points)
			{
				if ((vert - triangle.b).norm() < closest_dist)
				{
					closest_dist = (vert - triangle.b).norm();
					closest = vert;
				}
			}
			triangle.b = closest;
		}
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D &vert : all_points)
			{
				if ((vert - triangle.c).norm() < closest_dist)
				{
					closest_dist = (vert - triangle.c).norm();
					closest = vert;
				}
			}
			triangle.c = closest;
		}

		// Check if the triangle is still valid
		if ((triangle.a == triangle.b) || (triangle.b == triangle.c) || (triangle.c == triangle.a))
		{
			continue;
		}

		// Add triangle if either it has a point that is not in fringe
		// or if all 3 points are not in the same face
		if ((fringe_ids.count(triangle.a) == 0) ||
			(fringe_ids.count(triangle.b) == 0) ||
			(fringe_ids.count(triangle.c) == 0))
		{
			if (ids.count(triangle.a) == 0)
			{
				ids[triangle.a] = ids.size();
				vertices.push_back(triangle.a);
			}
			if (ids.count(triangle.b) == 0)
			{
				ids[triangle.b] = ids.size();
				vertices.push_back(triangle.b);
			}
			if (ids.count(triangle.c) == 0)
			{
				ids[triangle.c] = ids.size();
				vertices.push_back(triangle.c);
			}

			size_t ida = ids[triangle.a], idb = ids[triangle.b], idc = ids[triangle.c];
			polygons.push_back({ida, idb, idc});
		}
		else
		{

			size_t fringe_ida = fringe_ids[triangle.a];
			size_t fringe_idb = fringe_ids[triangle.b];
			size_t fringe_idc = fringe_ids[triangle.c];

			size_t fringe_maxid = max(max(fringe_ida, fringe_idb), fringe_idc);
			size_t fringe_minid = min(min(fringe_ida, fringe_idb), fringe_idc);

			unordered_set<size_t> distinct_ids = {fringe_ida, fringe_idb, fringe_idc};

			if (distinct_ids.size() == 3)
			{
				// Quickhull will generate faces cover the limb fringe vertices
				// We dont want those to be added to mesh
				// Do this by:
				// any_fringe_vertex_id divided by 4 is the group number
				// if the triangle's 3 vertices are in the same group,
				// then we don't want to add this to mesh cuz it's covering the fringe

				if ((fringe_maxid / 4) != (fringe_minid / 4))
				{
					if (ids.count(triangle.a) == 0)
					{
						ids[triangle.a] = ids.size();
						vertices.push_back(triangle.a);
					}
					if (ids.count(triangle.b) == 0)
					{
						ids[triangle.b] = ids.size();
						vertices.push_back(triangle.b);
					}
					if (ids.count(triangle.c) == 0)
					{
						ids[triangle.c] = ids.size();
						vertices.push_back(triangle.c);
					}

					size_t ida = ids[triangle.a], idb = ids[triangle.b], idc = ids[triangle.c];
					polygons.push_back({ida, idb, idc});
				}
			}
		}
	}

	// build halfedgeMesh
	if (mesh != nullptr)
	{
		delete mesh;
	}
	mesh = new HalfedgeMesh();
	int result = mesh->build(polygons, vertices);
	if (result == 0)
	{
		shader_method = mesh_faces_no_indices;
		std::cout << "HalfedgeMesh building succeeded." << std::endl;
	}
	else
	{
		if (mesh != nullptr)
		{
			delete mesh;
			mesh = nullptr;
		}
		shader_method = polygons_no_indices;
		std::cout << "HalfedgeMesh building failed, using polygons instead." << std::endl;
	}
}

void BMesh::print_skeleton()
{
	__print_skeleton(root);
}

void BMesh::__print_skeleton(SkeletalNode *root)
{
	if (!root)
		return;
	cout << "Current node radius " << root->radius << endl;
	for (SkeletalNode *child : *(root->children))
	{
		__print_skeleton(child);
	}
}