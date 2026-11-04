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

Model cubeModel;
Model pyramidModel;
GLuint cubeVao, cubeVbo[2];
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

// 각 면에 다른 색상
GLfloat cubeColors[6][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f},
	{1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}
};
GLfloat pyramidColors[5][3] = {
	{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 0.0f, 1.0f}
};

const int BACK = 0;     // 뒷면
const int LEFT = 1;     // 왼쪽면
const int TOP = 2;      // 윗면 
const int RIGHT = 3;    // 오른쪽면
const int BOTTOM = 4;   // 아랫면
const int FRONT = 5;    // 앞면 

const int sideFace[4] = { FRONT, RIGHT, BACK, LEFT };
const float cornerX[4] = { 0.5f, 0.5f, -0.5f, -0.5f };
const float cornerZ[4] = { 0.5f, -0.5f, -0.5f, 0.5f };

// 객체 상태
const float init_X = 0.0f;   
const float init_Y = 0.0f;
const float ROTATE_SPEED = 90.0f;   

float angleX;
float angleY;
int animY;       
int shape;           
bool hidden = true;   


bool topSpin;
float topAngle;


bool frontOpen;
float frontAngle;

bool sideSpin;
float sideAngle;

bool backShrink;
float backScale;

bool oneAnim;
int oneEdge, oneFaceA, oneFaceB;
float oneAngle;

bool swapAnim;
int swapFaceA, swapFaceB;
float swapAngle;
int quarter[6];      

bool revolveY;
float revolveYAngle;

bool revolveX;
float revolveXAngle;



void reset_state() {
	angleX = init_X;
	angleY = init_Y;
	animY = 0;
	shape = 0;

	topSpin = false;     topAngle = 0.0f;
	frontOpen = false;   frontAngle = 0.0f;
	sideSpin = false;    sideAngle = 0.0f;
	backShrink = false;  backScale = 1.0f;
	oneAnim = false;     oneAngle = 0.0f;
	swapAnim = false;    swapAngle = 0.0f;
	revolveY = false;    revolveYAngle = 0.0f;
	revolveX = false;    revolveXAngle = 0.0f;

	for (int i = 0; i < 6; i++)
		quarter[i] = 0;
}

int mod4(int v) {
	return ((v % 4) + 4) % 4;
}


int slot_of_face(int f) {
	for (int s = 0; s < 4; s++) {
		if (sideFace[s] == f)
			return mod4(s + quarter[f]);
	}
	return -1;
}


int face_at_slot(int slot) {
	for (int s = 0; s < 4; s++) {
		int f = sideFace[s];
		if (mod4(s + quarter[f]) == slot)
			return f;
	}
	return -1;
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

	if (topSpin)
		topAngle += 90.0f * dt;

	if (frontOpen)
		frontAngle = move_toward(frontAngle, 90.0f, 90.0f * dt);
	else
		frontAngle = move_toward(frontAngle, 0.0f, 90.0f * dt);

	if (sideSpin)
		sideAngle += 120.0f * dt;

	if (backShrink)
		backScale = move_toward(backScale, 0.0f, 0.5f * dt);
	else
		backScale = move_toward(backScale, 1.0f, 0.5f * dt);

	if (oneAnim) {
		oneAngle += 180.0f * dt;
		if (oneAngle >= 360.0f) {
			oneAngle = 0.0f;
			oneAnim = false;
		}
	}

	if (swapAnim) {
		swapAngle += 90.0f * dt;
		if (swapAngle >= 90.0f) {  
			quarter[swapFaceA] = mod4(quarter[swapFaceA] + 1);
			quarter[swapFaceB] = mod4(quarter[swapFaceB] - 1);
			swapAngle = 0.0f;
			swapAnim = false;
		}
	}

	if (revolveY)
		revolveYAngle += 60.0f * dt;
	if (revolveX)
		revolveXAngle += 60.0f * dt;
}


