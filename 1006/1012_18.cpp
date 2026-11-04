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

// 구 모델 (sphere.obj, 반지름 1, 중심 원점) - 중심 구 / 행성 / 달 모두 이 모델을 크기만 바꿔서 사용
Model sphereModel;
GLuint sphereVao, sphereVbo;      // 위치만 저장 (색은 glVertexAttrib3f로 그릴 때마다 지정)

// 궤도 선 (xz 평면 위의 단위 원, 반지름 1) - 그릴 때 scale/rotate/translate로 원하는 궤도를 만든다
const int ORBIT_SEG = 100;
GLuint orbitVao, orbitVbo;

// 행성 / 달 개수
const int PLANET_COUNT = 3;

typedef struct {
	float orbitRadius;      // 공전 반지름 (중심 구 기준)
	float orbitSpeed;       // 공전 속도 (도/초)
	float orbitAngle;       // 현재 공전 각도
	float tiltAngle;        // 궤도 평면 기울기 (경로1: 0, 경로2: +45 반시계, 경로3: -45 시계)
	float size;             // 행성 크기

	float moonRadius;       // 달 궤도 반지름 (행성 기준)
	float moonSpeed;        // 달 공전 속도
	float moonAngle;        // 달 현재 공전 각도
	float moonSpin;         // 3번 키: 달 궤도가 행성의 z축을 중심으로 자전한 각도
	float moonSize;         // 달 크기

	glm::vec3 color;        // 행성 색
	glm::vec3 moonColor;    // 달 색
} Planet;

Planet planet[PLANET_COUNT];

// 중심 구
float centerSize;           // 현재 크기
glm::vec3 centerColor;      // 현재 색

// 키보드 상태
bool perspectiveMode;       // p/P : true = 원근 투영, false = 직각 투영
bool wireMode;              // m/M : true = 와이어 모델, false = 솔리드 모델
float moveX, moveY, moveZ;  // w/a/s/d, +/- : 도형 전체 이동량
const float MOVE_STEP = 0.1f;

bool orbitScaleOn;          // 1 : 궤도 반지름 커지는/작아지는 애니메이션
float orbitScaleK;          // 궤도 반지름 배율 (예: 0.5 ~ 1.5 사이를 왕복)
float orbitScaleDir;        // +1 / -1

bool centerPulseOn;         // 2 : 중심 구 확대/축소 + 색 변경 애니메이션
float centerPulseDir;       // +1 / -1

bool moonSpinOn;            // 3 : 달 궤도 z축 자전 애니메이션

float trans_Rcolor = 1.0;
float trans_Gcolor = 0.7;
float trans_Bcolor = 0.0;

void reset_state() {
	perspectiveMode = true;
	wireMode = false;
	moveX = moveY = moveZ = 0.0f;

	orbitScaleOn = false;
	orbitScaleK = 1.0f;
	orbitScaleDir = 1.0f;

	centerPulseOn = false;
	centerPulseDir = 1.0f;
	centerSize = 0.5f;

	centerColor = glm::vec3(trans_Rcolor, trans_Gcolor, trans_Bcolor);

	moonSpinOn = false;

	// 행성 3개: 반지름, 속도, 기울기, 색을 모두 다르게
	float radius[PLANET_COUNT] = { 1.2f, 1.8f, 2.4f };
	float speed[PLANET_COUNT] = { 90.0f, 60.0f, 40.0f };
	float tilt[PLANET_COUNT] = { 0.0f, 45.0f, -45.0f };   // 경로1: xz평면 / 경로2: 반시계 45도 / 경로3: 시계 45도
	
	for (int i = 0; i < PLANET_COUNT; i++) {
		planet[i].orbitRadius = radius[i];
		planet[i].orbitSpeed = speed[i];
		planet[i].orbitAngle = 0.0f;
		planet[i].tiltAngle = tilt[i];
		planet[i].size = 0.2f;

		planet[i].moonRadius = 0.4f;
		planet[i].moonSpeed = 150.0f + 30.0f * i;
		planet[i].moonAngle = 0.0f;
		planet[i].moonSpin = 0.0f;
		planet[i].moonSize = 0.08f;

		planet[i].color = glm::vec3(0, 0.5f, 1);       // 행성 공통 색 (파랑)
		planet[i].moonColor = glm::vec3(0,1,0);
	}
}

