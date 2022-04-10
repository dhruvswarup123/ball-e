#include <vector>
#include <nanogui/nanogui.h>

#include "CGL/misc.h"
#include "CGL/vector3D.h"
#include "misc/sphere_drawing.h"
#include "bmesh.h"

using namespace std;
using namespace nanogui;
using namespace CGL;

void BMesh::fpHelper(MatrixXf& positions, SkeletalNode* root) {
	if (root == NULL) return;

	if (root->children->size() == 0) return;

	for (int i = 0; i < root->children->size(); i++) {
		SkeletalNode* child = root->children->at(i);

		positions.col(si) << root->pos.x, root->pos.y, root->pos.z, 1.0;
		positions.col(si + 1) << child->pos.x, child->pos.y, child->pos.z, 1.0;
		si += 2;

		fpHelper(positions, child);
	}
}

void BMesh::fillPositions(MatrixXf& positions) {
	si = 0;

	fpHelper(positions, root);
}

int BMesh::gnlHelper(SkeletalNode* root) {
	if (root == NULL) return 0;

	int size = root->children->size();

	for (int i = 0; i < root->children->size(); i++) {
		SkeletalNode* child = root->children->at(i);
		size += gnlHelper(child);
	}

	return size;
}

int BMesh::getNumLinks() {
	return gnlHelper(root);
}

void BMesh::dsHelper(GLShader& shader, Misc::SphereMesh msm, SkeletalNode* root) {
	if (root == NULL) return;


	nanogui::Color color(0.0f, 1.0f, 1.0f, 1.0f);

	if (root->selected) {
		color = nanogui::Color(1.0f, 0.0f, 0.0f, 1.0f);
	}

	msm.draw_sphere(shader, root->pos, root->radius, color);

	for (int i = 0; i < root->children->size(); i++) {
		SkeletalNode* child = root->children->at(i);
		dsHelper(shader, msm, child);
	}

	return;
}

void BMesh::drawSpheres(GLShader& shader) {
	Misc::SphereMesh msm;
	dsHelper(shader, msm, root);
}

bool BMesh::deleteNode(SkeletalNode* node) {
	if (node == root) {
		cout << "WARNING: Cant delete root!" << endl;
		return false;
	}

	// First remove the node from the master list
	int i = 0;
	for (SkeletalNode* temp : *all_nodes_vector) {
		if (temp == node) {
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
	for (SkeletalNode* temp : *(parent->children) ) {
		if (temp == node) {
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE 
			parent->children->erase(parent->children->begin() + i);
			break;
		}
		i += 1;
	}

	cout << "removed from parent's children" << endl;

	for (SkeletalNode* temp : *(node->children)) {
		parent->children->push_back(temp);
		temp->parent = parent;
	}

	cout << "transferred children" << endl;


	delete node;

	cout << "deleted node" << endl;


	return true;
}