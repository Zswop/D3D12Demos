#include "Constants.hlsl"

#define ACEScc_MAX      1.4679964
#define ACEScc_MIDGRAY  0.4135884

//
// Precomputed matrices
// See https://github.com/ampas/aces-dev/blob/master/transforms/ctl/README-MATRIX.md
//
static const float3x3 sRGB_2_AP0 = 
{
	0.4397010, 0.0897923, 0.0175440,
	0.3829780, 0.8134230, 0.1115440,
	0.1773350, 0.0967616, 0.8707040
};

static const float3x3 sRGB_2_AP1 = 
{
	0.61319, 0.07021, 0.02062,
	0.33951, 0.91634, 0.10957,
	0.04737, 0.01345, 0.86961
};

static const float3x3 AP0_2_sRGB = 
{
	 2.52169, -0.27648, -0.01538,
	-1.13413,  1.37272, -0.15298,
	-0.38756, -0.09624,  1.16835,
};

static const float3x3 AP1_2_sRGB = 
{
	 1.70505, -0.13026, -0.02400,
	-0.62179,  1.14080, -0.12897,
	-0.08326, -0.01055,  1.15297,
};

static const float3x3 AP0_2_AP1_MAT = 
{
	 1.4514393161, -0.0765537734, 0.0083161484,
	-0.2365107469,  1.1762296998, -0.0060324498,
	-0.2149285693, -0.0996759264,  0.9977163014
};

static const float3x3 AP1_2_AP0_MAT = 
{
	 0.6954522414, 0.0447945634, -0.0055258826,
	 0.1406786965, 0.8596711185, 0.0040252103,
	 0.1638690622, 0.0955343182, 1.0015006723
};

static const float3x3 AP1_2_XYZ_MAT = 
{
	 0.6624541811, 0.2722287168, -0.0055746495,
	 0.1340042065, 0.6740817658, 0.0040607335,
	 0.1561876870, 0.0536895174, 1.0103391003
};

static const float3x3 XYZ_2_AP1_MAT = 
{
	 1.6410233797, -0.6636628587,  0.0117218943,
	-0.3248032942,  1.6153315917, -0.0082844420,
	-0.2364246952,  0.0167563477,  0.9883948585
};

static const float3x3 XYZ_2_REC709_MAT = 
{
	 3.2409699419, -0.9692436363,  0.0556300797,
	-1.5373831776,  1.8759675015, -0.2039769589,
	-0.4986107603,  0.0415550574,  1.0569715142
};

static const float3x3 XYZ_2_REC2020_MAT = 
{
	 1.7166511880, -0.6666843518,   0.0176398574,
	-0.3556707838,  1.6164812366,  -0.0427706133,
	-0.2533662814,  0.0157685458 ,  0.9421031212
};

static const float3x3 XYZ_2_DCIP3_MAT = 
{
	 2.7253940305, -0.7951680258,  0.0412418914,
	-1.0180030062,  1.6897320548, -0.0876390192,
	-0.4401631952,  0.0226471906,  1.1009293786
};

static const float3 AP1_RGB2Y = float3(0.272229, 0.674082, 0.0536895);

static const float3x3 RRT_SAT_MAT = 
{
	0.9708890,  0.0108892,   0.0108892,
	0.0269633,  0.9869630,   0.0269633,
	0.00214758, 0.00214758,  0.96214800
};

static const float3x3 ODT_SAT_MAT = 
{
	0.949056,   0.019056,   0.019056,
	0.0471857,  0.9771860,  0.0471857,
	0.00375827, 0.00375827, 0.93375800
};

static const float3x3 D60_2_D65_CAT = 
{
	 0.98722400, -0.00759836,  0.00307257,
	-0.00611327,  1.00186000, -0.00509595,
	 0.0159533,   0.0053302 ,  1.0816800
};

float3 sRGB_to_ACES(float3 rgb)
{
	float3 aces = mul(rgb, sRGB_2_AP0);
	return aces;
}

float3 ACES_to_sRGB(float3 rgb)
{
	float3 aces = mul(rgb, AP0_2_sRGB);
	return aces;
}

