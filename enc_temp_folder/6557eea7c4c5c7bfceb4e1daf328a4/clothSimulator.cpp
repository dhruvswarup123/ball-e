#include <cmath>
#include <glad/glad.h>

#include <CGL/vector3D.h>
#include <nanogui/nanogui.h>

#include "clothSimulator.h"

#include "camera.h"
#include "cloth.h"
#include "collision/plane.h"
#include "collision/sphere.h"
#include "misc/camera_info.h"
#include "misc/file_utils.h"
// Needed to generate stb_image binaries. Should only define in exactly one source file importing stb_image.h.
#define STB_IMAGE_IMPLEMENTATION
#include "misc/stb_image.h"

#include "bmesh/bmesh.h"
using namespace nanogui;
using namespace std;

Vector3D load_texture(int frame_idx, GLuint handle, const char* where)
{
	Vector3D size_retval;

	if (strlen(where) == 0)
		return size_retval;

	glActiveTexture(GL_TEXTURE0 + frame_idx);
	glBindTexture(GL_TEXTURE_2D, handle);

	int img_x, img_y, img_n;
	unsigned char* img_data = stbi_load(where, &img_x, &img_y, &img_n, 3);
	size_retval.x = img_x;
	size_retval.y = img_y;
	size_retval.z = img_n;
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, img_x, img_y, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
	stbi_image_free(img_data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	return size_retval;
}

void load_cubemap(int frame_idx, GLuint handle, const std::vector<std::string>& file_locs)
{
	glActiveTexture(GL_TEXTURE0 + frame_idx);
	glBindTexture(GL_TEXTURE_CUBE_MAP, handle);
	for (int side_idx = 0; side_idx < 6; ++side_idx)
	{

		int img_x, img_y, img_n;
		unsigned char* img_data = stbi_load(file_locs[side_idx].c_str(), &img_x, &img_y, &img_n, 3);
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + side_idx, 0, GL_RGB, img_x, img_y, 0, GL_RGB, GL_UNSIGNED_BYTE, img_data);
		stbi_image_free(img_data);
		std::cout << "Side " << side_idx << " has dimensions " << img_x << ", " << img_y << std::endl;

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	}
}

void ClothSimulator::load_textures()
{
	glGenTextures(1, &m_gl_texture_1);
	glGenTextures(1, &m_gl_texture_2);
	glGenTextures(1, &m_gl_texture_3);
	glGenTextures(1, &m_gl_texture_4);
	glGenTextures(1, &m_gl_cubemap_tex);

	m_gl_texture_1_size = load_texture(1, m_gl_texture_1, (m_project_root + "/textures/texture_1.png").c_str());
	m_gl_texture_2_size = load_texture(2, m_gl_texture_2, (m_project_root + "/textures/texture_2.png").c_str());
	m_gl_texture_3_size = load_texture(3, m_gl_texture_3, (m_project_root + "/textures/texture_3.png").c_str());
	m_gl_texture_4_size = load_texture(4, m_gl_texture_4, (m_project_root + "/textures/texture_4.png").c_str());

	std::cout << "Texture 1 loaded with size: " << m_gl_texture_1_size << std::endl;
	std::cout << "Texture 2 loaded with size: " << m_gl_texture_2_size << std::endl;
	std::cout << "Texture 3 loaded with size: " << m_gl_texture_3_size << std::endl;
	std::cout << "Texture 4 loaded with size: " << m_gl_texture_4_size << std::endl;

	std::vector<std::string> cubemap_fnames = {
		m_project_root + "/textures/cube/posx.jpg",
		m_project_root + "/textures/cube/negx.jpg",
		m_project_root + "/textures/cube/posy.jpg",
		m_project_root + "/textures/cube/negy.jpg",
		m_project_root + "/textures/cube/posz.jpg",
		m_project_root + "/textures/cube/negz.jpg" };

	load_cubemap(5, m_gl_cubemap_tex, cubemap_fnames);
	std::cout << "Loaded cubemap texture" << std::endl;
}

void ClothSimulator::load_shaders()
{
	std::set<std::string> shader_folder_contents;
	bool success = FileUtils::list_files_in_directory(m_project_root + "/shaders", shader_folder_contents);
	if (!success)
	{
		std::cout << "Error: Could not find the shaders folder!" << std::endl;
	}

	std::string std_vert_shader = m_project_root + "/shaders/Default.vert";

	for (const std::string& shader_fname : shader_folder_contents)
	{
		std::string file_extension;
		std::string shader_name;

		FileUtils::split_filename(shader_fname, shader_name, file_extension);

		if (file_extension != "frag")
		{
			std::cout << "Skipping non-shader file: " << shader_fname << std::endl;
			continue;
		}

		std::cout << "Found shader file: " << shader_fname << std::endl;

		// Check if there is a proper .vert shader or not for it
		std::string vert_shader = std_vert_shader;
		std::string associated_vert_shader_path = m_project_root + "/shaders/" + shader_name + ".vert";
		if (FileUtils::file_exists(associated_vert_shader_path))
		{
			vert_shader = associated_vert_shader_path;
		}

		std::shared_ptr<GLShader> nanogui_shader = make_shared<GLShader>();
		nanogui_shader->initFromFiles(shader_name, vert_shader,
			m_project_root + "/shaders/" + shader_fname);

		// Special filenames are treated a bit differently
		ShaderTypeHint hint;
		if (shader_name == "Wireframe")
		{
			hint = ShaderTypeHint::WIREFRAME;
			std::cout << "Type: Wireframe" << std::endl;
		}
		else if (shader_name == "Normal")
		{
			hint = ShaderTypeHint::NORMALS;
			std::cout << "Type: Normal" << std::endl;
		}
		else
		{
			hint = ShaderTypeHint::PHONG;
			std::cout << "Type: Custom" << std::endl;
		}

		UserShader user_shader(shader_name, nanogui_shader, hint);

		shaders.push_back(user_shader);
		shaders_combobox_names.push_back(shader_name);
	}

	// Assuming that it's there, use "Wireframe" by default
	for (size_t i = 0; i < shaders_combobox_names.size(); ++i)
	{
		if (shaders_combobox_names[i] == "Wireframe")
		{
			active_shader_idx = i;
			break;
		}
	}
}

