#version 420 core
void main()
{
    // Output depth only
    gl_FragDepth = gl_FragCoord.z;
}