//=================================================================================================
// Reference Rendering Transform (RRT)
//
//   Input is ACES
//   Output is OCES
//
//=================================================================================================

float rgb_2_saturation(float3 rgb)
{
	const float TINY = 1e-4;
	float mi = min(min(rgb.r, rgb.g), rgb.b);
	float ma = max(max(rgb.r, rgb.g), rgb.b);
	return (max(ma, TINY) - max(mi, TINY)) / max(ma, 1e-2);
}

float rgb_2_yc(float3 rgb)
{
	const float ycRadiusWeight = 1.75;

	// Converts RGB to a luminance proxy, here called YC
	// YC is ~ Y + K * Chroma
	// Constant YC is a cone-shaped surface in RGB space, with the tip on the
	// neutral axis, towards white.
	// YC is normalized: RGB 1 1 1 maps to YC = 1
	//
	// ycRadiusWeight defaults to 1.75, although can be overridden in function
	// call to rgb_2_yc
	// ycRadiusWeight = 1 -> YC for pure cyan, magenta, yellow == YC for neutral
	// of same value
	// ycRadiusWeight = 2 -> YC for pure red, green, blue  == YC for  neutral of
	// same value.

	float r = rgb.x;
	float g = rgb.y;
	float b = rgb.z;
	float k = b * (b - g) + g * (g - r) + r * (r - b);
	k = max(k, 0.0f); // Clamp to avoid precision issue causing k < 0, making sqrt(k) undefined
	float chroma = sqrt(k);
	return (b + g + r + ycRadiusWeight * chroma) / 3.0;
}

float rgb_2_hue(float3 rgb)
{
	// Returns a geometric hue angle in degrees (0-360) based on RGB values.
	// For neutral colors, hue is undefined and the function will return a quiet NaN value.
	float hue;
	if (rgb.x == rgb.y && rgb.y == rgb.z)
		hue = 0.0; // RGB triplets where RGB are equal have an undefined hue
	else
		hue = (180.0 / Pi) * atan2(sqrt(3.0) * (rgb.y - rgb.z), 2.0 * rgb.x - rgb.y - rgb.z);

	if (hue < 0.0) hue = hue + 360.0;

	return hue;
}

float center_hue(float hue, float centerH)
{
	float hueCentered = hue - centerH;
	if (hueCentered < -180.0) hueCentered = hueCentered + 360.0;
	else if (hueCentered > 180.0) hueCentered = hueCentered - 360.0;
	return hueCentered;
}

float sigmoid_shaper(float x)
{
	// Sigmoid function in the range 0 to 1 spanning -2 to +2.

	float t = max(1.0 - abs(x / 2.0), 0.0);
	float y = 1.0 + sign(x) * (1.0 - t * t);

	return y / 2.0;
}

float glow_fwd(float ycIn, float glowGainIn, float glowMid)
{
	float glowGainOut;

	if (ycIn <= 2.0 / 3.0 * glowMid)
		glowGainOut = glowGainIn;
	else if (ycIn >= 2.0 * glowMid)
		glowGainOut = 0.0;
	else
		glowGainOut = glowGainIn * (glowMid / ycIn - 1.0 / 2.0);

	return glowGainOut;
}

static const float3x3 M = 
{
	 0.5, -1.0, 0.5,
	-1.0,  1.0, 0.5,
	 0.5,  0.0, 0.0
};

