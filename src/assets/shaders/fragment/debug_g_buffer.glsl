#version 450 core

in vec2 TexCoords;

out vec4 FragColor;

uniform sampler2D attachment;


void main() {
  FragColor = texture(attachment, TexCoords);
}
