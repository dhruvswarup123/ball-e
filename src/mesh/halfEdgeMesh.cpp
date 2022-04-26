#include "halfEdgeMesh.h"

namespace CGL {

    bool Halfedge::isBoundary(void) const
        // returns true if and only if this halfedge is on the boundary
    {
        return face()->isBoundary();
    }

    bool Edge::isBoundary(void) const
    {
        return halfedge()->isBoundary() || halfedge()->twin()->isBoundary();
    }

    Vector3D Face::normal(void) const
    {
        Vector3D N(0., 0., 0.);

        HalfedgeCIter h = halfedge();
        do
        {
            Vector3D pi = h->vertex()->position;
            Vector3D pj = h->next()->vertex()->position;

            N += cross(pi, pj);

            h = h->next();
        } while (h != halfedge());

        return N.unit();
    }

    Vector3D Vertex::normal(void) const
    {
        Vector3D N(0., 0., 0.);

        HalfedgeCIter h = this->halfedge();
        do
        {
            Vector3D pi = h->vertex()->position;
            Vector3D pj = h->next()->vertex()->position;
            Vector3D pk = h->next()->next()->vertex()->position;

            N += cross(pj - pk, pi-pk);

            h = h->twin()->next();

        } while (h != this->halfedge());

 

        return N.unit();
    }

