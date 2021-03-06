// EarthTess.cpp
// (c) Justin Thoreson
// 8 November 2021

#include <glad.h>
#include <GLFW/glfw3.h>
#include <stdio.h>
#include "Camera.h"
#include "Draw.h"
#include "GLXtras.h"
#include "Misc.h"
#include "Widgets.h"
#include "VecMat.h"

// display parameters
int         winWidth = 800, winHeight = 600;
Camera      camera(winWidth, winHeight, vec3(0, 0, 0), vec3(0, 0, -7));

// shading
GLuint      program = 0;
int			textureName = 0, textureUnit = 0;
const char *textureFilename = "C:/Users/jdtii/ComputerGraphics/Assets/Textures/Earth.jpg";

// interaction
vec3        light(-1.5f, 1.5
	, 1);
void       *picked = NULL;
Mover       mover;

// vertex shader (no op)
const char *vShaderCode = R"(
	#version 130
	void main() {
		gl_Position = vec4(0); // no-op
	}
)";

// tessellation evaluation
const char *teShaderCode = R"(
	#version 400
	layout (quads, equal_spacing, ccw) in;
	uniform mat4 modelview, persp;
	uniform float dt;
	out vec3 point, normal;
	out vec2 uv;
	vec3 PtFromWhateverThisTurnsOutToBe(float u, float v) {
		// u is longitude
		// v is latitude (PI/2 = N. pole, 0 = equator, -PI/2 = S. pole)
		float PI = 3.141592;
		float elevation = PI*v-PI/2;
		float y = sin(elevation);
		float angle = 2*PI*(1-u);
		// Tessellate back and forth from sphere to cylinder
		float tessTime = cos(sin(dt)*elevation);
		float x = tessTime*cos(angle), z = tessTime*sin(angle); 
		return vec3(x, y, z);
	}
	void main() {
		uv = gl_TessCoord.st;
		vec3 p = PtFromWhateverThisTurnsOutToBe(uv.s, uv.t);
		// Adjust the normals as the shape changes for better shading
		vec3 n = vec3(p.x, abs(sin(dt))*p.y, p.z);	
		point = (modelview*vec4(p, 1)).xyz;
		normal = (modelview*vec4(n, 0)).xyz;
		gl_Position = persp*vec4(point, 1);
	}
)";

// pixel shader
const char *pShaderCode = R"(
	#version 130
	in vec3 point, normal;
	in vec2 uv;
	uniform sampler2D textureMap;
	uniform vec3 light;
	void main() {
		vec3 N = normalize(normal);             // surface normal
		vec3 L = normalize(light-point);        // light vector
		vec3 E = normalize(point);              // eye vertex
		vec3 R = reflect(L, N);                 // highlight vector
		float dif = max(0, dot(N, L));          // one-sided diffuse
		float spec = pow(max(0, dot(E, R)), 50); 
		float ad = clamp(.15+dif, 0, 1);
		vec3 texColor = texture(textureMap, uv).rgb;
		gl_FragColor = vec4(ad*texColor+vec3(spec), 1);
	}
)";

// display

time_t start = clock();

