#include <unordered_map>
#include <unordered_set>
#include "bmesh.h"
#define QUICKHULL_IMPLEMENTATION 1
#include "quickhull.h"

using namespace std;
using namespace nanogui;
using namespace CGL;
using namespace Balle;

void BMesh::fpHelper(MatrixXf& positions, SkeletalNode* root)
{
	if (root == NULL)
		return;

	if (root->children->size() == 0)
		return;

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode* child = root->children->at(i);

		positions.col(si) << root->pos.x, root->pos.y, root->pos.z, 1.0;
		positions.col(si + 1) << child->pos.x, child->pos.y, child->pos.z, 1.0;
		si += 2;

		fpHelper(positions, child);
	}
}

void BMesh::fillPositions(MatrixXf& positions)
{
	si = 0;

	fpHelper(positions, root);
}

int BMesh::gnlHelper(SkeletalNode* root)
{
	if (root == NULL)
		return 0;

	int size = root->children->size();

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode* child = root->children->at(i);
		size += gnlHelper(child);
	}

	return size;
}

int BMesh::getNumLinks()
{
	return gnlHelper(root);
}

void BMesh::dsHelper(GLShader& shader, Misc::SphereMesh msm, SkeletalNode* root)
{
	if (root == NULL)
		return;

	nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);

	if (root->selected)
	{
		color = nanogui::Color(1.0f, 0.0f, 0.0f, 1.0f);
	}

	msm.draw_sphere(shader, root->pos, root->radius, color);

	for (int i = 0; i < root->children->size(); i++)
	{
		SkeletalNode* child = root->children->at(i);
		dsHelper(shader, msm, child);
	}

	return;
}

void BMesh::drawSpheres(GLShader& shader)
{
	Misc::SphereMesh msm;
	dsHelper(shader, msm, root);
}

bool BMesh::deleteNode(SkeletalNode* node)
{
	if (node == root)
	{
		cout << "WARNING: Cant delete root!" << endl;
		return false;
	}

	// First remove the node from the master list
	int i = 0;
	for (SkeletalNode* temp : *all_nodes_vector)
	{
		if (temp == node)
		{
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE
			all_nodes_vector->erase(all_nodes_vector->begin() + i);
			break;
		}
		i += 1;
	}

	cout << "removed from master" << endl;

	// First remove the node from the master list
	SkeletalNode* parent = node->parent;

	i = 0;
	for (SkeletalNode* temp : *(parent->children))
	{
		if (temp == node)
		{
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE . it removes the node entirely?
			parent->children->erase(parent->children->begin() + i);
			break;
		}
		i += 1;
	}

	cout << "removed from parent's children" << endl;

	for (SkeletalNode* temp : *(node->children))
	{
		parent->children->push_back(temp);
		temp->parent = parent;
	}

	cout << "transferred children" << endl;

	delete node;

	cout << "deleted node" << endl;

	return true;
}

void BMesh::interpolate_spheres()
{
	interpspheres_helper(root, 1);
}

void BMesh::interpspheres_helper(SkeletalNode* root, int divs)
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
		int i = 0;
		for (SkeletalNode* temp : *(root->children))
		{
			if (temp == child)
			{
				root->children->erase(root->children->begin() + i);
				break;
			}
			i += 1;
		}

		// Between the parent and each child, create new spheres
		SkeletalNode* prev = root;

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
			SkeletalNode* interp_sphere = new SkeletalNode(new_position, new_radius, prev);

			// Add the interp sphere to the struct
			prev->children->push_back(interp_sphere);
			all_nodes_vector->push_back(interp_sphere);

			// Update prev
			prev = interp_sphere;
		}

		// New re add the child node back to the end of the joint
		prev->children->push_back(child);
		child->parent = prev;

		// Now do the recursive call
		interpspheres_helper(child, divs);
	}
}

