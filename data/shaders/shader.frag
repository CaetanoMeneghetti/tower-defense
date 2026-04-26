//1 exec por pixel da face do objeto
#version 330 core
out vec4 FragColor;
in vec3 objNormal;
void main()
{
    vec3 color = objNormal * 0.5 + 0.5;
    FragColor = vec4(color, 1.0);

}
