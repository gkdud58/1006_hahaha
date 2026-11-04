#define WINDOW_TITLE "Example20"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <GL/glew.h>
#include <GL/glfw3.h>
#include <fstream>
#include <string>

#include <gl/glm/glm.hpp>
#include <gl/glm/ext.hpp>
#include <gl/glm/gtc/matrix_transform.hpp>
#include <gl/glm/gtc/type_ptr.hpp>

#define MAX_LINE_LENGTH 234

//--- 사용자 정의 함수
void make_vertexShaders();
void make_fragmentShaders();
int make_shaderProgram();

//--- 각 실습별로 구현하는 함수
void reset_state();
void update(float dt);
GLvoid drawScene();
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
#ifdef USE_MOUSE
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos);
#endif

typedef struct {
	float x, y, z;
} Vertex;

typedef struct {
	unsigned int v1, v2, v3;
} Face;

typedef struct {
	Vertex* vertices;
	size_t vertex_count;
	Face* faces;
	size_t face_count;
} Model;

std::string filetobuf(const char* file)
{
	std::ifstream shaderFile(file);
	if (!shaderFile.is_open())
	{
		return "";
	}
	//--- 파일 전체를 문자열로 읽기
	std::string source((std::istreambuf_iterator<char>(shaderFile)), std::istreambuf_iterator<char>());
	shaderFile.close();
	return source;
}

void read_newline(char* str) {
	char* pos;
	if ((pos = strchr(str, '\n')) != NULL)
		*pos = '\0';
}

void read_obj_file(const char* filename, Model* model) {
	FILE* file;
	fopen_s(&file, filename, "r");
	if (!file) {
		perror("Error opening file");
		exit(EXIT_FAILURE);
	}

	char line[MAX_LINE_LENGTH];
	model->vertex_count = 0;
	model->face_count = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);

		if (line[0] == 'v' && line[1] == ' ')
			model->vertex_count++;
		else if (line[0] == 'f' && line[1] == ' ')
			model->face_count++;
	}
	fseek(file, 0, SEEK_SET); // 포인터 위치를 원하는 곳으로 옮겨줌
	model->vertices = (Vertex*)malloc(model->vertex_count * sizeof(Vertex));
	model->faces = (Face*)malloc(model->face_count * sizeof(Face));
	size_t vertex_index = 0; size_t face_index = 0;

	while (fgets(line, sizeof(line), file)) {
		read_newline(line);

		if (line[0] == 'v' && line[1] == ' ') {
			int result = sscanf_s(line + 2, "%f %f %f", &model->vertices[vertex_index].x,
				&model->vertices[vertex_index].y,
				&model->vertices[vertex_index].z);
			vertex_index++;
		}
		else if (line[0] == 'f' && line[1] == ' ') {
			unsigned int v1 = 1, v2 = 1, v3 = 1, n1, n2, n3;
			int result = sscanf_s(line + 2, "%u//%u %u//%u %u//%u", &v1, &n1, &v2, &n2, &v3, &n3);
			if (result != 6)   // sphere.obj처럼 "f 1 3 2" 형식(법선 없음)도 읽도록
				sscanf_s(line + 2, "%u %u %u", &v1, &v2, &v3);
			model->faces[face_index].v1 = v1 - 1; // OBJ indices start at 1
			model->faces[face_index].v2 = v2 - 1;
			model->faces[face_index].v3 = v3 - 1;
			face_index++;
		}
	}
	fclose(file);
}

// ***************************************************************************************************************************************

//--- 필요한 변수 선언
GLint width, height;
GLuint shaderProgramID;
GLuint vertexShader;
GLuint fragmentShader;

// 큐브 모델 (cube.obj, 0~1 크기), 구 모델 (sphere.obj, 반지름 1 / 중심 원점)
Model cubeModel, sphereModel;
GLuint cubeVao, cubeVbo;      // 위치만 저장 (색은 glVertexAttrib3f로 그릴 때마다 지정)
GLuint sphereVao, sphereVbo;
GLuint quadVao, quadVbo;      // 평면 (xy 평면의 1x1 정사각형, 법선 +z)

