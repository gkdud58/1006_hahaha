#define WINDOW_TITLE "Exam"
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
//  시험용 템플릿  -  위쪽(obj 읽기 / 셰이더 / 그리기 도우미 / main)은 건드리지 않고 복붙만 한다.
//  시험에서는 아래 [1]~[4] 네 군데만 채우면 된다.
//
//   [1] 상태 변수          : 문제에 나오는 "것들"을 변수로 만든다 (켜짐/꺼짐 bool, 각도 float, 위치 vec3 ...)
//   [2] reset_state / update: 초기값 / 시간에 따라 변하는 것 (애니메이션, 이동, 충돌)
//   [3] drawScene          : 카메라(view, proj) 설정 + 물체 그리기
//   [4] key_callback / (마우스): 키보드, 마우스 명령
//
//  그리기 도우미 (위 base 에 이미 있음)
//   draw_box(parent, 중심, 크기, 색)            : 육면체. 중심이 기준. parent 행렬을 곱해주면 자식이 됨
//   draw_sphere(parent, 중심, 반지름, 색)       : 구
//   draw_plane(parent, 중심, 회전축, 각도, 가로, 세로, 색) : 평면. 회전 전 법선 +z (아래 [면 방향표] 참고)
//   set_camera(view, proj)                      : 뷰/투영 행렬을 셰이더로
//   random_range(lo, hi)                        : 랜덤 float
//
//  [면 방향표]  draw_plane 으로 "안쪽이 보이는" 벽을 만들 때 (컬링 켜져 있음)
//    바닥(법선 +y)    : 축 (1,0,0)  각도 -90      크기 (x길이, z길이)
//    천장(법선 -y)    : 축 (1,0,0)  각도 +90      크기 (x길이, z길이)
//    왼쪽벽(법선 +x)  : 축 (0,1,0)  각도 +90      크기 (z길이, 높이)
//    오른쪽벽(법선 -x): 축 (0,1,0)  각도 -90      크기 (z길이, 높이)
//    뒷벽(법선 +z)    : 회전 없음   각도 0        크기 (x길이, 높이)
//    앞벽(법선 -z)    : 축 (0,1,0)  각도 180
// ===================================================================================================

// ---------------- [1] 상태 변수 ----------------
bool perspectiveMode;           // true = 원근 투영, false = 직각 투영
float camAngle;                 // 카메라 y축 공전 각도
float camDist;                  // 카메라 거리 (줌)
float spinAngle;                // 예시: 회전하는 물체의 각도
bool spinOn;                    // 예시: 회전 켜짐/꺼짐

float swingAngle;
int swingDir;
bool swingOn;

float scaleY;
int scaleDir;
bool scaleYOn;

float swapT;        // 0 = 원래 자리, 1 = 바뀐 자리
float swapTarget;   // 0 또는 1 (키를 누를 때마다 번갈아)
// ---------------- [2] 초기화 / 업데이트 ----------------
void reset_state() {
	perspectiveMode = true;
	camAngle = 0.0f;
	camDist = 10.0f;

	spinAngle = 0.0f;
	spinOn = true;

	swingAngle = 0.5f;
	swingDir = 1;
	swingOn = true;

	scaleDir = 1;
	scaleY = 1.0f;
	scaleYOn = true;

	swapT = 0.0f;
	swapTarget = 0.0f;
}

// dt = 지난 프레임 이후 흐른 시간(초). "속도 * dt" 로 움직이면 컴퓨터가 빨라도 같은 속도
void update(float dt) {
	if (spinOn) {
		spinAngle += 60.0f * dt;        // 초당 60도
	}
	// 그네 스윙
	if (swingOn) {
		swingAngle += swingDir * 60.0f * dt;
		if (swingAngle > 45.0f) {
			swingAngle = 45.0f;
			swingDir = -swingDir;
		}
		if (swingAngle < -45.0f) {
			swingAngle = -45.0f;
			swingDir = -swingDir;
		}
	}
	// 기둥y크기
	if (scaleYOn) {
		scaleY += scaleDir * 0.5f * dt;
		if (scaleY > 1.5f) {
			scaleY = 1.5f;
			scaleDir = -scaleDir;
		}
		if (scaleY < 0.5f) {
			scaleY = 0.5f;
			scaleDir = -scaleDir;
		}
	}

	if (swapT < swapTarget) {
		swapT += dt / 1.5f;                 // 1.5초에 걸쳐 이동
		if (swapT > swapTarget) swapT = swapTarget;
	}
	else if (swapT > swapTarget) {
		swapT -= dt / 1.5f;
		if (swapT < swapTarget) swapT = swapTarget;
	}
}

