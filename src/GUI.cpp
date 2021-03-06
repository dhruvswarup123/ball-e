#include <cmath>
#include <glad/glad.h>

#include <CGL/vector3D.h>
#include <nanogui/nanogui.h>
#include "CGL/lodepng.h"

#include "GUI.h"
#include "logger.h"

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

void GUI::load_shaders()
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

GUI::GUI(std::string project_root, Screen *screen)
	: m_project_root(project_root)
{
	this->screen = screen;

	this->load_shaders();

	glEnable(GL_PROGRAM_POINT_SIZE);
	glEnable(GL_DEPTH_TEST);
}

GUI::~GUI()
{
	for (auto shader : shaders)
	{
		shader.nanogui_shader->free();
	}
}

void GUI::init()
{
	// Initialize GUI
	screen->setSize(default_window_size);
	if (!gui_init)
	{
		initGUI(screen);
		gui_init = true;
	}
	bmesh = new Balle::BMesh(logger);

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

void GUI::drawContents()
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
		shader_method_label->setCaption("HalfedgeMesh faces");
		file_menu_export_obj_button->setEnabled(true);
		break;
	case Balle::Method::mesh_wireframe_no_indices:
		shader_method_label->setCaption("HalfedgeMesh wireframe");
		file_menu_export_obj_button->setEnabled(true);
		break;
	case Balle::Method::polygons_no_indices:
		shader_method_label->setCaption("Polygon faces");
		file_menu_export_obj_button->setEnabled(false);
		break;
	case Balle::Method::polygons_wirefame_no_indices:
		shader_method_label->setCaption("Polygon wireframe");
		file_menu_export_obj_button->setEnabled(false);
		break;
	case Balle::Method::not_ready:
		shader_method_label->setCaption("Skeleton");
		file_menu_export_obj_button->setEnabled(false);
		break;
	}

	switch (active_shader.type_hint)
	{
	case WIREFRAME:
	default:
		color[0] = 0.;
		cam_pos = camera.position();
		shader.setUniform("u_cam_pos", Vector3f(cam_pos.x, cam_pos.y, cam_pos.z), false);
		shader.setUniform("u_light_pos", Vector3f(0.5, 2, 2), false);
		shader.setUniform("u_light_intensity", Vector3f(10, 10, 10), false);

		shader.setUniform("u_color", color, false);
		drawWireframe(shader);
		break;
	}
	bmesh->tick();
}