ClothSimulator::ClothSimulator(std::string project_root, Screen* screen)
	: m_project_root(project_root)
{
	this->screen = screen;

	this->load_shaders();
	this->load_textures();

	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_DEPTH_TEST);
}

ClothSimulator::~ClothSimulator()
{
	for (auto shader : shaders)
	{
		shader.nanogui_shader->free();
	}
	glDeleteTextures(1, &m_gl_texture_1);
	glDeleteTextures(1, &m_gl_texture_2);
	glDeleteTextures(1, &m_gl_texture_3);
	glDeleteTextures(1, &m_gl_texture_4);
	glDeleteTextures(1, &m_gl_cubemap_tex);

	if (cloth)
		delete cloth;
	if (cp)
		delete cp;
	if (collision_objects)
		delete collision_objects;
}

void ClothSimulator::loadCloth(Cloth* cloth) { this->cloth = cloth; }

void ClothSimulator::loadClothParameters(ClothParameters* cp) { this->cp = cp; }

void ClothSimulator::loadCollisionObjects(vector<CollisionObject*>* objects) { this->collision_objects = objects; }

/**
 * Initializes the cloth simulation and spawns a new thread to separate
 * rendering from simulation.
 */
void ClothSimulator::init()
{
	bmesh = new Balle::BMesh();

	// Initialize GUI
	screen->setSize(default_window_size);
	initGUI(screen);

	// Initialize camera

	CGL::Collada::CameraInfo camera_info;
	camera_info.hFov = 50;
	camera_info.vFov = 35;
	camera_info.nClip = 0.01;
	camera_info.fClip = 10000;

	// Try to intelligently figure out the camera target

	Vector3D avg_pm_position(0, 0, 0);

	for (auto& pm : cloth->point_masses)
	{
		avg_pm_position += pm.position / cloth->point_masses.size();
	}

	CGL::Vector3D target(avg_pm_position.x, avg_pm_position.y / 2,
		avg_pm_position.z);
	CGL::Vector3D c_dir(0., 0., 0.);
	canonical_view_distance = max(cloth->width, cloth->height) * 0.9;
	scroll_rate = canonical_view_distance / 10;

	view_distance = canonical_view_distance * 2;
	min_view_distance = canonical_view_distance / 10.0;
	max_view_distance = canonical_view_distance * 20.0;

	// canonicalCamera is a copy used for view resets

	camera.place(target, acos(c_dir.y), atan2(c_dir.x, c_dir.z), view_distance,
		min_view_distance, max_view_distance);
	canonicalCamera.place(target, acos(c_dir.y), atan2(c_dir.x, c_dir.z),
		view_distance, min_view_distance, max_view_distance);

	canonicalCamera_xy.place(target, acos(c_dir.y), atan2(c_dir.x, c_dir.z),
		view_distance, min_view_distance, max_view_distance);
	canonicalCamera_xy.rotate_by(-PI / 2, 0);

	canonicalCamera_yz.place(target, acos(c_dir.y), atan2(c_dir.x, c_dir.z),
		view_distance, min_view_distance, max_view_distance);
	canonicalCamera_yz.rotate_by(0, -PI / 2);

	screen_w = default_window_size(0);
	screen_h = default_window_size(1);

	camera.configure(camera_info, screen_w, screen_h);
	canonicalCamera.configure(camera_info, screen_w, screen_h);
}

bool ClothSimulator::isAlive() { return is_alive; }

void ClothSimulator::drawContents()
{
	glEnable(GL_DEPTH_TEST);

	if (!is_paused)
	{
		vector<Vector3D> external_accelerations = { gravity };

		for (int i = 0; i < simulation_steps; i++)
		{
			cloth->simulate(frames_per_sec, simulation_steps, cp, external_accelerations, collision_objects);
		}
	}

	// Bind the active shader

	const UserShader& active_shader = shaders[active_shader_idx];

	GLShader& shader = *active_shader.nanogui_shader;
	shader.bind();

	// Prepare the camera projection matrix

	Matrix4f model;
	model.setIdentity();

	Matrix4f view = getViewMatrix();
	Matrix4f projection = getProjectionMatrix();

	Matrix4f viewProjection = projection * view;

	shader.setUniform("u_model", model);
	shader.setUniform("u_view_projection", viewProjection);
	Vector3D cam_pos;

	switch (active_shader.type_hint)
	{
	case WIREFRAME:
		color[0] = 0.;
		cam_pos = camera.position();
		shader.setUniform("u_cam_pos", Vector3f(cam_pos.x, cam_pos.y, cam_pos.z), false);
		shader.setUniform("u_light_pos", Vector3f(0.5, 2, 2), false);
		shader.setUniform("u_light_intensity", Vector3f(10, 10, 10), false);

		shader.setUniform("u_color", color, false);
		drawWireframe(shader);
		break;
	case NORMALS:
		drawNormals(shader);
		break;
	case PHONG:

		// Others
		cam_pos = camera.position();
		shader.setUniform("u_color", color, false);
		shader.setUniform("u_cam_pos", Vector3f(cam_pos.x, cam_pos.y, cam_pos.z), false);
		shader.setUniform("u_light_pos", Vector3f(0.5, 2, 2), false);
		shader.setUniform("u_light_intensity", Vector3f(3, 3, 3), false);
		shader.setUniform("u_texture_1_size", Vector2f(m_gl_texture_1_size.x, m_gl_texture_1_size.y), false);
		shader.setUniform("u_texture_2_size", Vector2f(m_gl_texture_2_size.x, m_gl_texture_2_size.y), false);
		shader.setUniform("u_texture_3_size", Vector2f(m_gl_texture_3_size.x, m_gl_texture_3_size.y), false);
		shader.setUniform("u_texture_4_size", Vector2f(m_gl_texture_4_size.x, m_gl_texture_4_size.y), false);
		// Textures
		shader.setUniform("u_texture_1", 1, false);
		shader.setUniform("u_texture_2", 2, false);
		shader.setUniform("u_texture_3", 3, false);
		shader.setUniform("u_texture_4", 4, false);

		shader.setUniform("u_normal_scaling", m_normal_scaling, false);
		shader.setUniform("u_height_scaling", m_height_scaling, false);

		shader.setUniform("u_texture_cubemap", 5, false);
		drawPhong(shader);
		break;
	}

	for (CollisionObject* co : *collision_objects)
	{
		co->render(shader);
	}
}