    int HalfedgeMesh::build(const vector< vector<Index> >& polygons,
        const vector<Vector3D>& vertexPositions)
        // This method initializes the halfedge data structure from a raw list of polygons,
        // where each input polygon is specified as a list of vertex indices.  The input
        // must describe a manifold, oriented surface, where the orientation of a polygon
        // is determined by the order of vertices in the list.  Polygons must have at least
        // three vertices.  Note that there are no special conditions on the vertex indices,
        // i.e., they do not have to start at 0 or 1, nor does the collection of indices have
        // to be contiguous.  Overall, this initializer is designed to be robust but perhaps
        // not incredibly fast (though of course this does not affect the performance of the
        // resulting data structure).  One could also implement faster initializers that
        // handle important special cases (e.g., all triangles, or data that is known to be
        // manifold).
        //
        // Since there are no strong conditions on the indices of polygons, we assume that
        // the list of vertex positions is given in lexicographic order (i.e., that the
        // lowest index appearing in any polygon corresponds to the first entry of the list
        // of positions and so on).
    {
        // define some types, to improve readability
        typedef vector<Index> IndexList;
        typedef IndexList::const_iterator IndexListCIter;
        typedef vector<IndexList> PolygonList;
        typedef PolygonList::const_iterator PolygonListCIter;
        typedef pair<Index, Index> IndexPair; // ordered pair of vertex indices, corresponding to an edge of an oriented polygon

        // Clear any existing elements.
        halfedges.clear();
        vertices.clear();
        edges.clear();
        faces.clear();
        boundaries.clear();

        // Since the vertices in our halfedge mesh are stored in a linked list,
        // we will temporarily need to keep track of the correspondence between
        // indices of vertices in our input and pointers to vertices in the new
        // mesh (which otherwise can't be accessed by index).  Note that since
        // we're using a general-purpose map (rather than, say, a vector), we can
        // be a bit more flexible about the indexing scheme: input vertex indices
        // aren't required to be 0-based or 1-based; in fact, the set of indices
        // doesn't even have to be contiguous.  Taking advantage of this fact makes
        // our conversion a bit more robust to different types of input, including
        // data that comes from a subset of a full mesh.
        map<Index, VertexIter> indexToVertex; // maps a vertex index to the corresponding vertex

        // Also store the vertex degree, i.e., the number of polygons that use each
        // vertex; this information will be used to check that the mesh is manifold.
        map<VertexIter, Size> vertexDegree;

        // First, we do some basic sanity checks on the input.
        for (PolygonListCIter p = polygons.begin(); p != polygons.end(); p++)
        {
            if (p->size() < 3)
            {
                // Refuse to build the mesh if any of the polygons have fewer than three vertices.
                // (Note that if we omit this check the code will still construct something fairly
                // meaningful for 1- and 2-point polygons, but enforcing this stricter requirement
                // on the input will help simplify code further downstream, since it can be certain
                // it doesn't have to check for these rather degenerate cases.)
                cerr << "Error converting polygons to halfedge mesh: each polygon must have at least three vertices." << endl;
                return -1;
            }

            // We want to count the number of distinct vertex indices in this
            // polygon, to make sure it's the same as the number of vertices
            // in the polygon---if they disagree, then the polygon is not valid
            // (or at least, for simplicity we don't handle polygons of this type!).
            set<Index> polygonIndices;

            // loop over polygon vertices
            for (IndexListCIter i = p->begin(); i != p->end(); i++)
            {
                polygonIndices.insert(*i);

                // allocate one vertex for each new index we encounter
                if (indexToVertex.find(*i) == indexToVertex.end())
                {
                    VertexIter v = newVertex();
                    v->halfedge() = halfedges.end(); // this vertex doesn't yet point to any halfedge
                    indexToVertex[*i] = v;
                    vertexDegree[v] = 1; // we've now seen this vertex only once
                }
                else
                {
                    // keep track of the number of times we've seen this vertex
                    vertexDegree[indexToVertex[*i]]++;
                }

            } // end loop over polygon vertices

            // check that all vertices of the current polygon are distinct
            Size degree = p->size(); // number of vertices in this polygon
            if (polygonIndices.size() < degree)
            {
                cerr << "Error converting polygons to halfedge mesh: one of the input polygons does not have distinct vertices!" << endl;
                cerr << "(vertex indices:";
                for (IndexListCIter i = p->begin(); i != p->end(); i++)
                {
                    cerr << " " << *i;
                }
                cerr << ")" << endl;
                return -1;
            } // end check that polygon vertices are distinct

        } // end basic sanity checks on input

        // The number of vertices in the mesh is the
        // number of unique indices seen in the input.
        Size nVertices = indexToVertex.size();

        // The number of faces is just the number of polygons in the input.
        Size nFaces = polygons.size();
        faces.resize(nFaces); // allocate storage for faces in our new mesh

        // We will store a map from ordered pairs of vertex indices to
        // the corresponding halfedge object in our new (halfedge) mesh;
        // this map gets constructed during the next loop over polygons.
        map<IndexPair, HalfedgeIter> pairToHalfedge;

        // Next, we actually build the halfedge connectivity by again looping over polygons
        PolygonListCIter p;
        FaceIter f;
        for (p = polygons.begin(), f = faces.begin();
            p != polygons.end();
            p++, f++)
        {
            vector<HalfedgeIter> faceHalfedges; // cyclically ordered list of the half edges of this face
            Size degree = p->size(); // number of vertices in this polygon

            // loop over the halfedges of this face (equivalently, the ordered pairs of consecutive vertices)
            for (Index i = 0; i < degree; i++)
            {
                Index a = (*p)[i]; // current index
                Index b = (*p)[(i + 1) % degree]; // next index, in cyclic order
                IndexPair ab(a, b);
                HalfedgeIter hab;

                // check if this halfedge already exists; if so, we have a problem!
                if (pairToHalfedge.find(ab) != pairToHalfedge.end())
                {
                    cerr << "Error converting polygons to halfedge mesh: found multiple oriented edges with indices (" << a << ", " << b << ")." << endl;
                    cerr << "This means that either (i) more than two faces contain this edge (hence the surface is nonmanifold), or" << endl;
                    cerr << "(ii) there are exactly two faces containing this edge, but they have the same orientation (hence the surface is" << endl;
                    cerr << "not consistently oriented." << endl;
                    return -1;
                }
                else // otherwise, the halfedge hasn't been allocated yet
                {
                    // so, we point this vertex pair to a new halfedge
                    hab = newHalfedge();
                    pairToHalfedge[ab] = hab;

                    // link the new halfedge to its face
                    hab->face() = f;
                    hab->face()->halfedge() = hab;

                    // also link it to its starting vertex
                    hab->vertex() = indexToVertex[a];
                    hab->vertex()->halfedge() = hab;

                    // keep a list of halfedges in this face, so that we can later
                    // link them together in a loop (via their "next" pointers)
                    faceHalfedges.push_back(hab);
                }

                // Also, check if the twin of this halfedge has already been constructed (during
                // construction of a different face).  If so, link the twins together and allocate
                // their shared halfedge.  By the end of this pass over polygons, the only halfedges
                // that will not have a twin will hence be those that sit along the domain boundary.
                IndexPair ba(b, a);
                map<IndexPair, HalfedgeIter>::iterator iba = pairToHalfedge.find(ba);
                if (iba != pairToHalfedge.end())
                {
                    HalfedgeIter hba = iba->second;

                    // link the twins
                    hab->twin() = hba;
                    hba->twin() = hab;

                    // allocate and link their edge
                    EdgeIter e = newEdge();
                    hab->edge() = e;
                    hba->edge() = e;
                    e->halfedge() = hab;
                }
                else // If we didn't find a twin...
                {
                    // ...mark this halfedge as being twinless by pointing
                    // it to the end of the list of halfedges. If it remains
                    // twinless by the end of the current loop over polygons,
                    // it will be linked to a boundary face in the next pass.
                    hab->twin() = halfedges.end();
                }

            } // end loop over the current polygon's halfedges

            // Now that all the halfedges of this face have been allocated,
            // we can link them together via their "next" pointers.
            for (Index i = 0; i < degree; i++)
            {
                Index j = (i + 1) % degree; // index of the next halfedge, in cyclic order
                faceHalfedges[i]->next() = faceHalfedges[j];
            }

        } // done building basic halfedge connectivity

        // For each vertex on the boundary, advance its halfedge pointer to one that is also on the boundary.
        for (VertexIter v = verticesBegin(); v != verticesEnd(); v++)
        {
            // loop over halfedges around vertex
            HalfedgeIter h = v->halfedge();
            do
            {
                if (h->twin() == halfedges.end())
                {
                    v->halfedge() = h;
                    break;
                }

                h = h->twin()->next();
            } while (h != v->halfedge()); // end loop over halfedges around vertex

        } // done advancing halfedge pointers for boundary vertices

        // Next we construct new faces for each boundary component.
        for (HalfedgeIter h = halfedgesBegin(); h != halfedgesEnd(); h++) // loop over all halfedges
        {
            // Any halfedge that does not yet have a twin is on the boundary of the domain.
            // If we follow the boundary around long enough we will of course eventually make a
            // closed loop; we can represent this boundary loop by a new face. To make clear the
            // distinction between faces and boundary loops, the boundary face will (i) have a flag
            // indicating that it is a boundary loop, and (ii) be stored in a list of boundaries,
            // rather than the usual list of faces.  The reason we need the both the flag *and* the
            // separate list is that faces are often accessed in two fundamentally different ways:
            // either by (i) local traversal of the neighborhood of some mesh element using the
            // halfedge structure, or (ii) global traversal of all faces (or boundary loops).
            if (h->twin() == halfedges.end())
            {
                FaceIter b = newBoundary();
                vector<HalfedgeIter> boundaryHalfedges; // keep a list of halfedges along the boundary, so we can link them together

                // We now need to walk around the boundary, creating new
                // halfedges and edges along the boundary loop as we go.
                HalfedgeIter i = h;
                do
                {
                    // create a twin, which becomes a halfedge of the boundary loop
                    HalfedgeIter t = newHalfedge();
                    boundaryHalfedges.push_back(t); // keep a list of all boundary halfedges, in cyclic order
                    i->twin() = t;
                    t->twin() = i;
                    t->face() = b;
                    t->vertex() = i->next()->vertex();

                    // create the shared edge
                    EdgeIter e = newEdge();
                    e->halfedge() = i;
                    i->edge() = e;
                    t->edge() = e;

                    // Advance i to the next halfedge along the current boundary loop
                    // by walking around its target vertex and stopping as soon as we
                    // find a halfedge that does not yet have a twin defined.
                    i = i->next();
                    while (i != h && // we're done if we end up back at the beginning of the loop
                        i->twin() != halfedges.end()) // otherwise, we're looking for the next twinless halfedge along the loop
                    {
                        i = i->twin();
                        i = i->next();
                    }
                } while (i != h);

                // The only pointers that still need to be set are the "next" pointers of the twins;
                // these we can set from the list of boundary halfedges, but we must use the opposite
                // order from the order in the list, since the orientation of the boundary loop is
                // opposite the orientation of the halfedges "inside" the domain boundary.
                Size degree = boundaryHalfedges.size();
                for (Index p = 0; p < degree; p++)
                {
                    Index q = (p - 1 + degree) % degree;
                    boundaryHalfedges[p]->next() = boundaryHalfedges[q];
                }

            } // end construction of one of the boundary loops

            // Note that even though we are looping over all halfedges, we will still construct
            // the appropriate number of boundary loops (and not, say, one loop per boundary
            // halfedge).  The reason is that as we continue to iterate through halfedges, we
            // check whether their twin has been assigned, and since new twins may have been
            // assigned earlier in this loop, we will end up skipping many subsequent halfedges.

        } // done adding "virtual" faces corresponding to boundary loops

        // To make later traversal of the mesh easier, we will now advance the halfedge
        // associated with each vertex such that it refers to the *first* non-boundary
        // halfedge, rather than the last one.
        for (VertexIter v = verticesBegin(); v != verticesEnd(); v++)
        {
            v->halfedge() = v->halfedge()->twin()->next();
        }

        // Finally, we check that all vertices are manifold.
        for (VertexIter v = vertices.begin(); v != vertices.end(); v++)
        {
            // First check that this vertex is not a "floating" vertex;
            // if it is then we do not have a valid 2-manifold surface.
            if (v->halfedge() == halfedges.end())
            {
                cerr << "Error converting polygons to halfedge mesh: some vertices are not referenced by any polygon." << endl;
                return -1;
            }

            // Next, check that the number of halfedges emanating from this vertex in our half
            // edge data structure equals the number of polygons containing this vertex, which
            // we counted during our first pass over the mesh.  If not, then our vertex is not
            // a "fan" of polygons, but instead has some other (nonmanifold) structure.
            Size count = 0;
            HalfedgeIter h = v->halfedge();
            do
            {
                if (!h->face()->isBoundary())
                {
                    count++;
                }
                h = h->twin()->next();
            } while (h != v->halfedge());

            if (count != vertexDegree[v])
            {
                cerr << "Error converting polygons to halfedge mesh: at least one of the vertices is nonmanifold." << endl;
                return -1;
            }
        } // end loop over vertices

        // Now that we have the connectivity, we copy the list of vertex
        // positions into member variables of the individual vertices.
        if (vertexPositions.size() != vertices.size())
        {
            cerr << "Error converting polygons to halfedge mesh: number of vertex positions is different from the number of distinct vertices!" << endl;
            cerr << "(number of positions in input: " << vertexPositions.size() << ")" << endl;
            cerr << "(  number of vertices in mesh: " << vertices.size() << ")" << endl;
            return -1;
        }
        // Since an STL map internally sorts its keys, we can iterate over the map from vertex indices to
        // vertex iterators to visit our (input) vertices in lexicographic order
        int i = 0;
        for (map<Index, VertexIter>::const_iterator e = indexToVertex.begin(); e != indexToVertex.end(); e++)
        {
            // grab a pointer to the vertex associated with the current key (i.e., the current index)
            VertexIter v = e->second;

            // set the position of this vertex to the corresponding position in the input
            v->position = vertexPositions[i];
            i++;
        }
        return 0;
    } // end HalfedgeMesh::build()