void BMesh::generate_bmesh()
{
	_joint_iterate(root);
	_stitch_faces();
}
void BMesh::_update_limb(SkeletalNode* root, SkeletalNode* child, bool add_root, Limb* limbmesh)
{
	Vector3D root_center = root->pos;
	double root_radius = root->radius;
	Vector3D child_center = child->pos;
	double child_radius = child->radius;
	Vector3D localx;

	if ((root->children->size() == 1) && (root->parent != NULL)) {
		localx = ((root_center - root->parent->pos) + (child_center - root_center)).unit();
	}
	else {
		localx = (child_center - root_center).unit();
	}
	// y = Z x x;
	Vector3D localy = cross({ 0, 0, 1 }, localx).unit();
	Vector3D localz = cross(localx, localy).unit();
	Vector3D root_rtup = root_center + root_radius * localy + root_radius * localz;
	Vector3D root_rtdn = root_center + root_radius * localy - root_radius * localz;
	Vector3D root_lfup = root_center - root_radius * localy + root_radius * localz;
	Vector3D root_lfdn = root_center - root_radius * localy - root_radius * localz;

	Vector3D child_rtup = child_center + child_radius * localy + child_radius * localz;
	Vector3D child_rtdn = child_center + child_radius * localy - child_radius * localz;
	Vector3D child_lfup = child_center - child_radius * localy + child_radius * localz;
	Vector3D child_lfdn = child_center - child_radius * localy - child_radius * localz;

	if (add_root)
	{
		limbmesh->add_layer(root_lfdn, root_rtdn, root_rtup, root_lfup);
	}
	else
	{
		limbmesh->add_layer(child_lfdn, child_rtdn, child_rtup, child_lfup);
	}
}
// Joint node helper
// TODO: For the root, if the thing has 2 children, then it is not a joint
void BMesh::_joint_iterate(SkeletalNode* root)
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
	for (SkeletalNode* child : *(root->children))
	{
		// cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << endl;
		// cout << "Joint node: " << root->radius << endl;
		// Create a new limb for this child
		Limb* childlimb = new Limb(); // Add the sweeping stuff here
		bool first = true;

		if (child->children->size() == 0)
		{ // Leaf node
			// That child should only have one rectangle mesh (essentially 2D)
			_update_limb(root, child, false, childlimb);
			child->limb = childlimb;
			childlimb->seal();
		}
		else if (child->children->size() == 1)
		{ // limb node
			SkeletalNode* temp = child;
			SkeletalNode* last;
			while (temp->children->size() == 1)
			{
				temp->limb = childlimb;
				_update_limb(temp, (*temp->children)[0], true, childlimb);
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
				_update_limb(last, temp, false, childlimb);
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

void BMesh::_add_faces(SkeletalNode* root)
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
	for (SkeletalNode* child : *(root->children))
	{
		for (const Quadrangle& quadrangle : child->limb->quadrangles)
		{
			quadrangles.push_back(quadrangle);
		}
	}

	// Assume the last 4 mesh vertices of parent skeletal nodes are fringe vertices
	if (root->parent && root->parent->limb)
	{
		for (const Vector3D& point : root->parent->limb->get_last_four_points())
		{
			fringe_points.push_back(point);
		}
	}
	// Also, the first 4 mesh vertices of child skeletal node are fringe vertices
	for (SkeletalNode* child : *(root->children))
	{
		for (const Vector3D& point : child->limb->get_first_four_points())
		{
			fringe_points.push_back(point);
		}
	}

	// QuickHull algorithm
	size_t n = fringe_points.size();
	qh_vertex_t* vertices = (qh_vertex_t*)malloc(sizeof(qh_vertex_t) * n);
	for (size_t i = 0; i < n; i++)
	{
		vertices[i].x = fringe_points[i].x;
		vertices[i].y = fringe_points[i].y;
		vertices[i].z = fringe_points[i].z;
	}

	// Build a convex hull using quickhull algorithm and add the hull triangles
	qh_mesh_t mesh = qh_quickhull3d(vertices, n);

	for (size_t i = 0; i < mesh.nindices; i += 3)
	{
		Vector3D a(mesh.vertices[i].x, mesh.vertices[i].y, mesh.vertices[i].z);
		Vector3D b(mesh.vertices[i + 1].x, mesh.vertices[i + 1].y, mesh.vertices[i + 1].z);
		Vector3D c(mesh.vertices[i + 2].x, mesh.vertices[i + 2].y, mesh.vertices[i + 2].z);

		triangles.push_back({ a, b, c });
	}
	qh_free_mesh(mesh);


}

void BMesh::_stitch_faces()
{
	// label vertices in triangles and quadrangles
	// id starts from 0
	unordered_map<Vector3D, size_t> ids;
	// label fringe points first in groups of 4
	// (0, 1, 2, 3), (4, 5, 6, 7), etc.
	for (const Vector3D& fringe_point : fringe_points)
	{
		if (ids.count(fringe_point) == 0)
		{
			ids[fringe_point] = ids.size();
			vertices.push_back(fringe_point);
		}
	}
	// label quadrangle vertices
	for (const Quadrangle& quadrangle : quadrangles)
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
		unordered_set<size_t> distinct_ids = { ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d] };
		if (distinct_ids.size() == 4)
		{
			polygons.push_back({ ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d] });
		}
	}
	// label triangle vertices
	for (Triangle& triangle : triangles)
	{
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D& vert : fringe_points) {
				if ((vert - triangle.a).norm() < closest_dist) {
					closest_dist = (vert - triangle.a).norm();
					closest = vert;
				}
			}
			triangle.a = closest;
		}
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D& vert : fringe_points) {
				if ((vert - triangle.b).norm() < closest_dist) {
					closest_dist = (vert - triangle.b).norm();
					closest = vert;
				}
			}
			triangle.b = closest;
		}
		{
			Vector3D closest;
			float closest_dist = 100;
			for (const Vector3D& vert : fringe_points) {
				if ((vert - triangle.c).norm() < closest_dist) {
					closest_dist = (vert - triangle.c).norm();
					closest = vert;
				}
			}
			triangle.c = closest;
		}

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
		// Quickhull will generate faces cover the limb fringe vertices
		// dont want those to be added to mesh
		size_t ida = ids[triangle.a], idb = ids[triangle.b], idc = ids[triangle.c];
		size_t maxid = max(max(ida, idb), idc);
		size_t minid = min(min(ida, idb), idc);
		unordered_set<size_t> distinct_ids = { ida, idb, idc };
		if (distinct_ids.size() == 3)
		{
			// any_fringe_vertex_id divided by 4 is the group number
			// if the triangle's 3 vertices are in the same group,
			// then we don't want to add this to mesh cuz it's covering the fringe

			if ((maxid / 4) != (minid / 4)) // || maxid >= fringe_points.size() TODO
			{
				polygons.push_back({ ida, idb, idc });
			}

		}
	}

	// build halfedgeMesh
	mesh = new HalfedgeMesh();
	//mesh->build(polygons, vertices); // Comment this line to get polygon rendered without error
	mesh_ready = true;
	cout << "edge number "<< mesh->nEdges() << endl;

	cout << "vertices number " << mesh->nVertices() << endl;
}