void ClothSimulator::drawWireframe(GLShader& shader)
{
	bool mesh_ready = bmesh->mesh_ready;
	if (!mesh_ready)
	{
		// enable sphere rendering
		int numlinks = bmesh->getNumLinks();

		MatrixXf positions(4, numlinks * 2);
		MatrixXf normals(4, numlinks * 2);

		for (int i = 0; i < numlinks; i++)
		{
			normals.col(i * 2) << 0., 0., 0., 0.0;
			normals.col(i * 2 + 1) << 0., 0., 0., 0.0;
		}

		bmesh->fillPositions(positions);

		shader.uploadAttrib("in_position", positions, false);
		shader.uploadAttrib("in_normal", normals, false);

		shader.drawArray(GL_LINES, 0, numlinks * 2);

		bmesh->drawSpheres(shader);
	}
	else
	{
		// METHOD 1: Draw the polygons not using indices (WORKING)
		HalfedgeMesh* mesh = bmesh->mesh;
		MatrixXf mesh_positions(3, bmesh->triangles.size()*3 + bmesh->quadrangles.size() * 6);
		MatrixXf mesh_normals(3, bmesh->triangles.size()*3 + bmesh->quadrangles.size() * 6);

		int ind = 0;
		for (const vector<size_t>& polygon : bmesh->polygons)
		{
			if (polygon.size() == 3)
			{
				continue;
				Vector3D vertex0 = bmesh->vertices[polygon[0]];
				Vector3D vertex1 = bmesh->vertices[polygon[1]];
				Vector3D vertex2 = bmesh->vertices[polygon[2]];

				Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex0)).unit();

				mesh_positions.col(ind * 3) << vertex0.x, vertex0.y, vertex0.z;
				mesh_positions.col(ind * 3 + 1) << vertex1.x, vertex1.y, vertex1.z;
				mesh_positions.col(ind * 3 + 2) << vertex2.x, vertex2.y, vertex2.z;

				mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

				ind += 1;
			}
			else if (polygon.size() == 4) {
				Vector3D vertex0 = bmesh->vertices[polygon[0]];
				Vector3D vertex1 = bmesh->vertices[polygon[1]];
				Vector3D vertex2 = bmesh->vertices[polygon[2]];
				Vector3D vertex3 = bmesh->vertices[polygon[3]];
				Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex3) + cross(vertex3, vertex0)).unit();

				mesh_positions.col(ind * 3) << vertex0.x, vertex0.y, vertex0.z;
				mesh_positions.col(ind * 3 + 1) << vertex1.x, vertex1.y, vertex1.z;
				mesh_positions.col(ind * 3 + 2) << vertex2.x, vertex2.y, vertex2.z;

				mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

				ind += 1;

				mesh_positions.col(ind * 3) << vertex2.x, vertex2.y, vertex2.z;
				mesh_positions.col(ind * 3 + 1) << vertex3.x, vertex3.y, vertex3.z;
				mesh_positions.col(ind * 3 + 2) << vertex0.x, vertex0.y, vertex0.z;

				mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

				ind += 1;
			}
		}
		size_t actual_triangles_to_draw = ind*3;
		ind = 0;
		shader.uploadAttrib("in_normal", mesh_normals);
		shader.uploadAttrib("in_position", mesh_positions);
		shader.drawArray(GL_TRIANGLES, 0, actual_triangles_to_draw);

		// METHOD 2: Draw the mesh faces not using indices (WORKING)
		/*HalfedgeMesh* mesh = bmesh->mesh;
		MatrixXf mesh_positions(3, mesh->nFaces() * 6);
		MatrixXf mesh_normals(3, mesh->nFaces() * 6);

		int ind = 0;
		for (auto face = mesh->facesBegin(); face != mesh->facesEnd(); face++)
		{	
			Vector3D normal = face->normal();

			int deg = face->degree();
			if (deg == 3)
			{
				Vector3D v0 = face->halfedge()->vertex()->position;
				Vector3D v1 = face->halfedge()->next()->vertex()->position;
				Vector3D v2 = face->halfedge()->next()->next()->vertex()->position;

				mesh_positions.col(ind*3) << v0.x, v0.y, v0.z;
				mesh_positions.col(ind*3+1) << v1.x, v1.y, v1.z;
				mesh_positions.col(ind*3+2) << v2.x, v2.y, v2.z;

				mesh_normals.col(ind*3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind*3+1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind*3+2) << normal.x, normal.y, normal.z;

				ind += 1;
			}
			else if (deg == 4) {
				Vector3D v0 = face->halfedge()->vertex()->position;
				Vector3D v1 = face->halfedge()->next()->vertex()->position;
				Vector3D v2 = face->halfedge()->next()->next()->vertex()->position;
				Vector3D v3 = face->halfedge()->next()->next()->next()->vertex()->position;

				mesh_positions.col(ind * 3) << v0.x, v0.y, v0.z;
				mesh_positions.col(ind * 3 + 1) << v1.x, v1.y, v1.z;
				mesh_positions.col(ind * 3 + 2) << v2.x, v2.y, v2.z;

				mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

				ind += 1;

				mesh_positions.col(ind * 3) << v2.x, v2.y, v2.z;
				mesh_positions.col(ind * 3 + 1) << v3.x, v3.y, v3.z;
				mesh_positions.col(ind * 3 + 2) << v0.x, v0.y, v0.z;

				mesh_normals.col(ind * 3) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 1) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind * 3 + 2) << normal.x, normal.y, normal.z;

				ind += 1;
			}
		}

		shader.uploadAttrib("in_normal", mesh_normals);
		shader.uploadAttrib("in_position", mesh_positions);
		shader.drawArray(GL_TRIANGLES, 0, ind*3);
		*/


		// METHOD 3: Draw the polygons using indices (NOT WORKING)
		/*
		HalfedgeMesh* mesh = bmesh->mesh;
		MatrixXu mesh_indices(3, bmesh->triangles.size() + bmesh->quadrangles.size() * 2);
		MatrixXf mesh_positions(3, bmesh->vertices.size());
		MatrixXf mesh_normals(3, bmesh->triangles.size() + bmesh->quadrangles.size() * 2);

		int ind = 0;
		for (const vector<size_t>& polygon : bmesh->polygons)
		{
			if (polygon.size() == 3)
			{
				mesh_indices.col(ind) << polygon[0], polygon[1], polygon[2];

				Vector3D vertex0 = bmesh->vertices[polygon[0]];
				Vector3D vertex1 = bmesh->vertices[polygon[1]];
				Vector3D vertex2 = bmesh->vertices[polygon[2]];

				Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex0)).unit();

				//Vector3D normal = cross(vertex0 - vertex1, vertex0 - vertex2).unit();
				////////////////// HERE THE NORMALS DO NOT CORRESPOND TO EACH VERTEX
				mesh_normals.col(ind) << normal.x, normal.y, normal.z;
				ind += 1;
			}
			else if (polygon.size() == 4) {
				mesh_indices.col(ind) << polygon[0], polygon[1], polygon[2];
				mesh_indices.col(ind + 1) << polygon[2], polygon[3], polygon[0];

				Vector3D vertex0 = bmesh->vertices[polygon[0]];
				Vector3D vertex1 = bmesh->vertices[polygon[1]];
				Vector3D vertex2 = bmesh->vertices[polygon[2]];
				Vector3D vertex3 = bmesh->vertices[polygon[3]];
				Vector3D normal = (cross(vertex0, vertex1) + cross(vertex1, vertex2) + cross(vertex2, vertex3) + cross(vertex3, vertex0)).unit();

				//Vector3D normal = cross(vertex3 - vertex2, vertex1 - vertex2).unit();

				mesh_normals.col(ind) << normal.x, normal.y, normal.z;
				mesh_normals.col(ind + 1) << normal.x, normal.y, normal.z;
				ind += 2;
			}
		}
		size_t actual_triangles_to_draw = ind;
		ind = 0;
		for (const Vector3D& vertex : bmesh->vertices)
		{
			mesh_positions.col(ind) << vertex.x, vertex.y, vertex.z;

			ind += 1;
		}
		shader.bind();
		shader.uploadIndices(mesh_indices);
		shader.uploadAttrib("in_normal", mesh_normals);
		shader.uploadAttrib("in_position", mesh_positions);
		shader.drawIndexed(GL_TRIANGLES, 0, actual_triangles_to_draw);
		*/

		// METHOD 4: Draw the Wireframe not using indices (WORKING)
		/*
		MatrixXf mesh_positions(3, bmesh->mesh->nEdges()*2);
		MatrixXf mesh_normals(3, bmesh->mesh->nEdges()*2);

		int ind = 0;
		for (EdgeIter i = bmesh->mesh->edgesBegin(); i != bmesh->mesh->edgesEnd(); i++) {
			// TODO: Fix this for faces
			Vector3D vertex1 = i->halfedge()->vertex()->position;
			Vector3D vertex2 = i->halfedge()->next()->vertex()->position;

			// Vector3D normal = i->face()->normal();

			mesh_positions.col(ind) << vertex1.x, vertex1.y, vertex1.z;
			mesh_positions.col(ind + 1) << vertex2.x, vertex2.y, vertex2.z;

			mesh_normals.col(ind) << 0., 0., 0.;
			mesh_normals.col(ind + 1) << 0., 0., 0.;

			ind += 2;
		}

		shader.uploadAttrib("in_position", mesh_positions, false);
		shader.uploadAttrib("in_normal", mesh_normals, false);

		shader.drawArray(GL_LINES, 0, bmesh->mesh->nEdges()*2);
		*/
		
		// METHOD 5: Draw the spheres too (WORKING)
		/*
		int numlinks = bmesh->getNumLinks();

		MatrixXf positions(4, numlinks * 2);
		MatrixXf normals(4, numlinks * 2);

		for (int i = 0; i < numlinks; i++)
		{
			normals.col(i * 2) << 0., 0., 0., 0.0;
			normals.col(i * 2 + 1) << 0., 0., 0., 0.0;
		}

		bmesh->fillPositions(positions);

		shader.uploadAttrib("in_position", positions, false);
		shader.uploadAttrib("in_normal", normals, false);

		shader.drawArray(GL_LINES, 0, numlinks * 2);

		bmesh->drawSpheres(shader);
		*/
	}
}

