#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

Model pyramidModel;
GLuint pyrVao, pyrVbo[2];

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

GLfloat pyramidColors[5][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}
};


// 객체 상태
const float ROTATE_SPEED = 90.0f;
const float MAX_OPEN = 180.0f;   // 180 - atan(1.5 / 1): 옆면이 바닥과 평평해지는 각도

float angleX, angleY, angleZ;
int animX, animY, animZ;

bool all_Open;	 float all_Angle;

const float ONE_MAX = 90.0f;   // 7번: 한 면이 열리는 최대 각도
bool one_Open;	 float one_Angle[4];

const float SWAP_MAX = 180.0f;   // 8번: 마주보는 두 면이 꼭짓점(y축) 기준으로 도는 각도
bool swap_Open;	 float swap_Angle;
int swapPair;   // 0: 앞·뒤, 1: 오른쪽·왼쪽

const float APEX_MAX = 90.0f;    // 9번: 꼭짓점 기준으로 옆면이 바깥으로 열리는 각도
bool apex_Open;	 float apex_Angle;

bool hidden = true;


void reset_state() {
	angleX = 0.0f;
	angleY = 0.0f;
	angleZ = 0.0f;
	animX = 0;
	animY = 0;
	animZ = 0;
	all_Open = false;
	all_Angle = 0.0f;
	one_Open = false;
	for (int i = 0; i < 4; i++)
		one_Angle[i] = 0.0f;
	swap_Open = false;
	swapPair = 0;
	swap_Angle = 0.0f;
	apex_Open = false;
	apex_Angle = 0.0f;
}

// 모델의 삼각형마다 정점을 복사하고, 삼각형별 색상(colorIndex)을 줘서 VAO, VBO(위치, 색상)에 올림
void make_model_buffers(Model* model, int* colorIndex, GLfloat colors[][3], GLuint* vao, GLuint* vbo) {
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
			col[k * 3 + 0] = colors[c][0];
			col[k * 3 + 1] = colors[c][1];
			col[k * 3 + 2] = colors[c][2];
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
	angleY += animY * ROTATE_SPEED * dt;
	float target = 0.0f;
	if (all_Open)
		target = MAX_OPEN;
	all_Angle = move_toward(all_Angle, target, 180.0f * dt);

	// 7번: 앞 -> 오른쪽 -> 뒤 -> 왼쪽 순서로 한 면씩 열고, 닫을 때는 반대 순서로 닫음
	for (int f = 0; f < 4; f++) {
		float faceTarget = one_Angle[f];
		if (one_Open) {
			if (f == 0 || one_Angle[f - 1] >= ONE_MAX)
				faceTarget = ONE_MAX;
		}
		else {
			if (f == 3 || one_Angle[f + 1] <= 0.0f)
				faceTarget = 0.0f;
		}
		one_Angle[f] = move_toward(one_Angle[f], faceTarget, 90.0f * dt);
	}

	// 8번: 마주보는 두 면이 자리를 바꿈 / 9번: 꼭짓점 기준으로 바깥쪽으로 열림
	float swapTarget = 0.0f;
	if (swap_Open)
		swapTarget = SWAP_MAX;
	swap_Angle = move_toward(swap_Angle, swapTarget, 90.0f * dt);

	float apexTarget = 0.0f;
	if (apex_Open)
		apexTarget = APEX_MAX;
	apex_Angle = move_toward(apex_Angle, apexTarget, 90.0f * dt);
}

// 옆면 f(0 앞, 1 오른쪽, 2 뒤, 3 왼쪽)를 바닥 모서리를 축으로 바깥쪽으로 눕히는 행렬
glm::mat4 face_animation_matrix(int f) {
	// 빨강(앞 왼), 초록(앞 오른), 뒤 오른쪽, 뒤 왼쪽
	glm::vec3 hinge[4] = { glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, -1), glm::vec3(-1, 0, 0) };
	glm::vec3 axis[4] = { glm::vec3(1, 0, 0), glm::vec3(0, 0, 1), glm::vec3(1, 0, 0), glm::vec3(0, 0, 1) };
	float sign[4] = { 1.0f, -1.0f, -1.0f, 1.0f };

	float angle = all_Angle;
	if (one_Angle[f] > angle)
		angle = one_Angle[f];

	// 8번: swapPair로 고른 마주보는 두 면만 y축(꼭짓점을 지나는 축)으로 회전
	float swapMask[2][4] = {
		{ 1.0f, 0.0f, 1.0f, 0.0f },   // 앞(0)과 뒤(2)
		{ 0.0f, 1.0f, 0.0f, 1.0f }    // 오른쪽(1)과 왼쪽(3)
	};
	glm::mat4 swapM = glm::rotate(glm::mat4(1.0f), glm::radians(swap_Angle * swapMask[swapPair][f]), glm::vec3(0, 1, 0));

	// 9번: 꼭짓점(0, 1.5, 0)을 지나는 축으로 바깥쪽 회전
	glm::vec3 apex = glm::vec3(0.0f, 1.5f, 0.0f);
	float apexSign[4] = { -1.0f, 1.0f, 1.0f, -1.0f };
	glm::mat4 apexM = glm::translate(glm::mat4(1.0f), apex)
		* glm::rotate(glm::mat4(1.0f), glm::radians(apexSign[f] * apex_Angle), axis[f])
		* glm::translate(glm::mat4(1.0f), -apex);

	glm::mat4 openM = glm::translate(glm::mat4(1.0f), hinge[f])
		* glm::rotate(glm::mat4(1.0f), glm::radians(sign[f] * angle), axis[f])
		* glm::translate(glm::mat4(1.0f), -hinge[f]);

	return swapM * apexM * openM;
}