void GUI::drawGrid(GLShader &shader)
{
	int grid_num_blocks = 40;
	float grid_width = 20;
	float grid_block_width = grid_width / grid_num_blocks;

	MatrixXf positions_xy(3, 2);
	MatrixXf normals_xy(3, 2);

	// Reset the buffers
	Matrix4f view = getViewMatrix();
	Matrix4f projection = getProjectionMatrix();
	Matrix4f viewProjection = projection * view;
	Matrix4f temp;
	temp << 0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0,
		0, 0, 0, 0;

	viewProjection += temp;

	shader.setUniform("u_view_projection", viewProjection);

	float scale_axes = 1;

	// Draw the x axis
	{
		positions_xy.col(0) << -grid_width * scale_axes / 2., 0, 0;
		positions_xy.col(1) << +grid_width * scale_axes / 2., 0, 0;

		normals_xy.col(0) << 0., 0., 0.;
		normals_xy.col(1) << 0., 0., 0.;

		nanogui::Color colorx(1.0f, 0.25f, 0.25f, 1.0f);
		shader.setUniform("u_color", colorx);
		shader.setUniform("u_solid", true, false);

		shader.setUniform("u_balls", true, false);
		shader.uploadAttrib("in_position", positions_xy, false);
		shader.uploadAttrib("in_normal", normals_xy, false);

		shader.drawArray(GL_LINES, 0, 2);
		shader.setUniform("u_solid", false, false);
	}

	// Draw the y axis
	{
		positions_xy.col(0) << 0, -grid_width * scale_axes / 2., 0;
		positions_xy.col(1) << 0, +grid_width * scale_axes / 2., 0;

		normals_xy.col(0) << 0., 0., 0.;
		normals_xy.col(1) << 0., 0., 0.;

		nanogui::Color colory(0.25f, 1.0f, 0.25f, 1.0f);
		shader.setUniform("u_color", colory);
		shader.setUniform("u_solid", true, false);

		shader.setUniform("u_balls", true, false);
		shader.uploadAttrib("in_position", positions_xy, false);
		shader.uploadAttrib("in_normal", normals_xy, false);

		shader.drawArray(GL_LINES, 0, 2);
		shader.setUniform("u_solid", false, false);
	}

	// Draw the z axis
	{
		positions_xy.col(0) << 0, 0, -grid_width * scale_axes / 2.;
		positions_xy.col(1) << 0, 0, +grid_width * scale_axes / 2.;

		normals_xy.col(0) << 0., 0., 0.;
		normals_xy.col(1) << 0., 0., 0.;

		nanogui::Color colorz(0.25f, 0.25f, 1.0f, 1.0f);
		shader.setUniform("u_color", colorz);
		shader.setUniform("u_solid", true, false);

		shader.setUniform("u_balls", true, false);
		shader.uploadAttrib("in_position", positions_xy, false);
		shader.uploadAttrib("in_normal", normals_xy, false);

		shader.drawArray(GL_LINES, 0, 2);
		shader.setUniform("u_solid", false, false);
	}

	// Reset the buffers
	view = getViewMatrix();
	projection = getProjectionMatrix();
	viewProjection = projection * view;
	shader.setUniform("u_view_projection", viewProjection);

	int total_points = (grid_num_blocks + 1) * 2 * 2;
	MatrixXf positions(3, total_points);
	MatrixXf normals(3, total_points);

	for (int i = 0; i < grid_num_blocks + 1; i++)
	{
		positions.col(i * 2) << -grid_width / 2., 0, -grid_width / 2. + i * grid_block_width;
		positions.col(i * 2 + 1) << +grid_width / 2., 0, -grid_width / 2. + i * grid_block_width;

		normals.col(i * 2) << 0., 0., 0.;
		normals.col(i * 2 + 1) << 0., 0., 0.;
	}

	for (int i = 0; i < grid_num_blocks + 1; i++)
	{
		int ind = i + grid_num_blocks + 1;
		positions.col(ind * 2) << -grid_width / 2. + i * grid_block_width, 0., -grid_width / 2.;
		positions.col(ind * 2 + 1) << -grid_width / 2. + i * grid_block_width, 0., +grid_width / 2.;

		normals.col(ind * 2) << 0., 0., 0.;
		normals.col(ind * 2 + 1) << 0., 0., 0.;
	}

	nanogui::Color color(0.29f, 0.29f, 0.29f, 1.0f);
	shader.setUniform("u_color", color);
	shader.setUniform("u_solid", true, false);

	shader.setUniform("u_balls", true, false);
	shader.uploadAttrib("in_position", positions, false);
	shader.uploadAttrib("in_normal", normals, false);

	shader.drawArray(GL_LINES, 0, total_points);
	shader.setUniform("u_solid", false, false);
}

void GUI::drawWireframe(GLShader &shader)
{
	drawGrid(shader);
	bmesh->shake_nodes_position();
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

void GUI::resetCamera() { camera.copy_placement(canonicalCamera); }
void GUI::resetCamera_xy() { camera.copy_placement(canonicalCamera_xy); }
void GUI::resetCamera_yz() { camera.copy_placement(canonicalCamera_yz); }

Matrix4f GUI::getProjectionMatrix()
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

Matrix4f GUI::getViewMatrix()
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

bool GUI::cursorPosCallbackEvent(double x, double y)
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
			scale_node_action();
		}
		else if (gui_state == GUI_STATES::GRABBING)
		{
			grab_node_action();
		}
	}

	mouse_x = x;
	mouse_y = y;

	return true;
}