void ClothSimulator::drawNormals(GLShader& shader)
{
	int num_tris = cloth->clothMesh->triangles.size();

	MatrixXf positions(4, num_tris * 3);
	MatrixXf normals(4, num_tris * 3);

	for (int i = 0; i < num_tris; i++)
	{
		Triangle* tri = cloth->clothMesh->triangles[i];

		Vector3D p1 = tri->pm1->position;
		Vector3D p2 = tri->pm2->position;
		Vector3D p3 = tri->pm3->position;

		Vector3D n1 = tri->pm1->normal();
		Vector3D n2 = tri->pm2->normal();
		Vector3D n3 = tri->pm3->normal();

		positions.col(i * 3) << p1.x, p1.y, p1.z, 1.0;
		positions.col(i * 3 + 1) << p2.x, p2.y, p2.z, 1.0;
		positions.col(i * 3 + 2) << p3.x, p3.y, p3.z, 1.0;

		normals.col(i * 3) << n1.x, n1.y, n1.z, 0.0;
		normals.col(i * 3 + 1) << n2.x, n2.y, n2.z, 0.0;
		normals.col(i * 3 + 2) << n3.x, n3.y, n3.z, 0.0;
	}

	shader.uploadAttrib("in_position", positions, false);
	shader.uploadAttrib("in_normal", normals, false);

	shader.drawArray(GL_TRIANGLES, 0, num_tris * 3);
}

