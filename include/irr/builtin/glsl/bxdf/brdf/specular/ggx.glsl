#ifndef _IRR_BSDF_BRDF_SPECULAR_GGX_INCLUDED_
#define _IRR_BSDF_BRDF_SPECULAR_GGX_INCLUDED_

#include <irr/builtin/glsl/bxdf/common.glsl>
#include <irr/builtin/glsl/bxdf/brdf/specular/ndf/ggx.glsl>
#include <irr/builtin/glsl/bxdf/brdf/specular/geom/smith.glsl>
#include <irr/builtin/glsl/bxdf/brdf/specular/fresnel/fresnel.glsl>

vec3 irr_glsl_ggx_height_correlated_aniso_cos_eval(in irr_glsl_BSDFAnisotropicParams params, in mat2x3 ior2, in float a2, in vec2 atb, in float aniso)
{
    float g = irr_glsl_ggx_smith_height_correlated_aniso_wo_numerator(atb.x, atb.y, params.TdotL, params.TdotV, params.BdotL, params.BdotV, params.isotropic.NdotL, params.isotropic.interaction.NdotV);
    float ndf = irr_glsl_ggx_burley_aniso(aniso, a2, params.TdotH, params.BdotH, params.isotropic.NdotH);
    vec3 fr = irr_glsl_fresnel_conductor(ior2[0], ior2[1], params.isotropic.VdotH);

    return params.isotropic.NdotL * g*ndf*fr;
}
vec3 irr_glsl_ggx_height_correlated_cos_eval(in irr_glsl_BSDFIsotropicParams params, in mat2x3 ior2, in float a2)
{
    float g = irr_glsl_ggx_smith_height_correlated_wo_numerator(a2, params.NdotL, params.interaction.NdotV);
    float ndf = irr_glsl_ggx_trowbridge_reitz(a2, params.NdotH*params.NdotH);
    vec3 fr = irr_glsl_fresnel_conductor(ior2[0], ior2[1], params.VdotH);

    return params.NdotL * g*ndf*fr;
}

//Heitz's 2018 paper "Sampling the GGX Distribution of Visible Normals"
//Also: problem is our anisotropic ggx ndf (above) has extremely weird API (anisotropy and a2 instead of ax and ay) and so it's incosistent with sampling function
//  currently using isotropic trowbridge_reitz for PDF
irr_glsl_BSDFSample irr_glsl_ggx_cos_generate(in irr_glsl_AnisotropicViewSurfaceInteraction interaction, in vec2 _sample, in float _ax, in float _ay)
{
    vec2 u = _sample;

    mat3 m = irr_glsl_getTangentFrame(interaction);

    vec3 V = interaction.isotropic.V.dir;
    V = normalize(V*m);//transform to tangent space
    V = normalize(vec3(_ax*V.x, _ay*V.y, V.z));//stretch view vector so that we're sampling as if roughness=1.0

    float lensq = V.x*V.x + V.y*V.y;
    vec3 T1 = lensq > 0.0 ? vec3(-V.y, V.x, 0.0)*inversesqrt(lensq) : vec3(1.0,0.0,0.0);
    vec3 T2 = cross(V,T1);

    float r = sqrt(u.x);
    float phi = 2.0 * irr_glsl_PI * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + V.z);
    t2 = (1.0 - s)*sqrt(1.0 - t1*t1) + s*t2;
    
    //reprojection onto hemisphere
    vec3 H = t1*T1 + t2*T2 + sqrt(max(0.0, 1.0-t1*t1-t2*t2))*V;
    //unstretch
    H = normalize(vec3(_ax*H.x, _ay*H.y, max(0.0,H.z)));
    float NdotH = H.z;

    irr_glsl_BSDFSample smpl;
    //==== compute L ====
    H = normalize(m*H);//transform to correct space
    float HdotV = dot(H,interaction.isotropic.V.dir);
    //reflect V on H to actually get L
    smpl.L = H*2.0*HdotV - interaction.isotropic.V.dir;

    return smpl;
}
irr_glsl_BSDFSample irr_glsl_ggx_cos_generate(in irr_glsl_AnisotropicViewSurfaceInteraction interaction, in uvec2 _sample, in float _ax, in float _ay)
{
    vec2 u = vec2(_sample)/float(UINT_MAX);
    return irr_glsl_ggx_cos_generate(interaction, u, _ax, _ay);
}

