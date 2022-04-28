#include <cmath>
#include <glad/glad.h>

#include <CGL/vector3D.h>
#include <nanogui/nanogui.h>

#include "clothSimulator.h"

#include "camera.h"
#include <ctime>
#include "misc/camera_info.h"
#include "misc/file_utils.h"

// Needed to generate stb_image binaries. Should only define in exactly one source file importing stb_image.h.
#define STB_IMAGE_IMPLEMENTATION
#include "misc/stb_image.h"

#include "bmesh/bmesh.h"
using namespace nanogui;
using namespace std;

void ClothSimulator::load_shaders()
{
	std::set<std::string> shader_folder_contents;
	bool success = FileUtils::list_files_in_directory(m_project_root + "/shaders", shader_folder_contents);
	if (!success)
	{
		std::cout << "Error: Could not find the shaders folder!" << std::endl;
	}

	std::string std_vert_shader = m_project_root + "/shaders/Default.vert";

	for (const std::string &shader_fname : shader_folder_contents)
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

ClothSimulator::ClothSimulator(std::string project_root, Screen *screen)
	: m_project_root(project_root)
{
	this->screen = screen;

	this->load_shaders();

	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_DEPTH_TEST);
}

ClothSimulator::~ClothSimulator()
{
	for (auto shader : shaders)
	{
		shader.nanogui_shader->free();
	}

}

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

	CGL::Vector3D target(avg_pm_position.x, avg_pm_position.y / 2,
						 avg_pm_position.z);
	CGL::Vector3D c_dir(0., 0., 0.);
	canonical_view_distance = 0.9;
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

void ClothSimulator::drawContents()
{
	glEnable(GL_DEPTH_TEST);

	// Bind the active shader

	const UserShader &active_shader = shaders[active_shader_idx];

	GLShader &shader = *active_shader.nanogui_shader;
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

	// Update shader method label
	switch (bmesh->shader_method)
	{
	case Balle::Method::mesh_faces_no_indices:
		shader_method_label->setCaption("HalfedgeMesh with faces");
		break;
	case Balle::Method::mesh_wireframe_no_indices:
		shader_method_label->setCaption("HalfedgeMesh with wireframe");
		break;
	case Balle::Method::polygons_no_indices:
		shader_method_label->setCaption("Polygon std::vector with faces");
		break;
	case Balle::Method::polygons_wirefame_no_indices:
		shader_method_label->setCaption("Polygon std::vector with wireframe");
		break;
	case Balle::Method::not_ready:
		shader_method_label->setCaption("Not Ready, draw plain spheres");
		break;
	}

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
	}
}

void ClothSimulator::drawWireframe(GLShader &shader)
{
	if (bmesh->shader_method == Balle::Method::not_ready)
	{
		bmesh->draw_skeleton(shader);
	}
	else if (bmesh->shader_method == Balle::Method::polygons_no_indices)
	{ // METHOD 1: Draw the polygons not using indices (WORKING)
		bmesh->draw_polygon_faces(shader);
	}
	else if (bmesh->shader_method == Balle::Method::mesh_faces_no_indices)
	{ // METHOD 2: Draw the mesh faces not using indices (WORKING)
		bmesh->draw_mesh_faces(shader);
	}
	else if (bmesh->shader_method == Balle::Method::polygons_wirefame_no_indices)
	{ // METHOD 3: Draw the polygons wireframe without indices (WORKING)
		bmesh->draw_polygon_wireframe(shader);
	}
	else if (bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices)
	{ // METHOD 4: Draw the Wireframe not using indices (WORKING)
		bmesh->draw_mesh_wireframe(shader);
	}
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
			left_down = true;
			sceneIntersect(mouse_x, mouse_y);
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = true;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			right_down = true;
			break;
		}
		return true;

	case GLFW_RELEASE:
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			left_down = false;
			finish_grab_scale();
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = false;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			right_down = false;
			finish_grab_scale();
			break;
		}
		return true;
	}

	return false;
}

void ClothSimulator::finish_grab_scale()
{

	gui_state = GUI_STATES::IDLE;
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
	if (selected == nullptr)
	{
		camera.rotate_by(-dy * (PI / screen_h), -dx * (PI / screen_w));
	}
	else
	{
		grab_node(x, y);
	}
}

void ClothSimulator::mouseRightDragged(double x, double y)
{
	if (selected == nullptr)
	{
		camera.move_by(mouse_x - x, y - mouse_y, canonical_view_distance);
	}
	else
	{
		scale_node(x, y);
	}
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
			if (ctrl_down) {
				export_bmesh();
			}
			break;
		case 'g':
		case 'G':
			// Extrude the currently sel sphere
			// grab_node();
			break;
		case 's':
		case 'S':
			// Extrude the currently sel sphere
			// scale_node();
			if (ctrl_down) {
				save_bmesh_to_file();
			}
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
			if (bmesh->shader_method == Balle::Method::not_ready)
			{
				bmesh->generate_bmesh();
			}
			else
			{
				bmesh->clear_mesh();
			}
			break;
		case 'W':
		case 'w':
			if (bmesh->shader_method == Balle::Method::mesh_faces_no_indices)
			{
				bmesh->shader_method = Balle::Method::mesh_wireframe_no_indices;
			}
			else if (bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices)
			{
				bmesh->shader_method = Balle::Method::mesh_faces_no_indices;
			}
			else if (bmesh->shader_method == Balle::Method::polygons_no_indices)
			{
				bmesh->shader_method = Balle::Method::polygons_wirefame_no_indices;
			}
			else if (bmesh->shader_method == Balle::Method::polygons_wirefame_no_indices)
			{
				bmesh->shader_method = Balle::Method::polygons_no_indices;
			}
			break;

		case 'D': // subdivision
		case 'd':
			bmesh->subdivision();
			break;
		case 'R':
		case 'r':
			bmesh->clear_mesh();
			init();
			break;
		case 'L':
		case 'l':
			if (ctrl_down) {
				load_bmesh_from_file();
			}
			break;

		}
	}

	return true;
}