void ClothSimulator::drawPhong(GLShader& shader)
{
	int num_tris = cloth->clothMesh->triangles.size();

	MatrixXf positions(4, num_tris * 3);
	MatrixXf normals(4, num_tris * 3);
	MatrixXf uvs(2, num_tris * 3);
	MatrixXf tangents(4, num_tris * 3);

	for (int i = 0; i < num_tris; i++)
	{
		Triangle* tri = cloth->clothMesh->triangles[i];

		Vector3D p1 = tri->pm1->position;
		Vector3D p2 = tri->pm2->position;
		Vector3D p3 = tri->pm3->position;

		Vector3D n1 = tri->pm1->normal();
		Vector3D n2 = tri->pm2->normal();
		Vector3D n3 = tri->pm3->normal();

		positions.col(i * 3) << p1.x, p1.y, p1.z, 1.0;
		positions.col(i * 3 + 1) << p2.x, p2.y, p2.z, 1.0;
		positions.col(i * 3 + 2) << p3.x, p3.y, p3.z, 1.0;

		normals.col(i * 3) << n1.x, n1.y, n1.z, 0.0;
		normals.col(i * 3 + 1) << n2.x, n2.y, n2.z, 0.0;
		normals.col(i * 3 + 2) << n3.x, n3.y, n3.z, 0.0;

		uvs.col(i * 3) << tri->uv1.x, tri->uv1.y;
		uvs.col(i * 3 + 1) << tri->uv2.x, tri->uv2.y;
		uvs.col(i * 3 + 2) << tri->uv3.x, tri->uv3.y;

		tangents.col(i * 3) << 1.0, 0.0, 0.0, 1.0;
		tangents.col(i * 3 + 1) << 1.0, 0.0, 0.0, 1.0;
		tangents.col(i * 3 + 2) << 1.0, 0.0, 0.0, 1.0;
	}

	shader.uploadAttrib("in_position", positions, false);
	shader.uploadAttrib("in_normal", normals, false);
	shader.uploadAttrib("in_uv", uvs, false);
	shader.uploadAttrib("in_tangent", tangents, false);

	shader.drawArray(GL_TRIANGLES, 0, num_tris * 3);
}

// ----------------------------------------------------------------------------
// CAMERA CALCULATIONS
//
// OpenGL 3.1 deprecated the fixed pipeline, so we lose a lot of useful OpenGL
// functions that have to be recreated here.
// ----------------------------------------------------------------------------

void ClothSimulator::resetCamera() { camera.copy_placement(canonicalCamera); }
void ClothSimulator::resetCamera_xy() { camera.copy_placement(canonicalCamera_xy); }
void ClothSimulator::resetCamera_yz() { camera.copy_placement(canonicalCamera_yz); }

Matrix4f ClothSimulator::getProjectionMatrix()
{
	Matrix4f perspective;
	perspective.setZero();

	double cam_near = camera.near_clip();
	double cam_far = camera.far_clip();

	double theta = camera.v_fov() * PI / 360;
	double range = cam_far - cam_near;
	double invtan = 1. / tanf(theta);

	perspective(0, 0) = invtan / camera.aspect_ratio();
	perspective(1, 1) = invtan;
	perspective(2, 2) = -(cam_near + cam_far) / range;
	perspective(3, 2) = -1;
	perspective(2, 3) = -2 * cam_near * cam_far / range;
	perspective(3, 3) = 0;

	return perspective;
}

Matrix4f ClothSimulator::getViewMatrix()
{
	Matrix4f lookAt;
	Matrix3f R;

	lookAt.setZero();

	// Convert CGL vectors to Eigen vectors
	// TODO: Find a better way to do this!

	CGL::Vector3D c_pos = camera.position();
	CGL::Vector3D c_udir = camera.up_dir();
	CGL::Vector3D c_target = camera.view_point();

	Vector3f eye(c_pos.x, c_pos.y, c_pos.z);
	Vector3f up(c_udir.x, c_udir.y, c_udir.z);
	Vector3f target(c_target.x, c_target.y, c_target.z);

	R.col(2) = (eye - target).normalized();
	R.col(0) = up.cross(R.col(2)).normalized();
	R.col(1) = R.col(2).cross(R.col(0));

	lookAt.topLeftCorner<3, 3>() = R.transpose();
	lookAt.topRightCorner<3, 1>() = -R.transpose() * eye;
	lookAt(3, 3) = 1.0f;

	return lookAt;
}

// ----------------------------------------------------------------------------
// EVENT HANDLING
// ----------------------------------------------------------------------------

