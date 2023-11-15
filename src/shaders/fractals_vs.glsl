#version 430
uniform mat4 PVM;
uniform float point_size;

in vec3 pos_attrib; //this variable holds the position of mesh vertices
//in double density_attrib;

out vec4 v_pos;
out float v_density;
out vec3 texCoords;

float rand(vec2 co) {
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453) * 2.0 - 1.0;
}

void main(void)
{
	//v_density = pos_attrib.w;
	gl_PointSize = point_size;
	vec3 rand_position = vec3(pos_attrib.x + (rand(vec2(pos_attrib.y, pos_attrib.z)) / 500.0), 
							  pos_attrib.y + (rand(vec2(pos_attrib.x, pos_attrib.z)) / 500.0), 
							  pos_attrib.z + (rand(vec2(pos_attrib.y, pos_attrib.x)) / 500.0));
	gl_Position = vec4(pos_attrib, 1.0); //transform vertices and send result into pipeline
	v_pos = vec4(pos_attrib, 1.0);
	texCoords = pos_attrib.xyz;
	//v_density = density_attrib;
}