void BMesh::print_skeleton()
{
	_print_skeleton(root);
}
void BMesh::_print_skeleton(SkeletalNode* root)
{
	if (!root)
		return;
	cout << "At root: " << root->radius << endl;
	for (SkeletalNode* child : *(root->children))
	{
		// cout << root->radius << "->" << child->radius << endl;
		_print_skeleton(child);
	}
}

// ======================== for subdivision =====================
void BMesh::subdivision(){
	_catmull_clark(*mesh);
}


Vector3D get_face_point(const FaceIter f)
{
	Vector3D fp{Vector3D(0,0,0)};
	int num_vertices{0};

	HalfedgeCIter h_start = f->halfedge();
	HalfedgeCIter h = f->halfedge();

	do{
		num_vertices ++;
		fp += h->vertex()->position;
		h = h->next();
	} while(h != h_start);
	fp = fp / num_vertices;
	return fp;
}

Vector3D get_edge_point(const EdgeIter e)
{
	Vector3D ep{Vector3D(0,0,0)};

	HalfedgeCIter h0 = e->halfedge();
	HalfedgeCIter h1 = h0->twin();

	// face points
	ep += h0->face()->newPosition + h1->face()->newPosition;
	// vertices
	ep += h0->vertex()->position + h1->vertex()->position;
	ep = ep/4.0;

	return ep;
}

