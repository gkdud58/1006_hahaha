#define WINDOW_TITLE "Example23"
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
//  실습 23 로봇 그리기  -  exam_template.cpp 와 같은 구조로 정리한 코드
//  위쪽(obj 읽기 / 셰이더 / 그리기 도우미 / main)은 템플릿과 동일. 아래 [1]~[4] 가 실습 내용이다.
//   [1] 상태 변수   [2] reset_state / update   [3] drawScene   [4] key_callback (+ 마우스)
//  (그리기 도우미 설명과 레시피 모음은 exam_template.cpp 참고)
// ===================================================================================================

// ---------------- [1] 상태 변수 (공통 상수 / 도우미 포함) ----------------

// ---------------- 실습 23 : 로봇 그리기 ----------------
// 무대: x : -SX~SX, z : -SZ~SZ, 높이 0~SH. 앞면(+z, 카메라 쪽)이 위로 슬라이딩하며 열린다
const float SX = 4.0f, SZ = 3.0f, SH = 3.5f;
const int OBSTACLE_COUNT = 3;
const float ROBOT_R = 0.25f;            // 로봇 충돌 반지름
const float WALK_SPEED = 1.5f;
const float GRAVITY = 14.0f;
const float JUMP_SPEED = 6.0f;
const float MOVE_STEP = 0.2f;

struct Obstacle {
	float x, z;
	float w, d, h;
	glm::vec3 color;
};
Obstacle obstacles[OBSTACLE_COUNT];

// 로봇
float robotX, robotZ, robotY, robotVy;  // 발 밑 위치 (robotY : 발 높이)
int dirX, dirZ;                         // 걷는 방향 (w : 0,-1  s : 0,+1  a : -1,0  d : +1,0)
bool onGround;
float walkPhase, speedMul;              // +/- : 걷는 속도 배율 (속도가 빨라지면 팔다리 각도도 커진다)

bool frontOpen;                         // o : 앞면 열기/닫기
float openT;

const glm::vec3 CAM_START = glm::vec3(0.0f, 5.0f, 12.0f);
const glm::vec3 CAM_TARGET = glm::vec3(0.0f, 1.2f, 0.0f);
glm::vec3 cameraPos;                    // z/Z, x/X : 카메라 이동
float camOrbitDir, camOrbitAngle;       // y/Y : 화면 중심 y축을 기준으로 공전


// ---------------- [2] 초기화 / 업데이트 ----------------
void reset_state() {
	robotX = 0.0f;
	robotZ = 2.0f;
	robotY = 0.0f;
	robotVy = 0.0f;
	dirX = 0;
	dirZ = -1;
	onGround = true;
	walkPhase = 0.0f;
	speedMul = 1.0f;

	frontOpen = false;
	openT = 0.0f;

	cameraPos = CAM_START;
	camOrbitDir = 0.0f;
	camOrbitAngle = 0.0f;

	// 바닥에 크기가 다른 장애물 3개를 랜덤한 위치에 (로봇 시작 위치와 서로 겹치지 않게)
	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		Obstacle& o = obstacles[i];
		for (int tries = 0; tries < 200; tries++) {
			o.w = random_range(0.6f, 1.1f);
			o.d = random_range(0.6f, 1.1f);
			o.h = random_range(0.3f, 0.8f);
			o.x = random_range(-SX + 0.8f, SX - 0.8f);
			o.z = random_range(-SZ + 0.8f, SZ - 0.8f);

			bool ok = (fabsf(o.x - robotX) > 1.3f || fabsf(o.z - robotZ) > 1.3f);
			for (int k = 0; k < i; k++)
				if (fabsf(o.x - obstacles[k].x) < 1.4f && fabsf(o.z - obstacles[k].z) < 1.4f)
					ok = false;
			if (ok)
				break;
		}
		o.color = glm::vec3(random_range(0.3f, 1.0f), random_range(0.3f, 1.0f), random_range(0.3f, 1.0f));
	}
}

// (x, z) 위치에 로봇이 서 있을 때 발 밑 높이 (장애물 위면 장애물 윗면, 아니면 바닥 0)
float ground_at(float x, float z) {
	float g = 0.0f;
	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		const Obstacle& o = obstacles[i];
		if (fabsf(x - o.x) < o.w / 2 + ROBOT_R && fabsf(z - o.z) < o.d / 2 + ROBOT_R && o.h > g)
			g = o.h;
	}
	return g;
}

// (x, z) 로 이동하려 할 때 막히는지: 가장자리 밖이거나, 발 높이보다 높은 장애물이 있으면 막힘
bool edge_hit(float x, float z) {
	return x > SX - ROBOT_R || x < -SX + ROBOT_R || z > SZ - ROBOT_R || z < -SZ + ROBOT_R;
}
bool obstacle_hit(float x, float z) {
	return ground_at(x, z) > robotY + 0.05f;
}

