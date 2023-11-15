#version 430

uniform mat4 P;
uniform mat4 V;
uniform float order;
uniform float step_size;
uniform int max_iterations;
uniform int fractal_type;
uniform vec3 cam_pos;
//uniform sampler3D voxelTexture;
uniform sampler3D densityTexture;
uniform int window_width;
uniform int window_height;

uniform vec3 color1;
uniform vec3 color2;
uniform vec3 color3;
uniform vec3 color4;
uniform vec3 color5;


in vec4 v_pos;
in float v_density;
in vec3 texCoords;

out vec4 fragcolor; //the output color for this fragment 

struct Quaternion {
	float w;
	vec3 xyz;
};

Quaternion q_add(Quaternion q1, Quaternion q2) {
	return Quaternion(q1.w + q2.w, q1.xyz + q2.xyz);
}

float q_length2(Quaternion q) {
	return q.w * q.w + dot(q.xyz, q.xyz);
}

Quaternion q_conjugate(Quaternion q) {
	return Quaternion(q.w, -q.xyz);
}

Quaternion q_multiply(Quaternion q1, Quaternion q2) {
	float w = q1.w * q2.w - dot(q1.xyz, q2.xyz);
	vec3 xyz = q1.w * q2.xyz + q2.w * q1.xyz + cross(q1.xyz, q2.xyz);
	return Quaternion(w, xyz);
}

vec3 rotate_vector(vec3 v, vec3 axis, float angle) {
	Quaternion q_rot = Quaternion(cos(angle / 2.0), normalize(axis) * sin(angle / 2.0));
	Quaternion q_vec = Quaternion(0.0, v);
	Quaternion q_rot_conj = q_conjugate(q_rot);
	Quaternion q_rotated = q_multiply(q_multiply(q_rot, q_vec), q_rot_conj);
	return q_rotated.xyz;
}

float Julia3D(vec3 pos, int maxIterations) {
	Quaternion z = Quaternion(0.0, pos);
	Quaternion juliaQuaternionC = Quaternion(0.5, vec3(0.355, 0.355, 0.0));
	int i;
	for (i = 0; i < maxIterations; ++i) {
		z = q_add(q_multiply(z, z), juliaQuaternionC);
		if (q_length2(z) > 4.0) {
			break;  // Diverged
		}
	}
	if (float(i) / float(maxIterations) <= 0.0) discard;
	return float(i) / float(maxIterations);
}


float Mandelbulb(vec3 position, int maxIterations)
{
	float x = 0.f;
	float y = 0.f;
	float z = 0.f;

	for (unsigned int iter = 0; iter <= maxIterations; ++iter) {
		float xx = x * x;
		float yy = y * y;
		float zz = z * z;
		float rad = sqrt(xx + yy + zz);

		if (rad > 2.0)
			discard;

		float theta = atan(sqrt(xx + yy), z);
		float phi = atan(y, x);

		// Mandelbulb algorithm
		x = v_pos.x + pow(rad, order) * sin(theta * order) * cos(phi * order);
		y = v_pos.y + pow(rad, order) * sin(theta * order) * sin(phi * order);
		z = v_pos.z + pow(rad, order) * cos(theta * order);
	}
	float dist = sqrt(position.x * position.x + position.y * position.y + position.z * position.z) / sqrt(2.0);
	return dist;
}

float Mandelbox(vec3 pos, vec3 c, int maxIterations) {
	vec3 zeta = pos;
	float delta_rad = 1.0;
	float rad = length(zeta);

	for (int i = 0; i < maxIterations; i++) {
		// Box fold
		zeta.x = clamp(zeta.x, -1.0, 1.0) * 2.0 - zeta.x;
		zeta.y = clamp(zeta.y, -1.0, 1.0) * 2.0 - zeta.y;
		zeta.z = clamp(zeta.z, -1.0, 1.0) * 2.0 - zeta.z;

		// Sphere fold
		if (rad < 0.5)
			zeta *= 4.0;
		else if (rad < 1.0)
			zeta /= rad * rad;

		delta_rad = delta_rad * (length(zeta) / rad) * order + 1.0;
		zeta = zeta * order + c;
		rad = length(zeta); // update the radius for the next iteration
	}

	float dist = length(zeta) / abs(delta_rad);
	if (dist > 2.0) 
		discard;
	return dist;
}

