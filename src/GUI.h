#ifndef CGL_GUI_H
#define CGL_GUI_H

#include <nanogui/nanogui.h>
#include <memory>

#include "camera.h"
#include "bmesh/bmesh.h"

using namespace nanogui;

struct UserShader;
enum ShaderTypeHint
{
	WIREFRAME = 0,
	NORMALS = 1,
	PHONG = 2
};

enum GUI_STATES
{
	IDLE,
	GRABBING,
	SCALING
};

class GUI
{
public:
	bool is_alive = true;
	GUI(std::string project_root, Screen* screen);
	~GUI();

	void init();
	virtual void drawContents();

	// Screen events

	virtual bool cursorPosCallbackEvent(double x, double y);
	virtual bool mouseButtonCallbackEvent(int button, int action, int modifiers);
	virtual bool keyCallbackEvent(int key, int scancode, int action, int mods);
	virtual bool dropCallbackEvent(int count, const char** filenames);
	virtual bool scrollCallbackEvent(double x, double y);
	virtual bool resizeCallbackEvent(int width, int height);

private:
	virtual void initGUI(Screen* screen);
	void drawWireframe(GLShader& shader);
	void drawGrid(GLShader& shader);
	void load_shaders();

	// File management

	std::string m_project_root;

	// Camera methods
	virtual void resetCamera();
	void resetCamera_yz();
	void resetCamera_xy();
	virtual Matrix4f getProjectionMatrix();
	virtual Matrix4f getViewMatrix();

	// Default simulation values

	int frames_per_sec = 90;
	int simulation_steps = 30;

	CGL::Vector3D gravity = CGL::Vector3D(0, -9.8, 0);
	nanogui::Color color = nanogui::Color(1.0f, 1.0f, 1.0f, 1.0f);

	Balle::BMesh* bmesh;

	// OpenGL attributes

	int active_shader_idx = 0;

	vector<UserShader> shaders;
	vector<std::string> shaders_combobox_names;


	// OpenGL customizable inputs

	double m_normal_scaling = 2.0;
	double m_height_scaling = 0.1;

	// Camera attributes

	CGL::Camera camera;
	CGL::Camera canonicalCamera;
	CGL::Camera canonicalCamera_xy;
	CGL::Camera canonicalCamera_yz;

	double view_distance;
	double canonical_view_distance;
	double min_view_distance;
	double max_view_distance;

	double scroll_rate;

	// Screen methods

	Screen* screen;
	void mouseLeftDragged(double x, double y);
	void mouseRightDragged(double x, double y);
	void mouseMoved(double x, double y);
	void sceneIntersect(double x, double y);

	// Mouse flags

	bool left_down = false;
	bool right_down = false;
	bool middle_down = false;

	// Keyboard flags

	bool ctrl_down = false;

	// Simulation flags

	bool is_paused = true;

	// Screen attributes

	float mouse_x;
	float mouse_y;

	int screen_w;
	int screen_h;

	Balle::SkeletalNode* selected = NULL;
	GUI_STATES gui_state = GUI_STATES::IDLE;

	void export_bmesh();
	void save_bmesh_to_file();
	void load_bmesh_from_file();
	void reset_grab_scale();
	void finish_grab_scale();
	void delete_node();
	void extrude_node();
	void grab_node(double x, double y);
	void scale_node(double x, double y);
	void select_next();
	void select_parent();
	void select_child();
	void interpolate_spheres();

	// Store scaling stuff
	float scale_mouse_x = 0;
	float scale_mouse_y = 0;
	float original_rad = 0;

	// Store grabbing stuff
	float grab_mouse_x = 0;
	float grab_mouse_y = 0;
	Vector3D original_pos;

	Vector2i default_window_size = Vector2i(1024, 800);

	// nanogui stuff
	Window* file_menu_window;
	Button* file_menu_load_button;
	Button* file_menu_save_button;
	Window* shader_method_window;
	Label* shader_method_label;
};

struct UserShader
{
	UserShader(std::string display_name, std::shared_ptr<GLShader> nanogui_shader, ShaderTypeHint type_hint)
		: display_name(display_name), nanogui_shader(nanogui_shader), type_hint(type_hint)
	{
	}

	std::shared_ptr<GLShader> nanogui_shader;
	std::string display_name;
	ShaderTypeHint type_hint;
};

#endif // CGL_GUI_H