const float PI = 3.14159265f;

// obj를 읽어서 VAO/VBO에 올림 (삼각형마다 정점 복사, 위치만)
void make_model_buffers(const char* filename, Model* model, GLuint* vao, GLuint* vbo) {
	read_obj_file(filename, model);

	size_t n = model->face_count * 3;
	GLfloat* pos = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));
	for (size_t i = 0; i < model->face_count; i++) {
		unsigned int idx[3] = { model->faces[i].v1, model->faces[i].v2, model->faces[i].v3 };
		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;
			pos[k * 3 + 0] = model->vertices[idx[j]].x;
			pos[k * 3 + 1] = model->vertices[idx[j]].y;
			pos[k * 3 + 2] = model->vertices[idx[j]].z;
		}
	}

	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glGenBuffers(1, vbo);
	glBindBuffer(GL_ARRAY_BUFFER, *vbo);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	free(pos);
}

// 평면 하나 (반시계 방향으로 +z 쪽이 앞면). 벽/바닥은 이 평면을 회전시켜서 안쪽이 앞면이 되도록 배치한다
void make_quad_buffer() {
	GLfloat pos[] = {
		-0.5f, -0.5f, 0.0f,   0.5f, -0.5f, 0.0f,   0.5f, 0.5f, 0.0f,
		-0.5f, -0.5f, 0.0f,   0.5f, 0.5f, 0.0f,   -0.5f, 0.5f, 0.0f
	};
	glGenVertexArrays(1, &quadVao);
	glBindVertexArray(quadVao);
	glGenBuffers(1, &quadVbo);
	glBindBuffer(GL_ARRAY_BUFFER, quadVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
}

void draw_cube(const glm::mat4& model, const glm::vec3& color) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(cubeVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);   // 색 배열을 쓰지 않으므로 고정 색으로 지정
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(cubeModel.face_count * 3));
}

// 부모 행렬(parent) 기준으로 중심이 pos, 크기가 size인 큐브를 그림
// cube.obj는 0~1 이라 모서리가 원점이므로, 먼저 -0.5 이동해서 중심을 원점으로 맞춘 뒤 크기를 적용한다
void draw_box(const glm::mat4& parent, const glm::vec3& pos, const glm::vec3& size, const glm::vec3& color) {
	glm::mat4 m = parent
		* glm::translate(glm::mat4(1.0f), pos)
		* glm::scale(glm::mat4(1.0f), size)
		* glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
	draw_cube(m, color);
}

// 부모 행렬 기준으로 중심이 pos, 반지름이 radius인 구를 그림
void draw_sphere(const glm::mat4& parent, const glm::vec3& pos, float radius, const glm::vec3& color) {
	glm::mat4 m = parent
		* glm::translate(glm::mat4(1.0f), pos)
		* glm::scale(glm::mat4(1.0f), glm::vec3(radius));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(m));
	glBindVertexArray(sphereVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(sphereModel.face_count * 3));
}

// 평면(w x h)을 중심 pos에 놓고 axis 축으로 deg도 회전시켜서 그림 (회전 전 법선은 +z)
void draw_plane(const glm::mat4& parent, const glm::vec3& pos, const glm::vec3& axis, float deg, float w, float h, const glm::vec3& color) {
	glm::mat4 m = parent
		* glm::translate(glm::mat4(1.0f), pos)
		* glm::rotate(glm::mat4(1.0f), glm::radians(deg), axis)
		* glm::scale(glm::mat4(1.0f), glm::vec3(w, h, 1.0f));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(m));
	glBindVertexArray(quadVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);
	glDrawArrays(GL_TRIANGLES, 0, 6);
}

// 뷰잉 변환 / 투영 변환 행렬을 셰이더로 보냄
void set_camera(const glm::mat4& view, const glm::mat4& proj) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));
}

float random_range(float lo, float hi) {
	return lo + (hi - lo) * (rand() % 1001) / 1000.0f;
}

