#pragma once

#include <vector>
#include "CGL/CGL.h"
#include "../mesh/halfEdgeMesh.h"
#include "primitives.h"

using namespace std;
using namespace CGL;

namespace Balle
{
	// The struct for the tree itself
	struct SkeletalNode
	{
	public:
		SkeletalNode(Vector3D pos, float radius, SkeletalNode *parent) : pos(pos), radius(radius), parent(parent)
		{
			children = new vector<SkeletalNode *>;
		};

		~SkeletalNode()
		{
		}

		SkeletalNode *parent;

		// Radius of the ball
		float radius;

		// w coordiante
		float w = 0; // TODO HOW TO INIT

		// World Coordinate position of the ball
		Vector3D pos;
		bool selected = false;
<<<<<<< HEAD
		bool visited = false;
=======
		bool interpolated = false;
>>>>>>> 5747b72c3f461cf768b89d48d59b2d9164470d1a

		// The children of the ball
		// Leaf node == end node -> connects to only one bone
		// connection node if it connects to two bones
		// joint node -> more than 2 bones
		vector<SkeletalNode *> *children;

		// Limb primitives that surrounds this skeletal node
		Limb *limb = nullptr;
	};
};