//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// h: 은면제거
	if (hidden)
		glEnable(GL_DEPTH_TEST);
	else
		glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	glUseProgram(shaderProgramID);

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 100.0f);

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));

	//--- 좌표축: 이동, 회전하지 않고 고정. 항상 같은 각도로 비스듬히 기울여서 y 위, x 오른쪽 아래, z 왼쪽 아래로 보이게 함
	glm::mat4 axisRx = glm::rotate(glm::mat4(1.0f), glm::radians(35.0f), glm::vec3(1, 0, 0));
	glm::mat4 axisRy = glm::rotate(glm::mat4(1.0f), glm::radians(-45.0f), glm::vec3(0, 1, 0));
	glm::mat4 axisModel = axisRx * axisRy;
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(axisModel));
	glBindVertexArray(axisVao);
	glDrawArrays(GL_LINES, 0, 6);


	//--- 모델링 변환: x축으로 기울이고 -> y축 회전(자전). 오른쪽 행렬부터 적용됨
	glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(angleX), glm::vec3(1, 0, 0));
	glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(angleY), glm::vec3(0, 1, 0));
	glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(0.0f), glm::vec3(0, 0, 1));

	// 사각뿔: 높이 1.5의 절반만큼 내려 중심을 맞추고, 크기를 줄인 뒤 회전
	glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.75f, 0.0f));
	glBindVertexArray(pyrVao);

	// 바닥(삼각형 2개)은 고정
	glm::mat4 model = axisModel * Rz * Ry * Rx * center;   // 축과 같은 기울기를 먼저 곱함
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
	glDrawArrays(GL_TRIANGLES, 12, 6);

	// 옆면 4개는 각자 바닥 모서리를 축으로 회전
	for (int f = 0; f < 4; f++) {
		model = axisModel * Rz * Ry * Rx * center * face_animation_matrix(f);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glDrawArrays(GL_TRIANGLES, f * 3, 3);
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
	// 사각뿔: 옆면 4개(삼각형 1개씩)는 각자 다른 색, 바닥(삼각형 2개)은 같은 색
	int pyrColorIndex[6];
	for (int i = 0; i < 6; i++) {
		pyrColorIndex[i] = 4;
		if (i < 4)
			pyrColorIndex[i] = i;
	}
	make_model_buffers(&pyramidModel, pyrColorIndex, pyramidColors, &pyrVao, pyrVbo);

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
	if (action != GLFW_PRESS)
		return;

	switch (key) {
		// h: 은면 제거 설정/해제
	case GLFW_KEY_H:
		hidden = !hidden;
		break;

		// y: y축자전
	case GLFW_KEY_Y:
		if (animY == 0)
			animY = 1;
		else
			animY = 0;
		break;

		// 모든 면 열고/닫기
	case GLFW_KEY_6:
		all_Open = !all_Open;
		break;

		// 각 면이 한 개씩 열린다/닫는다
	case GLFW_KEY_7:
		one_Open = !one_Open;
		break;
		// 마주보는 두 옆면 꼭짓점 기준 회전
	case GLFW_KEY_8:
		swapPair = rand() % 2;
		swap_Open = !swap_Open;
		break;
		// 옆면들이 꼭짓점 기준 바깥쪽으로 열린다/닫는다
	case GLFW_KEY_9:
		apex_Open = !apex_Open;
		break;

		// r: 리셋
	case GLFW_KEY_R:
		reset_state();
		break;

		// q: 프로그램 종료
	case GLFW_KEY_Q:
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;
	}
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
	GLFWwindow* window = glfwCreateWindow(width, height, "Example15", nullptr, nullptr);


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

	// 사각뿔 오브젝트 읽기
	read_obj_file("pyramid.obj", &pyramidModel);
	srand((unsigned int)time(NULL));
	reset_state();

	//--- VBO, VAO 생성하고 설정하기
	InitBuffer();

	glfwSetKeyCallback(window, key_callback);

	//**************************렌더링 루프******************************
	double lastTime = glfwGetTime();
	while (!glfwWindowShouldClose(window)) {
		double nowTime = glfwGetTime();
		update((float)(nowTime - lastTime));   // 애니메이션 갱신
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