bool ClothSimulator::cursorPosCallbackEvent(double x, double y)
{
	if (left_down && !middle_down && !right_down)
	{
		if (ctrl_down)
		{
			mouseRightDragged(x, y);
		}
		else
		{
			mouseLeftDragged(x, y);
		}
	}
	else if (!left_down && !middle_down && right_down)
	{
		mouseRightDragged(x, y);
	}
	else if (!left_down && !middle_down && !right_down)
	{
		mouseMoved(x, y);

		// Nothing was clicked
		// check and perform grabbing if needed
		if (gui_state == GUI_STATES::SCALING)
		{

			double scaleval = sqrt(pow(scale_mouse_x - x, 2) + pow(scale_mouse_y - y, 2)) * 0.002;

			if (mouse_x > scale_mouse_x)
			{ // Scale up
				selected->radius = original_rad + scaleval;
			}
			else
			{
				if (original_rad - scaleval > 0.01)
				{
					selected->radius = original_rad - scaleval;
				}
				else
				{
					selected->radius = 0.01;
				}
			}
		}
		else if (gui_state == GUI_STATES::GRABBING)
		{
			// First change the point to camera coordanates
			Matrix4f view = getViewMatrix();
			Matrix4f projection = getProjectionMatrix();
			Matrix4f viewProjection = projection * view; // World to screen

			// Get the original position of the sphere
			Vector4f original_worldpos(original_pos.x, original_pos.y, original_pos.z, 1.);

			Vector4f original_screenpos = viewProjection * original_worldpos;

			Vector4f movebyvec(x - grab_mouse_x, -(y - grab_mouse_y), 0, 0);

			Vector4f new_sphere_pos_world = viewProjection.inverse() * (original_screenpos + movebyvec * 0.01);
			Vector3D sphere_pos_world_v3d(new_sphere_pos_world[0], new_sphere_pos_world[1], new_sphere_pos_world[2]);
			selected->pos = sphere_pos_world_v3d;
		}
	}

	mouse_x = x;
	mouse_y = y;

	return true;
}

bool ClothSimulator::mouseButtonCallbackEvent(int button, int action,
	int modifiers)
{
	switch (action)
	{
	case GLFW_PRESS:
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			// Find point here.
			sceneIntersect(mouse_x, mouse_y);
			left_down = true;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = true;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			reset_grab_scale();
			right_down = true;
			break;
		}
		return true;

	case GLFW_RELEASE:
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			left_down = false;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = false;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			right_down = false;
			break;
		}
		return true;
	}

	return false;
}

void ClothSimulator::reset_grab_scale()
{
	if (gui_state == GUI_STATES::GRABBING)
	{
		selected->pos = original_pos;
		cout << "Reset. Cant move it anymore" << endl;
		gui_state = GUI_STATES::IDLE;
	}
	else if (gui_state == GUI_STATES::SCALING)
	{
		selected->radius = original_rad;
		cout << "Reset. Cant scale it anymore" << endl;
		gui_state = GUI_STATES::IDLE;
	}
}

void ClothSimulator::mouseMoved(double x, double y)
{
	y = screen_h - y;
}

void ClothSimulator::mouseLeftDragged(double x, double y)
{
	float dx = x - mouse_x;
	float dy = y - mouse_y;

	camera.rotate_by(-dy * (PI / screen_h), -dx * (PI / screen_w));
}

void ClothSimulator::mouseRightDragged(double x, double y)
{
	camera.move_by(mouse_x - x, y - mouse_y, canonical_view_distance);
}

bool ClothSimulator::keyCallbackEvent(int key, int scancode, int action,
	int mods)
{
	ctrl_down = (bool)(mods & GLFW_MOD_CONTROL);

	if (action == GLFW_PRESS)
	{
		switch (key)
		{
		case GLFW_KEY_ESCAPE:
			is_alive = false;
			break;
		case '1': // Front view
			resetCamera();
			break;
		case '3': // Side view
			resetCamera_yz();
			break;
		case '7': // top view
			resetCamera_xy();
			break;
		case 'x':
		case 'X':
			// Delete the currently selected sphere
			delete_node();
			break;
		case 'e':
		case 'E':
			// Extrude the currently sel sphere
			extrude_node();
			break;
		case 'g':
		case 'G':
			// Extrude the currently sel sphere
			grab_node();
			break;
		case 's':
		case 'S':
			// Extrude the currently sel sphere
			scale_node();
			break;
		case 'N':
		case 'n':
			select_next();
			break;
		case 'P':
		case 'p':
			select_parent();
			break;
		case 'C':
		case 'c':
			select_child();
			break;
		case 'I':
		case 'i':
			interpolate_spheres();
			break;
		case ' ':
			if (!bmesh->mesh_ready) {
				bmesh->generate_bmesh();
			}
			else {
				delete bmesh->mesh;
				bmesh->triangles.clear();
				bmesh->quadrangles.clear();
				bmesh->polygons.clear();
				bmesh->fringe_points.clear();
				bmesh->vertices.clear();
				bmesh->mesh_ready = false;
			}
			break;
		}
	}

	return true;
}