vec3 irr_glsl_ggx_cos_remainder_and_pdf(out float pdf, in irr_glsl_BSDFAnisotropicParams params, in irr_glsl_AnisotropicViewSurfaceInteraction interaction, in uvec2 u, in mat2x3 ior2, in float a2)
{
	irr_glsl_BSDFSample s = irr_glsl_ggx_cos_generate(interaction, u, a2, a2);//TODO take sample as parameter
	
	float NdotL2 = s.LdotN*s.LdotN;
	float lambda_V = irr_glsl_smith_ggx_Lambda(irr_glsl_smith_ggx_C2(interaction.isotropic.NdotV_squared, a2));
	float lambda_L = irr_glsl_smith_ggx_Lambda(irr_glsl_smith_ggx_C2(NdotL2, a2));
	float onePlusLambda_V = 1.0 + lambda_V;
	float G = 1.0 / onePlusLambda_V;
	pdf = irr_glsl_ggx_trowbridge_reitz(a2,s.NdotH*s.NdotH)*G*abs(dot(interaction.isotropic.V.dir,H))/interaction.isotropic.NdotV;
	G = onePlusLambda_V/(onePlusLambda_V+lambda_L);//remainder
	
	vec3 fr = irr_glsl_fresnel_conductor(ior2[0], ior2[1], s.VdotH);
	return fr*G;	
}

vec3 irr_glsl_ggx_aniso_cos_remainder_and_pdf(out float pdf, in irr_glsl_BSDFAnisotropicParams params, in irr_glsl_AnisotropicViewSurfaceInteraction interaction, in uvec2 u, in mat2x3 ior2, in float ax, in float ay)
{
	irr_glsl_BSDFSample s = irr_glsl_ggx_cos_generate(interaction, u, a2, a2);//TODO take sample as parameter
	
	float Vterm = s.LdotN * length(vec3(at*params.TdotV, ab*params.BdotV, params.NdotV));
	float Lterm = params.NdotV * length(vec3(at*s.LdotT, ab*s.LdotB, s.LdotN));
	float G1_rcp = params.NdotV*s.LdotN+Lterm;
	float G2_over_G1 = (Vterm+Lterm)*G1_rcp;
	G1_rcp *= 2.0;
	pdf = irr_glsl_ggx_aniso(s.TdotH*s.TdotH,s.BdotH*s.BdotH,s.NdotH*s.NdotH,ax,ay,ax*ax,ay*ay)/(4.0*G1_rcp*abs(NdotV));//u sure about div by 4.0?
	
	vec3 fr = irr_glsl_fresnel_conductor(ior2[0], ior2[1], s.VdotH);
	return fr*G2_over_G1;	
}

vec3 irr_glsl_ggx_cos_remainder_and_pdf(out float pdf, in irr_glsl_BSDFAnisotropicParams params, in irr_glsl_AnisotropicViewSurfaceInteraction interaction, in uvec2 u, in mat2x3 ior2, in float ax, in float ay)
{
	irr_glsl_BSDFSample s = irr_glsl_ggx_cos_generate(interaction, u, ax, ay);
	
	// TODO: redo (split into 2 functions separate for iso and aniso case)
	// aniso
	/*{
		float Vterm = NdotL * length(vec3(at*TdotV, ab*BdotV, NdotV));
		float Lterm = NdotV * length(vec3(at*TdotL, ab*BdotL, NdotL));
		float G1_rcp = NdotV*NdotL+Lterm;
		float G2_over_G1 = (Vterm+Lterm)*G1_rcp;
		G1_rcp *= 2.0;
		pdf = NDF()/(4.0*G1_rcp*abs(NdotV));
	}*/
	{ // TODO: redo for iso
		float a2 = ax*ay;
		float NdotL2 = s.LdotN*s.LdotN;
		float lambda_V = irr_glsl_smith_ggx_Lambda(irr_glsl_smith_ggx_C2(interaction.isotropic.NdotV_squared, a2));
		float lambda_L = irr_glsl_smith_ggx_Lambda(irr_glsl_smith_ggx_C2(NdotL2, a2));
		float onePlusLambda_V = 1.0 + lambda_V;
		float G = 1.0 / onePlusLambda_V;
		pdf = irr_glsl_ggx_trowbridge_reitz(a2,s.NdotH*s.NdotH)*G*abs(dot(interaction.isotropic.V.dir,H))/interaction.isotropic.NdotV;
		G = onePlusLambda_V/(onePlusLambda_V+lambda_L);//remainder
	}
	
	vec3 fr = irr_glsl_fresnel_conductor(ior2[0], ior2[1], s.VdotH);
	return fr*G;
}

#endif