float MengerSponge(vec3 pos, float scale, int iterations) {
	vec3 init_pos = pos;
	for (int i = 0; i < iterations; ++i) {
		pos = mod(pos, scale) - 0.5 * scale; // Fold space back onto itself
		if (scale > 0.01) { // Avoid division by zero
			pos = abs(pos) / scale;
		}

		if ((pos.x > 1.0 / 3.0 && pos.x < 2.0 / 3.0) &&
			(pos.y > 1.0 / 3.0 && pos.y < 2.0 / 3.0  || 
			 pos.z > 1.0 / 3.0 && pos.z < 2.0 / 3.0) ||
			(pos.y > 1.0 / 3.0 && pos.y < 2.0 / 3.0 &&
			 pos.z > 1.0 / 3.0 && pos.z < 2.0 / 3.0)) {
			discard; // Inside a hole, so discard	
		}

		scale /= 3.0;
	}
	return length(init_pos) / 3.0; // Not inside any hole, so keep
}

// Return the normalized escape-time density of a point for the Mandelbulb
float MandelbulbDensity(vec3 position, float bailout, int maxIterations, float power) {
	vec3 z = position;
	float dr = 1.0;
	float r = 0.0;
	for (int i = 0; i < maxIterations; i++) {
		r = length(z);
		if (r > bailout) {
			// The point escaped, return a normalized density based on the iteration count
			float density = float(i) / float(maxIterations);
			return density;
		}

		// Convert to polar coordinates
		float theta = acos(z.z / r);
		float phi = atan(z.y, z.x);
		dr = pow(r, power - 1.0) * power * dr + 1.0;

		// Scale and rotate the point
		float zr = pow(r, power);
		theta *= power;
		phi *= power;

		// Convert back to Cartesian coordinates
		z = zr * vec3(sin(theta) * cos(phi), sin(phi) * sin(theta), cos(theta));
		z += position;
	}
	// The point didn't escape, consider it as fully inside the fractal (max density)
	return 1.0;
}


vec4 TransferFunction(float density) {
	// Convert density to a grayscale color
	vec3 color = vec3(density);

	// Set opacity based on density
	float alpha = clamp(density, 0.0, 1.0);

	return vec4(color, alpha);
}

vec4 getColorFromDensity(float density, vec3 position) {
	vec3 color;
	float normDensity = exp(clamp(density, 0.0, 1.0)); // Normalize density between 0 and 
	normDensity *= length(position);

	// Define color bands
	//vec3 color1 = vec3(0.0, 0.0, 1.0); // Blue
	//vec3 color2 = vec3(0.0, 1.0, 1.0); // Cyan
	//vec3 color3 = vec3(0.0, 1.0, 0.0); // Green
	//vec3 color4 = vec3(1.0, 1.0, 0.0); // Yellow
	//vec3 color5 = vec3(1.0, 0.0, 0.0); // Red

	// Interpolate between colors based on normalized density
	if (normDensity < 0.6) {
		color = mix(color1, color2, normDensity / 0.6);
	}
	else if (normDensity < 0.7) {
		color = mix(color2, color3, (normDensity - 0.6) / 0.1);
	}
	else if (normDensity < 0.8) {
		color = mix(color3, color4, (normDensity - 0.7) / 0.1);
	}
	else if (normDensity < 0.9) {
		color = mix(color4, color5, (normDensity - 0.8) / 0.1);
	}
	else {
		color = color5;
	}


	return vec4(color, density); // Full opacity
}

float ambientOcclusion(vec3 pos, vec3 normal, float scale, float bias, int samples) {
	float ao = 0.0;
	float weight = 1.0;

	for (int i = 0; i < samples; ++i) {
		float dist = float(i) / float(samples) * scale;
		vec3 samplePos = pos + normal * (dist + bias);
		float sampleDensity = texture(densityTexture, samplePos).r;
		ao += (dist - bias - sampleDensity) * weight;
		weight *= 0.5;
	}

	//return ao;
	return clamp(1.0 - ao / float(samples), 0.0, 1.0);
}

