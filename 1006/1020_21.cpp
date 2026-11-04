#define WINDOW_TITLE "Example21"
#define USE_MOUSE
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
//  실습 21 3차원 공 튕기기  -  exam_template.cpp 와 같은 구조로 정리한 코드
//  위쪽(obj 읽기 / 셰이더 / 그리기 도우미 / main)은 템플릿과 동일. 아래 [1]~[4] 가 실습 내용이다.
//   [1] 상태 변수   [2] reset_state / update   [3] drawScene   [4] key_callback (+ 마우스)
//  (그리기 도우미 설명과 레시피 모음은 exam_template.cpp 참고)
// ===================================================================================================

// ---------------- [1] 상태 변수 (공통 상수 / 도우미 포함) ----------------

// ---------------- 실습 21 : 3차원 공 튕기기 ----------------
// 공간(직육면체)은 로컬 좌표에서 x : -HX~HX, y : -HY~HY, z : -HZ~HZ. 앞쪽(+z, 카메라 쪽)은 열려 있다
const float HX = 3.0f, HY = 2.0f, HZ = 3.0f;
const float GRAVITY = 9.8f;
const int MAX_BALLS = 5;
const int OBSTACLE_COUNT = 3;
const float TILT_LIMIT = 60.0f;

struct Ball {
	bool active;
	bool falling;           // 바닥이 열려서 떨어지는 중
	glm::vec3 pos, vel;
	float radius;
	glm::vec3 color;
};

struct Obstacle {           // 바닥 위의 육면체 (z축을 따라 일렬로 배치)
	bool alive;
	bool falling;
	float x, y, z;          // 중심 좌표 (y 는 바닥에 놓였을 때의 높이에서 시작)
	float vy;
	glm::vec3 size;
	glm::vec3 color;
};

Ball balls[MAX_BALLS];
Obstacle obstacles[OBSTACLE_COUNT];

float tiltAngle;            // 마우스 드래그 : 공간(육면체)의 z축 회전 각도
float sceneYaw;             // y/Y : 화면의 y축에 대한 회전
float camZ;                 // z/Z : 카메라 z축 이동
float openT;                // o : 바닥 열리기 (0 = 닫힘, 1 = 열림)
bool floorOpening;
bool dragging;
double lastMouseX;

void spawn_ball() {
	for (int i = 0; i < MAX_BALLS; i++) {
		if (balls[i].active)
			continue;
		Ball& b = balls[i];
		b.active = true;
		b.falling = false;
		b.radius = 0.3f;
		b.pos = glm::vec3(random_range(-1.5f, 1.5f), random_range(-0.5f, 1.0f), random_range(-1.5f, 1.5f));
		b.vel = glm::normalize(glm::vec3(random_range(-1, 1), random_range(-1, 1), random_range(-1, 1)) + glm::vec3(0.1f)) * 4.0f;
		b.color = glm::vec3(random_range(0.3f, 1.0f), random_range(0.3f, 1.0f), random_range(0.3f, 1.0f));
		return;
	}
}


// ---------------- [2] 초기화 / 업데이트 ----------------
void reset_state() {
	tiltAngle = 0.0f;
	sceneYaw = 0.0f;
	camZ = 0.0f;
	openT = 0.0f;
	floorOpening = false;
	dragging = false;

	// 크기가 다른 육면체 3개를 z축을 따라 일렬로
	float sizes[OBSTACLE_COUNT][3] = { {1.0f, 0.8f, 1.2f}, {1.4f, 1.2f, 1.4f}, {0.8f, 1.6f, 1.0f} };
	float zs[OBSTACLE_COUNT] = { -2.0f, 0.0f, 2.0f };
	float xs[OBSTACLE_COUNT] = { 0.0f, -1.0f, 1.0f };
	glm::vec3 colors[OBSTACLE_COUNT] = { glm::vec3(0.9f, 0.5f, 0.2f), glm::vec3(0.3f, 0.8f, 0.8f), glm::vec3(0.8f, 0.8f, 0.2f) };
	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		obstacles[i].alive = true;
		obstacles[i].falling = false;
		obstacles[i].size = glm::vec3(sizes[i][0], sizes[i][1], sizes[i][2]);
		obstacles[i].x = xs[i];
		obstacles[i].y = -HY + sizes[i][1] / 2;
		obstacles[i].z = zs[i];
		obstacles[i].vy = 0.0f;
		obstacles[i].color = colors[i];
	}

	for (int i = 0; i < MAX_BALLS; i++)
		balls[i].active = false;
	spawn_ball();
}