void ClothSimulator::select_next()
{
	if (selected == NULL)
	{
		selected = bmesh->root;
		selected->selected = true;
	}
	else if (selected == bmesh->root)
	{
		selected->selected = false;
		selected = (*bmesh->root->children)[0];
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

void ClothSimulator::select_parent()
{
	if (selected == NULL)
	{
		selected = bmesh->root;
		selected->selected = true;
	}
	else if (selected == bmesh->root)
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

void ClothSimulator::select_child()
{
	if (selected == NULL)
	{
		selected = bmesh->root;
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

void ClothSimulator::scale_node()
{
	if (selected == NULL)
	{
		return;
	}
	else if (gui_state == GUI_STATES::IDLE)
	{
		scale_mouse_x = mouse_x;
		scale_mouse_y = mouse_y;
		original_rad = selected->radius;
		gui_state = GUI_STATES::SCALING;

		cout << "Scaling" << endl;
	}
}

void ClothSimulator::delete_node()
{
	if (selected == NULL)
	{
		return;
	}
	else
	{
		// delete it  and set selected to NULL
		cout << "Deleting" << endl;
		if (bmesh->deleteNode(selected))
		{
			selected = NULL;
		}
	}
}

void ClothSimulator::extrude_node()
{
	if (selected == NULL)
	{
		return;
	}
	else if (gui_state == GUI_STATES::IDLE)
	{
		Balle::SkeletalNode* temp = new Balle::SkeletalNode(selected->pos, selected->radius, selected);
		selected->children->push_back(temp);
		bmesh->all_nodes_vector->push_back(temp);

		cout << "Created a new node" << endl;

		selected->selected = false;
		selected = temp;
		selected->selected = true;

		grab_node();
		gui_state = GUI_STATES::GRABBING;
		cout << "Move it around" << endl;
	}
}

void ClothSimulator::grab_node()
{
	if (selected == NULL)
	{
		return;
	}
	else
	{
		cout << "Grabbed" << endl;
		grab_mouse_x = mouse_x;
		grab_mouse_y = mouse_y;
		original_pos = selected->pos;

		gui_state = GUI_STATES::GRABBING;

		// do grab stuff
		// set state to grabbing
		// remove grab state in the callback func
	}
}

void ClothSimulator::interpolate_spheres()
{
	bmesh->interpolate_spheres();
}

void ClothSimulator::sceneIntersect(double x, double y)
{
	// If grabbing, but click again
	if (gui_state == GUI_STATES::GRABBING || gui_state == GUI_STATES::SCALING)
	{
		gui_state = GUI_STATES::IDLE;
		cout << "Done grabbing. Cant move it anymore" << endl;
		return;
	}

	// Go over each sphere, and check if it intersects
	// TODO: NO support for layered spheres, NO support for random radius
	bool found = false;

	for (Balle::SkeletalNode* node : *(bmesh->all_nodes_vector))
	{
		Matrix4f view = getViewMatrix();
		Matrix4f projection = getProjectionMatrix();
		Matrix4f viewProjection = projection * view; // World to screen

		// Get the original position of the sphere
		Vector4f node_worldpos(node->pos.x, node->pos.y, node->pos.z, 1.);
		Vector4f node_clippos = viewProjection * node_worldpos;

		// Convert to NDC
		node_clippos /= node_clippos[3];

		Vector3D screenCoords;
		screenCoords.x = (node_clippos[0] + 1.) * screen_w * 0.5;
		screenCoords.y = (1. - node_clippos[1]) * screen_h * 0.5;

		if (pow(screenCoords.x - x, 2) + pow(screenCoords.y - y, 2) <= 400)
		{
			if (selected != NULL)
			{
				selected->selected = false;
			}

			cout << "selecting" << endl;
			selected = node; // remove this
			selected->selected = true;
			found = true;
			break;
		}
	}

	if ((found == false) && (selected != NULL))
	{
		selected->selected = false;
		selected = NULL;
	}
}

bool ClothSimulator::sphereSelectionTest(double x, double y, Vector3D center, double radius, float& w)
{
	/*
	 * Algorithm for computing from model space coordinates X to
	 * screen space with a depth value using opengl.
	 *
	 * Start with Vector X.
	 * 1. Get P = opengl projection Matrix.
	 *    Get M = opengl Model transform matrix.
	 *
	 * 2. Convert into our personal 462 library matrix representation.
	 *
	 * 3. Model to world space: X = M*X;
	 *
	 * 4. World to homogenous space X = P*X;
	 *
	 * 5. Projection divide.(homogenesous space to 3D unit cube space.)
	 *
	 *  - X.x = X.x/X.w;
	 *  - X.y = X.y/X.w;
	 *  - X.z = X.z/X.w;
	 *
	 * x, y, and z are now in the range [-1, 1].
	 *
	 * 6. Unit cube space to screen space.
	 * screen_x = viewport.x + viewport.width  * (X.x +1)/2;
	 * screen_y = viewport.y + viewport.height * (X.y +1)/2;
	 *
	 * In our case, the input screen coordinates are already in
	 * window space, so we can omit the viewport offset values.
	 *
	 * 7. Note that opengl coordinate shceme is from lower left corner.
	 */

	Matrix4f model;
	model.setIdentity();

	Matrix4f view = getViewMatrix();
	Matrix4f projection = getProjectionMatrix();

	Matrix4f viewProjection = projection * view;

	// center = projection * model * center;

	double sx = screen_w * (center[0] + 1) / 2;
	double sy = screen_h * (center[1] + 1) / 2;

	if (pow(sx - x, 2) + pow(sy - y, 2) <= radius * radius)
	{
		return true;
	}
	return false;
}

bool ClothSimulator::dropCallbackEvent(int count, const char** filenames)
{
	return true;
}

bool ClothSimulator::scrollCallbackEvent(double x, double y)
{
	camera.move_forward(y * scroll_rate);
	return true;
}

bool ClothSimulator::resizeCallbackEvent(int width, int height)
{
	screen_w = width;
	screen_h = height;

	camera.set_screen_size(screen_w, screen_h);
	return true;
}

void ClothSimulator::initGUI(Screen* screen)
{

	/*
	Window* window;

	window = new Window(screen, "Simulation");
	window->setPosition(Vector2i(default_window_size(0) - 245, 15));
	window->setLayout(new GroupLayout(15, 6, 14, 5));

	// Spring types

	new Label(window, "Spring types", "sans-bold");

	{
		Button* b = new Button(window, "structural");
		b->setFlags(Button::ToggleButton);
		b->setPushed(cp->enable_structural_constraints);
		b->setFontSize(14);
		b->setChangeCallback(
			[this](bool state) { cp->enable_structural_constraints = state; });

		b = new Button(window, "shearing");
		b->setFlags(Button::ToggleButton);
		b->setPushed(cp->enable_shearing_constraints);
		b->setFontSize(14);
		b->setChangeCallback(
			[this](bool state) { cp->enable_shearing_constraints = state; });

		b = new Button(window, "bending");
		b->setFlags(Button::ToggleButton);
		b->setPushed(cp->enable_bending_constraints);
		b->setFontSize(14);
		b->setChangeCallback(
			[this](bool state) { cp->enable_bending_constraints = state; });
	}

	// Mass-spring parameters

	new Label(window, "Parameters", "sans-bold");

	{
		Widget* panel = new Widget(window);
		GridLayout* layout =
			new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 5, 5);
		layout->setColAlignment({ Alignment::Maximum, Alignment::Fill });
		layout->setSpacing(0, 10);
		panel->setLayout(layout);

		new Label(panel, "density :", "sans-bold");

		FloatBox<double>* fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(cp->density / 10);
		fb->setUnits("g/cm^2");
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { cp->density = (double)(value * 10); });

		new Label(panel, "ks :", "sans-bold");

		fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(cp->ks);
		fb->setUnits("N/m");
		fb->setSpinnable(true);
		fb->setMinValue(0);
		fb->setCallback([this](float value) { cp->ks = value; });
	}

	// Simulation constants

	new Label(window, "Simulation", "sans-bold");

	{
		Widget* panel = new Widget(window);
		GridLayout* layout =
			new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 5, 5);
		layout->setColAlignment({ Alignment::Maximum, Alignment::Fill });
		layout->setSpacing(0, 10);
		panel->setLayout(layout);

		new Label(panel, "frames/s :", "sans-bold");

		IntBox<int>* fsec = new IntBox<int>(panel);
		fsec->setEditable(true);
		fsec->setFixedSize(Vector2i(100, 20));
		fsec->setFontSize(14);
		fsec->setValue(frames_per_sec);
		fsec->setSpinnable(true);
		fsec->setCallback([this](int value) { frames_per_sec = value; });

		new Label(panel, "steps/frame :", "sans-bold");

		IntBox<int>* num_steps = new IntBox<int>(panel);
		num_steps->setEditable(true);
		num_steps->setFixedSize(Vector2i(100, 20));
		num_steps->setFontSize(14);
		num_steps->setValue(simulation_steps);
		num_steps->setSpinnable(true);
		num_steps->setMinValue(0);
		num_steps->setCallback([this](int value) { simulation_steps = value; });
	}

	// Damping slider and textbox

	new Label(window, "Damping", "sans-bold");

	{
		Widget* panel = new Widget(window);
		panel->setLayout(
			new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 5));

		Slider* slider = new Slider(panel);
		slider->setValue(cp->damping);
		slider->setFixedWidth(105);

		TextBox* percentage = new TextBox(panel);
		percentage->setFixedWidth(75);
		percentage->setValue(to_string(cp->damping));
		percentage->setUnits("%");
		percentage->setFontSize(14);

		slider->setCallback([percentage](float value) {
			percentage->setValue(std::to_string(value));
			});
		slider->setFinalCallback([&](float value) {
			cp->damping = (double)value;
			// cout << "Final slider value: " << (int)(value * 100) << endl;
			});
	}

	// Gravity

	new Label(window, "Gravity", "sans-bold");

	{
		Widget* panel = new Widget(window);
		GridLayout* layout =
			new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 5, 5);
		layout->setColAlignment({ Alignment::Maximum, Alignment::Fill });
		layout->setSpacing(0, 10);
		panel->setLayout(layout);

		new Label(panel, "x :", "sans-bold");

		FloatBox<double>* fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(gravity.x);
		fb->setUnits("m/s^2");
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { gravity.x = value; });

		new Label(panel, "y :", "sans-bold");

		fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(gravity.y);
		fb->setUnits("m/s^2");
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { gravity.y = value; });

		new Label(panel, "z :", "sans-bold");

		fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(gravity.z);
		fb->setUnits("m/s^2");
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { gravity.z = value; });
	}

	window = new Window(screen, "Appearance");
	window->setPosition(Vector2i(15, 15));
	window->setLayout(new GroupLayout(15, 6, 14, 5));

	// Appearance

	{


		ComboBox* cb = new ComboBox(window, shaders_combobox_names);
		cb->setFontSize(14);
		cb->setCallback(
			[this, screen](int idx) { active_shader_idx = idx; });
		cb->setSelectedIndex(active_shader_idx);
	}

	// Shader Parameters

	new Label(window, "Color", "sans-bold");

	{
		ColorWheel* cw = new ColorWheel(window, color);
		cw->setColor(this->color);
		cw->setCallback(
			[this](const nanogui::Color& color) { this->color = color; });
	}

	new Label(window, "Parameters", "sans-bold");

	{
		Widget* panel = new Widget(window);
		GridLayout* layout =
			new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 5, 5);
		layout->setColAlignment({ Alignment::Maximum, Alignment::Fill });
		layout->setSpacing(0, 10);
		panel->setLayout(layout);

		new Label(panel, "Normal :", "sans-bold");

		FloatBox<double>* fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(this->m_normal_scaling);
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { this->m_normal_scaling = value; });

		new Label(panel, "Height :", "sans-bold");

		fb = new FloatBox<double>(panel);
		fb->setEditable(true);
		fb->setFixedSize(Vector2i(100, 20));
		fb->setFontSize(14);
		fb->setValue(this->m_height_scaling);
		fb->setSpinnable(true);
		fb->setCallback([this](float value) { this->m_height_scaling = value; });
	}

	*/
}