    const HalfedgeMesh& HalfedgeMesh :: operator=(const HalfedgeMesh& mesh)
        // The assignment operator does a "deep" copy of the halfedge mesh data structure; in
        // other words, it makes new instances of each mesh element, and ensures that pointers
        // in the copy point to the newly allocated elements rather than elements in the original
        // mesh.  This behavior is especially important for making assignments, since the mesh
        // on the right-hand side of an assignment may be temporary (hence any pointers to elements
        // in this mesh will become invalid as soon as it is released.)
    {
        // Clear any existing elements.
        halfedges.clear();
        vertices.clear();
        edges.clear();
        faces.clear();
        boundaries.clear();

        // These maps will be used to identify elements of the old mesh
        // with elements of the new mesh.  (Note that we can use a single
        // map for both interior and boundary faces, because the map
        // doesn't care which list of faces these iterators come from.)
        map< HalfedgeCIter, HalfedgeIter > halfedgeOldToNew;
        map<   VertexCIter, VertexIter >   vertexOldToNew;
        map<     EdgeCIter, EdgeIter >     edgeOldToNew;
        map<     FaceCIter, FaceIter >     faceOldToNew;

        // Copy geometry from the original mesh and create a map from
        // pointers in the original mesh to those in the new mesh.
        for (HalfedgeCIter h = mesh.halfedgesBegin(); h != mesh.halfedgesEnd(); h++) halfedgeOldToNew[h] = halfedges.insert(halfedges.end(), *h);
        for (VertexCIter v = mesh.verticesBegin(); v != mesh.verticesEnd(); v++)   vertexOldToNew[v] = vertices.insert(vertices.end(), *v);
        for (EdgeCIter e = mesh.edgesBegin(); e != mesh.edgesEnd(); e++)     edgeOldToNew[e] = edges.insert(edges.end(), *e);
        for (FaceCIter f = mesh.facesBegin(); f != mesh.facesEnd(); f++)     faceOldToNew[f] = faces.insert(faces.end(), *f);
        for (FaceCIter b = mesh.boundariesBegin(); b != mesh.boundariesEnd(); b++)     faceOldToNew[b] = boundaries.insert(boundaries.end(), *b);

        // "Search and replace" old pointers with new ones.
        for (HalfedgeIter he = halfedgesBegin(); he != halfedgesEnd(); he++)
        {
            he->next() = halfedgeOldToNew[he->next()];
            he->twin() = halfedgeOldToNew[he->twin()];
            he->vertex() = vertexOldToNew[he->vertex()];
            he->edge() = edgeOldToNew[he->edge()];
            he->face() = faceOldToNew[he->face()];
        }
        for (VertexIter v = verticesBegin(); v != verticesEnd(); v++) v->halfedge() = halfedgeOldToNew[v->halfedge()];
        for (EdgeIter e = edgesBegin(); e != edgesEnd(); e++) e->halfedge() = halfedgeOldToNew[e->halfedge()];
        for (FaceIter f = facesBegin(); f != facesEnd(); f++) f->halfedge() = halfedgeOldToNew[f->halfedge()];
        for (FaceIter b = boundariesBegin(); b != boundariesEnd(); b++) b->halfedge() = halfedgeOldToNew[b->halfedge()];

        // Return a reference to the new mesh.
        return *this;
    }

