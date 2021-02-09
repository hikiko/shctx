#version 450

layout(location = 0) out vec4 f_color;

void main()
{
	int xor = (int(gl_FragCoord.x) ^ int(gl_FragCoord.y));
	float r = float((xor * 2) & 0xff) / 255.0;
	float g = float((xor * 4) & 0xff) / 255.0;
	float b = float((xor * 8) & 0xff) / 255.0;
	vec3 col = vec3(r, g, b);
	f_color = vec4(col, 1.0);
}