void make_vertexShaders() {
	std::string vertexSource = filetobuf("vertex.glsl");
	const char* source = vertexSource.c_str();
	//--- 셰이더 생성하고 코드 연결한 후 컴파일 하기
	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &source, NULL);
	glCompileShader(vertexShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(vertexShader, 512, NULL, errorLog);
		std::cerr << "ERROR: vertex shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

void make_fragmentShaders() {
	std::string fragmentSource = filetobuf("fragment.glsl");
	const char* source = fragmentSource.c_str();
	//--- 셰이더 생성하고 코드 연결한 후 컴파일 하기
	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &source, NULL);
	glCompileShader(fragmentShader);
	GLint result;
	GLchar errorLog[512];
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &result);
	if (!result)
	{
		glGetShaderInfoLog(fragmentShader, 512, NULL, errorLog);
		std::cerr << "ERROR: fragment shader 컴파일 실패\n" << errorLog << std::endl;
		return;
	}
}

//--- 세이더 프로그램 생성하기
int make_shaderProgram() {
	//-- shader Program
	shaderProgramID = glCreateProgram();
	glAttachShader(shaderProgramID, vertexShader);
	glAttachShader(shaderProgramID, fragmentShader);
	glLinkProgram(shaderProgramID);
	//--- 세이더 삭제하기
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	//--- Shader Program 사용하기
	glUseProgram(shaderProgramID);

	return shaderProgramID;
}

// 창 크기가 바뀌면 뷰포트 계산에 쓰는 width/height 를 갱신
void framebuffer_size_callback(GLFWwindow* window, int w, int h) {
	width = w;
	height = h;
}

// ***************************************************************************************************************************************
//--- 메인 함수
int main(int argc, char** argv)
{
	width = 1200;
	height = 900;

	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//--- 윈도우 생성
	GLFWwindow* window = glfwCreateWindow(width, height, WINDOW_TITLE, nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	//--- 생성한 윈도우의 OpenGL Context를 현재 Context로 설정
	glfwMakeContextCurrent(window);
	glfwGetFramebufferSize(window, &width, &height);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	//--- GLEW 초기화
	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK)
	{
		glfwDestroyWindow(window);
		glfwTerminate();
		return -1;
	}

	//--- 세이더 읽어와서 세이더 프로그램 만들기
	make_vertexShaders();
	make_fragmentShaders();
	shaderProgramID = make_shaderProgram();

	make_model_buffers("cube.obj", &cubeModel, &cubeVao, &cubeVbo);
	make_model_buffers("sphere.obj", &sphereModel, &sphereVao, &sphereVbo);
	make_quad_buffer();

	srand((unsigned int)time(NULL));
	reset_state();
	glfwSetKeyCallback(window, key_callback);
#ifdef USE_MOUSE
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_pos_callback);
#endif

	//**************************렌더링 루프******************************
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		double nowTime = glfwGetTime();
		float dt = (float)(nowTime - lastTime);
		if (dt > 0.05f) dt = 0.05f;      // 창을 끌고 있을 때 등 프레임이 튀는 것 방지
		lastTime = nowTime;

		update(dt);
		drawScene();
		//--- 윈도우에 렌더링 결과 출력
		glfwSwapBuffers(window);
		//--- 이벤트 처리
		glfwPollEvents();
	}

	//--- 종료
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}

// ***************************************************************************************************************************************

// ===================================================================================================
//  실습 20 택배 물류 센터  -  exam_template.cpp 와 같은 구조로 정리한 코드
//  위쪽(obj 읽기 / 셰이더 / 그리기 도우미 / main)은 템플릿과 동일. 아래 [1]~[4] 가 실습 내용이다.
//   [1] 상태 변수   [2] reset_state / update   [3] drawScene   [4] key_callback (+ 마우스)
//  (그리기 도우미 설명과 레시피 모음은 exam_template.cpp 참고)
// ===================================================================================================

// ---------------- [1] 상태 변수 (공통 상수 / 도우미 포함) ----------------