    HalfedgeMesh::HalfedgeMesh(const HalfedgeMesh& mesh)
    {
        *this = mesh;
    }

    EdgeIter HalfedgeMesh::flipEdge(EdgeIter e0) {
        // TODO Part 4.
        // This method should flip the given edge and return an iterator to the flipped edge.

        // Diagram source: https://cmu-graphics.github.io/Scotty3D/meshedit/local/edge_flip_diagram.png 

        // First get all the elements
        // Get all the halfedges
        HalfedgeIter h0 = e0->halfedge();
        HalfedgeIter h1 = h0->next();
        HalfedgeIter h2 = h1->next();

        HalfedgeIter h3 = h0->twin();
        HalfedgeIter h4 = h3->next();
        HalfedgeIter h5 = h4->next();

        HalfedgeIter h6 = h1->twin();
        HalfedgeIter h7 = h2->twin();
        HalfedgeIter h8 = h4->twin();
        HalfedgeIter h9 = h5->twin();

        // Get all vertices
        VertexIter v0 = h0->vertex();
        VertexIter v1 = h3->vertex();
        VertexIter v2 = h5->vertex();
        VertexIter v3 = h2->vertex();

        // Get all the edges
        EdgeIter e1 = h5->edge();
        EdgeIter e2 = h4->edge();
        EdgeIter e3 = h2->edge();
        EdgeIter e4 = h1->edge();

        // Get all the faces
        FaceIter f0 = h0->face();
        FaceIter f1 = h3->face();

        // Check for boundary
        if (f0->isBoundary() || f1->isBoundary()) return e0;

        // Now make the changes and reassign stuff
        // Edit halfedges
        h0->setNeighbors(h2, h3, v2, e0, f0); // next, twin, vertex, edge, face;
        h1->setNeighbors(h3, h6, v1, e4, f1);
        h2->setNeighbors(h4, h7, v3, e3, f0);
        h3->setNeighbors(h5, h0, v3, e0, f1);
        h4->setNeighbors(h0, h8, v0, e2, f0);
        h5->setNeighbors(h1, h9, v2, e1, f1);

        // Edit vertices
        v0->halfedge() = h4;
        v1->halfedge() = h1;
        v2->halfedge() = h5;
        v3->halfedge() = h2;

        // Edit faces
        f0->halfedge() = h0;
        f1->halfedge() = h3;

        return e0;
    }

