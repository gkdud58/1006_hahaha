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
			if (result != 6)   // pyramid.obj처럼 "f 4 3 5" 형식(법선 없음)도 읽도록
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

// 큐브 모델 (cube.obj)
Model cubeModel;
GLuint cubeVao, cubeVbo;      // 위치만 저장 (색은 glVertexAttrib3f로 그릴 때마다 지정)

bool perspectiveMode;       // true = 원근 투영, false = 직각 투영
bool wireMode;              // true = 와이어 모델, false = 솔리드 모델

const float PI = 3.14159265f;
const float MOVE_STEP = 0.1f;
const float LIMIT = 1.8f;   // 탱크가 바닥 위에서 움직일 수 있는 범위 (x, z 각각 -LIMIT ~ LIMIT)

float tankX, tankZ;         // 방향키: 탱크의 xz 평면 위치

bool lowerScaleOn;          // d : 아래 몸체 y 크기 확대/축소
float lowerScaleY, lowerScaleDir;

bool middleSpinOn;          // t : 중앙 몸체 y축 회전
float middleAngle;

float swapT, swapTarget;    // l : 상부 몸체 위치 교환 (0 = 원래 자리, 1 = 바뀐 자리)

bool barrelSpinOn;          // g : 포신 y축 회전
float barrelAngle;

bool flagSpinOn;            // p : 깃대 x축 회전
float flagAngle;

const glm::vec3 CAM_START = glm::vec3(3.0f, 3.0f, 10.0f);   // 카메라 시작 위치 (원점을 바라봄)
glm::vec3 cameraPos;        // z/Z, x/X : 카메라 이동
float camSelfDir, camSelfAngle;    // y/Y : 카메라 제자리 자전 (0 = 멈춤, 1 = 시계, -1 = 반시계)
float camOrbitDir, camOrbitAngle;  // r/R : 화면 중심 y축 공전

// 탱크 부위별 색: 0 아래L 1 아래R 2 중앙 3 상부L 4 상부R 5 포신L 6 포신R 7 깃대L 8 깃대R
glm::vec3 partColor[9];

void reset_state() {
	perspectiveMode = true;
	wireMode = false;
	tankX = 0.0f;
	tankZ = 0.0f;

	lowerScaleOn = false;
	lowerScaleY = 1.0f;
	lowerScaleDir = 1.0f;

	middleSpinOn = false;
	middleAngle = 0.0f;

	swapT = 0.0f;
	swapTarget = 0.0f;

	barrelSpinOn = false;
	barrelAngle = 0.0f;

	flagSpinOn = false;
	flagAngle = 0.0f;

	cameraPos = CAM_START;
	camSelfDir = 0.0f;
	camSelfAngle = 0.0f;
	camOrbitDir = 0.0f;
	camOrbitAngle = 0.0f;

	partColor[0] = glm::vec3(0.1f, 0.5f, 0.1f);
	partColor[1] = glm::vec3(0.2f, 0.7f, 0.2f);
	partColor[2] = glm::vec3(1.0f, 0.5f, 0.0f);
	partColor[3] = glm::vec3(1.0f, 0.2f, 0.8f);
	partColor[4] = glm::vec3(0.8f, 0.2f, 1.0f);
	partColor[5] = glm::vec3(0.0f, 0.5f, 1.0f);
	partColor[6] = glm::vec3(0.0f, 0.8f, 1.0f);
	partColor[7] = glm::vec3(1.0f, 1.0f, 0.0f);
	partColor[8] = glm::vec3(1.0f, 0.6f, 0.0f);
}

// cube.obj를 읽어서 VAO/VBO에 올림 (삼각형마다 정점 복사, 위치만)
void make_cube_buffers() {
	read_obj_file("cube.obj", &cubeModel);

	size_t n = cubeModel.face_count * 3;
	GLfloat* pos = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));
	for (size_t i = 0; i < cubeModel.face_count; i++) {
		unsigned int idx[3] = { cubeModel.faces[i].v1, cubeModel.faces[i].v2, cubeModel.faces[i].v3 };
		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;
			pos[k * 3 + 0] = cubeModel.vertices[idx[j]].x;
			pos[k * 3 + 1] = cubeModel.vertices[idx[j]].y;
			pos[k * 3 + 2] = cubeModel.vertices[idx[j]].z;
		}
	}

	glGenVertexArrays(1, &cubeVao);
	glBindVertexArray(cubeVao);
	glGenBuffers(1, &cubeVbo);
	glBindBuffer(GL_ARRAY_BUFFER, cubeVbo);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	free(pos);
}


// 큐브 하나 그리기: 모델 행렬 + 색을 받아서 그림
void draw_cube(const glm::mat4& model, const glm::vec3& color) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(cubeVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);   // 색 배열을 쓰지 않으므로 고정 색으로 지정
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(cubeModel.face_count * 3));
}