void Display() {
	// background, blending, zbuffer
	glClearColor(.6, .6, .6, 1);
	glClear(GL_COLOR_BUFFER_BIT);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_DEPTH_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	glUseProgram(program);
	// update matrices
	float dt = (float)(clock() - start) / CLOCKS_PER_SEC;
	mat4 m = RotateY(-30*dt)*RotateZ(-23.4)*RotateY(180 * dt); // Earth's rotation, then axis tilt, then axis rotation
	SetUniform(program, "modelview", camera.modelview*m);
	SetUniform(program, "persp", camera.persp);
	SetUniform(program, "dt", dt);
	// transform light and send to pixel shader
	vec4 hLight = camera.modelview*vec4(light, 1);
	glUniform3fv(glGetUniformLocation(program, "light"), 1, (float *) &hLight);
	// set texture
	SetUniform(program, "textureMap", textureUnit);
	glActiveTexture(GL_TEXTURE0+textureUnit);       // active texture corresponds with textureUnit
	glBindTexture(GL_TEXTURE_2D, textureName);      // bind active texture to textureName
	// tessellate patch
	glPatchParameteri(GL_PATCH_VERTICES, 4);
	float res = 25, outerLevels[] = {res, res, res, res}, innerLevels[] = {res, res};
	glPatchParameterfv(GL_PATCH_DEFAULT_OUTER_LEVEL, outerLevels);
	glPatchParameterfv(GL_PATCH_DEFAULT_INNER_LEVEL, innerLevels);
	glDrawArrays(GL_PATCHES, 0, 4);
	// light
	glDisable(GL_DEPTH_TEST);
	UseDrawShader(camera.fullview);
	Disk(light, 12, vec3(1, 0, 0));
	glFlush();
}

// mouse

void MouseButton(GLFWwindow *w, int butn, int action, int mods) {
	double x, y;
	glfwGetCursorPos(w, &x, &y);
	y = winHeight-y; // invert y for upward-increasing screen space
	picked = NULL;
	if (action == GLFW_RELEASE)
		camera.MouseUp();
	if (action == GLFW_PRESS) {
		if (MouseOver(x, y, light, camera.fullview)) {
			mover.Down(&light, (int) x, (int) y, camera.modelview, camera.persp);
			picked = &mover;
		}
		else {
			picked = &camera;
			camera.MouseDown(x, y);
		}
	}
}

void MouseMove(GLFWwindow *w, double x, double y) {
	if (glfwGetMouseButton(w, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		y = winHeight-y;
		if (picked == &mover)
			mover.Drag((int) x, (int) y, camera.modelview, camera.persp);
		if (picked == &camera)
			camera.MouseDrag((int) x, (int) y, Shift());
	}
}

void MouseWheel(GLFWwindow *w, double ignore, double spin) {
	camera.MouseWheel(spin > 0, Shift());
}

// application

void Resize(GLFWwindow *window, int width, int height) {
	camera.Resize(winWidth = width, winHeight = height);
	glViewport(0, 0, width, height);
}

const char* credit = "\
	Created by Justin Thoreson\n\
-------------------------------------------------------\n\
";

const char* usage = "\n\
                 DRAG RED DOT: move light source\n\
            LEFT-CLICK + DRAG: rotate view\n\
    SHIFT + LEFT-CLICK + DRAG: move objects\n\
                       SCROLL: rotate view\n\
               SHIFT + SCROLL: zoom in and out\n\
";

int main(int ac, char **av) {
	// init app window
	if (!glfwInit())
		return 1;
	glfwWindowHint(GLFW_SAMPLES, 4); // Anti-alias
	GLFWwindow *w = glfwCreateWindow(winWidth, winHeight, "Earth Tessellation", NULL, NULL);
	glfwSetWindowPos(w, 100, 100);
	glfwMakeContextCurrent(w);
	// init OpenGL, shader program, texture
	gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
	program = LinkProgramViaCode(&vShaderCode, NULL, &teShaderCode, NULL, &pShaderCode);
	textureName = LoadTexture(textureFilename, textureUnit);
	// callbacks
	glfwSetCursorPosCallback(w, MouseMove);
	glfwSetMouseButtonCallback(w, MouseButton);
	glfwSetScrollCallback(w, MouseWheel);
	glfwSetWindowSizeCallback(w, Resize);
	printf("\n%s\n", credit);
	printf("Usage:\n%s\n", usage);
	// event loop
	glfwSwapInterval(1);
	while (!glfwWindowShouldClose(w)) {
		Display();
		glfwPollEvents();
		glfwSwapBuffers(w);
	}
	glfwDestroyWindow(w);
	glfwTerminate();
}
