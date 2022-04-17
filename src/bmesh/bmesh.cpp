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
			// TODO: FIX THIS. IT IS HIGHLY UNSTABLE . it removes the node entirely?
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

void BMesh::interpolate_spheres() {
	interpspheres_helper(root, 2);
}

void BMesh::interpspheres_helper(SkeletalNode* root, int divs) {
	if (root == NULL) {
		return;
	}

	// Iterate through each child node
	vector<SkeletalNode*> * original_children = new vector<SkeletalNode*>();
	for (SkeletalNode* child : *(root->children)) {
		original_children->push_back(child);
	}

	for (SkeletalNode* child : *(original_children)) {
		// Remove the child from the current parents list of children
		int i = 0;
		for (SkeletalNode* temp : *(root->children)) {
			if (temp == child) {
				root->children->erase(root->children->begin() + i);
				break;
			}
			i += 1;
		}

		// Between the parent and each child, create new spheres
		SkeletalNode * prev = root;

		// Distance or radius step size between the interpolated spheres
		Vector3D pos_step = (child->pos - root->pos) / (divs + 1.);
		float rad_step = (child->radius - root->radius) / (divs + 1.);

		// For each sphere to be created
		for (int i = 0; i < divs; i++) {
			// Final pos, rad of the interp sphere
			Vector3D new_position = root->pos + (i+1) * pos_step;
			float new_radius = root->radius + (i+1) * rad_step;

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

void BMesh::generate_bmesh() {
	_joint_iterate(root);

	//vector<array<SkeletalNode, 2>> * limbs = new vector<array<SkeletalNode, 2>>;

	//get_limbs_helper(root, limbs);

	//// Print out the limbs to check
	//cout << "Start limbs" << endl;
	//for (auto i : *limbs){
	//	cout << "(" << i[0].radius << "->" << i[1].radius << ")" << endl;
	//}
	//cout << "End limbs" << endl;
}

// Joint node helper
// TODO: For the root, if the thing has 2 children, then it is not a joint
void BMesh::_joint_iterate(SkeletalNode * root) {
	if (root == NULL) {
		cout << "ERROR: joint node is null" << endl;
		return; // This should not be possible cause im calling this func only on joint nodes
	}

	// Because this is a joint node, iterate through all children
	for (SkeletalNode* child : *(root->children)) {
		cout << "vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv" << endl;
		cout << "Joint node: " << root->radius << endl;

		// Create a new mesh for this child
		HalfedgeMesh* limbmesh = new HalfedgeMesh; // Add the sweeping stuff here
		bool first = true;

		if (child->children->size() == 0) { // Leaf node
			// That child should only have one rectangle mesh (essentially 2D)
			cout << "ERROR: joint->child->children->size() = 0" << endl;
		}
		else if (child->children->size() == 1) { // limb node
			SkeletalNode* temp = child;
			while (temp->children->size() == 1) {
				temp->mesh = limbmesh;
				// Create the first local coordinate system
				Vector3D child_center = temp->pos;
				double child_radius = temp->radius;
				Vector3D children_center = (*temp->children)[0]->pos;
				double children_radius = (*temp->children)[0]->radius;
				Vector3D localx = (children_center - child_center).unit();
				//y = Z x x;
				Vector3D localy = cross({0,0,1}, localx).unit();
				Vector3D localz = cross(localx, localy).unit();
				Vector3D child_rtup = child_center + child_radius*localy + child_radius*localz;
				Vector3D child_rtdn =  child_center + child_radius*localy - child_radius*localz;
				Vector3D child_lfup =  child_center - child_radius*localy + child_radius*localz;
				Vector3D child_lfdn =  child_center - child_radius*localy - child_radius*localz;

				Vector3D children_rtup = children_center + children_radius*localy + children_radius*localz;
				Vector3D children_rtdn =  children_center + children_radius*localy - children_radius*localz;
				Vector3D children_lfup =  children_center - children_radius*localy + children_radius*localz;
				Vector3D children_lfdn =  children_center - children_radius*localy - children_radius*localz;

				if (first) {
					first = false;
					HalfedgeMesh* limbmesh = new HalfedgeMesh;
					FaceIter startface = limbmesh->newFace();
					//TODO: CREAT NEW FACE USING EXISTING POINTS
					// Create the first rectangle
				}
				//ADD FOUR FACE


				// Slide
				cout << "sliding: " << temp->radius << endl;
				temp = (*temp->children)[0];

			}

			if (temp->children->size() == 0) { // reached the end
				cout << "Reached the leaf " << temp->radius << endl;
				cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;
				// Add the last rectangle

				// Push it into the main limb list
			//	seperate_limb_meshes->push_back(limbmesh);
			}
			else { // The only other possible case is that we have reached a joint node
				cout << "Reached a joint " << temp->radius << endl;
				cout << "^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^" << endl;

				// Add the last rectangle

				// Push it into the main limb list
			//	seperate_limb_meshes->push_back(limbmesh);

				_joint_iterate(temp);
			}

		}
		else { // Joint node
			cout << "ERROR: joint->child->children->size() > 1" << endl;
			_joint_iterate(child);
		}
	}

	_stitch_joint_node(root);
}

void BMesh::_stitch_joint_node(SkeletalNode* root) {

}

void  BMesh::print_skeleton() {
	_print_skeleton(root);
}
void  BMesh::_print_skeleton(SkeletalNode* root) {
	if (!root) return;
	cout << "At root: " << root->radius << endl;
	for (SkeletalNode* child : *(root->children)) {
		//cout << root->radius << "->" << child->radius << endl;
		_print_skeleton(child);
	}

}