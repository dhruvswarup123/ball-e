#ifndef BMESH_H
#define BMESH_H

#include <vector>
#include <nanogui/nanogui.h>
#include "../misc/sphere_drawing.h"
#include "CGL/CGL.h"

using namespace std;
using namespace nanogui;
using namespace CGL;

// The struct for the tree itself
struct SkeletalNode {
public:
	SkeletalNode(Vector3D pos, float radius, SkeletalNode * parent) : pos(pos), radius(radius), parent(parent){
		children = new vector<SkeletalNode*>;
	};
	~SkeletalNode() {

	}
	SkeletalNode* parent;

	// Radius of the ball
	float radius;

	// w coordiante
	float w = 0; // TODO HOW TO INIT

	// World Coordinate position of the ball
	Vector3D pos;
	bool selected = false;

	// The children of the ball
	// Leaf node == end node -> connects to only one bone
	// connection node if it connects to two bones
	// joint node -> more than 2 bones
	vector<SkeletalNode*>* children;
};

// Contains the tree and other functions that access the tree
struct BMesh {
public:
	BMesh() {
		root = new SkeletalNode(Vector3D(0, 0, 0), 0.05, NULL); // root has no parent

		SkeletalNode* chest = new SkeletalNode(Vector3D(0, 1, 0), 0.05, root);
		SkeletalNode* arml = new SkeletalNode(Vector3D(-0.5, 0.5, 0), 0.05, chest);
		SkeletalNode* armr = new SkeletalNode(Vector3D(0.5, 0.5, 0), 0.05, chest);
		SkeletalNode* head = new SkeletalNode(Vector3D(0, 1.5, 0), 0.05, chest);
		SkeletalNode* footL = new SkeletalNode(Vector3D(-0.75, -1, 0), 0.05, root);
		SkeletalNode* footR = new SkeletalNode(Vector3D(0.75, -1, 0), 0.05, root);

		root->children->push_back(chest);
		root->children->push_back(footL);
		root->children->push_back(footR);

		chest->children->push_back(arml);
		chest->children->push_back(armr);
		chest->children->push_back(head);


		all_nodes_vector = new vector<SkeletalNode *>;
		all_nodes_vector->push_back(root);
		all_nodes_vector->push_back(chest);
		all_nodes_vector->push_back(arml);
		all_nodes_vector->push_back(armr);
		all_nodes_vector->push_back(head);
		all_nodes_vector->push_back(footL);
		all_nodes_vector->push_back(footR);
	};

	~BMesh() {};

	// Fill the positions array to draw in opengl
	void fillPositions(MatrixXf& positions);

	// Number of connections
	int getNumLinks();

	// Draw the spheres using the shader
	void drawSpheres(GLShader& shader);
	vector<SkeletalNode *> * all_nodes_vector;
	bool deleteNode(SkeletalNode* node);

	SkeletalNode* root;
private:

	// Temp counter used for the helpers
	int si = 0;

	void fpHelper(MatrixXf& positions, SkeletalNode* root);
	void dsHelper(GLShader& shader, Misc::SphereMesh msm, SkeletalNode* root);
	int gnlHelper(SkeletalNode* root);

};
#endif
