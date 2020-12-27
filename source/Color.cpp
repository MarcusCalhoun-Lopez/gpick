/*
 * Copyright (c) 2009-2020, Albertas Vyšniauskas
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *     * Neither the name of the software author nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Color.h"
#include <cmath>
#include <string>
#include <tuple>
#include <stdexcept>

// Constant used for lab->xyz transform. Should be calculated with maximum accuracy possible.
const double Epsilon = 216.0 / 24389.0;
const double Kk = 24389.0 / 27.0;
const Color Color::white = { 1.0f, 1.0f, 1.0f };
const Color Color::black = { 0.0f, 0.0f, 0.0f };
static Color::Matrix3d sRGBTransformation;
static Color::Matrix3d sRGBTransformationInverted;
static Color::Matrix3d d65d50AdaptationMatrixValue;
static Color::Matrix3d d50d65AdaptationMatrixValue;
const Color::Matrix3d &Color::sRGBMatrix = sRGBTransformation;
const Color::Matrix3d &Color::sRGBInvertedMatrix = sRGBTransformationInverted;
const Color::Matrix3d &Color::d65d50AdaptationMatrix = d65d50AdaptationMatrixValue;
const Color::Matrix3d &Color::d50d65AdaptationMatrix = d50d65AdaptationMatrixValue;
static Color::Vector3f references[][2] = {
	{ { 109.850f, 100.000f, 35.585f }, { 111.144f, 100.000f, 35.200f } },
	{ { 98.074f, 100.000f, 118.232f }, { 97.285f, 100.000f, 116.145f } },
	{ { 96.422f, 100.000f, 82.521f }, { 96.720f, 100.000f, 81.427f } },
	{ { 95.682f, 100.000f, 92.149f }, { 95.799f, 100.000f, 90.926f } },
	{ { 95.047f, 100.000f, 108.883f }, { 94.811f, 100.000f, 107.304f } },
	{ { 94.972f, 100.000f, 122.638f }, { 94.416f, 100.000f, 120.641f } },
	{ { 99.187f, 100.000f, 67.395f }, { 103.280f, 100.000f, 69.026f } },
	{ { 95.044f, 100.000f, 108.755f }, { 95.792f, 100.000f, 107.687f } },
	{ { 100.966f, 100.000f, 64.370f }, { 103.866f, 100.000f, 65.627f } },
};
void Color::initialize() {
	// constants used below are sRGB working space red, green and blue primaries for D65 reference white
	sRGBTransformation = getWorkingSpaceMatrix(0.6400f, 0.3300f, 0.3000f, 0.6000f, 0.1500f, 0.0600f, getReference(ReferenceIlluminant::D65, ReferenceObserver::_2));
	sRGBTransformationInverted = sRGBTransformation.inverse().value();
	d65d50AdaptationMatrixValue = getChromaticAdaptationMatrix(getReference(ReferenceIlluminant::D65, ReferenceObserver::_2), getReference(ReferenceIlluminant::D50, ReferenceObserver::_2));
	d50d65AdaptationMatrixValue = getChromaticAdaptationMatrix(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), getReference(ReferenceIlluminant::D65, ReferenceObserver::_2));
}
namespace util {
template<typename T>
inline void copy(const T *source, T *destination, size_t count = 1) {
	for (size_t i = 0; i < count; i++) {
		destination[i] = source[i];
	}
}
template<typename T, typename TSource>
inline void castCopy(const TSource *source, T *destination, size_t count = 1) {
	for (size_t i = 0; i < count; i++) {
		destination[i] = static_cast<T>(source[i]);
	}
}
template<typename T>
inline void fill(T *destination, T value, size_t count = 1) {
	for (size_t i = 0; i < count; i++) {
		destination[i] = value;
	}
}
}
Color &Color::zero() {
	for (auto i = 0; i < Color::MemberCount; i++)
		data[i] = 0;
	return *this;
}
Color Color::zero() const {
	return Color();
}
Color::Color() {
	util::fill(data, 0.0f, ChannelCount);
}
Color::Color(float value) {
	util::fill(data, value, ChannelCount);
}
Color::Color(int value) {
	util::fill(data, value / 255.0f, ChannelCount);
}
Color::Color(float red, float green, float blue) {
	rgb.red = red;
	rgb.green = green;
	rgb.blue = blue;
	util::fill(data + 3, 0.0f, ChannelCount - 3);
}
Color::Color(float value1, float value2, float value3, float value4) {
	data[0] = value1;
	data[1] = value2;
	data[2] = value3;
	data[3] = value4;
}
Color::Color(int red, int green, int blue) {
	rgb.red = static_cast<float>(red / 255.0f);
	rgb.green = static_cast<float>(green / 255.0f);
	rgb.blue = static_cast<float>(blue / 255.0f);
	util::fill(data + 3, 0.0f, ChannelCount - 3);
}
Color::Color(const Color &color) {
	util::copy(color.data, data, MemberCount);
}
Color::Color(const Vector3f &value) {
	util::copy(value.data, data, 3);
	util::fill(data + 3, 0.0f, ChannelCount - 3);
}
Color::Color(const Vector3d &value) {
	util::castCopy(value.data, data, 3);
	util::fill(data + 3, 0.0f, ChannelCount - 3);
}
template<>
Color::Vector3f Color::rgbVector() const {
	return Vector3f(rgb.red, rgb.green, rgb.blue);
}
template<>
Color::Vector3d Color::rgbVector() const {
	return Vector3d(rgb.red, rgb.green, rgb.blue);
}
Color Color::rgbToHsv() const {
	float min, max;
	std::tie(min, max) = math::minMax(rgb.red, rgb.green, rgb.blue);
	float delta = max - min;
	Color result;
	result.hsv.value = max;
	if (max != 0.0f)
		result.hsv.saturation = delta / max;
	else
		result.hsv.saturation = 0.0f;
	if (result.hsv.saturation == 0.0f) {
		result.hsv.hue = 0.0f;
	} else {
		if (rgb.red == max)
			result.hsv.hue = (rgb.green - rgb.blue) / delta;
		else if (rgb.green == max)
			result.hsv.hue = 2.0f + (rgb.blue - rgb.red) / delta;
		else if (rgb.blue == max)
			result.hsv.hue = 4.0f + (rgb.red - rgb.green) / delta;
		result.hsv.hue /= 6.0f;
		if (result.hsv.hue < 0.0f)
			result.hsv.hue += 1.0f;
		if (result.hsv.hue >= 1.0f)
			result.hsv.hue -= 1.0f;
	}
	return result;
}
Color Color::hsvToRgb() const {
	float v = hsv.value;
	Color result;
	if (hsv.saturation == 0.0f) {
		result.rgb.red = result.rgb.green = result.rgb.blue = hsv.value;
	} else {
		float h, f, x, y, z;
		h = (hsv.hue - std::floor(hsv.hue)) * 6.0f;
		int i = int(h);
		f = h - std::floor(h);
		x = v * (1.0f - hsv.saturation);
		y = v * (1.0f - (hsv.saturation * f));
		z = v * (1.0f - (hsv.saturation * (1.0f - f)));
		if (i == 0) {
			result.rgb.red = v;
			result.rgb.green = z;
			result.rgb.blue = x;
		} else if (i == 1) {
			result.rgb.red = y;
			result.rgb.green = v;
			result.rgb.blue = x;
		} else if (i == 2) {
			result.rgb.red = x;
			result.rgb.green = v;
			result.rgb.blue = z;
		} else if (i == 3) {
			result.rgb.red = x;
			result.rgb.green = y;
			result.rgb.blue = v;
		} else if (i == 4) {
			result.rgb.red = z;
			result.rgb.green = x;
			result.rgb.blue = v;
		} else {
			result.rgb.red = v;
			result.rgb.green = x;
			result.rgb.blue = y;
		}
	}
	return result;
}
Color Color::rgbToHsl() const {
	float min, max;
	std::tie(min, max) = math::minMax(rgb.red, rgb.green, rgb.blue);
	float delta = max - min;
	Color result;
	result.hsl.lightness = (max + min) / 2;
	if (delta == 0) {
		result.hsl.hue = 0;
		result.hsl.saturation = 0;
	} else {
		if (result.hsl.lightness < 0.5) {
			result.hsl.saturation = delta / (max + min);
		} else {
			result.hsl.saturation = delta / (2 - max - min);
		}
		if (rgb.red == max)
			result.hsv.hue = (rgb.green - rgb.blue) / delta;
		else if (rgb.green == max)
			result.hsv.hue = 2.0f + (rgb.blue - rgb.red) / delta;
		else if (rgb.blue == max)
			result.hsv.hue = 4.0f + (rgb.red - rgb.green) / delta;
		result.hsv.hue /= 6.0f;
		if (result.hsv.hue < 0.0f)
			result.hsv.hue += 1.0f;
		if (result.hsv.hue >= 1.0f)
			result.hsv.hue -= 1.0f;
	}
	return result;
}
Color Color::hslToRgb() const {
	Color result;
	if (hsl.saturation == 0) {
		result.rgb.red = result.rgb.green = result.rgb.blue = hsl.lightness;
	} else {
		float q;
		if (hsl.lightness < 0.5f)
			q = hsl.lightness * (1.0f + hsl.saturation);
		else
			q = hsl.lightness + hsl.saturation - hsl.lightness * hsl.saturation;
		float p = 2 * hsl.lightness - q;
		float R = hsl.hue + 1 / 3.0f;
		float G = hsl.hue;
		float B = hsl.hue - 1 / 3.0f;
		if (R > 1) R -= 1;
		if (B < 0) B += 1;
		if (6.0f * R < 1)
			result.rgb.red = p + (q - p) * 6.0f * R;
		else if (2.0 * R < 1)
			result.rgb.red = q;
		else if (3.0 * R < 2)
			result.rgb.red = p + (q - p) * ((2.0f / 3.0f) - R) * 6.0f;
		else
			result.rgb.red = p;
		if (6.0f * G < 1)
			result.rgb.green = p + (q - p) * 6.0f * G;
		else if (2.0f * G < 1)
			result.rgb.green = q;
		else if (3.0f * G < 2)
			result.rgb.green = p + (q - p) * ((2.0f / 3.0f) - G) * 6.0f;
		else
			result.rgb.green = p;
		if (6.0f * B < 1)
			result.rgb.blue = p + (q - p) * 6.0f * B;
		else if (2.0f * B < 1)
			result.rgb.blue = q;
		else if (3.0f * B < 2)
			result.rgb.blue = p + (q - p) * ((2.0f / 3.0f) - B) * 6.0f;
		else
			result.rgb.blue = p;
	}
	return result;
}
Color Color::rgbToXyz(const Matrix3d &transformation) const {
	return linearRgb().rgbVector<double>() * transformation;
}
Color Color::xyzToRgb(const Matrix3d &transformationInverted) const {
	return Color(rgbVector<double>() * transformationInverted).nonLinearRgbInplace();
}
Color Color::rgbToLab(const Vector3f &referenceWhite, const Matrix3d &transformation, const Matrix3d &adaptationMatrix) const {
	return rgbToXyz(transformation).xyzChromaticAdaptation(adaptationMatrix).xyzToLab(referenceWhite);
}
Color Color::labToRgb(const Vector3f &referenceWhite, const Matrix3d &transformationInverted, const Matrix3d &adaptationMatrixInverted) const {
	return labToXyz(referenceWhite).xyzChromaticAdaptation(adaptationMatrixInverted).xyzToRgb(transformationInverted);
}
Color Color::labToLch() const {
	double H;
	if (lab.a == 0 && lab.b == 0) {
		H = 0;
	} else {
		H = std::atan2(lab.b, lab.a);
	}
	H *= 180.0f / math::PI;
	if (H < 0) H += 360;
	if (H >= 360) H -= 360;
	return Color(lab.L, static_cast<float>(std::sqrt(lab.a * lab.a + lab.b * lab.b)), static_cast<float>(H));
}
Color Color::lchToLab() const {
	return Color(lch.L, static_cast<float>(lch.C * std::cos(lch.h * math::PI / 180.0f)), static_cast<float>(lch.C * std::sin(lch.h * math::PI / 180.0f)));
}
Color Color::hslToHsv() const {
	float l = hsl.lightness * 2.0f;
	float s = hsl.saturation * ((l <= 1.0f) ? (l) : (2.0f - l));
	Color result;
	result.hsv.hue = hsl.hue;
	if (l + s == 0) {
		result.hsv.value = 0;
		result.hsv.saturation = 1;
	} else {
		result.hsv.value = (l + s) / 2.0f;
		result.hsv.saturation = (2.0f * s) / (l + s);
	}
	return result;
}
Color Color::hsvToHsl() const {
	float l = (2.0f - hsv.saturation) * hsv.value;
	float s = (hsv.saturation * hsv.value) / ((l <= 1.0f) ? (l) : (2 - l));
	if (l == 0) s = 0;
	return Color(hsv.hue, s, l / 2.0f);
}
Color &Color::linearRgbInplace() {
	if (rgb.red > 0.04045f)
		rgb.red = std::pow((rgb.red + 0.055f) / 1.055f, 2.4f);
	else
		rgb.red = rgb.red / 12.92f;
	if (rgb.green > 0.04045f)
		rgb.green = std::pow((rgb.green + 0.055f) / 1.055f, 2.4f);
	else
		rgb.green = rgb.green / 12.92f;
	if (rgb.blue > 0.04045f)
		rgb.blue = std::pow((rgb.blue + 0.055f) / 1.055f, 2.4f);
	else
		rgb.blue = rgb.blue / 12.92f;
	return *this;
}
Color Color::linearRgb() const {
	Color result;
	if (rgb.red > 0.04045f)
		result.rgb.red = std::pow((rgb.red + 0.055f) / 1.055f, 2.4f);
	else
		result.rgb.red = rgb.red / 12.92f;
	if (rgb.green > 0.04045f)
		result.rgb.green = std::pow((rgb.green + 0.055f) / 1.055f, 2.4f);
	else
		result.rgb.green = rgb.green / 12.92f;
	if (rgb.blue > 0.04045f)
		result.rgb.blue = std::pow((rgb.blue + 0.055f) / 1.055f, 2.4f);
	else
		result.rgb.blue = rgb.blue / 12.92f;
	return result;
}
Color Color::nonLinearRgb() const {
	Color result;
	if (rgb.red > 0.0031308f)
		result.rgb.red = (1.055f * std::pow(rgb.red, 1.0f / 2.4f)) - 0.055f;
	else
		result.rgb.red = rgb.red * 12.92f;
	if (rgb.green > 0.0031308f)
		result.rgb.green = (1.055f * std::pow(rgb.green, 1.0f / 2.4f)) - 0.055f;
	else
		result.rgb.green = rgb.green * 12.92f;
	if (rgb.blue > 0.0031308f)
		result.rgb.blue = (1.055f * std::pow(rgb.blue, 1.0f / 2.4f)) - 0.055f;
	else
		result.rgb.blue = rgb.blue * 12.92f;
	return result;
}
Color &Color::nonLinearRgbInplace() {
	if (rgb.red > 0.0031308f)
		rgb.red = (1.055f * std::pow(rgb.red, 1.0f / 2.4f)) - 0.055f;
	else
		rgb.red = rgb.red * 12.92f;
	if (rgb.green > 0.0031308f)
		rgb.green = (1.055f * std::pow(rgb.green, 1.0f / 2.4f)) - 0.055f;
	else
		rgb.green = rgb.green * 12.92f;
	if (rgb.blue > 0.0031308f)
		rgb.blue = (1.055f * std::pow(rgb.blue, 1.0f / 2.4f)) - 0.055f;
	else
		rgb.blue = rgb.blue * 12.92f;
	return *this;
}
Color Color::rgbToLch(const Vector3f &referenceWhite, const Matrix3d &transformation, const Matrix3d &adaptationMatrix) const {
	return rgbToLab(referenceWhite, transformation, adaptationMatrix).labToLch();
}
Color Color::lchToRgb(const Vector3f &referenceWhite, const Matrix3d &transformationInverted, const Matrix3d &adaptationMatrixInverted) const {
	return lchToLab().labToRgb(referenceWhite, transformationInverted, adaptationMatrixInverted);
}
Color Color::rgbToLchD50() const {
	return rgbToLab(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), sRGBMatrix, d65d50AdaptationMatrix).labToLch();
}
Color Color::lchToRgbD50() const {
	return lchToLab().labToRgb(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), sRGBInvertedMatrix, d50d65AdaptationMatrix);
}
Color Color::rgbToLabD50() const {
	return rgbToLab(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), sRGBMatrix, d65d50AdaptationMatrix);
}
Color Color::labToRgbD50() const {
	return labToRgb(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), sRGBInvertedMatrix, d50d65AdaptationMatrix);
}
Color Color::rgbToCmy() const {
	return Color(1 - rgb.red, 1 - rgb.green, 1 - rgb.blue);
}
Color Color::cmyToRgb() const {
	return Color(1 - cmy.c, 1 - cmy.m, 1 - cmy.y);
}
Color Color::cmyToCmyk() const {
	float k = 1;
	if (cmy.c < k) k = cmy.c;
	if (cmy.m < k) k = cmy.m;
	if (cmy.y < k) k = cmy.y;
	Color result;
	if (k == 1) {
		result.cmyk.c = result.cmyk.m = result.cmyk.y = 0;
	} else {
		result.cmyk.c = (cmy.c - k) / (1 - k);
		result.cmyk.m = (cmy.m - k) / (1 - k);
		result.cmyk.y = (cmy.y - k) / (1 - k);
	}
	result.cmyk.k = k;
	return result;
}
Color Color::cmykToCmy() const {
	return Color(cmyk.c * (1 - cmyk.k) + cmyk.k, cmyk.m * (1 - cmyk.k) + cmyk.k, cmyk.y * (1 - cmyk.k) + cmyk.k);
}
Color Color::rgbToCmyk() const {
	return rgbToCmy().cmyToCmyk();
}
Color Color::cmykToRgb() const {
	return cmykToCmy().cmyToRgb();
}
Color Color::xyzToLab(const Vector3f &referenceWhite) const {
	float X = xyz.x / referenceWhite.x;
	float Y = xyz.y / referenceWhite.y;
	float Z = xyz.z / referenceWhite.z;
	if (X > Epsilon) {
		X = static_cast<float>(std::pow(X, 1.0f / 3.0f));
	} else {
		X = static_cast<float>((Kk * X + 16.0f) / 116.0f);
	}
	if (Y > Epsilon) {
		Y = static_cast<float>(std::pow(Y, 1.0f / 3.0f));
	} else {
		Y = static_cast<float>((Kk * Y + 16.0f) / 116.0f);
	}
	if (Z > Epsilon) {
		Z = static_cast<float>(std::pow(Z, 1.0f / 3.0f));
	} else {
		Z = static_cast<float>((Kk * Z + 16.0f) / 116.0f);
	}
	return Color((116 * Y) - 16, 500 * (X - Y), 200 * (Y - Z));
}
Color Color::labToXyz(const Vector3f &referenceWhite) const {
	float x, y, z;
	float fy = (lab.L + 16) / 116;
	float fx = lab.a / 500 + fy;
	float fz = fy - lab.b / 200;
	const double K = (24389.0 / 27.0);
	if (std::pow(fx, 3) > Epsilon) {
		x = static_cast<float>(std::pow(fx, 3));
	} else {
		x = static_cast<float>((116 * fx - 16) / K);
	}
	if (lab.L > K * Epsilon) {
		y = static_cast<float>(std::pow((lab.L + 16) / 116, 3));
	} else {
		y = static_cast<float>(lab.L / K);
	}
	if (std::pow(fz, 3) > Epsilon) {
		z = static_cast<float>(std::pow(fz, 3));
	} else {
		z = static_cast<float>((116 * fz - 16) / K);
	}
	return Color(x * referenceWhite.x, y * referenceWhite.y, z * referenceWhite.z);
}
Color::Matrix3d Color::getWorkingSpaceMatrix(float xr, float yr, float xg, float yg, float xb, float yb, const Vector3f &referenceWhite) {
	float Xr = xr / yr;
	float Yr = 1;
	float Zr = (1 - xr - yr) / yr;
	float Xg = xg / yg;
	float Yg = 1;
	float Zg = (1 - xg - yg) / yg;
	float Xb = xb / yb;
	float Yb = 1;
	float Zb = (1 - xb - yb) / yb;
	auto s = math::vectorCast<double>(referenceWhite) * Matrix3d { Xr, Xg, Xb, Yr, Yg, Yb, Zr, Zg, Zb }.inverse().value();
	return Matrix3d { Xr * s.r, Xg * s.g, Xb * s.b, Yr * s.r, Yg * s.g, Yb * s.b, Zr * s.r, Zg * s.g, Zb * s.b };
}
Color::Matrix3d Color::getChromaticAdaptationMatrix(const Vector3f &sourceReferenceWhite, const Vector3f &destinationReferenceWhite) {
	const Matrix3d bradfordMatrix { 0.8951, 0.2664, -0.1614, -0.7502, 1.7135, 0.0367, 0.0389, -0.0685, 1.0296 };
	const Matrix3d bradfordInvertedMatrix = *bradfordMatrix.inverse();
	auto source = math::vectorCast<double>(sourceReferenceWhite) * bradfordMatrix;
	auto destination = math::vectorCast<double>(destinationReferenceWhite) * bradfordMatrix;
	Matrix3d matrix;
	matrix[0] = destination.x / source.x;
	matrix[4] = destination.y / source.y;
	matrix[8] = destination.z / source.z;
	return bradfordMatrix * matrix * bradfordInvertedMatrix;
}
Color Color::xyzChromaticAdaptation(const Matrix3d &adaptation) const {
	return rgbVector<double>() * adaptation;
}
const Color::Vector3f &Color::getReference(ReferenceIlluminant illuminant, ReferenceObserver observer) {
	return references[static_cast<uint8_t>(illuminant)][static_cast<uint8_t>(observer)];
}
ReferenceIlluminant Color::getIlluminant(const std::string &illuminant) {
	const struct {
		const char *label;
		ReferenceIlluminant illuminant;
	} illuminants[] = {
		{ "A", ReferenceIlluminant::A },
		{ "C", ReferenceIlluminant::C },
		{ "D50", ReferenceIlluminant::D50 },
		{ "D55", ReferenceIlluminant::D55 },
		{ "D65", ReferenceIlluminant::D65 },
		{ "D75", ReferenceIlluminant::D75 },
		{ "F2", ReferenceIlluminant::F2 },
		{ "F7", ReferenceIlluminant::F7 },
		{ "F11", ReferenceIlluminant::F11 },
		{ nullptr, ReferenceIlluminant::D50 },
	};
	for (int i = 0; illuminants[i].label; i++) {
		if (illuminant == illuminants[i].label) {
			return illuminants[i].illuminant;
		}
	}
	return ReferenceIlluminant::D50;
};
ReferenceObserver Color::getObserver(const std::string &observer) {
	const struct {
		const char *label;
		ReferenceObserver observer;
	} observers[] = {
		{ "2", ReferenceObserver::_2 },
		{ "10", ReferenceObserver::_10 },
		{ nullptr, ReferenceObserver::_2 },
	};
	for (int i = 0; observers[i].label; i++) {
		if (observer == observers[i].label) {
			return observers[i].observer;
		}
	}
	return ReferenceObserver::_2;
}
bool Color::isOutOfRgbGamut() const {
	if (rgb.red < 0 || rgb.red > 1)
		return true;
	if (rgb.green < 0 || rgb.green > 1)
		return true;
	if (rgb.blue < 0 || rgb.blue > 1)
		return true;
	return false;
}
float Color::distance(const Color &a, const Color &b) {
	auto al = a.linearRgb();
	auto bl = b.linearRgb();
	return static_cast<float>(std::sqrt(
		std::pow(bl.rgb.red - al.rgb.red, 2) +
		std::pow(bl.rgb.green - al.rgb.green, 2) +
		std::pow(bl.rgb.blue - al.rgb.blue, 2)));
}
float Color::distanceLch(const Color &a, const Color &b) {
	auto al = a.labToLch();
	auto bl = b.labToLch();
	return static_cast<float>(std::sqrt(
		std::pow((bl.lch.L - al.lch.L) / 1, 2) +
		std::pow((bl.lch.C - al.lch.C) / (1 + 0.045 * al.lch.C), 2) +
		std::pow((std::pow(a.lab.a - b.lab.a, 2) + std::pow(a.lab.b - b.lab.b, 2) - (bl.lch.C - al.lch.C)) / (1 + 0.015 * al.lch.C), 2)));
}
Color mix(const Color &a, const Color &b, float ratio) {
	return (a.linearRgb() * (1.0f - ratio) + b.linearRgb() * ratio).nonLinearRgb();
}
bool Color::operator==(const Color &color) const {
	for (auto i = 0; i < MemberCount; i++) {
		if (math::abs(data[i] - color.data[i]) > 1e-6)
			return false;
	}
	return true;
}
bool Color::operator!=(const Color &color) const {
	for (auto i = 0; i < MemberCount; i++) {
		if (math::abs(data[i] - color.data[i]) > 1e-6)
			return true;
	}
	return false;
}
Color Color::operator*(float value) const {
	return Color(data[0] * value, data[1] * value, data[2] * value, data[3] * value);
}
Color Color::operator/(float value) const {
	return Color(data[0] / value, data[1] / value, data[2] / value, data[3] / value);
}
Color Color::operator+(const Color &color) const {
	Color result;
	for (auto i = 0; i < Color::MemberCount; i++)
		result.data[i] = data[i] + color.data[i];
	return result;
}
Color Color::operator-(const Color &color) const {
	Color result;
	for (auto i = 0; i < Color::MemberCount; i++)
		result.data[i] = data[i] - color.data[i];
	return result;
}
Color Color::operator*(const Color &color) const {
	Color result;
	for (auto i = 0; i < Color::MemberCount; i++)
		result.data[i] = data[i] * color.data[i];
	return result;
}
Color &Color::operator*=(const Color &color) {
	for (auto i = 0; i < Color::MemberCount; i++)
		data[i] *= color.data[i];
	return *this;
}
Color &Color::operator+=(const Color &color) {
	for (auto i = 0; i < Color::MemberCount; i++)
		data[i] += color.data[i];
	return *this;
}
float &Color::operator[](int index) {
	if (index < 0 || index >= MemberCount)
		throw std::invalid_argument("index");
	return data[index];
}
float Color::operator[](int index) const {
	if (index < 0 || index >= MemberCount)
		throw std::invalid_argument("index");
	return data[index];
}
Color Color::normalizeRgb() const {
	Color result(*this);
	result.rgb.red = math::clamp(rgb.red, 0.0f, 1.0f);
	result.rgb.green = math::clamp(rgb.green, 0.0f, 1.0f);
	result.rgb.blue = math::clamp(rgb.blue, 0.0f, 1.0f);
	return result;
}
Color &Color::normalizeRgbInplace() {
	rgb.red = math::clamp(rgb.red, 0.0f, 1.0f);
	rgb.green = math::clamp(rgb.green, 0.0f, 1.0f);
	rgb.blue = math::clamp(rgb.blue, 0.0f, 1.0f);
	return *this;
}
Color Color::absolute() const {
	Color result(*this);
	for (auto i = 0; i < Color::MemberCount; i++)
		result.data[i] = math::abs(data[i]);
	return result;
}
Color &Color::absoluteInplace() {
	for (auto i = 0; i < Color::MemberCount; i++)
		data[i] = math::abs(data[i]);
	return *this;
}
const Color &Color::getContrasting() const {
	auto t = rgbToLab(getReference(ReferenceIlluminant::D50, ReferenceObserver::_2), sRGBMatrix, d65d50AdaptationMatrix);
	return t.lab.L > 50 ? black : white;
}