void GUI::scale_node_action()
{
	double scaleval = sqrt(pow(scale_mouse_x - mouse_x, 2) + pow(scale_mouse_y - mouse_y, 2)) * 0.002;

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

	interpolate_spheres();
}

bool GUI::mouseButtonCallbackEvent(int button, int action,
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
			if (selected != nullptr)
			{
				grab_mouse_x = mouse_x;
				grab_mouse_y = mouse_y;
				original_pos = selected->pos;
			}
			left_down = true;
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = true;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			if (selected != nullptr)
			{
				scale_mouse_x = mouse_x;
				scale_mouse_y = mouse_y;
				original_rad = selected->radius;
			}
			// reset_grab_scale();
			right_down = true;
			break;
		}
		return true;

	case GLFW_RELEASE:
		switch (button)
		{
		case GLFW_MOUSE_BUTTON_LEFT:
			left_down = false;
			// finish_grab_scale();
			break;
		case GLFW_MOUSE_BUTTON_MIDDLE:
			middle_down = false;
			break;
		case GLFW_MOUSE_BUTTON_RIGHT:
			right_down = false;
			// finish_grab_scale();
			break;
		}
		return true;
	}

	return false;
}

void GUI::finish_grab_scale()
{
	gui_state = GUI_STATES::IDLE;
}

