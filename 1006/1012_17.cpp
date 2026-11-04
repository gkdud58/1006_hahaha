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

const int SHAPE_COUNT = 3;
Model models[SHAPE_COUNT];
GLuint modelVao[SHAPE_COUNT], modelVbo[SHAPE_COUNT][2];
glm::mat4 fixMatrix[SHAPE_COUNT];

// xyz축 (고정)
GLfloat axisVertices[] = {
	-1.8f,0,0,  1.8f,0,0,
	0,-1.8f,0,  0,1.8f,0,
	0,0,-1.8f,  0,0,1.8f
};
GLfloat axisColors[] = {
	0,0,0, 0,0,0,
	0,0,0, 0,0,0,
	0,0,0, 0,0,0
};
GLuint axisVao, axisVbo[2];

GLfloat palette[8][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f},
	{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.0f}, {0.5f, 0.5f, 1.0f}
};

typedef struct {
	int shape;
	int slot;
	float selfX, selfY;
	float orbit;
	float scale;                // 원점 기준 확대/축소 (b/B)
	float selfScale;            // 제자리 확대/축소 (a/A)
	float moveX, moveY;
} Object;

Object obj[2];

const float DIST = 1.0f;
const float ARC_HEIGHT = 0.9f;
const float ROTATE_STEP = 10.0f;
const float MOVE_STEP = 0.1f;

int selectMode;
int animType;
float animK;
bool vMode;

void reset_state() {
	selectMode = 3;
	animType = 0;
	animK = 0.0f;
	vMode = false;

	for (int i = 0; i < 2; i++) {
		obj[i].slot = i;
		obj[i].selfX = 0.0f;
		obj[i].selfY = 0.0f;
		obj[i].orbit = 0.0f;
		obj[i].scale = 1.0f;
		obj[i].selfScale = 1.0f;
		obj[i].moveX = 0.0f;
		obj[i].moveY = 0.0f;
	}
	obj[0].shape = 1;
	obj[1].shape = 0;
}

// 모델의 삼각형마다 정점을 복사하고, 삼각형별 색상(colorIndex)을 줘서 VAO, VBO(위치, 색상)에 올림
void make_model_buffers(Model* model, int* colorIndex, GLuint* vao, GLuint* vbo) {
	size_t n = model->face_count * 3;
	GLfloat* pos = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));
	GLfloat* col = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));

	for (size_t i = 0; i < model->face_count; i++) {
		unsigned int idx[3] = { model->faces[i].v1, model->faces[i].v2, model->faces[i].v3 };
		int c = colorIndex[i];

		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;
			pos[k * 3 + 0] = model->vertices[idx[j]].x;
			pos[k * 3 + 1] = model->vertices[idx[j]].y;
			pos[k * 3 + 2] = model->vertices[idx[j]].z;
			col[k * 3 + 0] = 0.0f;
			col[k * 3 + 1] = 0.0f;
			col[k * 3 + 2] = 0.0f;
		}
	}

	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glGenBuffers(2, vbo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), pos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), col, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	free(pos);
	free(col);
}

float move_toward(float current, float target, float step) {
	if (current < target) {
		current += step;
		if (current > target)
			current = target;
	}
	else if (current > target) {
		current -= step;
		if (current < target)
			current = target;
	}
	return current;
}