// ---------------- [3] 그리기 ----------------
GLvoid drawScene() {
	glEnable(GL_DEPTH_TEST);            // 은면제거
	glEnable(GL_CULL_FACE);             // 컬링 (필요 없으면 이 줄 지우기)

	glViewport(0, 0, width, height);
	glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	// 카메라: lookAt(눈 위치, 바라보는 점, 위쪽 벡터)
	float a = glm::radians(camAngle);
	glm::vec3 eye(camDist * sinf(a), 4.0f, camDist * cosf(a));      // y축 공전
	glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f), glm::vec3(0, 1, 0));

	float aspect = (float)width / (float)height;
	glm::mat4 proj;
	if (perspectiveMode)
		proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
	else
		proj = glm::ortho(-4.0f * aspect, 4.0f * aspect, -4.0f, 4.0f, 0.1f, 100.0f);
	set_camera(view, proj);

	glm::mat4 I(1.0f);

	// 바닥
	draw_plane(I, glm::vec3(0, 0, 0), glm::vec3(1, 0, 0), -90.0f, 8.0f, 8.0f, glm::vec3(0.5f));

	// 예시 : 부모-자식 변환. 부모 행렬을 만들고 자식은 그 행렬 기준 좌표로 그린다
	float H = 2.0f * scaleY;
	glm::mat4 parent1 = glm::translate(I, glm::vec3(0.0f, 1.0f, 0.0f))
		* glm::rotate(I, glm::radians(spinAngle), glm::vec3(0, 1, 0));
	draw_box(parent1, glm::vec3(1.0f, H / 2 - 1, 0.0f), glm::vec3(0.1f, H, 1.0f), glm::vec3(1.0f, 0.5f, 0.2f));         // 부모
	draw_box(parent1, glm::vec3(-1.0f, H / 2 - 1, 0.0f), glm::vec3(0.1f, H, 1.0f), glm::vec3(1.0f, 0.5f, 0.2f));
	draw_box(parent1, glm::vec3(0.0f, H - 1.0f, 0.0f), glm::vec3(2.0f, 0.1f, 1.0f), glm::vec3(0.2f, 0.7f, 1.0f));   // 자식 (부모와 같이 돎)



	glm::mat4 parent2 = parent1 * glm::translate(I, glm::vec3(0.0f, H - 1.0f, 0.0f))
		* glm::rotate(I, glm::radians(swingAngle), glm::vec3(1, 0, 0));
	//draw_box(parent2, glm::vec3(0.7f, -0.7f, 0.0f), glm::vec3(0.1f, 1.5f, 0.1f), glm::vec3(1.0f, 0.5f, 0.5f));
	//draw_box(parent2, glm::vec3(-0.7f, -0.7f, 0.0f), glm::vec3(0.1f, 1.5f, 0.1f), glm::vec3(1.0f, 0.5f, 0.5f));         // 부모
	
	draw_box(parent2, glm::vec3(0.0f, -1.5f, 0.0f), glm::vec3(1.4f, 0.1f, 0.5f), glm::vec3(0.2f, 0.7f, 1.0f));   // 자식 (부모와 같이 돎)
	
	
	// 줄 두 개를 y축(걸이 중심) 기준으로 같이 돌림 -> 180도면 좌우 자리가 바뀜
	glm::mat4 ropeM = parent2 * glm::rotate(I, glm::radians(180.0f * swapT), glm::vec3(0, 1, 0));
	draw_box(ropeM, glm::vec3(0.7f, -0.7f, 0.0f), glm::vec3(0.1f, 1.5f, 0.1f), glm::vec3(1.0f, 0.3f, 0.3f));   // 빨강
	draw_box(ropeM, glm::vec3(-0.7f, -0.7f, 0.0f), glm::vec3(0.1f, 1.5f, 0.1f), glm::vec3(0.3f, 0.3f, 1.0f));   // 파랑
	// 의자(판)는 그대로 parent2 기준으로 그림

	draw_sphere(I, glm::vec3(2.0f, 0.5f, 0.0f), 0.5f, glm::vec3(0.9f, 0.9f, 0.2f));
}

// ---------------- [4] 키보드 ----------------
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
	if (action == GLFW_RELEASE)
		return;
	bool press = (action == GLFW_PRESS);        // press = 처음 누를 때만, repeat 포함 = 꾹 누르면 계속

	// shift 안 눌렀으면 소문자로 통일 -> 'a' 와 'A' 를 구분해서 쓸 수 있다
	unsigned int ch = key;
	if (ch >= 'A' && ch <= 'Z' && !(mods & GLFW_MOD_SHIFT))
		ch += 32;

	// 꾹 누르면 계속 반응해야 하는 것 (이동, 줌)은 press 바깥에
	if (key == GLFW_KEY_LEFT)  camAngle -= 3.0f;
	if (key == GLFW_KEY_RIGHT) camAngle += 3.0f;
	if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD)       camDist -= 0.5f;     // +
	if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT)  camDist += 0.5f;     // -

	// 한 번 누를 때만 (토글)
	if (press) {
		if (ch == 's') spinOn = !spinOn;
		if (ch == '2') scaleYOn = !scaleYOn;
		if (ch == '1') swingOn = !swingOn;
		if (ch == '3') {
			if (swapTarget == 0.0f)
				swapTarget = 1.0f;
			else
				swapTarget = 0.0f;
		}
		if (ch == 'p') perspectiveMode = true;      // p : 원근
		if (ch == 'P') perspectiveMode = false;     // P : 직각 (대문자는 'P' 로 비교)
		if (ch == 'r') reset_state();
		if (ch == 'q') glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}













