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
#include "../logger.h"
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
		BMesh(Logger* logger);
		~BMesh() = default;

		/******************************
		 * Main Feature Function      *
		 ******************************/
		void clear_mesh();
		void rebuild();
		void generate_bmesh();
		void subdivision();
		void remesh();
		void remesh_flip() { __remesh_flip(*mesh); };
		void remesh_collapse() { __remesh_collapse(*mesh); };
		void remesh_split() { __remesh_split(*mesh); };
		void remesh_average() { __remesh_average(*mesh); };

		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void select_next_skeletal_node(SkeletalNode*& selected);
		void select_parent_skeletal_node(SkeletalNode*& selected);
		void select_child_skeletal_node(SkeletalNode*& selected);
		SkeletalNode* create_skeletal_node_after(SkeletalNode* parent);

		void interpolate_spheres();
		bool delete_node(SkeletalNode* node);
		unordered_set<SkeletalNode*> get_all_node();

		void export_to_file(const string& filename);
		void save_to_file(const string& filename);
		bool load_from_file(const string& filename);

		/******************************
		 * Rendering Functions        *
		 ******************************/
		void draw_skeleton(GLShader& shader);
		void draw_polygon_faces(GLShader& shader);
		void draw_mesh_faces(GLShader& shader);
		void draw_polygon_wireframe(GLShader& shader);
		void draw_mesh_wireframe(GLShader& shader);

		/******************************
		 * Animation (Shake it!)      *
		 ******************************/
		void shake();
		void tick();
		void shake_nodes_position();

		/******************************
		 * Debugging Function         *
		 ******************************/
		void print_skeleton();
		bool shaking = false;

	private:
		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void __interpspheres_helper(SkeletalNode* root, int divs);
		void __delete_interpolation_helper(SkeletalNode* root);
		void __skeleton_to_json(json& j);
		bool __json_to_skeleton(const json& j);
		void __avada_kedavra();
		void __reroot(SkeletalNode* target, SkeletalNode* parent);

		/******************************
		 * Sweeping and stitching     *
		 ******************************/
		void __joint_iterate(SkeletalNode* root);
		void __joint_iterate_limbs(SkeletalNode* root);
		void __update_limb(SkeletalNode* root, SkeletalNode* child, bool add_root, Limb* limbmesh, bool isleaf);
		void __add_limb_faces(SkeletalNode* root);
		void __stitch_faces();

		/******************************
		 * Catmull-Clark              *
		 ******************************/
		void __catmull_clark(HalfedgeMesh& mesh);
		//void __remesh(HalfedgeMesh& mesh);

		void __remesh_split(HalfedgeMesh& mesh);
		void __remesh_collapse(HalfedgeMesh& mesh);
		void __remesh_flip(HalfedgeMesh& mesh);
		void __remesh_average(HalfedgeMesh& mesh);

		/******************************
		 * Debugging                  *
		 ******************************/
		void __print_skeleton(SkeletalNode* root);

		/******************************
		 * Member Variables           *
		 ******************************/
	public:
		// Shader method
		Method shader_method = not_ready;
		Method previous_shader_method = not_ready;

	private:
		// Root of the tree structured skeletal nodes
		SkeletalNode* root = nullptr;
		unordered_set<SkeletalNode*> all_nodes;

		// Generated Halfedge Mesh
		HalfedgeMesh* mesh = nullptr;
		HalfedgeMesh* mesh_copy = nullptr;
		vector<vector<size_t>> polygons;
		vector<Vector3D> vertices;

		// Sweeping and stitching
		vector<Triangle> triangles;
		vector<Quadrangle> quadrangles;
		vector<Vector3D> fringe_points;
		vector<Vector3D> all_points;
		unordered_set<Vector3D> unique_extra_points;

		// Animation
		unsigned long long ts = 0ULL;
		
		Logger *logger;
	}; // END_STRUCT BMESH
};	   // END_NAMESPACE BALLE
#endif