void update(float dt) {
	if (animType != 0) {
		animK += dt / 1.5f;
		if (animK >= 1.0f) {
			obj[0].slot = 1 - obj[0].slot;
			obj[1].slot = 1 - obj[1].slot;
			animType = 0;
			animK = 0.0f;
		}
	}

	if (vMode) {
		for (int i = 0; i < 2; i++) {
			obj[i].selfY += 90.0f * dt;
			obj[i].orbit += 60.0f * dt;
		}
	}
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glUseProgram(shaderProgramID);

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 4.5f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 100.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));

	glm::mat4 tilt = glm::rotate(glm::mat4(1.0f), glm::radians(35.0f), glm::vec3(1, 0, 0))
		* glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(0, 1, 0));


	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(tilt));
	glBindVertexArray(axisVao);
	glDrawArrays(GL_LINES, 0, 6);

	float arcSign[2] = { 1.0f, -1.0f };
	float uFlag = 0.0f;
	if (animType == 2)
		uFlag = 1.0f;

	for (int i = 0; i < 2; i++) {
		float homeX = -DIST + 2.0f * DIST * obj[i].slot;
		float otherX = -DIST + 2.0f * DIST * (1 - obj[i].slot);
		float posX = homeX + (otherX - homeX) * animK + obj[i].moveX;
		float posY = uFlag * arcSign[i] * ARC_HEIGHT * sinf(3.14159265f * animK) + obj[i].moveY;

		glm::mat4 orbitM = tilt * glm::rotate(glm::mat4(1.0f), glm::radians(obj[i].orbit), glm::vec3(0, 1, 0));

		glm::vec4 worldPos = orbitM * glm::vec4(posX, posY, 0.0f, 1.0f);
		float vScale = 1.0f;
		if (vMode) {
			vScale = 1.0f + 0.6f * worldPos.z;
			if (vScale < 0.2f)
				vScale = 0.2f;
		}

		glm::mat4 model = orbitM
			* glm::scale(glm::mat4(1.0f), glm::vec3(obj[i].scale * vScale))
			* glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, 0.0f))
			* glm::rotate(glm::mat4(1.0f), glm::radians(obj[i].selfY), glm::vec3(0, 1, 0))
			* glm::rotate(glm::mat4(1.0f), glm::radians(obj[i].selfX), glm::vec3(1, 0, 0))
			* glm::scale(glm::mat4(1.0f), glm::vec3(obj[i].selfScale))
			* fixMatrix[obj[i].shape];

		int s = obj[i].shape;
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glBindVertexArray(modelVao[s]);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(models[s].face_count * 3));
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

	unsigned int ch = key;
	if (ch >= 'A' && ch <= 'Z' && !(mods & GLFW_MOD_SHIFT))
		ch += 32;

	if (ch == '1') selectMode = 1;
	if (ch == '2') selectMode = 2;
	if (ch == '3') selectMode = 3;

	float dir = 1.0f;
	if (ch >= 'A' && ch <= 'Z')
		dir = -1.0f;

	for (int i = 0; i < 2; i++) {
		if (selectMode != 3 && selectMode != i + 1)
			continue;

		if (ch == 'x' || ch == 'X')
			obj[i].selfX += dir * ROTATE_STEP;
		if (ch == 'y' || ch == 'Y')
			obj[i].selfY += dir * ROTATE_STEP;
		if (ch == 'r' || ch == 'R')
			obj[i].orbit += dir * ROTATE_STEP;

		if ((ch == 'a' || ch == 'A') && obj[i].selfScale + dir * 0.1f > 0.1f)
			obj[i].selfScale += dir * 0.1f;

		if ((ch == 'b' || ch == 'B') && obj[i].scale + dir * 0.1f > 0.1f)
			obj[i].scale += dir * 0.1f;

		if (ch == 'd' || ch == 'D')
			obj[i].moveX -= dir * MOVE_STEP;
		if (ch == 'e' || ch == 'E')
			obj[i].moveY += dir * MOVE_STEP;
	}

	if (press && ch == 't' && animType == 0)
		animType = 1;
	if (press && ch == 'u' && animType == 0)
		animType = 2;

	if (press && ch == 'v')
		vMode = !vMode;

	if (press && ch == 'c') {
		for (int i = 0; i < 2; i++)
			obj[i].shape = rand() % SHAPE_COUNT;
	}

	if (press && ch == 's')
		reset_state();

	if (press && ch == 'q')
		glfwSetWindowShouldClose(window, GLFW_TRUE);
}

// ***************************************************************************************************************************************
//--- 메인 함수
int main(int argc, char** argv)
{
	width = 800;
	height = 800;

	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	//--- 윈도우 생성
	GLFWwindow* window = glfwCreateWindow(width, height, "Example17", nullptr, nullptr);
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

	read_obj_file("cube.obj", &models[0]);
	read_obj_file("corn.obj", &models[1]);
	read_obj_file("sphere.obj", &models[2]);

	for (int s = 0; s < SHAPE_COUNT; s++) {
		int* colorIndex = (int*)malloc(models[s].face_count * sizeof(int));
		for (size_t i = 0; i < models[s].face_count; i++) {
			if (s == 0) {
				colorIndex[i] = (int)(i / 2);
			}
			else if (s == 1) {
				colorIndex[i] = 4;
				if (i < 280)
					colorIndex[i] = (int)((i % 40) / 2) % 8;
				else if (i < 300)
					colorIndex[i] = (int)(i - 280) % 8;
			}
			else {
				colorIndex[i] = (int)i;
			}
		}
		make_model_buffers(&models[s], colorIndex, &modelVao[s], modelVbo[s]);
		free(colorIndex);
	}

	fixMatrix[0] = glm::scale(glm::mat4(1.0f), glm::vec3(0.6f))
		* glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
	fixMatrix[1] = glm::scale(glm::mat4(1.0f), glm::vec3(0.35f))
		* glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
	fixMatrix[2] = glm::scale(glm::mat4(1.0f), glm::vec3(0.4f));

	glGenVertexArrays(1, &axisVao);
	glBindVertexArray(axisVao);
	glGenBuffers(2, axisVbo);

	glBindBuffer(GL_ARRAY_BUFFER, axisVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axisVertices), axisVertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, axisVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(axisColors), axisColors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

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