Vector3D get_new_vertex(const VertexIter v)
{
	Vector3D vp{Vector3D(0,0,0)};
	int num_edges{0};
	HalfedgeCIter h_start = v->halfedge();
	HalfedgeCIter h = v->halfedge();

	do{
		num_edges ++;
		vp += 1.0 * h->face()->newPosition;
		vp += 2.0 * h->edge()->newPosition;
		h = h->twin()->next();
	} while(h != h_start);

	vp += num_edges * v->position;
	vp = vp / (4 * num_edges);

	return vp;

}

void generate_new_quads(const FaceIter f, vector<Quadrangle> quadangles)
{
 	Vector3D v0 = f->newPosition;
	HalfedgeIter h_start = f->halfedge();
	HalfedgeIter h = f->halfedge();

	do{
		
		Vector3D v1 = h->edge()->newPosition;
		Vector3D v2 = h->next()->vertex()->newPosition;
		Vector3D v3 = h->next()->edge()->newPosition;

		// std::cout<<"in generate new quad, v0 " << v0 << std::endl;

		Quadrangle quad(v0, v1, v2, v3);
		quadangles.push_back(quad);

		h = h->next();
	} while(h != h_start);
}



HalfedgeMesh connect_new_mesh(vector<Quadrangle> quadrangles, vector<vector<size_t>> polygons, vector<Vector3D> vertices)
{
	polygons.clear();
	vertices.clear();

	unordered_map<Vector3D, size_t> ids;

	// std::cout<<"tag1 " << std::endl;
	// label quadrangle vertices
	for (const Quadrangle& quadrangle : quadrangles)
	{
		// std::cout<<"tag21 " << std::endl;
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
		unordered_set<size_t> distinct_ids = { ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d] };
		if (distinct_ids.size() == 4)
		{
			polygons.push_back({ ids[quadrangle.a], ids[quadrangle.b], ids[quadrangle.c], ids[quadrangle.d] });
		}

		// std::cout<<"tag22 " << std::endl;
	}

	HalfedgeMesh *mesh = new HalfedgeMesh();
	mesh->build(polygons, vertices); 
	return *mesh;

}

void BMesh::_catmull_clark(HalfedgeMesh& mesh)
{
	std::cout << "sub 1 " << std::endl;
	// 1. Add new face point
	for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
	{
		std::cout << "face " << std::endl;
		f->isNew = true;
		f->newPosition = get_face_point(f);
		// VertexIter fp = mesh.newVertex();
		// fp->position = f->newPosition;
		// f->newVertex = fp;
	}

	// 2. Add new edge point
	for (EdgeIter e = mesh.edgesBegin(); e != mesh.edgesEnd(); e++)
	{
		e->isNew = true;
		e->newPosition = get_edge_point(e);
		// VertexIter ep = mesh.newVertex();
		// ep->position = e->newPosition;
		// e->newVertex = ep;
	}

	// 3. Calculate new vertex
	for (VertexIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)
	{
		v->isNew = false;
		v->newPosition = get_new_vertex(v);
		// VertexIter vp = mesh.newVertex();
		// vp->position = v->newPosition;
		// v->newVertex = vp;
	}
	// std::cout<<"call subdivision1" << std::endl;

	// 4. generate quads, 1 face divide in to 4 faces
	quadrangles.clear();
	for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
	{
		generate_new_quads(f, quadrangles);
	}

	// std::cout<<"call subdivision 2, quad size" << quadrangles.size() << std::endl;

	// 5. mapping the quads to the polygon and vertex
	mesh = connect_new_mesh(quadrangles, polygons, vertices);

	// std::cout<<"call subdivision 3" << std::endl;
}
