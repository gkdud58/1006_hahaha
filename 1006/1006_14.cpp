#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
void InitBuffer();
int make_shaderProgram();
GLvoid drawScene();

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

Model cubeModel;
Model pyramidModel;
GLuint cubeVao, cubeVbo[2], cubeEbo;
GLuint pyrVao, pyrVbo[2], pyrEbo;

// 각 꼭지점에 다른 색상 (cube.obj 정점 8개, pyramid.obj 정점 5개)
GLfloat cubeColors[8][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f},
	{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.0f}, {0.5f, 0.0f, 1.0f}
};
GLfloat pyramidColors[5][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}
};

// xyz축 (고정)
GLfloat axisVertices[] = {
	-1.5,0,0,  1.5,0,0,
	0,-1.5,0,  0,1.5,0,
	0,0,-1.5,  0,0,1.5
};
GLfloat axisColors[] = {
	1,0,0, 1,0,0,
	0,1,0, 0,1,0,
	0,0,1, 0,0,1
};
GLuint axisVao, axisVbo[2];

// 객체 상태
const float init_X = 0.0f;   // 처음 회전 각도 (변경 가능)
const float init_Y = 0.0f;
const float ROTATE_SPEED = 90.0f;   // 회전 애니메이션 속도 (초당 도)
const float move = 0.1f;       // 방향키 한 번에 이동하는 거리

float angleX = init_X;
float angleY = init_Y;
float posX = 0.0f;
float posY = 0.0f;
float posZ = 0.0f;
int animX = 0;   // x축 자전: 0 정지, 1 양의 방향, -1 음의 방향
int animY = 0;   // y축 자전

int shape = 0;               // 0: 육면체, 1: 사각뿔
bool hidden = true;   // 은면제거 적용 여부
bool wireMode = false;       // true: 와이어 객체, false: 솔리드 객체

// 모델의 정점/면을 VAO, VBO(위치, 색상), EBO(면 인덱스)에 올림
void make_model_buffers(Model* model, GLfloat colors[][3], GLuint* vao, GLuint* vbo, GLuint* ebo) {
	glGenVertexArrays(1, vao);
	glBindVertexArray(*vao);
	glGenBuffers(2, vbo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, model->vertex_count * sizeof(Vertex), model->vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, model->vertex_count * 3 * sizeof(GLfloat), colors, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	// 면 인덱스는 EBO에
	glGenBuffers(1, ebo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, *ebo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, model->face_count * sizeof(Face), model->faces, GL_STATIC_DRAW);
}

// 객체의 시간에 따른 회전 갱신 (자전)
void update(float dt) {
	angleX += animX * ROTATE_SPEED * dt;
	angleY += animY * ROTATE_SPEED * dt;
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// h: 은면제거 적용/해제
	if (hidden) {
		glEnable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
	}
	else {
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
	}

	glUseProgram(shaderProgramID);

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 100.0f);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));

	glm::mat4 axisRx = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(1, 0, 0));
	glm::mat4 axisRy = glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(0, 1, 0));
	glm::mat4 axisModel = axisRx * axisRy; 
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(axisModel));
	glBindVertexArray(axisVao);
	glDrawArrays(GL_LINES, 0, 6);

	//--- 모델링 변환: (중심을 원점으로) -> x축 회전 -> y축 회전 -> 이동. 오른쪽 행렬부터 적용됨
	glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(angleX), glm::vec3(1, 0, 0));
	glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), glm::vec3(0, 1, 0));
	glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0, 0, 1));

	glm::mat4 Tx = glm::translate(glm::mat4(1.0f), glm::vec3(posX, posY, posZ));

	// w/W: 와이어 객체 / 솔리드 객체
	if (wireMode)
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	else
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (shape == 0) {
		// 육면체: 좌표가 0~1이라 중심을 원점으로 옮김
		glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
		glm::mat4 model = axisModel * Tx *  Rz * Ry * Rx * center;
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glBindVertexArray(cubeVao);
		glDrawElements(GL_TRIANGLES, (GLsizei)(cubeModel.face_count * 3), GL_UNSIGNED_INT, 0);
	}
	else {
		// 사각뿔: 높이 1.5의 절반만큼 내려 중심을 맞추고, 크기를 줄인 뒤 회전
		glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.75f, 0.0f));
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.6f, 0.6f));
		glm::mat4 model = Tx * Rz * Ry * Rx * scale * center;
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glBindVertexArray(pyrVao);
		glDrawElements(GL_TRIANGLES, (GLsizei)(pyramidModel.face_count * 3), GL_UNSIGNED_INT, 0);
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



void InitBuffer() {
	// 육면체, 사각뿔: 정점마다 다른 색상, 인덱스(EBO)로 면 구성
	make_model_buffers(&cubeModel, cubeColors, &cubeVao, cubeVbo, &cubeEbo);
	make_model_buffers(&pyramidModel, pyramidColors, &pyrVao, pyrVbo, &pyrEbo);

	// 축 VAO/VBO 생성
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
	if (action == GLFW_PRESS || action == GLFW_REPEAT) {
		switch (key) {
		case GLFW_KEY_LEFT:  posX -= move; break;
		case GLFW_KEY_RIGHT: posX += move; break;
		case GLFW_KEY_UP:    posY += move; break;
		case GLFW_KEY_DOWN:  posY -= move; break;
		}
	}

	if (action != GLFW_PRESS)
		return;

	bool shift = false;
	if (mods & GLFW_MOD_SHIFT)
		shift = true;

	switch (key) {
	// c: 육면체, p: 사각뿔
	case GLFW_KEY_C: shape = 0; break;
	case GLFW_KEY_P: shape = 1; break;

	// h: 은면제거
	case GLFW_KEY_H: hidden = !hidden; 
		break;

	// w: 와이어
	case GLFW_KEY_W:
		if (shift)
			wireMode = false;
		else
			wireMode = true;
		break;

	// x: x축자전
	case GLFW_KEY_X:
		if (shift) {
			if (animX == -1)
				animX = 0;
			else
				animX = -1;
		}
		else {
			if (animX == 1)
				animX = 0;
			else
				animX = 1;
		}
		break;

	// y: y축자전
	case GLFW_KEY_Y:
		if (shift) {
			if (animY == -1)
				animY = 0;
			else
				animY = -1;
		}
		else {
			if (animY == 1)
				animY = 0;
			else
				animY = 1;
		}
		break;

	// s: 초기 위치로 리셋, 모든 애니메이션 멈춤
	case GLFW_KEY_S:
		angleX = init_X;
		angleY = init_Y;
		posX = 0.0f;
		posY = 0.0f;
		animX = 0;
		animY = 0;
		break;

	case GLFW_KEY_ESCAPE:
		glfwSetWindowShouldClose(window, GLFW_TRUE);   // 종료
		break;
	}
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
	GLFWwindow* window = glfwCreateWindow(width, height, "Example14", nullptr, nullptr);


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

	// 육면체, 사각뿔 오브젝트 읽기
	read_obj_file("cube.obj", &cubeModel);
	read_obj_file("pyramid.obj", &pyramidModel);

	//--- VBO, VAO 생성하고 설정하기
	InitBuffer();

	glfwSetKeyCallback(window, key_callback);

	//**************************렌더링 루프******************************
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		double nowTime = glfwGetTime();
		update((float)(nowTime - lastTime));   // 회전 애니메이션 갱신
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
