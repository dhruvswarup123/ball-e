#ifndef BMESH_H
#define BMESH_H

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
	enum Method
	{
		polygons_no_indices,
		mesh_faces_no_indices,
		polygons_wirefame_no_indices,
		mesh_wireframe_no_indices,
		not_ready
	};

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

		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void select_next_skeletal_node(SkeletalNode*& selected);
		void select_parent_skeletal_node(SkeletalNode*& selected);
		void select_child_skeletal_node(SkeletalNode*& selected);
		SkeletalNode* create_skeletal_node_after(SkeletalNode* parent);

		void interpolate_spheres();
		bool delete_node(SkeletalNode *node);
		void delete_interpolation(SkeletalNode *root);
		vector<SkeletalNode*> get_all_node();

		/******************************
		 * Rendering Functions        *
		 ******************************/
		void draw_skeleton(GLShader &shader);
		void draw_polygon_faces(GLShader &shader);
		void draw_mesh_faces(GLShader &shader);
		void draw_polygon_wireframe(GLShader& shader);
		void draw_mesh_wireframe(GLShader& shader);
		/******************************
		 * Debugging Function         *
		 ******************************/
		void print_skeleton();

	private:
		/******************************
		 * Structual Manipulation     *
		 ******************************/
		void __interpspheres_helper(SkeletalNode *root, int divs);

		/******************************
		 * Rendering Functions Helper *
		 ******************************/
		void __fill_position(MatrixXf &positions, SkeletalNode *root, int& si);
		void __draw_skeletal_spheres(GLShader &shader, Misc::SphereMesh &msm, SkeletalNode *root);
		void __draw_mesh_vertices(GLShader &shader, Misc::SphereMesh &msm);
		int __get_num_bones(SkeletalNode *root);

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
		void __remesh(HalfedgeMesh& mesh);

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
		vector<SkeletalNode *> all_nodes_vector;

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
	};
};
#endif