float segmented_spline_c5_fwd(float x)
{
	const float coefsLow[6] = { -4.0000000000, -4.0000000000, -3.1573765773, -0.4852499958, 1.8477324706, 1.8477324706 }; // coefs for B-spline between minPoint and midPoint (units of log luminance)
	const float coefsHigh[6] = { -0.7185482425, 2.0810307172, 3.6681241237, 4.0000000000, 4.0000000000, 4.0000000000 }; // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	const float2 minPoint = float2(0.18 * exp2(-15.0), 0.0001); // {luminance, luminance} linear extension below this
	const float2 midPoint = float2(0.18, 0.48); // {luminance, luminance}
	const float2 maxPoint = float2(0.18 * exp2(18.0), 10000.0); // {luminance, luminance} linear extension above this
	const float slopeLow = 0.0; // log-log slope of low linear extension
	const float slopeHigh = 0.0; // log-log slope of high linear extension

	const int N_KNOTS_LOW = 4;
	const int N_KNOTS_HIGH = 4;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to ACESMIN.1
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 0.00006103515; // = pow(2.0, -14.0);

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(minPoint.x))
	{
		logy = logx * slopeLow + (log10(minPoint.y) - slopeLow * log10(minPoint.x));
	}
	else if ((logx > log10(minPoint.x)) && (logx < log10(midPoint.x)))
	{
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(minPoint.x)) / (log10(midPoint.x) - log10(minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsLow[j], coefsLow[j + 1], coefsLow[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, mul(cf, M));
	}
	else if ((logx >= log10(midPoint.x)) && (logx < log10(maxPoint.x)))
	{
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(midPoint.x)) / (log10(maxPoint.x) - log10(midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsHigh[j], coefsHigh[j + 1], coefsHigh[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, mul(cf, M));
	}
	else
	{ //if (logIn >= log10(maxPoint.x)) {
		logy = logx * slopeHigh + (log10(maxPoint.y) - slopeHigh * log10(maxPoint.x));
	}

	return pow(10.0, logy);
}

float segmented_spline_c9_fwd(float x)
{
	const float coefsLow[10] = { -1.6989700043, -1.6989700043, -1.4779000000, -1.2291000000, -0.8648000000, -0.4480000000, 0.0051800000, 0.4511080334, 0.9113744414, 0.9113744414 }; // coefs for B-spline between minPoint and midPoint (units of log luminance)
	const float coefsHigh[10] = { 0.5154386965, 0.8470437783, 1.1358000000, 1.3802000000, 1.5197000000, 1.5985000000, 1.6467000000, 1.6746091357, 1.6878733390, 1.6878733390 }; // coefs for B-spline between midPoint and maxPoint (units of log luminance)
	const float2 minPoint = float2(segmented_spline_c5_fwd(0.18 * exp2(-6.5)), 0.02); // {luminance, luminance} linear extension below this
	const float2 midPoint = float2(segmented_spline_c5_fwd(0.18), 4.8); // {luminance, luminance}
	const float2 maxPoint = float2(segmented_spline_c5_fwd(0.18 * exp2(6.5)), 48.0); // {luminance, luminance} linear extension above this
	const float slopeLow = 0.0; // log-log slope of low linear extension
	const float slopeHigh = 0.04; // log-log slope of high linear extension

	const int N_KNOTS_LOW = 8;
	const int N_KNOTS_HIGH = 8;

	// Check for negatives or zero before taking the log. If negative or zero,
	// set to OCESMIN.
	float xCheck = x;
	if (xCheck <= 0.0) xCheck = 1e-4;

	float logx = log10(xCheck);
	float logy;

	if (logx <= log10(minPoint.x))
	{
		logy = logx * slopeLow + (log10(minPoint.y) - slopeLow * log10(minPoint.x));
	}
	else if ((logx > log10(minPoint.x)) && (logx < log10(midPoint.x)))
	{
		float knot_coord = (N_KNOTS_LOW - 1) * (logx - log10(minPoint.x)) / (log10(midPoint.x) - log10(minPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsLow[j], coefsLow[j + 1], coefsLow[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, mul(cf, M));
	}
	else if ((logx >= log10(midPoint.x)) && (logx < log10(maxPoint.x)))
	{
		float knot_coord = (N_KNOTS_HIGH - 1) * (logx - log10(midPoint.x)) / (log10(maxPoint.x) - log10(midPoint.x));
		int j = knot_coord;
		float t = knot_coord - j;

		float3 cf = float3(coefsHigh[j], coefsHigh[j + 1], coefsHigh[j + 2]);
		float3 monomials = float3(t * t, t, 1.0);
		logy = dot(monomials, mul(cf, M));
	}
	else
	{ //if (logIn >= log10(maxPoint.x)) {
		logy = logx * slopeHigh + (log10(maxPoint.y) - slopeHigh * log10(maxPoint.x));
	}

	return pow(10.0, logy);
}

static const float RRT_GLOW_GAIN = 0.05;
static const float RRT_GLOW_MID = 0.08;

static const float RRT_RED_SCALE = 0.82;
static const float RRT_RED_PIVOT = 0.03;
static const float RRT_RED_HUE = 0.0;
static const float RRT_RED_WIDTH = 135.0;

static const float RRT_SAT_FACTOR = 0.96;

float3 RRT(float3 aces)
{
	// --- Glow module --- //
	float saturation = rgb_2_saturation(aces);
	float ycIn = rgb_2_yc(aces);
	float s = sigmoid_shaper((saturation - 0.4) / 0.2);
	float addedGlow = 1.0 + glow_fwd(ycIn, RRT_GLOW_GAIN * s, RRT_GLOW_MID);
	aces *= addedGlow;

	// --- Red modifier --- //
	float hue = rgb_2_hue(aces);
	float centeredHue = center_hue(hue, RRT_RED_HUE);
	float hueWeight;
	{
		//hueWeight = cubic_basis_shaper(centeredHue, RRT_RED_WIDTH);
		hueWeight = smoothstep(0.0, 1.0, 1.0 - abs(2.0 * centeredHue / RRT_RED_WIDTH));
		hueWeight *= hueWeight;
	}

	aces.r += hueWeight * saturation * (RRT_RED_PIVOT - aces.r) * (1.0 - RRT_RED_SCALE);

	// --- ACES to RGB rendering space --- //
	aces = clamp(aces, 0.0, FP16Max);  // avoids saturated negative colors from becoming positive in the matrix
	float3 rgbPre = mul(aces, AP0_2_AP1_MAT);
	rgbPre = clamp(rgbPre, 0, FP16Max);

	// --- Global desaturation --- //
	//rgbPre = mul(RRT_SAT_MAT, rgbPre);
	rgbPre = lerp(dot(rgbPre, AP1_RGB2Y).xxx, rgbPre, RRT_SAT_FACTOR.xxx);

	// --- Apply the tonescale independently in rendering-space RGB --- //
	float3 rgbPost;
	rgbPost.x = segmented_spline_c5_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c5_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c5_fwd(rgbPre.z);

	// --- RGB rendering space to OCES --- //
	float3 rgbOces = mul(rgbPost, AP1_2_AP0_MAT);

	return rgbOces;
}

//=================================================================================================
// Output Device Transform
//=================================================================================================

float3 Y_2_linCV(float3 Y, float Ymax, float Ymin)
{
	return (Y - Ymin) / (Ymax - Ymin);
}

float3 XYZ_2_xyY(float3 XYZ)
{
	float divisor = max(dot(XYZ, (1.0).xxx), 1e-4);
	return float3(XYZ.xy / divisor, XYZ.y);
}

float3 xyY_2_XYZ(float3 xyY)
{
	float m = xyY.z / max(xyY.y, 1e-4);
	float3 XYZ = float3(xyY.xz, (1.0 - xyY.x - xyY.y));
	XYZ.xz *= m;
	return XYZ;
}

static const float DIM_SURROUND_GAMMA = 0.9811;

float3 darkSurround_to_dimSurround(float3 linearCV)
{
	float3 XYZ = mul(linearCV, AP1_2_XYZ_MAT);

	float3 xyY = XYZ_2_xyY(XYZ);
	xyY.z = clamp(xyY.z, 0.0, FP16Max);
	xyY.z = pow(xyY.z, DIM_SURROUND_GAMMA);
	XYZ = xyY_2_XYZ(xyY);

	return mul(XYZ, XYZ_2_AP1_MAT);
}

float moncurve_r(float y, float gamma, float offs)
{
	// Reverse monitor curve
	float x;
	const float yb = pow(offs * gamma / ((gamma - 1.0) * (1.0 + offs)), gamma);
	const float rs = pow((gamma - 1.0) / offs, gamma - 1.0) * pow((1.0 + offs) / gamma, gamma);
	if (y >= yb)
		x = (1.0 + offs) * pow(y, 1.0 / gamma) - offs;
	else
		x = y * rs;
	return x;
}

static const float CINEMA_WHITE = 48.0;
static const float CINEMA_BLACK = CINEMA_WHITE / 2400.0;
static const float ODT_SAT_FACTOR = 0.93;

// <ACEStransformID>ODT.Academy.RGBmonitor_100nits_dim.a1.0.3</ACEStransformID>
// <ACESuserName>ACES 1.0 Output - sRGB</ACESuserName>

//
// Output Device Transform - RGB computer monitor
//

//
// Summary :
//  This transform is intended for mapping OCES onto a desktop computer monitor
//  typical of those used in motion picture visual effects production. These
//  monitors may occasionally be referred to as "sRGB" displays, however, the
//  monitor for which this transform is designed does not exactly match the
//  specifications in IEC 61966-2-1:1999.
//
//  The assumed observer adapted white is D65, and the viewing environment is
//  that of a dim surround.
//
//  The monitor specified is intended to be more typical of those found in
//  visual effects production.
//
// Device Primaries :
//  Primaries are those specified in Rec. ITU-R BT.709
//  CIE 1931 chromaticities:  x         y         Y
//              Red:          0.64      0.33
//              Green:        0.3       0.6
//              Blue:         0.15      0.06
//              White:        0.3127    0.329     100 cd/m^2
//
// Display EOTF :
//  The reference electro-optical transfer function specified in
//  IEC 61966-2-1:1999.
//
// Signal Range:
//    This transform outputs full range code values.
//
// Assumed observer adapted white point:
//         CIE 1931 chromaticities:    x            y
//                                     0.3127       0.329
//
// Viewing Environment:
//   This ODT has a compensation for viewing environment variables more typical
//   of those associated with video mastering.
//
float3 ODT_RGBmonitor_100nits_dim(float3 oces)
{
	// OCES to RGB rendering space
	float3 rgbPre = mul(oces, AP0_2_AP1_MAT);

	// Apply the tonescale independently in rendering-space RGB
	float3 rgbPost;
	rgbPost.x = segmented_spline_c9_fwd(rgbPre.x);
	rgbPost.y = segmented_spline_c9_fwd(rgbPre.y);
	rgbPost.z = segmented_spline_c9_fwd(rgbPre.z);

	// Scale luminance to linear code value
	float3 linearCV = Y_2_linCV(rgbPost, CINEMA_WHITE, CINEMA_BLACK);

	// Apply gamma adjustment to compensate for dim surround
	linearCV = darkSurround_to_dimSurround(linearCV);

	// Apply desaturation to compensate for luminance difference
	//linearCV = mul(ODT_SAT_MAT, linearCV);
	linearCV = lerp(dot(linearCV, AP1_RGB2Y).xxx, linearCV, ODT_SAT_FACTOR.xxx);

	// Convert to display primary encoding
	// Rendering space RGB to XYZ
	float3 XYZ = mul(linearCV, AP1_2_XYZ_MAT);

	// Apply CAT from ACES white point to assumed observer adapted white point
	XYZ = mul(XYZ, D60_2_D65_CAT);

	// CIE XYZ to display primaries
	linearCV = mul(XYZ, XYZ_2_REC709_MAT);

	// Handle out-of-gamut values
	// Clip values < 0 or > 1 (i.e. projecting outside the display primaries)
	linearCV = saturate(linearCV);
	
	// Encode linear code values with transfer function
	float3 outputCV;
	// moncurve_r with gamma of 2.4 and offset of 0.055 matches the EOTF found in IEC 61966-2-1:1999 (sRGB)
	const float DISPGAMMA = 2.4;
	const float OFFSET = 0.055;
	outputCV.x = moncurve_r(linearCV.x, DISPGAMMA, OFFSET);
	outputCV.y = moncurve_r(linearCV.y, DISPGAMMA, OFFSET);
	outputCV.z = moncurve_r(linearCV.z, DISPGAMMA, OFFSET);

	return outputCV;
}