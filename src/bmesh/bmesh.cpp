#include <unordered_map>
#include <unordered_set>
#include "bmesh.h"
#define QUICKHULL_IMPLEMENTATION 1
#include "quickhull.h"

using namespace std;
using namespace nanogui;
using namespace CGL;
using namespace Balle;

void BMesh::fpHelper(MatrixXf &positions, SkeletalNode *root)
{
	if (root == NULL)
		return;

	if (root->children->size() == 0)
		return;

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode *child = root->children->at(i);

		positions.col(si) << root->pos.x, root->pos.y, root->pos.z, 1.0;
		positions.col(si + 1) << child->pos.x, child->pos.y, child->pos.z, 1.0;
		si += 2;

		fpHelper(positions, child);
	}
}

void BMesh::fillPositions(MatrixXf &positions)
{
	si = 0;

	fpHelper(positions, root);
}

int BMesh::gnlHelper(SkeletalNode *root)
{
	if (root == NULL)
		return 0;

	int size = root->children->size();

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode *child = root->children->at(i);
		size += gnlHelper(child);
	}

	return size;
}

int BMesh::getNumLinks()
{
	return gnlHelper(root);
}

void BMesh::dsHelper(GLShader &shader, Misc::SphereMesh msm, SkeletalNode *root)
{
	if (root == NULL)
		return;

	nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);

	if (root->selected && root->interpolated)
	{
		color = nanogui::Color(0.8f, 0.5f, 0.5f, 1.0f);
	}
	else if (root->selected) {
		color = nanogui::Color(1.0f, 0.0f, 0.0f, 1.0f);
	}
	else if (root->interpolated) {
		color = nanogui::Color(0.6f, 0.6f, 0.6f, 1.0f);
	}

	msm.draw_sphere(shader, root->pos, root->radius, color);

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode *child = root->children->at(i);
		dsHelper(shader, msm, child);
	}

	return;
}

void BMesh::drawSpheres(GLShader &shader)
{
	Misc::SphereMesh msm;
	dsHelper(shader, msm, root);
}

bool BMesh::deleteNode(SkeletalNode *node)
{
	if (node == root)
	{
		cout << "WARNING: Cant delete root!" << endl;
		return false;
	}

	// First remove the node from the master list
	int i = 0;
	for (SkeletalNode *temp : *all_nodes_vector)
	{
		if (temp == node)
		{
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE
			all_nodes_vector->erase(all_nodes_vector->begin() + i);
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
	delete_interp(root);
	interpspheres_helper(root, 1);
}

void BMesh::delete_interp(SkeletalNode* root)
{
	if (root == NULL)
	{
		return;
	}

	// Iterate through each child node
	vector<SkeletalNode*>* original_children = new vector<SkeletalNode*>();
	for (SkeletalNode* child : *(root->children))
	{
		original_children->push_back(child);
	}

	for (SkeletalNode* child : *(original_children))
	{

		// Remove the child from the current parents list of children
		if (child->interpolated) {
			SkeletalNode* next = (*child->children)[0];
			deleteNode(child);
			delete_interp(next);
		}
		else {
			delete_interp(child);
		}		
	}
}


void BMesh::interpspheres_helper(SkeletalNode *root, int divs)
{
	if (root == NULL)
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
		// if ((root->children->size() == 1) && (child->children->size() <= 1))
		// {

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
			all_nodes_vector->push_back(interp_sphere);

			// Update prev
			prev = interp_sphere;
		}

		// New re add the child node back to the end of the joint
		prev->children->push_back(child);
		child->parent = prev;
		// }

		// Now do the recursive call
		interpspheres_helper(child, divs);
	}
}

void BMesh::generate_bmesh()
{
	interpolate_spheres();
	_joint_iterate(root);
	_stitch_faces();
}
void BMesh::_update_limb(SkeletalNode *root, SkeletalNode *child, bool add_root, Limb *limbmesh, bool isleaf)
{
	Vector3D root_center = root->pos;
	double root_radius = root->radius;

	Vector3D child_center = child->pos;
	double child_radius = child->radius;

	Vector3D localx;
	if ((root->children->size() == 1) && (root->parent != NULL))
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
		limbmesh->add_layer(root_lfdn, root_rtdn, root_rtup, root_lfup);
	}
	else
	{
		Vector3D child_rtup = child_center + child_radius * localy + child_radius * localz;
		Vector3D child_rtdn = child_center + child_radius * localy - child_radius * localz;
		Vector3D child_lfup = child_center - child_radius * localy + child_radius * localz;
		Vector3D child_lfdn = child_center - child_radius * localy - child_radius * localz;
		limbmesh->add_layer(child_lfdn, child_rtdn, child_rtup, child_lfup);
	}

	if (isleaf)
	{
		Vector3D child_rtup = child_center + child_radius * localx + child_radius / 3 * localy + child_radius / 3. * localz;
		Vector3D child_rtdn = child_center + child_radius * localx + child_radius / 3. * localy - child_radius / 3. * localz;
		Vector3D child_lfup = child_center + child_radius * localx - child_radius / 3. * localy + child_radius / 3. * localz;
		Vector3D child_lfdn = child_center + child_radius * localx - child_radius / 3. * localy - child_radius / 3. * localz;
		limbmesh->add_layer(child_lfdn, child_rtdn, child_rtup, child_lfup);
	}
}