    VertexIter HalfedgeMesh::splitEdge(EdgeIter e0) {
        // TODO Part 5.
        // This method should split the given edge and return an iterator to the newly inserted vertex.
        // The halfedge of this vertex should point along the edge that was split, rather than the new edges.

        // Diagram source: https://cmu-graphics.github.io/Scotty3D/meshedit/local/edge_flip_diagram.png 
        // Using the same initial diagram. The new post-split dia is in the gh pages
        // 
        // First get all the elements
        // Get all the halfedges

        HalfedgeIter h0 = e0->halfedge();
        HalfedgeIter h1 = h0->next();
        HalfedgeIter h2 = h1->next();

        HalfedgeIter h3 = h0->twin();
        HalfedgeIter h4 = h3->next();
        HalfedgeIter h5 = h4->next();

        HalfedgeIter h6 = h1->twin();
        HalfedgeIter h7 = h2->twin();
        HalfedgeIter h8 = h4->twin();
        HalfedgeIter h9 = h5->twin();

        // Get all vertices
        VertexIter v0 = h0->vertex();
        VertexIter v1 = h3->vertex();
        VertexIter v2 = h5->vertex();
        VertexIter v3 = h2->vertex();

        // Get all the edges
        EdgeIter e1 = h5->edge();
        EdgeIter e2 = h4->edge();
        EdgeIter e3 = h2->edge();
        EdgeIter e4 = h1->edge();

        // Get all the faces
        FaceIter f0 = h0->face();
        FaceIter f1 = h3->face();

        // Check for boundary
        if (f0->isBoundary() || f1->isBoundary()) return v0;

        // Create new elements
        HalfedgeIter h10 = newHalfedge();
        HalfedgeIter h11 = newHalfedge();
        HalfedgeIter h12 = newHalfedge();
        HalfedgeIter h13 = newHalfedge();
        HalfedgeIter h14 = newHalfedge();
        HalfedgeIter h15 = newHalfedge();

        VertexIter v4 = newVertex();

        EdgeIter e5 = newEdge();
        EdgeIter e6 = newEdge();
        EdgeIter e7 = newEdge();

        FaceIter f2 = newFace();
        FaceIter f3 = newFace();


        // Do assignments
        // Halfedges
        h0->setNeighbors(h1, h3, v4, e0, f0); // next, twin, vertex, edge, face;
        h1->setNeighbors(h10, h6, v1, e4, f0);
        h2->setNeighbors(h14, h7, v3, e3, f2);
        h3->setNeighbors(h12, h0, v1, e0, f1);
        h4->setNeighbors(h13, h8, v0, e2, f3);
        h5->setNeighbors(h3, h9, v2, e1, f1);

        //h6->setNeighbors(h, h, v, e, f);
        //h7->setNeighbors(h, h, v, e, f);
        //h8->setNeighbors(h, h, v, e, f);
        //h9->setNeighbors(h, h, v, e, f);

        h10->setNeighbors(h0, h11, v3, e5, f0);
        h11->setNeighbors(h2, h10, v4, e5, f2);
        h12->setNeighbors(h5, h13, v4, e6, f1);
        h13->setNeighbors(h15, h12, v2, e6, f3);
        h14->setNeighbors(h11, h15, v0, e7, f2);
        h15->setNeighbors(h4, h14, v4, e7, f3);

        // Vertices
        v0->halfedge() = h14;
        v1->halfedge() = h3;
        v2->halfedge() = h13;
        v3->halfedge() = h10;
        v4->halfedge() = h15;

        v4->position = (v0->position + v1->position) / 2;
        //v4->position = e0->newPosition;

        e0->halfedge() = h0;
        e1->halfedge() = h5;
        e2->halfedge() = h4;
        e3->halfedge() = h2;
        e4->halfedge() = h1;
        e5->halfedge() = h10;
        e6->halfedge() = h12;
        e7->halfedge() = h14;

        // TODO: Is this necessary?
        e7->newPosition = e0->newPosition;
        e7->isNew = e0->isNew;

        // Faces
        f0->halfedge() = h0;
        f1->halfedge() = h3;
        f2->halfedge() = h14;
        f3->halfedge() = h15;

        return v4;
    }

