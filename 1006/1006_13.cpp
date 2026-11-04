#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>   // $$$ srand(time) 용
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
GLvoid Reshape(int w, int h);

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
			int result = sscanf_s(line + 2, "%f %f %f", &model->vertices[vertex_index].x, &model->vertices[vertex_index].y, &model->vertices[vertex_index].z);

			vertex_index++;
		}
		else if (line[0] == 'f' && line[1] == ' ') {
			unsigned int v1 = 1, v2 = 1, v3 = 1, n1, n2, n3;
			int result = sscanf_s(line + 2, "%u//%u %u//%u %u//%u", &v1, &n1, &v2, &n2, &v3, &n3);

			if (result != 6)   
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
GLuint vao, vbo[2], ebo;


// 단면
GLfloat faceColor[6][3] = {
	{1,0,0}, {1,1,0}, {0,1,0}, {0,1,1}, {0,0,1}, {1,0,1}
};


// xyz축
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


float angleX = 0.0f;
float angleY = 0.0f;
float angleZ = 0.0f;

int oneFace = -1;   // $$$ -1: 육면체 전체, 0~5: 해당 면 하나만 그림 (키 1~6)
int corn_Face = -1; // 원 뿔
int ran_twoFace = -1; // 랜덤한 두 면 보여줌
int ran_twoFace2 = -1; // $$$ 랜덤 두 면 중 두 번째 면 (첫 번째는 ran_twoFace)

// $$$ 마지막으로 누른 키에 따라 무엇을 그릴지: 0 육면체 전체, 1 육면체 한 면(1~6), 2 사각뿔 한 면(7~0), 3 육면체 랜덤 두 면(c), 4 사각뿔 랜덤 두 면(c), 5 사각뿔 바닥 + 랜덤 옆면 1개(t)
int drawMode = 0;

Model pyramidModel;                 // $$$ pyramid.obj (사각뿔)
GLuint pyrVao, pyrVbo[2];

bool dragging = false;
bool draggingZ = false;
double lastX = 0.0, lastY = 0.0;
float lastang = 0.0f;



float mouse_angle_from_center(GLFWwindow* window, double xpos, double ypos) {
	int w, h;
	glfwGetWindowSize(window, &w, &h);
	float dx = (float)(xpos - w / 2.0);
	float dy = (float)(h / 2.0 - ypos);
	return atan2(dy, dx);
}



void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) {
			dragging = true;
			glfwGetCursorPos(window, &lastX, &lastY);   // 큐브가 튀지 않게
		}
		else if (action == GLFW_RELEASE) {
			dragging = false;
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) {
			draggingZ = true;
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			lastang = mouse_angle_from_center(window, x, y);
		}
		else if (action == GLFW_RELEASE) {
			draggingZ = false;
		}
	}
}


void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	if (dragging) {
		angleY += (float)(xpos - lastX);
		angleX += (float)(ypos - lastY);
		lastX = xpos;
		lastY = ypos;
	}

	if (draggingZ) {
		const float pie = glm::pi<float>();
		float angle = mouse_angle_from_center(window, xpos, ypos);
		float delta = angle - lastang;
		if (delta > pie)
			delta -= 2.0f * pie;
		if (delta < -pie)
			delta += 2.0f * pie;
		angleZ += glm::degrees(delta);
		lastang = angle;
	}
}

// $$$ 사각뿔의 f번째 면을 그림: 0~3은 옆면(삼각형 1개 = 정점 3개), 4는 바닥(삼각형 2개 = 정점 6개)
void drawPyramidFace(int f) {
	if (f < 4)
		glDrawArrays(GL_TRIANGLES, f * 3, 3);
	else
		glDrawArrays(GL_TRIANGLES, 12, 6);
}