void ClothSimulator::export_bmesh() {
	if (!((bmesh->shader_method == Balle::Method::mesh_faces_no_indices) ||
		(bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices))) {
		cout << "ERROR: Export Failed - No mesh to export." << endl;
		return;
	}

	std::string filename;

	filename = nanogui::file_dialog({ {"obj", "obj"} }, true);
	if (filename.length() == 0) {
		return;
	}

	size_t pos = filename.rfind('.', filename.length());
	if (pos == -1) {
		filename += ".obj";
	}

	bmesh->export_to_file(filename);
}

void ClothSimulator::save_bmesh_to_file()
{
	std::string filename;

	filename = nanogui::file_dialog({ {"balle", "balle"} }, true);
	if (filename.length() == 0) {
		return;
	}

	size_t pos = filename.rfind('.', filename.length());
	if (pos == -1) {
		filename += ".balle";
	}
	
	bmesh->save_to_file(filename);

	// Time source https://stackoverflow.com/a/16358264/13292618
	/*time_t rawtime;
	struct tm* timeinfo;
	char buffer[80];

	time(&rawtime);
	timeinfo = localtime(&rawtime);

	strftime(buffer, sizeof(buffer), "spheres_%d-%m-%Y_%H-%M-%S.balle", timeinfo);
	std::string str(buffer);

	cout << "Saving the config to " + str << endl;
	bmesh->save_to_file(str);*/
}

void ClothSimulator::load_bmesh_from_file()
{	
	std::string filename = nanogui::file_dialog({ {"balle", "balle"} }, false);
	bmesh->load_from_file(filename);
}

void ClothSimulator::select_next()
{
	bmesh->select_next_skeletal_node(selected);
}

void ClothSimulator::select_parent()
{
	bmesh->select_parent_skeletal_node(selected);
}

void ClothSimulator::select_child()
{
	bmesh->select_child_skeletal_node(selected);
}

void ClothSimulator::scale_node(double x, double y)
{
	if (selected == nullptr || selected->interpolated)
	{
		return;
	}

	if (gui_state == GUI_STATES::IDLE)
	{
		scale_mouse_x = mouse_x;
		scale_mouse_y = mouse_y;
		original_rad = selected->radius;
		gui_state = GUI_STATES::SCALING;

		cout << "Scaling" << endl;
	}
	else
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
}

void ClothSimulator::delete_node()
{
	if (selected == nullptr)
	{
		return;
	}
	else
	{

		// delete it  and set selected to nullptr
		cout << "Deleting" << endl;
		if (bmesh->delete_node(selected))
		{
			selected = nullptr;
		}
	}
}

void ClothSimulator::extrude_node()
{
	if (selected == nullptr || selected->interpolated)
	{
		return;
	}

	if (gui_state == GUI_STATES::IDLE)
	{
		Balle::SkeletalNode *temp = bmesh->create_skeletal_node_after(selected);
		cout << "Created a new node" << endl;
		Vector3D offset(0.05, 0.05, 0.05);
		if (temp->parent->parent) {
			offset = (temp->parent->pos - temp->parent->parent->pos).unit() * 0.08;
		}
		temp->pos = selected->pos + offset;
		cout << temp->pos << endl;
		selected->selected = false;
		selected = temp;
		selected->selected = true;
	}
}

void ClothSimulator::grab_node(double x, double y)
{
	if (selected == nullptr || selected->interpolated)
	{
		return;
	}

	if (gui_state == GUI_STATES::IDLE)
	{
		gui_state = GUI_STATES::GRABBING;
		grab_mouse_x = mouse_x;
		grab_mouse_y = mouse_y;
		original_pos = selected->pos;
		// cout << "gbms: " << grab_mouse_x << " " << grab_mouse_y << endl;
	}
	else if (gui_state == GUI_STATES::GRABBING)
	{
		// do grab stuff
		// set state to grabbing
		// remove grab state in the callback func
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

void ClothSimulator::interpolate_spheres()
{
	bmesh->interpolate_spheres();
}

void ClothSimulator::sceneIntersect(double x, double y)
{
	// Go over each sphere, and check if it intersects
	// TODO: NO support for layered spheres, NO support for random radius
	bool found = false;
	// cout << "x/screen_w: " << x << "/" << screen_w << " y/screen_h: " << y << "/" << screen_h << endl;

	for (Balle::SkeletalNode *node : bmesh->get_all_node())
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
			if (selected != nullptr)
			{
				selected->selected = false;
			}

			selected = node; // remove this
			selected->selected = true;
			found = true;
			break;
		}
	}

	if ((found == false) && (selected != nullptr))
	{
		selected->selected = false;
		selected = nullptr;
	}
}


bool ClothSimulator::dropCallbackEvent(int count, const char **filenames)
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

void ClothSimulator::initGUI(Screen *screen)
{
	Window *window;

	window = new Window(screen, "                Shader Method                ");
	window->setPosition(Vector2i(default_window_size(0) - 245, 15));
	window->setLayout(new GroupLayout(15, 6, 14, 5));

	shader_method_label = new Label(window, "Shader Method", "sans-bold");
	// sshader_method_label->setLayout(new GroupLayout(15, 6, 14, 5));

	/*
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