// ---------------- 택배 물류 센터 공통 부분 ----------------
// 컨베이어 벨트 경로: (-3, -2) 에서 +x 로 SEG1 만큼 간 뒤, 90도 꺾여서 +z 로 SEG2 만큼 ("ㄱ" 자)
const float BELT_H = 1.0f;          // 벨트 높이
const float BELT_W = 1.0f;          // 벨트 폭
const float SEG1 = 5.0f;            // 첫 번째 직선 구간 길이
const float SEG2 = 5.0f;            // 두 번째 직선 구간 길이 (꺾인 뒤)
const float BELT_LEN = SEG1 + SEG2;
const float BELT_SPEED = 1.2f;
const glm::vec3 PATH_START = glm::vec3(-3.0f, BELT_H, -2.0f);
const float CORNER_BLEND = 0.3f;    // 코너에서 상자가 방향을 돌리는 구간
const float FLOOR_HALF = 6.0f;      // 바닥 한 변의 절반
const float BOX_SIZE = 0.5f;
const int BOX_COUNT = 5;
const float BOX_GAP = 1.8f;         // 벨트 위 상자 사이 간격

const glm::vec3 boxColor[BOX_COUNT] = {
	glm::vec3(1.0f, 0.3f, 0.3f),    // 1 빨강
	glm::vec3(1.0f, 0.7f, 0.1f),    // 2 주황
	glm::vec3(1.0f, 1.0f, 0.3f),    // 3 노랑
	glm::vec3(0.3f, 0.9f, 0.4f),    // 4 초록
	glm::vec3(0.4f, 0.6f, 1.0f)     // 5 파랑
};

bool perspectiveMode;       // p: 원근 투영, P: 직각 투영
float camDist;              // +/- : 줌
float camAngle;             // 방향키: 중심 y축에 대한 카메라 회전 각도
const glm::vec3 CAM_TARGET = glm::vec3(0.5f, 0.6f, 0.5f);

float smooth01(float t) {
	if (t < 0.0f) t = 0.0f;
	if (t > 1.0f) t = 1.0f;
	return t * t * (3.0f - 2.0f * t);
}

// 벨트 위 거리 s 에서의 위치(윗면)와 진행 방향 yaw (y축 회전 각도, 도 단위)
// yaw 0 = +x 방향, yaw -90 = +z 방향. 코너 근처에서는 yaw 를 부드럽게 돌려서 상자가 같이 회전하도록 한다
void belt_pose(float s, glm::vec3& pos, float& yawDeg) {
	if (s < SEG1) {
		pos = glm::vec3(PATH_START.x + s, BELT_H, PATH_START.z);
		yawDeg = 0.0f;
	}
	else {
		pos = glm::vec3(PATH_START.x + SEG1, BELT_H, PATH_START.z + (s - SEG1));
		yawDeg = -90.0f;
	}
	float d = s - SEG1;
	if (fabsf(d) < CORNER_BLEND)
		yawDeg = -90.0f * smooth01((d + CORNER_BLEND) / (2.0f * CORNER_BLEND));
}

// 택배 상자 하나: 중심 pos, y축 회전 yawDeg. 윗면에 주사위 눈처럼 번호(1~5)만큼 점을 찍어서 구분한다
void draw_package(const glm::vec3& pos, float yawDeg, int idx) {
	glm::mat4 parent = glm::translate(glm::mat4(1.0f), pos)
		* glm::rotate(glm::mat4(1.0f), glm::radians(yawDeg), glm::vec3(0, 1, 0));
	draw_box(parent, glm::vec3(0.0f), glm::vec3(BOX_SIZE), boxColor[idx]);

	int n = idx + 1;
	float d = BOX_SIZE * 0.22f;
	float top = BOX_SIZE / 2 + 0.01f;
	glm::vec3 dot(BOX_SIZE * 0.14f, 0.02f, BOX_SIZE * 0.14f);
	glm::vec3 black(0.05f);
	if (n % 2 == 1)                                         // 1, 3, 5: 가운데 점
		draw_box(parent, glm::vec3(0.0f, top, 0.0f), dot, black);
	if (n >= 2) {                                           // 2, 3, 4, 5: 대각선 점
		draw_box(parent, glm::vec3(-d, top, -d), dot, black);
		draw_box(parent, glm::vec3(d, top, d), dot, black);
	}
	if (n >= 4) {                                           // 4, 5: 나머지 대각선 점
		draw_box(parent, glm::vec3(d, top, -d), dot, black);
		draw_box(parent, glm::vec3(-d, top, d), dot, black);
	}
}

