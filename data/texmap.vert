#version 300 es
layout(location = 0) in vec2 vertex;
out vec2 uvc;
void main()
{
   gl_Position = vec4(vec2(2.0, 2.0) * vertex - vec2(1.0, 1.0), 0.0, 1.0);
   uvc = vertex;
}