// 부모 행렬(parent) 기준으로 중심이 pos, 크기가 size인 큐브를 그림
// cube.obj는 0~1 이라 모서리가 원점이므로, 먼저 -0.5 이동해서 중심을 원점으로 맞춘 뒤 크기를 적용한다
// 순서: 부모 * 위치 * 크기 * 중심보정  (자식은 부모의 scale을 받지 않도록 parent에는 scale을 넣지 않는다)
void draw_box(const glm::mat4& parent, const glm::vec3& pos, const glm::vec3& size, const glm::vec3& color) {
	glm::mat4 m = parent
		* glm::translate(glm::mat4(1.0f), pos)
		* glm::scale(glm::mat4(1.0f), size)
		* glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
	draw_cube(m, color);
}


void update(float dt) {
	// d: 아래 몸체 y 크기 (0.5 ~ 1.5배 왕복)
	if (lowerScaleOn) {
		lowerScaleY += lowerScaleDir * 0.5f * dt;
		if (lowerScaleY < 0.5f) { lowerScaleY = 0.5f; lowerScaleDir = 1.0f; }
		if (lowerScaleY > 1.5f) { lowerScaleY = 1.5f; lowerScaleDir = -1.0f; }
	}

	// t: 중앙 몸체 회전
	if (middleSpinOn)
		middleAngle += 60.0f * dt;

	// l: 상부 몸체 위치 교환 (swapTarget 을 향해 1.5초에 걸쳐 이동)
	if (swapT < swapTarget) {
		swapT += dt / 1.5f;
		if (swapT > swapTarget)
			swapT = swapTarget;
	}
	else if (swapT > swapTarget) {
		swapT -= dt / 1.5f;
		if (swapT < swapTarget)
			swapT = swapTarget;
	}

	// g: 포신 회전, p: 깃대 회전
	if (barrelSpinOn)
		barrelAngle += 90.0f * dt;
	if (flagSpinOn)
		flagAngle += 90.0f * dt;

	// y/Y: 카메라 제자리 자전, r/R: 카메라 공전 (시계 방향 = 각도 감소)
	camSelfAngle -= camSelfDir * 45.0f * dt;
	camOrbitAngle -= camOrbitDir * 45.0f * dt;
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// 은면제거
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// 솔리드/와이어 모델 선택
	if (wireMode)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	glUseProgram(shaderProgramID);


	//--- 뷰잉 변환
	// 기본 바라보는 방향(시작 위치에서 원점을 향함)을 camSelfAngle 만큼 y축으로 돌려서 제자리 자전을 만든다
	glm::vec3 lookDir = glm::vec3(glm::rotate(glm::mat4(1.0f), glm::radians(camSelfAngle), glm::vec3(0, 1, 0))
		* glm::vec4(-CAM_START, 0.0f));
	glm::mat4 view = glm::lookAt(cameraPos, cameraPos + lookDir, glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(-camOrbitAngle), glm::vec3(0, 1, 0));

	//--- 투영 변환
	// 원근/직각 투영
	glm::mat4 proj;
	if (perspectiveMode) {
		proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
	}
	else {
		float aspect = (float)width / (float)height;
		proj = glm::ortho(-3.0f * aspect, 3.0f * aspect, -3.0f, 3.0f, 0.1f, 100.0f);
	}
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));

	// 바닥판 (윗면 y = 0.05)
	draw_box(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(5.0f, 0.1f, 5.0f), glm::vec3(0.5f));

	// 탱크 전체의 기준 행렬 (방향키로 xz 평면 이동)
	glm::mat4 tankM = glm::translate(glm::mat4(1.0f), glm::vec3(tankX, 0.0f, tankZ));

	float lowerH = 0.3f * lowerScaleY;
	float lowerTop = 0.05f + lowerH;
	draw_box(tankM, glm::vec3(-0.4f, 0.05f + lowerH / 2, 0.0f), glm::vec3(0.5f, lowerH, 0.6f), partColor[0]);
	draw_box(tankM, glm::vec3(0.4f, 0.05f + lowerH / 2, 0.0f), glm::vec3(0.5f, lowerH, 0.6f), partColor[1]);

	glm::mat4 middleM = tankM
		* glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, lowerTop + 0.1f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(middleAngle), glm::vec3(0, 1, 0));
	draw_box(middleM, glm::vec3(0.0f), glm::vec3(1.0f, 0.2f, 0.4f), partColor[2]);

	float arc = 0.6f * sinf(PI * swapT);
	glm::mat4 upperL = middleM * glm::translate(glm::mat4(1.0f), glm::vec3(-0.3f + 0.6f * swapT, 0.25f, arc));
	glm::mat4 upperR = middleM * glm::translate(glm::mat4(1.0f), glm::vec3(0.3f - 0.6f * swapT, 0.25f, -arc));
	draw_box(upperL, glm::vec3(0.0f), glm::vec3(0.5f, 0.3f, 0.2f), partColor[3]);
	draw_box(upperR, glm::vec3(0.0f), glm::vec3(0.5f, 0.3f, 0.2f), partColor[4]);

	glm::mat4 barrelL = upperL * glm::rotate(glm::mat4(1.0f), glm::radians(barrelAngle), glm::vec3(0, 1, 0)) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.3f));
	glm::mat4 barrelR = upperR * glm::rotate(glm::mat4(1.0f), glm::radians(-barrelAngle), glm::vec3(0, 1, 0)) * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.3f));
		
	draw_box(barrelL, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.1f, 0.1f, 0.4f), partColor[5]);
	draw_box(barrelR, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.1f, 0.1f, 0.4f), partColor[6]);

	glm::mat4 flagL = upperL * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.15f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(flagAngle), glm::vec3(1, 0, 0));
	glm::mat4 flagR = upperR * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.15f, 0.0f))
		* glm::rotate(glm::mat4(1.0f), glm::radians(-flagAngle), glm::vec3(1, 0, 0));
	draw_box(flagL, glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(0.05f, 0.5f, 0.05f), partColor[7]);
	draw_box(flagR, glm::vec3(0.0f, 0.25f, 0.0f), glm::vec3(0.05f, 0.5f, 0.05f), partColor[8]);
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

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE)
		return;
	bool press = (action == GLFW_PRESS);

	// 대문자/소문자 구분이 필요할 때를 위해 shift 없으면 소문자로 통일
	unsigned int ch = key;
	if (ch >= 'A' && ch <= 'Z' && !(mods & GLFW_MOD_SHIFT))
		ch += 32;

	// 방향키: 탱크를 xz 평면에서 이동 (꾹 누르면 반복), 바닥 밖으로 나가지 않게 제한
	if (key == GLFW_KEY_LEFT && tankX - MOVE_STEP >= -LIMIT)
		tankX -= MOVE_STEP;
	if (key == GLFW_KEY_RIGHT && tankX + MOVE_STEP <= LIMIT)
		tankX += MOVE_STEP;
	if (key == GLFW_KEY_UP && tankZ - MOVE_STEP >= -LIMIT)
		tankZ += MOVE_STEP;
	if (key == GLFW_KEY_DOWN && tankZ + MOVE_STEP <= LIMIT)
		tankZ -= MOVE_STEP;

	// 카메라 이동: z/Z = z축 양/음, x/X = x축 양/음 (꾹 누르면 반복)
	if (ch == 'z') cameraPos.z += MOVE_STEP;
	if (ch == 'Z') cameraPos.z -= MOVE_STEP;
	if (ch == 'x') cameraPos.x += MOVE_STEP; 
	if (ch == 'X') cameraPos.x -= MOVE_STEP;

	if (press) {
		if (ch == 'd') lowerScaleOn = !lowerScaleOn;      // 아래 몸체 확대/축소 시작/멈춤
		if (ch == 't') middleSpinOn = !middleSpinOn;      // 중앙 몸체 회전 시작/멈춤
		if (ch == 'l') {                                  // 상부 몸체 위치 교환 (누를 때마다 번갈아)
			if (swapTarget == 0.0f)
				swapTarget = 1.0f;
			else
				swapTarget = 0.0f;
		}
		if (ch == 'g') barrelSpinOn = !barrelSpinOn;      // 포신 회전 시작/멈춤
		if (ch == 'p') flagSpinOn = !flagSpinOn;          // 깃대 회전 시작/멈춤

		// y: 시계, Y: 반시계 (같은 키를 다시 누르면 멈춤)
		if (ch == 'y') {
			if (camSelfDir == 1.0f) camSelfDir = 0.0f;
			else camSelfDir = 1.0f;
		}
		if (ch == 'Y') {
			if (camSelfDir == -1.0f) camSelfDir = 0.0f;
			else camSelfDir = -1.0f;
		}
		if (ch == 'r') {
			if (camOrbitDir == 1.0f) camOrbitDir = 0.0f;
			else camOrbitDir = 1.0f;
		}
		if (ch == 'R') {
			if (camOrbitDir == -1.0f) camOrbitDir = 0.0f;
			else camOrbitDir = -1.0f;
		}

		// o: 모든 부위 색을 무작위로 변경
		if (ch == 'o') {
			for (int i = 0; i < 9; i++)
				partColor[i] = glm::vec3(rand() % 101 / 100.0f, rand() % 101 / 100.0f, rand() % 101 / 100.0f);
		}

		if (ch == 'c') reset_state();                      // 초기화
	}

	if (press && ch == 'q')
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// ***************************************************************************************************************************************
//--- 메인 함수
int main(int argc, char** argv)
{
	width = 1200;
	height = 1200;

	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//--- 윈도우 생성
	GLFWwindow* window = glfwCreateWindow(width, height, "Example19", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		return -1;
	}
	//--- 생성한 윈도우의 OpenGL Context를 현재 Context로 설정
	glfwMakeContextCurrent(window);

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

	make_cube_buffers();

	srand((unsigned int)time(NULL));
	reset_state();
	glfwSetKeyCallback(window, key_callback);

	//**************************렌더링 루프******************************
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		double nowTime = glfwGetTime();
		update((float)(nowTime - lastTime));
		lastTime = nowTime;

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
