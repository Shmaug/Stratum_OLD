#define PI 3.1415926535897932
#define INV_PI 0.31830988618

#include <include/sampling.hlsli>

struct DisneyMaterial {
    float3 BaseColor;
    float Metallic;
    float3 Emission;
    float Specular;
    float Anisotropy;
    float Roughness;
    float SpecularTint;
    float SheenTint;
    float Sheen;
    float ClearcoatGloss;
    float Clearcoat;
    float Subsurface;
    float Transmission;
    uint pad[3];
};

float SchlickFresnelReflectance(float u) {
    float m = clamp(1 - u, 0, 1);
    float m2 = m * m;
    return m2 * m2 * m;
}

float GTR1(float ndoth, float a) {
    if (a >= 1.f) return 1.f / PI;

    float a2 = a * a;
    float t = 1.f + (a2 - 1.f) * ndoth * ndoth;
    return (a2 - 1.f) / (PI * log(a2) * t);
}
float GTR2(float ndoth, float a) {
    float a2 = a * a;
    float t = 1.f + (a2 - 1.f) * ndoth * ndoth;
    return a2 / (PI * t * t);
}
float GTR2_Aniso(float ndoth, float hdotx, float hdoty, float ax, float ay) {
    float hdotxa2 = (hdotx / ax);
    hdotxa2 *= hdotxa2;
    float hdotya2 = (hdoty / ay);
    hdotya2 *= hdotya2;
    float denom = hdotxa2 + hdotya2 + ndoth * ndoth;
    return denom > 1e-5 ? (1.f / (PI * ax * ay * denom * denom)) : 0.f;
}

float SmithGGX_G(float ndotv, float a) {
    float a2 = a * a;
    float b = ndotv * ndotv;
    return 1.f / (ndotv + sqrt(a2 + b - a2 * b));
}
float SmithGGX_G_Aniso(float ndotv, float vdotx, float vdoty, float ax, float ay) {
    float vdotxax2 = (vdotx * ax) * (vdotx * ax);
    float vdotyay2 = (vdoty * ay) * (vdoty * ay);
    return 1.f / (ndotv + sqrt(vdotxax2 + vdotyay2 + ndotv * ndotv));
}

float Disney_GetPdf(DisneyMaterial mat, float3 wi, float3 wo) {
    float aspect = sqrt(1.f - mat.Anisotropy * 0.9f);

    float ax = max(0.001f, mat.Roughness * mat.Roughness * (1.f + mat.Anisotropy));
    float ay = max(0.001f, mat.Roughness * mat.Roughness * (1.f - mat.Anisotropy));
    float3 wh = normalize(wo + wi);
    float ndotwh = abs(wh.y);
    float hdotwo = abs(dot(wh, wo));

    float d_pdf = abs(wo.y) / PI;
    float r_pdf = GTR2_Aniso(ndotwh, wh.x, wh.z, ax, ay) * ndotwh / (4 * hdotwo);
    float c_pdf = GTR1(ndotwh, lerp(0.1, 0.001, mat.ClearcoatGloss)) * ndotwh / (4 * hdotwo);

    float3 cd_lin = mat.BaseColor;// pow(mat.BaseColor, 2.2);
    // Luminance approximmation
    float cd_lum = dot(cd_lin, float3(0.3, 0.6, 0.1));

    // Normalize lum. to isolate hue+sat
    float3 c_tint = cd_lum > 0 ? (cd_lin / cd_lum) : 1;

    float3 c_spec0 = lerp(mat.Specular * 0 * lerp(1, c_tint, mat.SpecularTint), cd_lin, mat.Metallic);

    float cs_lum = dot(c_spec0, float3(0.3, 0.6, 0.1));

    float cs_w = cs_lum / (cs_lum + (1 - mat.Metallic) * cd_lum);

    return c_pdf * mat.Clearcoat + (1 - mat.Clearcoat) * (cs_w * r_pdf + (1 - cs_w) * d_pdf);
}

