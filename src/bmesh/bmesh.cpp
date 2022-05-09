#include <unordered_map>
#include <unordered_set>
#include "renderer.h"
#include "../logger.h"
#include "bmesh.h"
#define QUICKHULL_IMPLEMENTATION 1
#include "quickhull.h"
#include "../json.hpp"
#include <iostream>
#include <fstream>

using namespace std;
using namespace nanogui;
using nlohmann::json;

namespace Balle
{
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

		all_nodes.emplace(root);
		// all_nodes.emplace(chest);
		// all_nodes.emplace(arml);
		// all_nodes.emplace(armr);
		// all_nodes.emplace(head);
		all_nodes.emplace(footL);
		all_nodes.emplace(footR);
	};
	void BMesh::rebuild(){
		if(mesh != nullptr){
			delete mesh;
		}
		mesh = nullptr;
		mesh = new HalfedgeMesh();
		mesh->build(polygons, vertices);
		__catmull_clark(*mesh);
	}
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
		previous_shader_method = shader_method;
		shader_method = Balle::Method::not_ready;
	}
	/******************************
	 * Render                     *
	 ******************************/

	void BMesh::draw_skeleton(GLShader &shader)
	{
		Balle::Renderer renderer;
		renderer.draw_skeleton(shader, root);
	}
	void BMesh::draw_polygon_faces(GLShader &shader)
	{
		Balle::Renderer renderer;
		renderer.draw_polygon_faces(shader, quadrangles, triangles, polygons, vertices);
	}

	void BMesh::draw_mesh_faces(GLShader &shader)
	{
		Balle::Renderer renderer;
		renderer.draw_mesh_faces(shader, mesh);
	}
	void BMesh::draw_polygon_wireframe(GLShader &shader)
	{
		Balle::Renderer renderer;
		renderer.draw_polygon_wireframe(shader, quadrangles, triangles, root, vertices);
	}
	void BMesh::draw_mesh_wireframe(GLShader &shader)
	{
		Balle::Renderer renderer;
		renderer.draw_mesh_wireframe(shader, mesh, root);
	}

	void BMesh::tick()
	{
		ts++;
	}

	void BMesh::shake()
	{
		shaking = shaking ^ true;
	}

	void BMesh::shake_nodes_position()
	{
		if (!shaking)
		{
			return;
		}
		clear_mesh();
		size_t idx = 0;
		double t = (ts % 360) * PI / 360;
		for (SkeletalNode *node_ptr : all_nodes)
		{
			if (node_ptr->interpolated)
			{
				continue;
			}
			size_t mode = idx % 3;
			switch (mode)
			{
			case 0:
				node_ptr->pos.x = sin(t) * 0.05 + node_ptr->equilibrium.x;
			case 1:
				node_ptr->pos.y = sin(t) * 0.05 + node_ptr->equilibrium.y;
			case 2:
			default:
				node_ptr->pos.z = sin(t) * 0.05 + node_ptr->equilibrium.z;
			}
			idx++;
		}
		generate_bmesh();
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
		SkeletalNode *temp;
		if (parent == nullptr)
		{
			temp = new Balle::SkeletalNode(Vector3D(0, 0, 0), 0.05, parent);
			root = temp;
		}
		else
		{
			temp = new Balle::SkeletalNode(parent->pos, parent->radius, parent);
			parent->children->push_back(temp);
		}
		all_nodes.emplace(temp);
		return temp;
	}

	bool BMesh::delete_node(SkeletalNode *node)
	{
		if (node == root)
		{
			Logger::info("Deleting root, trying to reassign root first.");
			if (node->children->size() > 0)
			{
				SkeletalNode *parent = (*node->children)[0];
				SkeletalNode *target = parent;
				while (target->interpolated)
				{
					target = (*target->children)[0];
				}
				__reroot(target, parent);
				Logger::info("Reassign root succeeded.");
			}
			else
			{
				Logger::warn("Deleting singular root!");
				delete node->children;
				delete node;
				all_nodes.erase(node);
				root = nullptr;
				return true;
			}
		}

		SkeletalNode *cur = node;
		vector<SkeletalNode *> node_children(*node->children);
		do
		{
			// First remove the node from the master list
			if (all_nodes.count(cur) != 0)
			{
				all_nodes.erase(cur);
			}
			// Second remove the node from the parent list
			SkeletalNode *parent = cur->parent;
			size_t idx = 0;
			for (SkeletalNode *temp : *(parent->children))
			{
				if (temp == cur)
				{
					break;
				}
				idx += 1;
			}
			parent->children->erase(parent->children->begin() + idx);

			// Delete cur
			delete cur;
			cur = parent;
		} while (cur != nullptr && cur->interpolated);

		// Third add origin node children to the first non-interpolated ancestor's
		for (SkeletalNode *node_child : node_children)
		{
			cur->children->emplace_back(node_child);
			node_child->parent = cur;
		}
		return true;
	}

	void BMesh::__reroot(SkeletalNode *target, SkeletalNode *parent)
	{
		SkeletalNode *cur = root;
		while (cur != target)
		{
			// First set cur->parent to parent
			cur->parent = parent;

			// Second remove parent from cur->children
			size_t idx;
			for (idx = 0; idx < cur->children->size(); idx++)
			{
				if ((*cur->children)[idx] == parent)
				{
					break;
				}
			}
			cur->children->erase(cur->children->begin() + idx);

			// Third add cur to parent->children
			parent->children->push_back(cur);

			// Fourth advance to target
			cur = parent;
			parent = (*parent->children)[0];
		}
		target->parent = nullptr;
		root = target;
	}

	void BMesh::interpolate_spheres()
	{
		__delete_interpolation_helper(root);
		__interpspheres_helper(root, 1);
	}

	void BMesh::__delete_interpolation_helper(SkeletalNode *root)
	{
		if (root == nullptr || root->interpolated)
		{
			// Cannot delete nullptr;
			// Will not delete interpolated node as root.
			return;
		}

		vector<SkeletalNode *> *new_children = new vector<SkeletalNode *>();

		for (SkeletalNode *child : *(root->children))
		{
			SkeletalNode *cur = child;
			while (cur != nullptr && cur->interpolated)
			{
				SkeletalNode *next = (*cur->children)[0];
				all_nodes.erase(cur);
				delete cur;
				cur = next;
			}
			if (cur != nullptr)
			{
				new_children->push_back(cur);
				__delete_interpolation_helper(cur);
			}
		}
		delete root->children;
		root->children = new_children;
	}

	unordered_set<SkeletalNode *> BMesh::get_all_node()
	{
		return all_nodes;
	}

	void BMesh::export_to_file(const string &filename)
	{
		Logger::info("Saving as: " + filename);
		ofstream file;
		file.open(filename);

		unordered_map<Vector3D, int> vert_to_ind;
		int i = 1;
		for (VertexIter v = mesh->verticesBegin(); v != mesh->verticesEnd(); v++)
		{
			file << "v"
				 << " " << v->position.x << " " << v->position.y << " " << v->position.z << endl;
			vert_to_ind[v->position] = i++;
		}

		file << "g all_faces" << endl;

		for (FaceIter f = mesh->facesBegin(); f != mesh->facesEnd(); f++)
		{
			file << "f";

			HalfedgeIter h = f->halfedge();
			do
			{
				file << " " << vert_to_ind[h->vertex()->position];
				h = h->next();
			} while (h != f->halfedge());

			file << endl;
		}

		file.close();
	}

	void BMesh::save_to_file(const string &filename)
	{
		Logger::info("Saving as: " + filename);
		// Load all info into json
		json j;
		__skeleton_to_json(j);

		// Dump json to file
		ofstream file;
		file.open(filename);
		file << std::setw(4) << j << std::endl;
		file.close();
	}

	bool BMesh::load_from_file(const string &filename)
	{
		Logger::info("Loading from: " + filename);
		ifstream file(filename);
		if (!file.good())
		{
			return false;
		}
		json j;
		file >> j;
		file.close();

		__json_to_skeleton(j);

		return true;
	}

	void BMesh::__skeleton_to_json(json &j)
	{

		/*
		 * {
		 *
		 *	"count": count,
		 *	"spheres" : {
		 *		index :
		 *			"index": index,
		 *			"parent": parent_idx,
		 *			"radius": radius,
		 *			"pos": [posx, posy, posz],
		 *			"children": [indices]
		 *		},
		 *	}
		 * }
		 */

		// REMOVE INTERPOLATED NODES
		__delete_interpolation_helper(root);

		j["count"] = all_nodes.size();
		unordered_map<SkeletalNode *, int> spheres_to_index;

		int i = 0;

		for (auto s : all_nodes)
		{
			spheres_to_index[s] = i;

			j["spheres"][i]["index"] = i; // Just for debugging
			j["spheres"][i]["radius"] = s->radius;
			j["spheres"][i]["pos"] = vector<double>({s->pos.x, s->pos.y, s->pos.z});

			i++;
		}

		for (auto s : all_nodes)
		{
			vector<int> children;
			for (SkeletalNode *child : *s->children)
			{
				children.push_back(spheres_to_index[child]);
			}

			j["spheres"][spheres_to_index[s]]["children"] = children;
			if (s->parent == NULL)
			{
				j["spheres"][spheres_to_index[s]]["parent"] = -1;
			}
			else
			{
				j["spheres"][spheres_to_index[s]]["parent"] = spheres_to_index[s->parent];
			}
		}
	}

	bool BMesh::__json_to_skeleton(const json &j)
	{

		int count = -1;

		if (j.find("count") != j.end())
		{
			count = j["count"];
		}
		else
		{
			return false;
		}

		if (j.find("spheres") == j.end())
		{
			return false;
		}

		// Delet all the spheres
		__avada_kedavra();
		all_nodes.clear();

		unordered_map<int, SkeletalNode *> index_to_spheres;

		// For each sphere
		for (auto s : j["spheres"])
		{
			Vector3D pos;
			pos.x = s["pos"][0];
			pos.y = s["pos"][1];
			pos.z = s["pos"][2];

			double radius = s["radius"];
			int i = s["index"];

			SkeletalNode *temp = new SkeletalNode(pos, radius, NULL);
			all_nodes.insert(temp);
			index_to_spheres[i] = temp;
		}

		for (auto s : j["spheres"])
		{
			int i = s["index"];
			int i_parent = s["parent"];
			SkeletalNode *temp = index_to_spheres[i];

			if (i_parent == -1)
			{
				temp->parent = NULL;
				root = temp;
			}
			else
			{
				temp->parent = index_to_spheres[i_parent];
			}

			for (int i_child : s["children"])
			{
				temp->children->push_back(index_to_spheres[i_child]);
			}
		}

		return true;
	}

	void BMesh::__avada_kedavra()
	{
		vector<SkeletalNode *> temp(all_nodes.begin(), all_nodes.end());
		for (SkeletalNode *node : temp)
		{
			if (all_nodes.count(node) != 0)
			{
				delete_node(node);
			}
		}
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
		if (mesh == nullptr)
		{
			Logger::error("CC subdivision: Subdividing a nullptr");
			return;
		}
		__catmull_clark(*mesh);
	}

	void BMesh::remesh()
	{
		__remesh_flip(*mesh);
		__remesh_collapse(*mesh);
		__remesh_split(*mesh);
		__remesh_average(*mesh);
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

		Logger::info("CC division: call subdivision, new quad size " + to_string(quadrangles.size()));

		// 5. mapping the quads to the polygon and vertex
		connect_new_mesh(quadrangles, polygons, vertices, mesh);
		Logger::info("CC division: call subdivision, finish connecting new mesh");
	}

	void BMesh::__remesh_split(HalfedgeMesh &mesh)
	{
		// Edge split operation
		vector<EdgeIter> edges;

		for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
		{
			if (f->degree() == 3)
			{
				EdgeIter e1 = f->halfedge()->edge();
				EdgeIter e2 = f->halfedge()->next()->edge();
				EdgeIter e3 = f->halfedge()->next()->next()->edge();

				float L = (e1->length() + e2->length() + e3->length()) / 3;

				for (EdgeIter e : {e1, e2, e3})
				{
					if (e->length() > 4 * L / 3)
					{
						edges.push_back(e);
					}
				}
			}
		}

		for (EdgeIter e : edges)
		{
			if ((e->halfedge()->face()->degree() == 3) && (e->halfedge()->twin()->face()->degree() == 3))
			{
				mesh.splitEdge(e);
			}
		}

		Logger::info("remesh: finish split, split edge number " + to_string(edges.size()));
	}

	void BMesh::__remesh_collapse(HalfedgeMesh &mesh)
	{

		// Edge collapse operation
		vector<EdgeIter> edges;

		for (FaceIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)
		{
			if (f->degree() == 3)
			{
				EdgeIter e1 = f->halfedge()->edge();
				EdgeIter e2 = f->halfedge()->next()->edge();
				EdgeIter e3 = f->halfedge()->next()->next()->edge();

				float L = (e1->length() + e2->length() + e3->length()) / 3;

				for (EdgeIter e : {e1, e2, e3})
				{
					if (e->length() < 4 * L / 5)
					{
						edges.push_back(e);
					}
				}
			}
		}

		for (EdgeIter e : edges)
		{
			if (!e->isDeleted &&
				(e->halfedge()->face()->degree() == 3) && (e->halfedge()->twin()->face()->degree() == 3))
			{
				mesh.collapseEdge(e);
			}
		}

		int n{0};

		for (EdgeIter e = mesh.edgesBegin(); e != mesh.edgesEnd();)
		{
			if (e->isDeleted)
			{
				n++;
				e = mesh.ReturnDeleteEdge(e);
			}
			else
			{
				e++;
			}
		}

		Logger::info("remesh: finish collapse, deleted edge number " + to_string(n));
	}

	void BMesh::__remesh_flip(HalfedgeMesh &mesh)
	{
		// Edge flip operation
		vector<EdgeIter> edges;

		for (EdgeIter e = mesh.edgesBegin(); e != mesh.edgesEnd(); e++)
		{
			// Both triangles
			if ((e->halfedge()->face()->degree() == 3) && (e->halfedge()->twin()->face()->degree() == 3))
			{
				int a1, a2, b1, b2;
				int degree_before, degree_after;

				a1 = e->halfedge()->vertex()->degree();
				a2 = e->halfedge()->twin()->vertex()->degree();

				b1 = e->halfedge()->next()->next()->vertex()->degree();
				b2 = e->halfedge()->twin()->next()->next()->vertex()->degree();

				degree_before = abs(a1 - 6) + abs(a2 - 6) + abs(b1 - 6) + abs(b2 - 6);

				b1 = e->halfedge()->vertex()->degree() + 1;
				b2 = e->halfedge()->twin()->vertex()->degree() + 1;

				a1 = e->halfedge()->next()->next()->vertex()->degree() - 1;
				a2 = e->halfedge()->twin()->next()->next()->vertex()->degree() - 1;

				degree_after = abs(a1 - 6) + abs(a2 - 6) + abs(b1 - 6) + abs(b2 - 6);

				if (degree_after < degree_before)
				{
					edges.push_back(e);
				}
			}
		}

		for (EdgeIter e : edges)
		{
			if ((e->halfedge()->face()->degree() == 3) && (e->halfedge()->twin()->face()->degree() == 3))
			{
				mesh.flipEdge(e);
			}
		}

		Logger::info("remesh: finish flip, flip edge number " + to_string(edges.size()));
	}

	void BMesh::__remesh_average(HalfedgeMesh &mesh)
	{
		// Vertex average
		for (VertexIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)
		{
			HalfedgeIter htemp = v->halfedge()->twin()->next();

			float count = 0;
			Vector3D centroid = 0;

			HalfedgeIter h = v->halfedge();
			do
			{
				centroid += h->next()->vertex()->position;
				count += 1.;
				h = h->twin()->next();

			} while (h != v->halfedge());

			centroid /= count;

			Vector3D V = centroid - v->position;
			V = V - dot(v->normal(), V) * v->normal();

			v->newPosition = v->position + 0.3 * V;
		}

		for (VertexIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)
		{
			v->position = v->newPosition;
		}

		Logger::info("remesh: finish vertex average");
	}

	/******************************
	 * PRIVATE                    *
	 ******************************/

	void edge_interpolate(const SkeletalNode &n1, const SkeletalNode &n2, float *rad, float *x)
	{
		// Interpolate a sphere touching n1

		float d = (n2.pos - n1.pos).norm();
		float r3 = n1.radius * (d + n2.radius - n1.radius) / (d - n2.radius + n1.radius);
		*x = n1.radius + r3;

		*rad = r3;
		//	*pos = n1.pos + x * (n2.pos - n1.pos).unit();
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

		float radius = 1000;
		float x = 0;

		// Get the smallest interpolation between joint, to parents and children
		for (SkeletalNode *child : *(original_children))
		{
			float temp_rad, temp_x;

			edge_interpolate(*root, *child, &temp_rad, &temp_x);

			if (temp_rad < radius)
			{
				radius = temp_rad;
				x = temp_x;
			}
		}

		if ((root->parent != NULL))
		{

			float temp_rad, temp_x;
			edge_interpolate(*root, *(root->parent), &temp_rad, &temp_x);
			if (temp_rad < radius)
			{
				radius = temp_rad;
				x = temp_x;
			}

			// Now interpolate to the parent
			SkeletalNode *interp_sphere = new SkeletalNode(root->pos + x * (root->parent->pos - root->pos).unit(), radius, root->parent);
			interp_sphere->interpolated = true;

			SkeletalNode *temp_parent = root->parent;

			int i = 0;
			for (SkeletalNode *temp : *(root->parent->children))
			{
				if (temp == root)
				{
					root->parent->children->erase(root->parent->children->begin() + i);
					break;
				}
				i += 1;
			}

			temp_parent->children->push_back(interp_sphere);
			interp_sphere->children->push_back(root);
			root->parent = interp_sphere;
			all_nodes.emplace(interp_sphere);
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
			divs = 1;
			// For each sphere to be created
			for (int i = 0; i < divs; i++)
			{
				// Final pos, rad of the interp sphere
				Vector3D new_position = root->pos + (i + 1) * pos_step;
				float new_radius = root->radius + (i + 1) * rad_step;

				// Create the interp sphere
				// SkeletalNode *interp_sphere = new SkeletalNode(new_position, new_radius, prev);
				SkeletalNode *interp_sphere = new SkeletalNode(root->pos + x * (child->pos - root->pos).unit(), radius, prev);
				interp_sphere->interpolated = true;

				// Add the interp sphere to the struct
				prev->children->push_back(interp_sphere);
				all_nodes.emplace(interp_sphere);

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
		if ((root->parent == nullptr) && (root->children->size() == 0))
		{ // Main  root only: just make a sphere
			// Add something related to current joint node
			// 6 uniform nodes across the joint node sphere

			vector<Vector3D> local_hull_points;

			for (int phi_deg = 0; phi_deg < 180; phi_deg += 90)
			{
				for (int theta_deg = 0; theta_deg < 360; theta_deg += 90)
				{
					Vector3D extra_point = root->pos;
					float rad = root->radius * 1.5;
					double phi = phi_deg * PI / 180, theta = theta_deg * PI / 180;
					double x = rad * cos(phi) * sin(theta);
					double y = rad * sin(phi) * sin(theta);
					double z = rad * cos(theta);
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

				unique_extra_points.insert(a);
				unique_extra_points.insert(b);
				unique_extra_points.insert(c);
			}
			qh_free_mesh(mesh);

			for (const Vector3D &unique_extra_point : unique_extra_points)
			{
				all_points.push_back(unique_extra_point);
			}

			return;
		}

		Vector3D root_center = root->pos;
		double root_radius = root->radius;

		// Because this is a joint node, iterate through all children
		for (SkeletalNode *child : *(root->children))
		{
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
		// if (root->children->size() <= 1)
		// {
		// 	throw runtime_error("Adding faces from a limb node.");
		// }

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
			if (previous_shader_method == mesh_faces_no_indices || previous_shader_method == mesh_wireframe_no_indices)
			{
				shader_method = previous_shader_method;
			} else {
				shader_method = mesh_faces_no_indices;
			}
			__catmull_clark(*mesh);
			Logger::info("HalfedgeMesh building succeeded.");
		}
		else
		{
			if (mesh != nullptr)
			{
				delete mesh;
				mesh = nullptr;
			}
			shader_method = polygons_no_indices;
			Logger::warn("HalfedgeMesh building failed, using polygons instead.");
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
		Logger::info("print_skeleton: Current node radius " + to_string(root->radius));
		for (SkeletalNode *child : *(root->children))
		{
			__print_skeleton(child);
		}
	}
}; // END_NAMESPACE BALLE