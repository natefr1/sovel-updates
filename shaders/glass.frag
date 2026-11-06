#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 area;
    vec2 panelSize;
    float corner;
    float tintAmount;
    float bendAmount;
} ub;
layout(binding=1) uniform sampler2D source;
void main(){
    vec2 uv=qt_TexCoord0;
    vec2 p=(uv-.5)*ub.panelSize;
    float r=min(ub.corner,min(ub.panelSize.x,ub.panelSize.y)*.5);
    vec2 q=abs(p)-(ub.panelSize*.5-r);
    float sd=length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
    float alpha=1.0-smoothstep(-1.0,0.7,sd);
    float edge=exp(-max(-sd,0.0)/11.0);
    vec2 normal=normalize(p+vec2(.001));
    vec2 at=ub.area.xy+uv*ub.area.zw + normal*edge*ub.bendAmount;
    vec2 dx=vec2(0.003,0),dy=vec2(0,0.004);
    vec3 c=texture(source,at).rgb*.4;
    c+=(texture(source,at+dx).rgb+texture(source,at-dx).rgb+texture(source,at+dy).rgb+texture(source,at-dy).rgb)*.15;
    // Bounded wallpaper lens. Does not capture other Wayland clients.
    c=mix(c,vec3(.035,.022,.065),ub.tintAmount);
    c+=edge*vec3(.11,.075,.16)*max(0.0,dot(normal,normalize(vec2(-.5,-1.0))));
    fragColor=vec4(c*alpha,alpha)*ub.qt_Opacity;
}
