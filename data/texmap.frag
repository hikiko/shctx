#version 300 es

layout(location = 0) out mediump vec4 fcolor;
in mediump vec2 uvc;
uniform sampler2D tex;

void main()
{
	fcolor = texture(tex, uvc);
}