// 바닥 + 컨베이어 벨트(평면) + 벨트 표면의 줄무늬(육면체) + 다리
void draw_environment(float stripePhase) {
	glm::mat4 I(1.0f);

	// 바닥 (xz 평면, 법선 +y)
	draw_plane(I, glm::vec3(0.0f), glm::vec3(1, 0, 0), -90.0f, FLOOR_HALF * 2, FLOOR_HALF * 2, glm::vec3(0.45f, 0.45f, 0.5f));

	// 벨트 두 구간 (코너에서 겹치도록 폭의 절반만큼 더 길게)
	float len = SEG1 + BELT_W / 2;
	glm::vec3 beltColor(0.18f, 0.18f, 0.2f);
	draw_plane(I, glm::vec3(PATH_START.x + len / 2, BELT_H, PATH_START.z), glm::vec3(1, 0, 0), -90.0f, len, BELT_W, beltColor);
	draw_plane(I, glm::vec3(PATH_START.x + SEG1, BELT_H + 0.001f, PATH_START.z - BELT_W / 2 + (SEG2 + BELT_W / 2) / 2),
		glm::vec3(1, 0, 0), -90.0f, BELT_W, SEG2 + BELT_W / 2, beltColor);

	// 줄무늬: 벨트와 함께 움직이고, 코너에서는 같이 꺾인다
	for (int k = 0; k < 20; k++) {
		float s = fmodf(stripePhase + k * 0.5f, BELT_LEN);
		glm::vec3 p; float yaw;
		belt_pose(s, p, yaw);
		glm::mat4 parent = glm::translate(glm::mat4(1.0f), p + glm::vec3(0.0f, 0.015f, 0.0f))
			* glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0, 1, 0));
		draw_box(parent, glm::vec3(0.0f), glm::vec3(0.1f, 0.03f, BELT_W), glm::vec3(0.7f, 0.7f, 0.75f));
	}

	// 벨트 다리
	glm::vec3 legColor(0.3f, 0.3f, 0.35f);
	float legH = BELT_H - 0.03f;
	float xs[] = { -2.5f, 0.0f };
	for (int i = 0; i < 2; i++) {
		draw_box(I, glm::vec3(xs[i], legH / 2, PATH_START.z - 0.55f), glm::vec3(0.1f, legH, 0.1f), legColor);
		draw_box(I, glm::vec3(xs[i], legH / 2, PATH_START.z + 0.55f), glm::vec3(0.1f, legH, 0.1f), legColor);
	}
	float zs[] = { 0.5f, 3.0f };
	for (int i = 0; i < 2; i++) {
		draw_box(I, glm::vec3(PATH_START.x + SEG1 - 0.55f, legH / 2, zs[i]), glm::vec3(0.1f, legH, 0.1f), legColor);
		draw_box(I, glm::vec3(PATH_START.x + SEG1 + 0.55f, legH / 2, zs[i]), glm::vec3(0.1f, legH, 0.1f), legColor);
	}
	draw_box(I, glm::vec3(PATH_START.x + SEG1 - 0.45f, legH / 2, PATH_START.z + 0.45f), glm::vec3(0.1f, legH, 0.1f), legColor);
	draw_box(I, glm::vec3(PATH_START.x + SEG1 + 0.45f, legH / 2, PATH_START.z - 0.45f), glm::vec3(0.1f, legH, 0.1f), legColor);
}

