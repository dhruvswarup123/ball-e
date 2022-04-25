#version 330

// The camera's position in world-space
uniform vec3 u_cam_pos;

// Color
uniform vec4 u_color;

// Properties of the single point light
uniform vec3 u_light_pos;
uniform vec3 u_light_intensity;

// We also get the uniform texture we want to use.
uniform sampler2D u_texture_1;
uniform bool u_balls;

// These are the inputs which are the outputs of the vertex shader.
in vec4 v_position;
in vec4 v_normal;

// This is where the final pixel color is output.
// Here, we are only interested in the first 3 dimensions (xyz).
// The 4th entry in this vector is for "alpha blending" which we
// do not require you to know about. For now, just set the alpha
// to 1.
out vec4 out_color;

void main() {

  if (u_balls == true){
	  vec4 light_vector = normalize(vec4(u_light_pos, v_position.a) - v_position);

	  float r2 = pow(distance(vec4(u_light_pos, 0), v_position), 2);

	  float max_temp = max(0, dot(normalize(v_normal), light_vector));

	  out_color = u_color * (vec4(u_light_intensity, 0) / r2) * max_temp;
   
	  // (Placeholder code. You will want to replace it.)
	  out_color.a = 1;
  }
  else {
	  if (gl_FrontFacing == false){
		out_color = (vec4(1, 1, 1, 0) - v_normal) / 2 * 0.3;
		out_color.a = 1;
	  }
	  else{
		out_color = (vec4(1, 1, 1, 0) + v_normal) / 2;
		out_color.a = 1;
	  }
  }

  
  

}