vec3 calculateNormal(vec3 pos) {
	float eps = 0.001;  // A small value to offset the position for gradient calculation
	vec3 normal;

	// Sample the density field at points around the current position
	float densityCenter = texture(densityTexture, pos).r;
	float densityX = texture(densityTexture, vec3(pos.x + eps, pos.y, pos.z)).r;
	float densityY = texture(densityTexture, vec3(pos.x, pos.y + eps, pos.z)).r;
	float densityZ = texture(densityTexture, vec3(pos.x, pos.y, pos.z + eps)).r;

	// Calculate gradient (central difference)
	normal.x = densityX - densityCenter;
	normal.y = densityY - densityCenter;
	normal.z = densityZ - densityCenter;

	// Normalize to get the unit normal
	return normalize(normal);
}


void main(void)
{
	//if (v_density <  1.0) discard;
	vec3 ray_pos = cam_pos;
	fragcolor = vec4(1.0, 0.1, 1.0, 1.0);

	vec2 ndc_pos = 2.0 * vec2(gl_FragCoord.x / window_width, gl_FragCoord.y / window_height) - 1.0;
	vec4 cam_dir = inverse(P) * vec4(ndc_pos, 1.0, 1.0);
	cam_dir /= cam_dir.w;
	cam_dir = vec4(cam_dir.xyz, 0.0);

	vec4 ray_dir = normalize(inverse(V) * cam_dir);

	vec4 accumulate_color = vec4(0.0);
	float accumulate_alpha = 0.0;
	float accumulated_density = 0.0;
	float step_size = 0.0005;
	float max_length = 10.0;
	float density = 0.0;
	vec4 color = vec4(0.0);
	float t = 0.0;

	for (t; t < max_length; t += step_size) {
		float density = texture(densityTexture, ray_pos+0.5).r;
		accumulated_density += density;

		ray_pos += ray_dir.xyz * step_size;
		if (accumulated_density >= 1.0) break;
	}

	vec3 normal = calculateNormal(ray_pos+0.5); // Implement this function to calculate the normal
	float ao = ambientOcclusion(ray_pos+0.5, normal, 1.0, 0.01, 5); // Adjust scale, bias, and samples as needed


	color = getColorFromDensity(accumulated_density, ray_pos);
	color.rgb *= ao;
	fragcolor = color;
	//fragcolor = vec4(vec3(ao), accumulated_density);
	//fragcolor = vec4(1.0, 0.0, 1.0, 1.0);



	//for (float t = 0.0; t <= max_step_size; t += step_size) {
	//	vec3 samplePos = ray_origin + t * ray_dir.xyz;
	//	density = texture(voxelTexture, samplePos).r;
	//	vec4 sampleColor = TransferFunction(density); // Convert density to RGBA

	//	// Simple front-to-back compositing
	//	//accumulatedColor.rgb += (1.0 - accumulatedAlpha) * sampleColor.a * sampleColor.rgb;
	//	accumulate_alpha += (1.0 - accumulate_alpha) * sampleColor.a;

	//	// Early termination for performance
	//	if (accumulate_alpha >= 1.0) break;
	//}
	//fragcolor = vec4(vec2(length(v_pos)) / sqrt(2.0), 1.0, v_density);
	//fragcolor = vec4(1.0, 0.0, 1.0, 1.0);

	// max_iterations and order seem to be different for different fractals here are some good values to start with
	// max_iterations and order can be changed through the GUI
	// Mandelbul:		maxIterations = 100;	order = 8.0;
	// Mandelbox:		maxIterations = 10;		order = 2.0;
	// MengerSponge:	maxIterations = 4;		order = Not really used
	/*float val;

	if (fractal_type == 1)
		val = Mandelbox(v_pos.xyz, v_pos.xyz, max_iterations);
	else if (fractal_type == 2)
		val = MengerSponge(v_pos.xyz, 1.0, max_iterations);
	else
		val = Mandelbulb(v_pos.xyz, max_iterations);

	fragcolor = vec4(vec2(val), 1.0, 1.0);*/

	/******** Quaternion Julia Fractal - Currently doesn't work ********/
	//vec3 rotatedPos = rotate_vector(v_pos.xyz, vec3(0.0, 0.0, 1.0), order); // Example rotation
	//val = Julia3D(rotatedPos, maxIterations);
	//fragcolor = vec4(vec3(val), 1.0);
}