glm::mat4 face_animation_matrix(int f) {
	glm::mat4 m = glm::mat4(1.0f);


	if (f == TOP) {
		// t
		m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f))
			* glm::rotate(glm::mat4(1.0f), glm::radians(topAngle), glm::vec3(0, 0, 1))
			* glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, 0.0f));
	}
	else if (f == FRONT) {
		// f
		m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.5f))
			* glm::rotate(glm::mat4(1.0f), glm::radians(-frontAngle), glm::vec3(1, 0, 0))
			* glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.5f, -0.5f));
	}
	else if (f == RIGHT) {
		// s
		m = glm::rotate(glm::mat4(1.0f), glm::radians(sideAngle), glm::vec3(1, 0, 0));
	}
	else if (f == BACK) {
		// b

		m = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -0.5f))
			* glm::scale(glm::mat4(1.0f), glm::vec3(backScale, backScale, backScale))
			* glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.5f));
	}

	if (slot_of_face(f) >= 0) {
		m = glm::rotate(glm::mat4(1.0f), glm::radians(quarter[f] * 90.0f), glm::vec3(0, 1, 0)) * m;

		// 2
		if (swapAnim && f == swapFaceA)
			m = glm::rotate(glm::mat4(1.0f), glm::radians(swapAngle), glm::vec3(0, 1, 0)) * m;
		if (swapAnim && f == swapFaceB)
			m = glm::rotate(glm::mat4(1.0f), glm::radians(-swapAngle), glm::vec3(0, 1, 0)) * m;

		// 1
		if (oneAnim && (f == oneFaceA || f == oneFaceB)) {
			m = glm::translate(glm::mat4(1.0f), glm::vec3(cornerX[oneEdge], 0.0f, cornerZ[oneEdge]))
				* glm::rotate(glm::mat4(1.0f), glm::radians(oneAngle), glm::vec3(0, 1, 0))
				* glm::translate(glm::mat4(1.0f), glm::vec3(-cornerX[oneEdge], 0.0f, -cornerZ[oneEdge]))
				* m;
		}

		// 4
		m = glm::rotate(glm::mat4(1.0f), glm::radians(revolveYAngle), glm::vec3(0, 1, 0)) * m;
	}

	// 5
	bool xRevolveFace = false;
	if (f == TOP || f == BOTTOM)
		xRevolveFace = true;
	if (slot_of_face(f) == 0 || slot_of_face(f) == 2)
		xRevolveFace = true;
	if (xRevolveFace)
		m = glm::rotate(glm::mat4(1.0f), glm::radians(revolveXAngle), glm::vec3(1, 0, 0)) * m;

	return m;
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
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
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


	if (shape == 0) {
		// 육면체: 좌표가 0~1이라 중심을 원점으로 옮김. 면마다 따로 애니메이션 행렬을 곱해서 그림
		glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));
		glBindVertexArray(cubeVao);

		for (int f = 0; f < 6; f++) {
			glm::mat4 model = axisModel * Rz * Ry * Rx * face_animation_matrix(f) * center;   // 축과 같은 기울기를 먼저 곱함
			glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
			glDrawArrays(GL_TRIANGLES, f * 6, 6);   // 한 면 = 삼각형 2개 = 정점 6개
		}
	}
	else {
		// 사각뿔: 높이 1.5의 절반만큼 내려 중심을 맞추고, 크기를 줄인 뒤 회전
		glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.75f, 0.0f));
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(0.6f, 0.6f, 0.6f));
		glm::mat4 model = axisModel * Rz * Ry * Rx * scale * center;
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glBindVertexArray(pyrVao);
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(pyramidModel.face_count * 3));
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
	// 육면체: 삼각형 2개가 한 면이라 면 번호 = 삼각형 번호 / 2
	int cubeColorIndex[12];
	for (int i = 0; i < 12; i++)
		cubeColorIndex[i] = i / 2;
	make_model_buffers(&cubeModel, cubeColorIndex, cubeColors, &cubeVao, cubeVbo);

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

	// p: 육면체/사각뿔
	case GLFW_KEY_P:
		if (shape == 0)
			shape = 1;
		else
			shape = 0;
		break;

	// y: y축자전
	case GLFW_KEY_Y:
		if (animY == 0)
			animY = 1;
		else
			animY = 0;
		break;

	// r: 리셋
	case GLFW_KEY_R:
		reset_state();
		break;

	// q: 프로그램 종료
	case GLFW_KEY_Q:
		glfwSetWindowShouldClose(window, GLFW_TRUE);
		break;

	// t: z축 중심으로 회전/멈추기
	case GLFW_KEY_T:
		topSpin = !topSpin;
		break;

	// f: 앞면 회전하여 열기/닫기
	case GLFW_KEY_F:
		frontOpen = !frontOpen;
		break;

	// s: 옆면이 중점을 기준으로 제자리에서 회전/멈추기
	case GLFW_KEY_S:
		sideSpin = !sideSpin;
		break;

	// b: 뒷면이 작아지면서 없어진다/다시 커지면서 원래 크기가 된다
	case GLFW_KEY_B:
		backShrink = !backShrink;
		break;

	case GLFW_KEY_1:
		if (shape == 0 && !oneAnim && !swapAnim) {
			oneEdge = rand() % 4;                         
			oneFaceA = face_at_slot(oneEdge);             
			oneFaceB = face_at_slot((oneEdge + 1) % 4);
			oneAngle = 0.0f;
			oneAnim = true;
		}
		break;

	case GLFW_KEY_2:
		if (shape == 0 && !oneAnim && !swapAnim) {
			int edge = rand() % 4;
			swapFaceA = face_at_slot(edge); // +90도 공전
			swapFaceB = face_at_slot((edge + 1) % 4); // -90도 공전
			swapAngle = 0.0f;
			swapAnim = true;
		}
		break;

	case GLFW_KEY_4:
		revolveY = !revolveY;
		break;

	case GLFW_KEY_5:
		revolveX = !revolveX;
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

	// 육면체, 사각뿔 오브젝트 읽기
	read_obj_file("cube.obj", &cubeModel);
	read_obj_file("pyramid.obj", &pyramidModel);

	srand((unsigned int)time(NULL));   // 랜덤 선택용
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