//--- 그리기 콜백 함수
GLvoid drawScene() {
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // 은면
	glEnable(GL_DEPTH_TEST); // 은면

	glUseProgram(shaderProgramID);

	//--- 모델링 변환: (큐브 중심을 원점으로) -> 회전 -> 이동. 오른쪽 행렬부터 적용됨
	glm::mat4 center = glm::translate(glm::mat4(1.0f), glm::vec3(-0.5f, -0.5f, -0.5f));

	glm::mat4 Rx = glm::rotate(glm::mat4(1.0f), glm::radians(45.0f), glm::vec3(1, 0, 0));
	glm::mat4 Ry = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 1, 0));
	glm::mat4 Rz = glm::rotate(glm::mat4(1.0f), glm::radians(30.0f), glm::vec3(0, 0, 1));

	glm::mat4 Tx = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
	glm::mat4 model = Tx * Rz * Ry * Rx * center;

	//--- 뷰잉 변환, 투영 변환
	glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::mat4 proj = glm::perspective(glm::radians(90.0f), (float)width / (float)height, 0.1f, 100.0f);
	glm::mat4 axis = Tx * Rz * Ry * Rx;

	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "viewTransform"), 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "projectionTransform"), 1, GL_FALSE, glm::value_ptr(proj));


	glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(axis));
	glBindVertexArray(axisVao);
	glDrawArrays(GL_LINES, 0, 6);

	if (drawMode == 2 || drawMode == 4 || drawMode == 5) {
		glm::mat4 pyrScale = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));
		glm::mat4 pyrModel = Tx * Rz * Ry * Rx * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.75f, 0.0f));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(pyrModel));
		glBindVertexArray(pyrVao);

		if (drawMode == 2) {
			drawPyramidFace(corn_Face);
		}
		else if (drawMode == 4) {   // 사각뿔의 랜덤 두 면
			drawPyramidFace(ran_twoFace);
			drawPyramidFace(ran_twoFace2);
		}
		else {   // 사각뿔의 바닥 + 랜덤
			drawPyramidFace(4);
			drawPyramidFace(ran_twoFace);
		}
	}
	else {
		glUniformMatrix4fv(glGetUniformLocation(shaderProgramID, "modelTransform"), 1, GL_FALSE, glm::value_ptr(model));
		glBindVertexArray(vao);

		if (drawMode == 1) {
			glDrawArrays(GL_TRIANGLES, oneFace * 6, 6);
		}
		else if (drawMode == 3) {
			glDrawArrays(GL_TRIANGLES, ran_twoFace * 6, 6);
			glDrawArrays(GL_TRIANGLES, ran_twoFace2 * 6, 6);
		}
		else {
			glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(cubeModel.face_count * 3));
		}
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
	// 정점마다 색상 생성 (정점 좌표 0~1을 그대로 색으로 사용)

	size_t n = cubeModel.face_count * 3;
	GLfloat* pos = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));
	GLfloat* colors = (GLfloat*)malloc(n * 3 * sizeof(GLfloat));

	for (size_t i = 0; i < cubeModel.face_count; ++i) {
		unsigned int idx[3] = { cubeModel.faces[i].v1, cubeModel.faces[i].v2, cubeModel.faces[i].v3 };

		float r = faceColor[i / 2][0], g = faceColor[i / 2][1], b = faceColor[i / 2][2];

		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;
			pos[k * 3 + 0] = cubeModel.vertices[idx[j]].x;
			pos[k * 3 + 1] = cubeModel.vertices[idx[j]].y;
			pos[k * 3 + 2] = cubeModel.vertices[idx[j]].z;

			colors[k * 3 + 0] = r;  
			colors[k * 3 + 1] = g;  
			colors[k * 3 + 2] = b;
		}
	}

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(2, vbo);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), pos, GL_STATIC_DRAW);  
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, n * 3 * sizeof(GLfloat), colors, GL_STATIC_DRAW); 
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	

	free(pos);
	free(colors);

	size_t pn = pyramidModel.face_count * 3;
	GLfloat* ppos = (GLfloat*)malloc(pn * 3 * sizeof(GLfloat));
	GLfloat* pcol = (GLfloat*)malloc(pn * 3 * sizeof(GLfloat));
	for (size_t i = 0; i < pyramidModel.face_count; ++i) {
		unsigned int idx[3] = { pyramidModel.faces[i].v1, pyramidModel.faces[i].v2, pyramidModel.faces[i].v3 };

		int c = 4;
		if (i < 4)
			c = (int)i;
		for (int j = 0; j < 3; j++) {
			size_t k = i * 3 + j;

			ppos[k * 3 + 0] = pyramidModel.vertices[idx[j]].x;
			ppos[k * 3 + 1] = pyramidModel.vertices[idx[j]].y;
			ppos[k * 3 + 2] = pyramidModel.vertices[idx[j]].z;

			pcol[k * 3 + 0] = faceColor[c][0];
			pcol[k * 3 + 1] = faceColor[c][1];
			pcol[k * 3 + 2] = faceColor[c][2];
		}
	}
	glGenVertexArrays(1, &pyrVao);
	glBindVertexArray(pyrVao);
	glGenBuffers(2, pyrVbo);

	glBindBuffer(GL_ARRAY_BUFFER, pyrVbo[0]);
	glBufferData(GL_ARRAY_BUFFER, pn * 3 * sizeof(GLfloat), ppos, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, pyrVbo[1]);
	glBufferData(GL_ARRAY_BUFFER, pn * 3 * sizeof(GLfloat), pcol, GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(1);

	free(ppos);
	free(pcol);


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
	case GLFW_KEY_1: 
		oneFace = 0; 
		drawMode = 1; 
		break;
	case GLFW_KEY_2: 
		oneFace = 1; 
		drawMode = 1; 
		break;
	case GLFW_KEY_3: 
		oneFace = 2; 
		drawMode = 1; 
		break;
	case GLFW_KEY_4: 
		oneFace = 3; 
		drawMode = 1; 
		break;
	case GLFW_KEY_5: 
		oneFace = 4; 
		drawMode = 1; 
		break;
	case GLFW_KEY_6: 
		oneFace = 5; 
		drawMode = 1; 
		break;

	// 7/8/9/0
	case GLFW_KEY_7: 
		corn_Face = 0; 
		drawMode = 2; 
		break;
	case GLFW_KEY_8: 
		corn_Face = 1; 
		drawMode = 2; 
		break;
	case GLFW_KEY_9: 
		corn_Face = 2; 
		drawMode = 2; 
		break;
	case GLFW_KEY_0: 
		corn_Face = 3; 
		drawMode = 2; 
		break;

	case GLFW_KEY_C:
	{
		bool pyramidShown = false;
		if (drawMode == 2 || drawMode == 4 || drawMode == 5)
			pyramidShown = true;

		if (pyramidShown) {
			ran_twoFace = rand() % 5;
			do {
				ran_twoFace2 = rand() % 5;
			} while (ran_twoFace2 == ran_twoFace);
			drawMode = 4;
		}
		else {
			ran_twoFace = rand() % 6;
			do {
				ran_twoFace2 = rand() % 6;
			} while (ran_twoFace2 == ran_twoFace);
			drawMode = 3;
		}
		break;
	}

	case GLFW_KEY_T:
		ran_twoFace = rand() % 4;   // 옆면 0~3 중 하나
		drawMode = 5;
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
	width = 1000;
	height = 1000;

	//--- GLFW 초기화
	if (!glfwInit())
		return -1;

	//--- OpenGL 버전 및 프로파일 설정
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


	//--- 윈도우 생성
	GLFWwindow* window = glfwCreateWindow(width, height, "Example1", nullptr, nullptr);


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

	// 큐브 오브제 호출
	read_obj_file("cube.obj", &cubeModel);
	read_obj_file("pyramid.obj", &pyramidModel);   // $$$
	srand((unsigned int)time(NULL));               // $$$ 랜덤 두 면용

	//--- VBO, VAO 생성하고 설정하기
	InitBuffer();


	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);



	glfwSetKeyCallback(window, key_callback);

	//**************************렌더링 루프******************************
	while (!glfwWindowShouldClose(window)) {
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