// sphere.obj를 읽어서 VAO/VBO에 올림 (삼각형마다 정점 복사, 위치만)
void make_sphere_buffers() {
	read_obj_file("sphere.obj", &sphereModel);

	size_t n = sphereModel.face_count * 3;
	GLfloat* pos = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));
	for (size_t i = 0; i < sphereModel.face_count; i++) {
		unsigned int idx[3] = { sphereModel.faces[i].v1, sphereModel.faces[i].v2, sphereModel.faces[i].v3 };
		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;
			pos[k * 3 + 0] = sphereModel.vertices[idx[j]].x;
			pos[k * 3 + 1] = sphereModel.vertices[idx[j]].y;
			pos[k * 3 + 2] = sphereModel.vertices[idx[j]].z;
		}
	}

	glGenVertexArrays(1, &sphereVao);
	glBindVertexArray(sphereVao);
	glGenBuffers(1, &sphereVbo);
	glBindBuffer(GL_ARRAY_BUFFER, sphereVbo);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	free(pos);
}

// xz 평면 위의 단위 원 (GL_LINE_LOOP로 그림)
void make_orbit_buffers() {
	GLfloat pos[ORBIT_SEG * 3];
	for (int i = 0; i < ORBIT_SEG; i++) {
		float a = 2.0f * 3.14159265f * i / ORBIT_SEG;
		pos[i * 3 + 0] = cosf(a);
		pos[i * 3 + 1] = 0.0f;
		pos[i * 3 + 2] = sinf(a);
	}

	glGenVertexArrays(1, &orbitVao);
	glBindVertexArray(orbitVao);
	glGenBuffers(1, &orbitVbo);
	glBindBuffer(GL_ARRAY_BUFFER, orbitVbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(pos), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
}

// 구 하나 그리기: 모델 행렬 + 색을 받아서 그림
void draw_sphere(const glm::mat4& model, const glm::vec3& color) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(sphereVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);   // 색 배열을 쓰지 않으므로 고정 색으로 지정
	glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(sphereModel.face_count * 3));
}

// 궤도 선 하나 그리기: 모델 행렬 + 색을 받아서 그림 (선은 항상 와이어로 보여야 하므로 호출 쪽에서 모드 처리)
void draw_orbit(const glm::mat4& model, const glm::vec3& color) {
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(orbitVao);
	glVertexAttrib3f(1, color.r, color.g, color.b);
	glDrawArrays(GL_LINE_LOOP, 0, ORBIT_SEG);
}