// ===================================================================================================
//  레시피 모음 (컴파일 안 됨 - 필요한 부분을 위 [1]~[4] 로 복사해서 쓴다)
// ===================================================================================================
#if 0

// ---- A. 마우스 드래그 (파일 맨 위에 #define USE_MOUSE 를 추가해야 main 이 콜백을 등록함) ----
bool dragging; double lastMouseX;
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		dragging = (action == GLFW_PRESS);
		if (dragging) glfwGetCursorPos(window, &lastMouseX, nullptr);
	}
}
void cursor_pos_callback(GLFWwindow* window, double x, double y) {
	if (!dragging) return;
	angle -= (float)(x - lastMouseX) * 0.25f;       // 왼쪽 드래그 -> 각도 증가 (부호는 문제에 맞게)
	lastMouseX = x;
}

// ---- B. 뷰포트 여러 개 (각 영역을 scissor 로 따로 지운 뒤, view/proj 바꿔서 같은 장면을 다시 그림) ----
void set_viewport(int x, int y, int w, int h, const glm::vec3& bg, const glm::mat4& view, const glm::mat4& proj) {
	glViewport(x, y, w, h);
	glEnable(GL_SCISSOR_TEST); glScissor(x, y, w, h);
	glClearColor(bg.r, bg.g, bg.b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);
	set_camera(view, proj);
}
//  평면도(위에서): lookAt((0,10,0), (0,0,0), up=(0,0,-1)) + ortho
//  정면도(앞에서): lookAt((0,1,10), (0,1,0), up=(0,1,0)) + ortho

// ---- C. 부모-자식 계층 (탱크/로봇) : 관절 회전 = "관절 위치로 이동 * 회전" 후 물체는 관절 기준으로 그림 ----
//  glm::mat4 arm = body * glm::translate(I, glm::vec3(0.3f, 1.0f, 0.0f))      // 어깨 위치
//                       * glm::rotate(I, glm::radians(swing), glm::vec3(1,0,0)); // 어깨를 축으로 회전
//  draw_box(arm, glm::vec3(0, -0.25f, 0), glm::vec3(0.1f, 0.5f, 0.1f), color);   // 팔은 어깨 아래로 늘어뜨림
//  행렬 곱 순서: 부모 * 이동 * 회전 * 크기  (오른쪽부터 먼저 적용된다고 생각)
//  자전 = 물체 중심에서 회전 / 공전 = rotate 를 translate 앞에 둠:  rotate(공전) * translate(거리) * rotate(자전)

// ---- D. 왕복 / 목표값으로 이동 ----
//  왕복:  v += dir*speed*dt;  if (v > max) { v = max; dir = -1; }  if (v < min) { v = min; dir = 1; }
//  토글 이동: target = on ? 1.0f : 0.0f;  if (t < target) t = min(t + dt/걸리는시간, target); else if (t > target) t = max(t - dt/걸리는시간, target);
//  부드럽게:  t = t*t*(3 - 2*t)      포물선:  y = height * sinf(PI * t)

// ---- E. 충돌 ----
//  벽(공):  if (pos.x > HX - r) { pos.x = HX - r; vel.x = -fabsf(vel.x); }   if (pos.x < -HX + r) { pos.x = -HX + r; vel.x = fabsf(vel.x); }
//  AABB 위 서기: 중심이 상자 범위 안(|x-ox| < w/2 + R, |z-oz| < d/2 + R)이면 윗면 높이 = h, 아니면 0
//  중력/점프:  vy -= G*dt;  y += vy*dt;  if (y <= ground) { y = ground; vy = 0; onGround = true; }   점프키: vy = 6
//  구-상자:  closest = glm::clamp(pos, c - half, c + half);  diff = pos - closest;  dist² < r² 이면 n = normalize(diff) 로 반사

// ---- F. 카메라 ----
//  1인칭: eye = 물체위치 + 눈높이;  lookAt(eye, eye + 바라보는방향, up)   방향 = (cos, 0, -sin) 또는 (sin, 0, cos) 식으로 각도에서 계산
//  제자리 자전: lookDir 을 rotate(자전각, y) 로 돌림 / 화면중심 공전: view = lookAt(...) * rotate(-공전각, y)
//  투영 바꾸기: 원근 glm::perspective(fov, aspect, near, far) / 직각 glm::ortho(l, r, b, t, near, far)

// ---- G. 상태 하나를 여러 개로 : 구조체 배열 + active 플래그 (공 최대 5개 등) ----
//  struct Ball { bool active; glm::vec3 pos, vel; float r; glm::vec3 color; };  Ball balls[5];
//  생성: 첫 번째 active==false 인 칸을 찾아서 채움 / 그릴 때 active 인 것만

// ---- H. 와이어 모드 : glPolygonMode(GL_FRONT_AND_BACK, wire ? GL_LINE : GL_FILL); ----

#endif