void update(float dt) {
	// 앞면 슬라이딩
	float target = frontOpen ? 1.0f : 0.0f;
	if (openT < target) { openT += dt / 1.2f; if (openT > target) openT = target; }
	else if (openT > target) { openT -= dt / 1.2f; if (openT < target) openT = target; }

	// 걷기: 가장자리에 도달하면 뒤로 돌아서 걷고, 장애물을 만나면 이동하지 못하고 방향을 바꾼다
	float step = WALK_SPEED * speedMul * dt;
	float nx = robotX + dirX * step;
	float nz = robotZ + dirZ * step;
	if (edge_hit(nx, nz)) {
		dirX = -dirX;
		dirZ = -dirZ;
	}
	else if (obstacle_hit(nx, nz)) {
		// 좌/우로 꺾어 보고(랜덤 순서) 갈 수 있는 쪽으로, 둘 다 안되면 뒤로
		int sign = (rand() % 2) ? 1 : -1;
		int cand[3][2] = { { -dirZ * sign, dirX * sign }, { dirZ * sign, -dirX * sign }, { -dirX, -dirZ } };
		for (int c = 0; c < 3; c++) {
			float px = robotX + cand[c][0] * 0.2f;
			float pz = robotZ + cand[c][1] * 0.2f;
			if ((!edge_hit(px, pz) && !obstacle_hit(px, pz)) || c == 2) {
				dirX = cand[c][0];
				dirZ = cand[c][1];
				break;
			}
		}
	}
	else {
		robotX = nx;
		robotZ = nz;
	}

	// 팔다리 흔들기
	walkPhase += 6.0f * speedMul * dt;

	// 점프 / 낙하: 장애물 위로 올라가고 내려갈 수 있다
	robotVy -= GRAVITY * dt;
	robotY += robotVy * dt;
	float g = ground_at(robotX, robotZ);
	if (robotY <= g) {
		robotY = g;
		robotVy = 0.0f;
		onGround = true;
	}
	else {
		onGround = false;
	}

	camOrbitAngle -= camOrbitDir * 45.0f * dt;
}