void update(float dt) {
	// 행성/달 공전 각도 갱신
	for (int i = 0; i < PLANET_COUNT; ++i) {
		planet[i].orbitAngle += planet[i].orbitSpeed * dt;
		planet[i].moonAngle  += planet[i].moonSpeed  * dt;
	}
	// 키 1: 궤도 반지름 배율 왕복
	if (orbitScaleOn) {
		orbitScaleK += orbitScaleDir * 0.5f * dt;
		if (orbitScaleK < 0.5f) { orbitScaleK = 0.5f; orbitScaleDir = 1.0f; }
		if (orbitScaleK > 1.5f) { orbitScaleK = 1.5f; orbitScaleDir = -1.0f; }
	}

	// 키 2: 중심 구 크기/색 왕복
	if (centerPulseOn) {
		centerSize += centerPulseDir * 0.3f * dt;
		if (centerSize > 0.7f) { centerSize = 0.7f; centerPulseDir = -1.0f; }
		if (centerSize < 0.3f) { centerSize = 0.3f; centerPulseDir = 1.0f; }

		// 크기에 맞춰 색도 변경
		float temp = centerSize * 0.2;
		centerColor = glm::vec3(trans_Rcolor + temp, trans_Gcolor + temp , trans_Bcolor + temp);
	}

	// 키 3: 달 궤도 자전 각도 갱신
	if (moonSpinOn) {
		for (int i = 0; i < PLANET_COUNT; ++i) {
			planet[i].moonSpin += planet[i].moonSpeed * dt;
		}
	}
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
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

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


	// 도형 전체 이동 (w/a/s/d, +/-)
	// 도형 전체 이동 및 회전
	glm::mat4 world = glm::translate(glm::mat4(1.0f), glm::vec3(moveX, moveY, moveZ));

	// ---------- 중심 구 ----------
	glm::mat4 centerM = world * glm::scale(glm::mat4(1.0f), glm::vec3(centerSize));
	draw_sphere(centerM, centerColor);

	for (int i = 0; i < PLANET_COUNT; i++) {
		// 이 행성의 궤도 평면 기울기 (xz 평면을 z축 기준으로 기울임)
		glm::mat4 tilt = glm::rotate(glm::mat4(1.0f), glm::radians(planet[i].tiltAngle), glm::vec3(0, 0, 1));

		float r = planet[i].orbitRadius * orbitScaleK;
		float moonR = planet[i].moonRadius * orbitScaleK;

		// ---------- 행성 궤도 선 ----------
		// 와이어/솔리드와 상관없이 선으로 그려지는 부분이므로 glDrawArrays(GL_LINE_LOOP) 사용 (draw_orbit)
		glm::mat4 orbitM = world * tilt * glm::scale(glm::mat4(1.0f), glm::vec3(r));
		draw_orbit(orbitM, glm::vec3(1.0f));

		// ---------- 행성 ----------
		// 행성: 공전 각도만큼 회전 후 궤도 반지름만큼 이동
		glm::mat4 planetPos = world * tilt
			* glm::rotate(glm::mat4(1.0f), glm::radians(planet[i].orbitAngle), glm::vec3(0, 1, 0))
			* glm::translate(glm::mat4(1.0f), glm::vec3(r, 0.0f, 0.0f));
		glm::mat4 planetM = planetPos * glm::scale(glm::mat4(1.0f), glm::vec3(planet[i].size));
		draw_sphere(planetM, planet[i].color);

		// ---------- 달 궤도 / 달 ----------
		// 달: 행성 위치를 기준으로 공전 (키 3: 달 궤도 자전)
		// 행성 위치(마지막 열)만 뽑아서 달 궤도의 중심으로 사용
		glm::mat4 moonBase = glm::translate(glm::mat4(1.0f), glm::vec3(planetPos[3]));
		// 행성의 z축 기준 달 궤도 자전
		glm::mat4 moonSpinM = glm::rotate(glm::mat4(1.0f), glm::radians(planet[i].moonSpin), glm::vec3(0, 0, 1));

		// 달 궤도 평면: 행성이 중심 구를 도는 평면과 같은 기울기(tilt)로, xz 평면 위에서 y축 기준 공전
		glm::mat4 moonOrbitM = moonBase * moonSpinM
			* glm::scale(glm::mat4(1.0f), glm::vec3(moonR));
		draw_orbit(moonOrbitM, glm::vec3(0.7f));

		glm::mat4 moonM = moonBase * moonSpinM
			* glm::rotate(glm::mat4(1.0f), glm::radians(planet[i].moonAngle), glm::vec3(0, 1, 0))
			* glm::translate(glm::mat4(1.0f), glm::vec3(moonR, 0.0f, 0.0f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(planet[i].moonSize));
		draw_sphere(moonM, planet[i].moonColor);
	}
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

	// p/P: 직각/원근 투영
	if (press && ch == 'p')
		perspectiveMode = false;
	if (press && ch == 'P')
		perspectiveMode = true;
	// m/M: 솔리드/와이어
	if (press && ch == 'm')
		wireMode = false;
	if (press && ch == 'M')
		wireMode = true;

	// w/a/s/d: 위/왼쪽/아래/오른쪽 이동
	if (ch == 'w')
		moveY += MOVE_STEP;
	if (ch == 'a')
		moveX -= MOVE_STEP;
	if (ch == 's')
		moveY -= MOVE_STEP;
	if (ch == 'd')
		moveX += MOVE_STEP;

	// +/-: 앞/뒤 이동
	if (key == GLFW_KEY_EQUAL)
		moveZ += MOVE_STEP;
	if (key == GLFW_KEY_MINUS)
		moveZ -= MOVE_STEP;

	// 1: 궤도 반지름 애니메이션
	if (press && ch == '1')
		orbitScaleOn = !orbitScaleOn;
	// 2: 중심 구 확대/축소 애니메이션
	if (press && ch == '2')
		centerPulseOn = !centerPulseOn;
	// 3: 달 궤도 자전 애니메이션
	if (press && ch == '3')
		moonSpinOn = !moonSpinOn;


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
	GLFWwindow* window = glfwCreateWindow(width, height, "Example18", nullptr, nullptr);
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

	make_sphere_buffers();
	make_orbit_buffers();

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