// 공과 육면체(축에 정렬된 상자)의 충돌: 가장 가까운 점 방향으로 튕김
void collide_ball_box(Ball& b, const Obstacle& o) {
	glm::vec3 half = o.size / 2.0f;
	glm::vec3 c(o.x, o.y, o.z);
	glm::vec3 closest = glm::clamp(b.pos, c - half, c + half);
	glm::vec3 diff = b.pos - closest;
	float d2 = glm::dot(diff, diff);
	if (d2 >= b.radius * b.radius)
		return;
	glm::vec3 n = (d2 > 0.0001f) ? diff / sqrtf(d2) : glm::vec3(0.0f, 1.0f, 0.0f);
	if (glm::dot(b.vel, n) < 0.0f)
		b.vel -= 2.0f * glm::dot(b.vel, n) * n;
	b.pos = closest + n * b.radius;
}

void update(float dt) {
	// 바닥이 열리는 중이면 바닥을 한쪽으로 치운다 (floorEdge 오른쪽은 바닥이 없음), 다시 o 를 누르면 닫힌다
	if (floorOpening && openT < 1.0f) {
		openT += dt / 1.5f;
		if (openT > 1.0f) openT = 1.0f;
	}
	else if (!floorOpening && openT > 0.0f) {
		openT -= dt / 1.5f;
		if (openT < 0.0f) openT = 0.0f;
	}
	float floorEdge = -HX + 2 * HX * (1.0f - openT);

	// 육면체: 공간이 z축으로 기울어지면 낮은 쪽 바닥면을 따라 미끄러진다
	float slide = -sinf(glm::radians(tiltAngle)) * 4.0f;
	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		Obstacle& o = obstacles[i];
		if (!o.alive)
			continue;
		if (!o.falling) {
			o.x += slide * dt;
			float lim = HX - o.size.x / 2;
			if (o.x > lim) o.x = lim;
			if (o.x < -lim) o.x = -lim;
			if (o.x + o.size.x / 2 > floorEdge + 0.001f && floorEdge < HX)   // 바닥이 사라진 곳에 걸치면 떨어짐
				o.falling = true;
		}
		else {
			o.vy -= GRAVITY * dt;
			o.y += o.vy * dt;
			if (o.y < -HY - 10.0f)
				o.alive = false;
		}
	}

	for (int i = 0; i < MAX_BALLS; i++) {
		Ball& b = balls[i];
		if (!b.active)
			continue;
		float r = b.radius;

		if (b.falling)
			b.vel.y -= GRAVITY * dt;
		b.pos += b.vel * dt;

		// 벽에 맞으면 방향을 바꿔서 이동
		if (b.pos.x > HX - r) { b.pos.x = HX - r; b.vel.x = -fabsf(b.vel.x); }
		if (b.pos.x < -HX + r) { b.pos.x = -HX + r; b.vel.x = fabsf(b.vel.x); }
		if (b.pos.z > HZ - r) { b.pos.z = HZ - r; b.vel.z = -fabsf(b.vel.z); }
		if (b.pos.z < -HZ + r) { b.pos.z = -HZ + r; b.vel.z = fabsf(b.vel.z); }
		if (b.pos.y > HY - r) { b.pos.y = HY - r; b.vel.y = -fabsf(b.vel.y); }

		if (!b.falling) {
			if (b.pos.y < -HY + r) {
				if (b.pos.x - r > floorEdge) {      // 바닥이 열려 있는 곳이면 떨어짐
					b.falling = true;
				}
				else {
					b.pos.y = -HY + r;
					b.vel.y = fabsf(b.vel.y);
				}
			}
			for (int k = 0; k < OBSTACLE_COUNT; k++)
				if (obstacles[k].alive && !obstacles[k].falling)
					collide_ball_box(b, obstacles[k]);
		}
		else if (b.pos.y < -HY - 8.0f) {
			b.active = false;       // 떨어져서 사라짐 (B 로 다시 만들 수 있음)
		}
	}
}


