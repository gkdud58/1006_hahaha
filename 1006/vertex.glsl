#version 330 core
//--- Position: attribute index 0
//--- Color: attribute index 1
layout (location = 0) in vec3 vPosition; //--- 위치 변수: attribute position 0
layout (location = 1) in vec3 vColor; //--- 컬러 변수: attribute position 1

out vec3 out_Color; //--- 프래그먼트 세이더에게 전달

uniform mat4 modelTransform;		//--- 모델링 변환 행렬
uniform mat4 viewTransform;			//--- 뷰잉 변환 행렬
uniform mat4 projectionTransform;	//--- 투영 변환 행렬

void main(void)
{
	gl_Position = projectionTransform * viewTransform * modelTransform * vec4 (vPosition, 1.0);
	out_Color = vColor;
}