float3 Disney_Evaluate(DisneyMaterial mat, float3 wi, float3 wo) {
    float ndotwi = abs(wi.y);
    float ndotwo = abs(wo.y);

    float3 h = normalize(wi + wo);
    float ndoth = abs(h.y);
    float hdotwo = abs(dot(h, wo));

    float3 cd_lin = mat.BaseColor;// pow(mat.BaseColor, 2.2);
    // Luminance approximmation
    float cd_lum = dot(cd_lin, float3(0.3, 0.6, 0.1));

    // Normalize lum. to isolate hue+sat
    float3 c_tint = cd_lum > 0 ? (cd_lin / cd_lum) : 1;

    float3 c_spec0 = lerp(mat.Specular * 0.1 * lerp(1, c_tint, mat.SpecularTint), cd_lin, mat.Metallic);

    float3 c_sheen = lerp(1, c_tint, mat.SheenTint);

    // Diffuse fresnel - go from 1 at normal incidence to 0.5 at grazing
    // and lerp in diffuse retro-reflection based on Roughness
    float f_wo = SchlickFresnelReflectance(ndotwo);
    float f_wi = SchlickFresnelReflectance(ndotwi);

    float fd90 = 0.5 + 2 * hdotwo * hdotwo * mat.Roughness;
    float fd = lerp(1, fd90, f_wo) * lerp(1, fd90, f_wi);

    // Based on Hanrahan-Krueger brdf approximation of isotropic bssrdf
    // 1.25 scale is used to (roughly) preserve albedo
    // fss90 used to "flatten" retroreflection based on Roughness
    float fss90 = hdotwo * hdotwo * mat.Roughness;
    float fss = lerp(1, fss90, f_wo) * lerp(1, fss90, f_wi);
    float ss = 1.25 * (fss * (1 / (ndotwo + ndotwi) - 0.5) + 0.5);

    // Specular
    float ax = max(0.001, mat.Roughness * mat.Roughness * (1 + mat.Anisotropy));
    float ay = max(0.001, mat.Roughness * mat.Roughness * (1 - mat.Anisotropy));
    float ds = GTR2_Aniso(ndoth, h.x, h.z, ax, ay);
    float fh = SchlickFresnelReflectance(hdotwo);
    float3 fs = lerp(c_spec0, 1, fh);

    float gs;
    gs = SmithGGX_G_Aniso(ndotwo, wo.x, wo.z, ax, ay);
    gs *= SmithGGX_G_Aniso(ndotwi, wi.x, wi.z, ax, ay);

    // Sheen
    float3 f_sheen = fh * mat.Sheen * c_sheen;

    // Clearcoat (ior = 1.5 -> F0 = 0.04)
    float dr = GTR1(ndoth, lerp(0.1, 0.001, mat.ClearcoatGloss));
    float fr = lerp(0.04, 1, fh);
    float gr = SmithGGX_G(ndotwo, 0.25) * SmithGGX_G(ndotwi, 0.25);

    return ((1 / PI) * lerp(fd, ss, mat.Subsurface) * cd_lin + f_sheen) * (1 - mat.Metallic) + gs * fs * ds + mat.Clearcoat * gr * fr * dr;
}
float3 Disney_Sample(DisneyMaterial mat, float3 wi, float2 sample, out float3 wo, out float pdf) {
    float ax = max(0.001, mat.Roughness * mat.Roughness * (1 + mat.Anisotropy));
    float ay = max(0.001, mat.Roughness * mat.Roughness * (1 - mat.Anisotropy));

    float mis_weight = 1;
    float3 wh;

    if (sample.x < mat.Clearcoat) {
        sample.x /= (mat.Clearcoat);

        float a = lerp(0.1, 0.001, mat.ClearcoatGloss);
        float ndotwh = sqrt((1 - pow(a * a, 1 - sample.y)) / (1 - a * a));
        float sintheta = sqrt(1 - ndotwh * ndotwh);
        wh = normalize(float3(cos(2 * PI * sample.x) * sintheta, ndotwh, sin(2 * PI * sample.x) * sintheta));
        wo = -wi + 2 * abs(dot(wi, wh)) * wh;
    } else {
        sample.x -= (mat.Clearcoat);
        sample.x /= (1 - mat.Clearcoat);

        float3 cd_lin = mat.BaseColor;// pow(mat.BaseColor, 2.2);
        // Luminance approximmation
        float cd_lum = dot(cd_lin, float3(0.3, 0.6, 0.1));

        // Normalize lum. to isolate hue+sat
        float3 c_tint = cd_lum > 0.f ? (cd_lin / cd_lum) : 1;

        float3 c_spec0 = lerp(mat.Specular * 0.3 * lerp(1, c_tint, mat.SpecularTint), cd_lin, mat.Metallic);

        float cs_lum = dot(c_spec0, float3(0.3, 0.6, 0.1));
        float cs_w = cs_lum / (cs_lum + (1 - mat.Metallic) * cd_lum);

        if (sample.y < cs_w) {
            sample.y /= cs_w;

            float t = sqrt(sample.y / (1 - sample.y));
            wh = normalize(float3(t * ax * cos(2 * PI * sample.x), 1, t * ay * sin(2 * PI * sample.x)));

            wo = -wi + 2.f * abs(dot(wi, wh)) * wh;
        } else {
            sample.y -= cs_w;
            sample.y /= (1.f - cs_w);

            wo = Sample_MapToHemisphere(sample, float3(0, 1, 0), 1);
            wh = normalize(wo + wi);
        }
    }

    //float ndotwh = abs(wh.y);
    //float hdotwo = abs(dot(wh, *wo));

    pdf = Disney_GetPdf(mat, wi, wo);
    return Disney_Evaluate(mat, wi, wo);
}