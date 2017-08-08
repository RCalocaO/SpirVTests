// Compiled by HLSLCC 0.66
// @Inputs: f2;0:in_TEXCOORD0,f4;1:in_TEXCOORD1,f4;-1:gl_FragCoord
// @Outputs: f4;0:out_Target0,f4;1:out_Target1
// @PackedGlobals: TextureComponentReplicate(h:0,4),TextureComponentReplicateAlpha(h:4,4),bEnableEditorPrimitiveDepthTest(u:0,1)
// @Samplers: InTexture(0:1[InTextureSampler]),SceneDepthTextureNonMS(1:1)
#version 430 
#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable
layout(set=3, binding=2, std140) uniform HLSLCC_CBh
{
	vec4 pu_h[2];
};

layout(set=3, binding=3, std140) uniform HLSLCC_CBu
{
	uvec4 pu_u[1];
};

layout(set=3, binding=0) uniform sampler2D InTexture;
layout(set=3, binding=1) uniform sampler2D SceneDepthTextureNonMS;
layout(location=0) in vec2 in_TEXCOORD0;
layout(location=1) in vec4 in_TEXCOORD1;
layout(location=0) out vec4 out_Target0;
layout(location=1) out vec4 out_Target1;
void main()
{
	if (bool(pu_u[0].x))
	{
		ivec3 v0;
		v0.z = 0;
		v0.xy = ivec2(gl_FragCoord.xy);
		if (((gl_FragCoord.z+(-texelFetch(SceneDepthTextureNonMS,v0.xy,0).x))<0.000000e+00)) discard;
	}
	vec4 v1;
	v1.xyzw = texture(InTexture,in_TEXCOORD0);
	vec4 v2;
	v2.xyzw = v1;
	if (any(notEqual(pu_h[0],vec4(0.000000e+00,0.000000e+00,0.000000e+00,0.000000e+00))))
	{
		v2.xyzw = vec4(dot(v1,pu_h[0]));
	}
	v2.w = dot(v2,pu_h[1]);
	out_Target0.xyzw = (v2*in_TEXCOORD1);
	out_Target1.xyzw = vec4(0.000000e+00,0.000000e+00,0.000000e+00,0.000000e+00);
}