// ---------------- [3] 그리기 ----------------
//--- 그리기 콜백 함수
GLvoid drawScene() {
	// 원근 투영 / 은면제거 / 컬링
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glViewport(0, 0, width, height);
	glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	glm::vec3 eye(0.0f, 1.0f, 10.0f + camZ);
	glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, camZ), glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(sceneYaw), glm::vec3(0, 1, 0));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
	set_camera(view, proj);

	// 공간 전체가 z축에 대하여 회전한다 (마우스 드래그)
	glm::mat4 roomM = glm::rotate(glm::mat4(1.0f), glm::radians(tiltAngle), glm::vec3(0, 0, 1));

	// 각 면의 법선이 안쪽을 향하도록 평면을 회전시켜 배치 -> 컬링으로 안쪽 면만 보인다
	draw_plane(roomM, glm::vec3(-HX, 0, 0), glm::vec3(0, 1, 0), 90.0f, 2 * HZ, 2 * HY, glm::vec3(0.9f, 0.3f, 0.3f));     // 왼쪽
	draw_plane(roomM, glm::vec3(HX, 0, 0), glm::vec3(0, 1, 0), -90.0f, 2 * HZ, 2 * HY, glm::vec3(0.3f, 0.9f, 0.3f));     // 오른쪽
	draw_plane(roomM, glm::vec3(0, 0, -HZ), glm::vec3(0, 1, 0), 0.0f, 2 * HX, 2 * HY, glm::vec3(0.3f, 0.5f, 0.9f));      // 뒤쪽
	draw_plane(roomM, glm::vec3(0, HY, 0), glm::vec3(1, 0, 0), 90.0f, 2 * HX, 2 * HZ, glm::vec3(0.9f, 0.9f, 0.3f));      // 위쪽

	// 아래쪽 (바닥): 열리면 왼쪽 끝을 기준으로 길이가 줄어든다
	float floorW = 2 * HX * (1.0f - openT);
	if (floorW > 0.01f)
		draw_plane(roomM, glm::vec3(-HX + floorW / 2, -HY, 0), glm::vec3(1, 0, 0), -90.0f, floorW, 2 * HZ, glm::vec3(0.7f, 0.3f, 0.9f));

	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		const Obstacle& o = obstacles[i];
		if (o.alive)
			draw_box(roomM, glm::vec3(o.x, o.y, o.z), o.size, o.color);
	}

	for (int i = 0; i < MAX_BALLS; i++)
		if (balls[i].active)
			draw_sphere(roomM, balls[i].pos, balls[i].radius, balls[i].color);
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

	// z/Z : 카메라(화면)를 z축 양/음 방향으로 이동, y/Y : 화면의 y축에 대하여 시계/반시계 회전 (꾹 누르면 반복)
	if (ch == 'z') camZ += 0.2f;
	if (ch == 'Z') camZ -= 0.2f;
	if (ch == 'y') sceneYaw += 3.0f;
	if (ch == 'Y') sceneYaw -= 3.0f;

	if (press) {
		if (ch == 'B' || ch == 'b') spawn_ball();           // B : 볼이 새로 생겨서 튕기기 시작 (최대 5개)
		if (ch == 'o') floorOpening = !floorOpening;        // o : 바닥 열기 (다시 누르면 닫는 방향) -> 물체가 떨어짐
		if (ch == 'c') reset_state();                       // c : 초기화
		if (ch == 'q') glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}


// ---------------- [4b] 마우스 ----------------
// 마우스 왼쪽 버튼을 누른 채로 드래그
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			dragging = true;
			glfwGetCursorPos(window, &lastMouseX, nullptr);
		}
		else if (action == GLFW_RELEASE) {
			dragging = false;
		}
	}
}

void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos) {
	if (!dragging)
		return;
	// 왼쪽으로 드래그 -> 왼쪽으로 회전(반시계), 오른쪽으로 드래그 -> 오른쪽으로 회전(시계)
	tiltAngle -= (float)(xpos - lastMouseX) * 0.25f;
	if (tiltAngle > TILT_LIMIT) tiltAngle = TILT_LIMIT;
	if (tiltAngle < -TILT_LIMIT) tiltAngle = -TILT_LIMIT;
	lastMouseX = xpos;
}