// ---------------- [3] 그리기 ----------------
// 로봇: 머리, 몸통, 양 팔, 양 다리. 로컬 +z 가 앞쪽 (코가 붙어 있음)
void draw_robot() {
	float yaw = glm::degrees(atan2f((float)dirX, (float)dirZ));
	glm::mat4 base = glm::translate(glm::mat4(1.0f), glm::vec3(robotX, robotY, robotZ))
		* glm::rotate(glm::mat4(1.0f), glm::radians(yaw), glm::vec3(0, 1, 0));

	float amp = 20.0f + 12.0f * speedMul;       // 속도가 빠르면 팔다리를 더 크게 흔든다
	float swing = amp * sinf(walkPhase);

	glm::vec3 skin(0.95f, 0.8f, 0.6f);
	draw_box(base, glm::vec3(0.0f, 0.8f, 0.0f), glm::vec3(0.44f, 0.6f, 0.26f), glm::vec3(0.3f, 0.7f, 0.3f));           // 몸통
	draw_box(base, glm::vec3(0.0f, 1.25f, 0.0f), glm::vec3(0.3f, 0.3f, 0.3f), skin);                                   // 머리
	draw_box(base, glm::vec3(0.0f, 1.23f, 0.19f), glm::vec3(0.08f, 0.08f, 0.1f), glm::vec3(1.0f, 0.1f, 0.1f));         // 코 (앞면)

	// 다리: 엉덩이(y = 0.5)를 축으로 x축 회전, 양쪽 다리는 반대 위상 / 서로 다른 색
	glm::mat4 legL = base * glm::translate(glm::mat4(1.0f), glm::vec3(-0.12f, 0.5f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(swing), glm::vec3(1, 0, 0));
	glm::mat4 legR = base * glm::translate(glm::mat4(1.0f), glm::vec3(0.12f, 0.5f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(-swing), glm::vec3(1, 0, 0));
	draw_box(legL, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.16f, 0.5f, 0.16f), glm::vec3(0.2f, 0.3f, 1.0f));
	draw_box(legR, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.16f, 0.5f, 0.16f), glm::vec3(1.0f, 0.9f, 0.2f));

	// 팔: 어깨(y = 1.05)를 축으로 x축 회전, 다리와 반대로 흔든다
	glm::mat4 armL = base * glm::translate(glm::mat4(1.0f), glm::vec3(-0.31f, 1.05f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(-swing), glm::vec3(1, 0, 0));
	glm::mat4 armR = base * glm::translate(glm::mat4(1.0f), glm::vec3(0.31f, 1.05f, 0.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(swing), glm::vec3(1, 0, 0));
	draw_box(armL, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.12f, 0.5f, 0.12f), glm::vec3(0.1f, 0.9f, 0.9f));
	draw_box(armR, glm::vec3(0.0f, -0.25f, 0.0f), glm::vec3(0.12f, 0.5f, 0.12f), glm::vec3(1.0f, 0.4f, 0.8f));
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	// 은면제거 + 컬링 (카메라가 어디로 돌아도 안쪽 면만 보인다)
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	glViewport(0, 0, width, height);
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	// 카메라: 이동은 위치만, 공전은 월드를 -각도만큼 y축 회전 (화면 중심 y축 기준 공전)
	glm::vec3 lookDir = CAM_TARGET - CAM_START;
	glm::mat4 view = glm::lookAt(cameraPos, cameraPos + lookDir, glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(-camOrbitAngle), glm::vec3(0, 1, 0));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
	set_camera(view, proj);

	glm::mat4 I(1.0f);

	// 무대: 각 면을 다른 색으로, 법선이 안쪽을 향하도록 배치
	draw_plane(I, glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), -90.0f, 2 * SX, 2 * SZ, glm::vec3(0.55f, 0.45f, 0.35f));            // 바닥
	draw_plane(I, glm::vec3(0, SH, 0), glm::vec3(1, 0, 0), 90.0f, 2 * SX, 2 * SZ, glm::vec3(0.9f, 0.9f, 0.4f));               // 천장
	draw_plane(I, glm::vec3(-SX, SH / 2, 0), glm::vec3(0, 1, 0), 90.0f, 2 * SZ, SH, glm::vec3(0.8f, 0.3f, 0.3f));             // 왼쪽
	draw_plane(I, glm::vec3(SX, SH / 2, 0), glm::vec3(0, 1, 0), -90.0f, 2 * SZ, SH, glm::vec3(0.3f, 0.8f, 0.3f));             // 오른쪽
	draw_plane(I, glm::vec3(0, SH / 2, -SZ), glm::vec3(0, 1, 0), 0.0f, 2 * SX, SH, glm::vec3(0.3f, 0.4f, 0.85f));             // 뒤쪽

	// 앞면: 위쪽 모서리를 고정한 채 y 크기가 줄어들며 위로 슬라이딩 (바깥에서 보이도록 법선이 +z)
	float frontH = SH * (1.0f - openT);
	if (frontH > 0.01f)
		draw_plane(I, glm::vec3(0, SH - frontH / 2, SZ), glm::vec3(0, 1, 0), 0.0f, 2 * SX, frontH, glm::vec3(0.8f, 0.5f, 0.9f));

	for (int i = 0; i < OBSTACLE_COUNT; i++) {
		const Obstacle& o = obstacles[i];
		draw_box(I, glm::vec3(o.x, o.h / 2, o.z), glm::vec3(o.w, o.h, o.d), o.color);
	}

	draw_robot();
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

	// 카메라 이동: z/Z 앞뒤, x/X 좌우 (꾹 누르면 반복)
	if (ch == 'z') cameraPos.z -= MOVE_STEP;
	if (ch == 'Z') cameraPos.z += MOVE_STEP;
	if (ch == 'x') cameraPos.x += MOVE_STEP;
	if (ch == 'X') cameraPos.x -= MOVE_STEP;

	// +/- : 걷는 속도 (꾹 누르면 반복)
	if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
		speedMul += 0.25f;
		if (speedMul > 3.0f) speedMul = 3.0f;
	}
	if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
		speedMul -= 0.25f;
		if (speedMul < 0.25f) speedMul = 0.25f;
	}

	if (press) {
		if (ch == 'o') frontOpen = !frontOpen;              // 앞면 열기/닫기

		// w/a/s/d : 앞/뒤/좌/우 방향으로 걷는 방향 변경
		if (ch == 'w') { dirX = 0;  dirZ = -1; }
		if (ch == 's') { dirX = 0;  dirZ = 1; }
		if (ch == 'a') { dirX = -1; dirZ = 0; }
		if (ch == 'd') { dirX = 1;  dirZ = 0; }

		if (ch == 'j' && onGround) {                        // 점프
			robotVy = JUMP_SPEED;
			onGround = false;
		}

		// y: 시계, Y: 반시계 공전 (같은 키를 다시 누르면 멈춤)
		if (ch == 'y') camOrbitDir = (camOrbitDir == 1.0f) ? 0.0f : 1.0f;
		if (ch == 'Y') camOrbitDir = (camOrbitDir == -1.0f) ? 0.0f : -1.0f;

		if (ch == 'i') reset_state();                       // 모든 변환 리셋
		if (ch == 'q') glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}