// 고정 3인칭 쿼터뷰: CAM_TARGET 을 바라보며, camAngle 만큼 y축에 대해 돌고, camDist 로 줌
glm::mat4 quarter_view() {
	float elev = glm::radians(35.0f);
	float a = glm::radians(camAngle);
	glm::vec3 eye = CAM_TARGET + camDist * glm::vec3(cosf(elev) * sinf(a), sinf(elev), cosf(elev) * cosf(a));
	return glm::lookAt(eye, CAM_TARGET, glm::vec3(0.0f, 1.0f, 0.0f));
}

// 투영 변환: 원근 투영 / 직각 투영 (직각 투영은 줌에 맞춰 보이는 범위를 조절)
glm::mat4 make_projection(bool perspective) {
	float aspect = (float)width / (float)height;
	if (perspective)
		return glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	float half = camDist * 0.4f;
	return glm::ortho(-half * aspect, half * aspect, -half, half, 0.1f, 100.0f);
}

// ---------------- 실습 20 : 택배 물류 센터 ----------------
bool beltOn;                    // b : 벨트 이동 시작/정지
float boxS[BOX_COUNT];          // 각 상자의 벨트 위 거리
float stripePhase;              // 줄무늬 위치 (벨트와 같이 움직임)


// ---------------- [2] 초기화 / 업데이트 ----------------
void reset_state() {
	perspectiveMode = true;
	camDist = 14.0f;
	camAngle = 45.0f;
	beltOn = false;
	stripePhase = 0.0f;
	for (int i = 0; i < BOX_COUNT; i++)
		boxS[i] = 8.0f - i * BOX_GAP;       // 1번 상자가 맨 앞
}

void update(float dt) {
	if (!beltOn)
		return;
	float move = BELT_SPEED * dt;
	stripePhase = fmodf(stripePhase + move, BELT_LEN);
	for (int i = 0; i < BOX_COUNT; i++)
		boxS[i] = fmodf(boxS[i] + move, BELT_LEN);      // 끝에 도달하면 다시 시작 부분에서 이동
}


// ---------------- [3] 그리기 ----------------
//--- 그리기 콜백 함수
GLvoid drawScene() {
	// 은면제거 + 컬링
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glViewport(0, 0, width, height);
	glClearColor(0.12f, 0.12f, 0.18f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	set_camera(quarter_view(), make_projection(perspectiveMode));

	draw_environment(stripePhase);

	for (int i = 0; i < BOX_COUNT; i++) {
		glm::vec3 pos; float yaw;
		belt_pose(boxS[i], pos, yaw);
		pos.y += 0.02f + BOX_SIZE / 2;      // 벨트 윗면 + 줄무늬 위에 올려놓음
		draw_package(pos, yaw, i);
	}
}


// ---------------- [4] 키보드 ----------------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE)
		return;
	bool press = (action == GLFW_PRESS);

	// 대문자/소문자 구분이 필요할 때를 위해 shift 없으면 소문자로 통일
	unsigned int ch = key;
	if (ch >= 'A' && ch <= 'Z' && !(mods & GLFW_MOD_SHIFT))
		ch += 32;

	// 방향키: 카메라를 중심 y축에 대하여 회전 (←: 시계방향, →: 반시계방향), 꾹 누르면 반복
	if (key == GLFW_KEY_LEFT)  camAngle -= 3.0f;
	if (key == GLFW_KEY_RIGHT) camAngle += 3.0f;

	// +/- : 줌 인 / 줌 아웃
	if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
		camDist -= 0.5f;
		if (camDist < 4.0f) camDist = 4.0f;
	}
	if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
		camDist += 0.5f;
		if (camDist > 30.0f) camDist = 30.0f;
	}

	if (press) {
		if (ch == 'b') beltOn = !beltOn;                    // 벨트 이동 시작/정지
		if (ch == 'p') perspectiveMode = true;              // p : 원근 투영
		if (ch == 'P') perspectiveMode = false;             // P : 직각 투영
		if (ch == 'r') reset_state();                       // 리셋
		if (ch == 'q') glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}