    VertexIter HalfedgeMesh::collapseEdge(EdgeIter e0) {
        // https://cmu-graphics.github.io/Scotty3D/meshedit/local/edge_flip_diagram.png

        // should use this one
        // https://cmu-graphics.github.io/Scotty3D/meshedit/local/collapse_edge.svg
        HalfedgeIter h0 = e0->halfedge();
        HalfedgeIter h1 = h0->next();
        HalfedgeIter h2 = h1->next();

        HalfedgeIter h3 = h0->twin();
        HalfedgeIter h4 = h3->next();
        HalfedgeIter h5 = h4->next();

        HalfedgeIter h6 = h1->twin();
        HalfedgeIter h7 = h2->twin();
        HalfedgeIter h8 = h4->twin();
        HalfedgeIter h9 = h5->twin();

        // Get all vertices
        VertexIter v0 = h0->vertex();
        VertexIter v1 = h3->vertex();
        VertexIter v2 = h5->vertex();
        VertexIter v3 = h2->vertex();

        // Get all the edges
        EdgeIter e1 = h5->edge();
        EdgeIter e2 = h4->edge();
        EdgeIter e3 = h2->edge();
        EdgeIter e4 = h1->edge();

        // Get all the faces
        FaceIter f0 = h0->face();
        FaceIter f1 = h3->face();

      /*  std::cout << "face degree 1: " << f0->degree() << std::endl;
        std::cout << "face degree 2: " << f1->degree() << std::endl;
        std::cout << "v degree 1: " << v0->degree() << std::endl;
        std::cout << "v degree 2: " << v1->degree() << std::endl;*/
       
        // Fix halfedges
         h6->setNeighbors(h6->next(), h7, v3, e3, h6->face()); // next, twin, vertex, edge, face;
         h7->setNeighbors(h7->next(), h6, v0, e3, h7->face()); // next, twin, vertex, edge, face;

         h8->setNeighbors(h8->next(), h9, v2, e2, h8->face()); // next, twin, vertex, edge, face;
         h9->setNeighbors(h9->next(), h8, v0, e2, h9->face()); // next, twin, vertex, edge, face;


        // For every halfedge from v1, set its origin to v0
         HalfedgeIter h = h9->twin()->next();
         do
         {
             h->vertex() = v0;
             h = h->twin()->next();

         } while (h != h9);
         //std::cout << "step 2: "<< std::endl;
       
         // Fix Vertices
         v0->halfedge() = h7;
         v2->halfedge() = h8;
         v3->halfedge() = h6;
         v0->position = (v0->position + v1->position);

         // Fix Edges
         e2->halfedge() = h8;
         e3->halfedge() = h7;

   
         //std::cout << "step 3: "  << std::endl;

        
         // Delete stuff
         deleteHalfedge(h0);

         //std::cout << "step 4: "  << std::endl;
         deleteHalfedge(h1);

         //std::cout << "step 5: "  << std::endl;
         deleteHalfedge(h2);
         deleteHalfedge(h3);
         deleteHalfedge(h4);
         deleteHalfedge(h5);

         deleteVertex(v1);

         deleteEdge(e0);
         deleteEdge(e1);
         deleteEdge(e4);
         deleteFace(f0);
         deleteFace(f1);
         
        std::cout << "finish collapse once " << std::endl;
        return v0;
    }

    VertexIter HalfedgeMesh::avgVertex(VertexIter v) {
        return VertexIter();
    }

} // End of CMU 462 namespace.