// Joint node helper
// TODO: For the root, if the thing has 2 children, then it is not a joint
void BMesh::_joint_iterate(SkeletalNode *root)
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
			_update_limb(root, child, false, childlimb, true);
			child->limb = childlimb;
			childlimb->seal();
		}
		else if (child->children->size() == 1)
		{ // limb node
			SkeletalNode *temp = child;
			SkeletalNode *last;
			while (temp->children->size() == 1)
			{
				temp->limb = childlimb;
				_update_limb(temp, (*temp->children)[0], true, childlimb, false);
				if (first)
				{
					first = false;
				}
				// cout << "sliding: " << temp->radius << endl;
				last = temp;
				temp = (*temp->children)[0];
			}
			if (temp->children->size() == 0)
			{ // reached the end
				_update_limb(last, temp, false, childlimb, true);
				temp->limb = childlimb;
				childlimb->seal();
				// cout << "Reached the leaf " << temp->radius << endl;
				// cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
			}
			else
			{ // The only other possible case is that we have reached a joint node
				// cout << "Reached a joint " << temp->radius << endl;
				// cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
				_joint_iterate(temp);
			}
		}
		else
		{ // Joint node
			_joint_iterate(child);
		}
	}

	_add_faces(root);
}

void BMesh::_add_faces(SkeletalNode *root)
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
	for (int tmp = 0; tmp < 500; tmp++)
	{
		Vector3D extra_point = root->pos;
		double phi = rand() * PI / RAND_MAX, theta = rand() * 2 * PI / RAND_MAX;
		double x = root->radius * cos(phi) * sin(theta);
		double y = root->radius * sin(phi) * sin(theta);
		double z = root->radius * cos(theta);
		extra_point = extra_point + Vector3D(x, y, z);
		local_hull_points.push_back(extra_point);
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

void BMesh::_stitch_faces()
{
	// label vertices in triangles and quadrangles
	// id starts from 0
	unordered_map<Vector3D, size_t> ids;
	unordered_map<Vector3D, size_t> fringe_ids;
	// label fringe points first in groups of 4
	// (0, 1, 2, 3), (4, 5, 6, 7), 8 etc.

	// This is just for checking fringe
	for (const Vector3D& point : fringe_points)
	{
		if (fringe_ids.count(point) == 0)
		{
			fringe_ids[point] = fringe_ids.size();
		}
	}

	/*for (const Vector3D &point : all_points)
	{
		if (ids.count(point) == 0)
		{
			ids[point] = ids.size();
			vertices.push_back(point);
		}
	}*/

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
			(fringe_ids.count(triangle.c) == 0)) {

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
			polygons.push_back({ ida, idb, idc });
		}
		else {
			
			size_t fringe_ida = fringe_ids[triangle.a];
			size_t fringe_idb = fringe_ids[triangle.b];
			size_t fringe_idc = fringe_ids[triangle.c];

			size_t fringe_maxid = max(max(fringe_ida, fringe_idb), fringe_idc);
			size_t fringe_minid = min(min(fringe_ida, fringe_idb), fringe_idc);

			unordered_set<size_t> distinct_ids = { fringe_ida, fringe_idb, fringe_idc };

			if (distinct_ids.size() == 3)
			{
				// any_fringe_vertex_id divided by 4 is the group number
				// if the triangle's 3 vertices are in the same group,
				// then we don't want to add this to mesh cuz it's covering the fringe

				if ((fringe_maxid / 4) != (fringe_minid / 4)) // || maxid >= fringe_points.size() TODO
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
					polygons.push_back({ ida, idb, idc });
				}
			}

		}



		
		// Quickhull will generate faces cover the limb fringe vertices
		// dont want those to be added to mesh
		
	}

	// build halfedgeMesh
	if (mesh != nullptr)
	{
		delete mesh;
	}
	mesh = new HalfedgeMesh();
	int result = mesh->build(polygons, vertices); // Comment this line to get polygon rendered without error
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
	_print_skeleton(root);
}
void BMesh::_print_skeleton(SkeletalNode *root)
{
	if (!root)
		return;
	cout << "At root: " << root->radius << endl;
	for (SkeletalNode *child : *(root->children))
	{
		// cout << root->radius << "->" << child->radius << endl;
		_print_skeleton(child);
	}
}

// ======================== for subdivision =====================
Vector3D get_face_point(const FaceIter f)
{
	return {0, 0, 0};
}

Vector3D get_edge_point(const EdgeIter e)
{
	return {0, 0, 0};
}

void connect_face(const FaceIter f)
{
}

void BMesh::_catmull_clark(HalfedgeMesh &mesh)
{

	// 1. Add new face point
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
		Vector3D pt;
		// iterate connected edges

		// iterate connected faces

		v->isNew = false;
		v->newPosition = pt;
	}

	// 4.update the position of vertex
	for (VertexIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)
	{
		v->position = v->newPosition;
	}

	// 5. iterate faces, connect the face point and edge points
	for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
	{
		connect_face(f);
	}
}
