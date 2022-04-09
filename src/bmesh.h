#ifndef BMESH_H
#define BMESH_H

#include <vector>
#include <nanogui/nanogui.h>
#include "misc/sphere_drawing.h"
#include "CGL/CGL.h"

using namespace std;
using namespace nanogui;
using namespace CGL;

// The struct for the tree itself
struct SkeletalNode {
public:
	SkeletalNode(Vector3D pos, float radius) : pos(pos), radius(radius) {
		children = new vector<SkeletalNode*>;
	};
	~SkeletalNode() {

	}

	// Radius of the ball
	float radius;

	// World Coordinate position of the ball
	Vector3D pos;

	// The children of the ball
	// Leaf node == end node -> connects to only one bone
	// connection node if it connects to two bones
	// joint node -> more than 2 bones
	vector<SkeletalNode*>* children;
};

// Contains the tree and other functions that access the tree
struct BMesh {
public:
	BMesh(SkeletalNode* skeletalnodes) : skeletalnodes(skeletalnodes) {
	};

	BMesh() {
		skeletalnodes = new SkeletalNode(Vector3D(0, 0, 0), 0.01);

		SkeletalNode* chest = new SkeletalNode(Vector3D(0, 1, 0), 0.01);
		SkeletalNode* arml = new SkeletalNode(Vector3D(-0.5, 0.5, 0), 0.1);
		SkeletalNode* armr = new SkeletalNode(Vector3D(0.5, 0.5, 0), 0.1);
		SkeletalNode* head = new SkeletalNode(Vector3D(0, 1.5, 0), 0.2);
		SkeletalNode* footL = new SkeletalNode(Vector3D(-0.75, -1, 0), 0.1);
		SkeletalNode* footR = new SkeletalNode(Vector3D(0.75, -1, 0), 0.1);


		skeletalnodes->children->push_back(chest);
		skeletalnodes->children->push_back(footL);
		skeletalnodes->children->push_back(footR);

		chest->children->push_back(arml);
		chest->children->push_back(armr);
		chest->children->push_back(head);

	};

	~BMesh() {};

	// Fill the positions array to draw in opengl
	void fillPositions(MatrixXf& positions);

	// Number of connections
	int getNumLinks();

	// Draw the spheres using the shader
	void drawSpheres(GLShader& shader);

private: 
	SkeletalNode* skeletalnodes;

	// Temp counter used for the helpers
	int si = 0;

	void fpHelper(MatrixXf& positions, SkeletalNode* root);
	void dsHelper(GLShader& shader, Misc::SphereMesh msm, SkeletalNode* root);
	int gnlHelper(SkeletalNode* root);

};
#endif
