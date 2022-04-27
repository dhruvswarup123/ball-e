#ifndef BMESH_H
#define BMESH_H

#include <vector>
#include <unordered_set>
#include <nanogui/nanogui.h>

#include "../misc/sphere_drawing.h"
#include "CGL/CGL.h"
#include "../mesh/halfEdgeMesh.h"
#include "skeletalNode.h"
#include "primitives.h"
#include "../json.hpp"

using namespace std;
using namespace nanogui;
using namespace CGL;
using json = nlohmann::json;

namespace Balle
{
	enum Method
	{
		polygons_no_indices,
		mesh_faces_no_indices,
		polygons_wirefame_no_indices,
		mesh_wireframe_no_indices,
		not_ready
	};

	enum SaveType { fbx, balle };


	// Contains the tree and other functions that access the tree
	struct BMesh
	{
		/******************************
		 * Member Functions           *
		 ******************************/
	public:
		/******************************
		 * Constructor/Destructor     *
		 ******************************/
		BMesh();
		~BMesh() = default;

		/******************************
		 * Main Feature Function      *
		 ******************************/
		void clear_mesh();
		void generate_bmesh();
		void subdivision();
		void remesh();

		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void select_next_skeletal_node(SkeletalNode *&selected);
		void select_parent_skeletal_node(SkeletalNode *&selected);
		void select_child_skeletal_node(SkeletalNode *&selected);
		SkeletalNode *create_skeletal_node_after(SkeletalNode *parent);

		void interpolate_spheres();
		bool delete_node(SkeletalNode *node);
		unordered_set<SkeletalNode *> get_all_node();

		void export_to_file(const string& filename);
		void save_to_file(const string& filename);
		bool load_from_file(const string& filename);

		/******************************
		 * Rendering Functions        *
		 ******************************/
		// Rendering Class
		void draw_skeleton(GLShader &shader);
		void draw_polygon_faces(GLShader &shader);
		void draw_mesh_faces(GLShader &shader);
		void draw_polygon_wireframe(GLShader &shader);
		void draw_mesh_wireframe(GLShader &shader);

		/******************************
		 * Debugging Function         *
		 ******************************/
		void print_skeleton();

	private:
		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void __interpspheres_helper(SkeletalNode *root, int divs);
		void __delete_interpolation_helper(SkeletalNode *root);
		void __skeleton_to_json(json& j);
		bool __json_to_skeleton(const json& j);
		void __avada_kedavra();

		/******************************
		 * Sweeping and stitching     *
		 ******************************/
		void __joint_iterate(SkeletalNode *root);
		void __joint_iterate_limbs(SkeletalNode *root);
		void __update_limb(SkeletalNode *root, SkeletalNode *child, bool add_root, Limb *limbmesh, bool isleaf);
		void __add_limb_faces(SkeletalNode *root);
		void __stitch_faces();

		/******************************
		 * Catmull-Clark              *
		 ******************************/
		void __catmull_clark(HalfedgeMesh &mesh);
		void __remesh(HalfedgeMesh &mesh);

		/******************************
		 * Debugging                  *
		 ******************************/
		void __print_skeleton(SkeletalNode *root);

		/******************************
		 * Member Variables           *
		 ******************************/
	public:
		// Shader method
		Method shader_method = not_ready;

	private:
		// Root of the tree structured skeletal nodes
		SkeletalNode *root = nullptr;
		unordered_set<SkeletalNode *> all_nodes;

		// Generated Halfedge Mesh
		HalfedgeMesh *mesh = nullptr;
		vector<vector<size_t>> polygons;
		vector<Vector3D> vertices;

		// Sweeping and stitching
		vector<Triangle> triangles;
		vector<Quadrangle> quadrangles;
		vector<Vector3D> fringe_points;
		vector<Vector3D> all_points;
		unordered_set<Vector3D> unique_extra_points;
	}; // END_STRUCT BMESH
};	   // END_NAMESPACE BALLE
#endif