void GUI::reset_grab_scale()
{
	if (gui_state == GUI_STATES::GRABBING)
	{
		selected->pos = original_pos;
		selected->equilibrium = original_pos;
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

void GUI::mouseMoved(double x, double y)
{
	y = screen_h - y;
}

void GUI::mouseLeftDragged(double x, double y)
{
	float dx = x - mouse_x;
	float dy = y - mouse_y;

	if (selected == nullptr)
	{
		camera.rotate_by(-dy * (PI / screen_h), -dx * (PI / screen_w));
	}
	else if (mouse_enable)
	{
		grab_node_action();
	}
}

void GUI::grab_node_action()
{
	Matrix4f view = getViewMatrix();
	Matrix4f projection = getProjectionMatrix();
	Matrix4f viewProjection = projection * view; // World to screen

	// Get the original position of the sphere
	Vector4f original_worldpos(original_pos.x, original_pos.y, original_pos.z, 1.);

	Vector4f original_screenpos = viewProjection * original_worldpos;

	Vector4f movebyvec(mouse_x - grab_mouse_x, -(mouse_y - grab_mouse_y), 0, 0);

	Vector4f new_sphere_pos_world = viewProjection.inverse() * (original_screenpos + movebyvec * 0.01);
	Vector3D sphere_pos_world_v3d(new_sphere_pos_world[0], new_sphere_pos_world[1], new_sphere_pos_world[2]);
	selected->pos = sphere_pos_world_v3d;
	selected->equilibrium = sphere_pos_world_v3d;

	interpolate_spheres();
}

void GUI::mouseRightDragged(double x, double y)
{
	if (selected == nullptr)
	{
		camera.move_by(mouse_x - x, y - mouse_y, canonical_view_distance);
	}
	else if (mouse_enable)
	{
		scale_node_action();
	}
}

bool GUI::keyCallbackEvent(int key, int scancode, int action,
						   int mods)
{
	// if(!keyboard_enable) return false;
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

		case '0': // screen shot once
			write_screen_shot(0);
			break;

		case '2':
			bmesh->remesh_split();
			break;
		case '4':
			bmesh->remesh_collapse();
			break;
		case '6':
			bmesh->remesh_flip();
			break;
		case '8':
			bmesh->remesh_average();
			break;

		case 'x':
		case 'X':
			// Delete the currently selected sphere
			delete_node();
			break;
		case 'e':
		case 'E':
			// Extrude the currently sel sphere
			if (ctrl_down)
			{
				export_bmesh();
			}
			else
			{
				extrude_node();
			}
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
			if (ctrl_down)
			{
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
			interpolate_spheres();

			if (bmesh->shader_method == Balle::Method::not_ready)
			{
				bmesh->generate_bmesh();
			}
			else
			{
				bmesh->clear_mesh();
			}

			if (bmesh->shader_method == Balle::Method::mesh_faces_no_indices || bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices)
			{
				file_menu_export_obj_button->setEnabled(true);
			}
			else
			{
				file_menu_export_obj_button->setEnabled(false);
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
			subdiv_level++;
			break;
		case 'R':
		case 'r':
			if (ctrl_down)
			{
				bmesh->remesh();
				break;
			}

			bmesh->clear_mesh();
			delete bmesh;
			init();
			break;
		case 'L':
		case 'l':
			if (ctrl_down)
			{
				load_bmesh_from_file();
			}
			break;
		}
	}

	return true;
}

void GUI::export_bmesh()
{
	if (!((bmesh->shader_method == Balle::Method::mesh_faces_no_indices) ||
		  (bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices)))
	{
		logger->error("Export Failed - No mesh to export.");
		return;
	}

	std::string filename;

	filename = nanogui::file_dialog({{"obj", "obj"}}, true);
	if (filename.length() == 0)
	{
		return;
	}

	size_t pos = filename.rfind('.', filename.length());
	if (pos == -1)
	{
		filename += ".obj";
	}

	bmesh->export_to_file(filename);
}

void GUI::save_bmesh_to_file()
{
	std::string filename;

	filename = nanogui::file_dialog({{"balle", "balle"}}, true);
	if (filename.length() == 0)
	{
		return;
	}

	size_t pos = filename.rfind('.', filename.length());
	if (pos == -1)
	{
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

void GUI::load_bmesh_from_file()
{
	std::string filename = nanogui::file_dialog({{"balle", "balle"}}, false);
	bmesh->load_from_file(filename);
	bmesh->clear_mesh();
}

void GUI::select_next()
{
	bmesh->select_next_skeletal_node(selected);
}

void GUI::select_parent()
{
	bmesh->select_parent_skeletal_node(selected);
}

void GUI::select_child()
{
	bmesh->select_child_skeletal_node(selected);
}

void GUI::scale_node()
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

		logger->info("GUI::scale_node: Scaling");
	}
}

void GUI::delete_node()
{
	if (selected == nullptr)
	{
		return;
	}
	else
	{

		// delete it  and set selected to nullptr
		logger->info("Deleting");
		if (bmesh->delete_node(selected))
		{
			selected = nullptr;
		}
	}
}

void GUI::extrude_node()
{
	if ((selected == nullptr && bmesh->get_all_node().size() > 0) || (selected != nullptr && selected->interpolated))
	{
		return;
	}

	if (gui_state == GUI_STATES::IDLE)
	{
		Balle::SkeletalNode *temp = bmesh->create_skeletal_node_after(selected);
		if (selected != nullptr)
		{
			Vector3D offset(0.05, 0.05, 0.05);
			if (temp->parent->parent)
			{
				offset = (temp->parent->pos - temp->parent->parent->pos).unit() * 0.08;
			}
			temp->pos = selected->pos + offset;
			selected->selected = false;
		}
		logger->info("Created a new node");
		selected = temp;
		temp->equilibrium = temp->pos;
		selected->selected = true;

		interpolate_spheres();
	}
}

void GUI::grab_node()
{
	if (selected == NULL)
	{
		return;
	}
	else
	{

		logger->info("Grabbed");
		grab_mouse_x = mouse_x;
		grab_mouse_y = mouse_y;
		original_pos = selected->pos;

		gui_state = GUI_STATES::GRABBING;

		// do grab stuff
		// set state to grabbing
		// remove grab state in the callback func
	}
}

void GUI::interpolate_spheres()
{
	bmesh->interpolate_spheres();
}

void GUI::shake_it()
{
	bmesh->shake();
}

void GUI::sceneIntersect(double x, double y)
{
	// Go over each sphere, and check if it intersects
	// TODO: NO support for layered spheres, NO support for random radius
	// cout << "x/screen_w: " << x << "/" << screen_w << " y/screen_h: " << y << "/" << screen_h << endl;

	if (gui_state == GUI_STATES::GRABBING || gui_state == GUI_STATES::SCALING)
	{
		gui_state = GUI_STATES::IDLE;
		logger->info("Done grabbing. Cant move it anymore");
		return;
	}
	bool found = false;

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

bool GUI::dropCallbackEvent(int count, const char **filenames)
{
	return true;
}

bool GUI::scrollCallbackEvent(double x, double y)
{
	camera.move_forward(y * scroll_rate);
	return true;
}

bool GUI::resizeCallbackEvent(int width, int height)
{
	screen_w = width;
	screen_h = height;

	camera.set_screen_size(screen_w, screen_h);
	return true;
}

void GUI::initGUI(Screen *screen)
{
	/*
	 * Things required in the GUI:
	 * 1. SkeletalNode controls.
	 *	For grab, scale, extrude:
	 *	a. Have a button that enables each of these, and that automatically resets when action is completed --done
	 *	b. Have a checkbox to enable/disable mouse/keyboard based control for s, g. (I preferred that lol :) ) --done
	 *	c. Maybe hover to show keybind.
	 *	d. Reset mesh to many default shapes? Have a list of prebuilt shapes --urgent
	 *
	 *	For Interpolation:
	 *	a. Button to maybe refresh?
	 *
	 * 2. Mesh Controls:
	 *	a. Slider for number of subdivs, apply subdiv automatically --working on this
	 *	b. Switching between wireframe, and normal mode
	 *	c. Text output/ Small output window showing terminal output/ warnings/errors/
	 *
	 * 3. Misc:
	 *	a. Button to reset camera views -- done
	 */

	Window *window;

	window = new Window(screen, "                UI Function           ");
	window->setPosition(Vector2i(default_window_size(0) - 245, 15));
	window->setLayout(new GroupLayout(15, 6, 14, 5));
	shader_method_label = new Label(window, "Shader Method", "sans-bold");
	new Label(window, "Functions", "sans-bold");

	{
		Button *extrude_but = new Button(window, "Extrude");
		extrude_but->setFlags(Button::ToggleButton);
		extrude_but->setFontSize(14);
		extrude_but->setCallback([this]
								 { extrude_node(); });
	}
	new Label(window, "Control Mode", "sans-bold");

	{
		CheckBox *mouse_cb = new CheckBox(window, "Mouse Grab/Scale");
		mouse_cb->setFontSize(14);
		mouse_cb->setChecked(true);
		mouse_cb->setCallback([this](bool state)
							  { mouse_enable = !mouse_enable; });
	}
	new Label(window, "Camera View", "sans-bold");

	{
		ComboBox *cb = new ComboBox(window, cameraview_names);
		cb->setSide(Popup::Left);
		cb->setFontSize(14);
		// cb->setCallback(
		//	[this, screen](int idx) { active_camera_idx = idx; });
		cb->setSelectedIndex(active_camera_idx);
		cb->setCallback([this](int id)
						{
			if (id == 0) {
				resetCamera();
			}
			else if (id == 1) {
				resetCamera_yz();

			}
			else {
				resetCamera_xy();
			} });
	}
	new Label(window, "Import/Export File", "sans-bold");
	{
		file_menu_load_button = new Button(window, "Load .balle file");
		file_menu_load_button->setCallback([this]()
										   { this->load_bmesh_from_file(); });
		file_menu_load_button->setFontSize(14);
		file_menu_save_button = new Button(window, "Save .balle file");
		file_menu_save_button->setCallback([this]()
										   { this->save_bmesh_to_file(); });
		file_menu_save_button->setFontSize(14);
		file_menu_export_obj_button = new Button(window, "Export .obj file");
		file_menu_export_obj_button->setCallback([this]()
												 { this->export_bmesh(); });
		file_menu_export_obj_button->setFontSize(14);
	}
	new Label(window, "Mesh Display", "sans-bold");
	{
		ComboBox *cb = new ComboBox(window, display_names);
		cb->setSide(Popup::Left);
		cb->setFontSize(14);
		cb->setSelectedIndex(active_display_idx);
		cb->setCallback([this](int id)
						{

			if (id == 0) {
				if (bmesh->shader_method == Balle::Method::mesh_wireframe_no_indices) {
					bmesh->shader_method = Balle::Method::mesh_faces_no_indices;
				}
				else if (bmesh->shader_method == Balle::Method::polygons_wirefame_no_indices) {
					bmesh->shader_method = Balle::Method::polygons_no_indices;
				}
			}
			else if (id == 1) {
				if (bmesh->shader_method == Balle::Method::mesh_faces_no_indices) {
					bmesh->shader_method = Balle::Method::mesh_wireframe_no_indices;
				}
				else if (bmesh->shader_method == Balle::Method::polygons_no_indices) {
					bmesh->shader_method = Balle::Method::polygons_wirefame_no_indices;
				}
			} });
		animation_menu_shake_it_button = new Button(window, "Shake it! But pls not break it!");
		animation_menu_shake_it_button->setCallback([this]()
													{ this->shake_it(); });
	}
	new Label(window, "Subdivision", "sans-bold");
	{
		Widget *panel = new Widget(window);
		panel->setLayout(new BoxLayout(Orientation::Horizontal,
									   Alignment::Middle, 0, 20));

		Slider *slider = new Slider(panel);
		slider->setValue(0.0f);
		slider->setFixedWidth(100);

		TextBox *textBox = new TextBox(panel);
		textBox->setFixedSize(Vector2i(60, 25));
		textBox->setValue("0");
		slider->setCallback([this, textBox](float value)
							{
			if (bmesh->shader_method == Balle::Method::mesh_faces_no_indices && !bmesh->shaking){
			
            textBox->setValue(std::to_string((int) (value * 100/20)));
			if ((int) (value * 100/20) >= subdiv_level ){
				for(int i = 0 ; i < (int) (value * 100/20) -  subdiv_level; i++){
					bmesh->subdivision();
					subdiv_level++;
				}
			}else{
				bmesh->rebuild();
				subdiv_level = 0;
				for(int i = 0; i < (int) (value * 100/20); i++){
					bmesh->subdivision();
					subdiv_level++;
				}
			}

			} });
	}
	new Label(window, "Terminal OUTPUT", "sans-bold");
	{
		TextBox *tb = new TextBox(window);
		tb->setValue("");
		tb->setHeight(200);
		tb->setFontSize(10);
		logger = new Logger{tb};
	}
}

string GUI::get_frame_name(int index)
{
	string file_name;
	string x = to_string(index);

	for (int i = 0; i < 5 - x.size(); i++)
	{
		file_name.append("0");
	}
	file_name.append(x);
	file_name.append(".png");
	return file_name;
}

void GUI::write_screen_shot(int index)
{
	int width = screen_w;
	int height = screen_h;

	vector<unsigned char> windowPixels(4 * width * height);
	glReadPixels(0, 0,
				 width,
				 height,
				 GL_RGBA,
				 GL_UNSIGNED_BYTE,
				 &windowPixels[0]);

	vector<unsigned char> flippedPixels(4 * width * height);
	for (int row = 0; row < height; ++row)
		memcpy(&flippedPixels[row * width * 4], &windowPixels[(height - row - 1) * width * 4], 4 * width);

	string file_name = get_frame_name(index);
	std::cout << "Writing file " << file_name << "...";
	if (lodepng::encode(file_name, flippedPixels, width, height))
		cerr << "Could not be written" << endl;
	else
		cout << "Success!" << endl;
}
