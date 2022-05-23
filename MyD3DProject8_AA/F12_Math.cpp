#include "F12_Math.h"

using namespace DirectX::PackedVector;

namespace Framework
{

// == Float2 ======================================================================================

Float2::Float2()
{
	x = y = 0.0f;
}

Float2::Float2(float x_)
{
	x = y = x_;
}

Float2::Float2(float x_, float y_)
{
	x = x_;
	y = y_;
}

Float2::Float2(const DirectX::XMFLOAT2& xy) 
{
	x = xy.x;
	y = xy.y;
}

Float2::Float2(DirectX::FXMVECTOR xy)
{
	DirectX::XMStoreFloat2(reinterpret_cast<DirectX::XMFLOAT2*>(this), xy);
}

Float2& Float2::operator+=(const Float2& other)
{
	x += other.x;
	y += other.y;
	return *this;
}

Float2 Float2::operator+(const Float2& other) const
{
	Float2 result;
	result.x = x + other.x;
	result.y = y + other.y;
	return result;
}

Float2& Float2::operator-=(const Float2& other)
{
	x -= other.x;
	y -= other.y;
	return *this;
}

Float2 Float2::operator-(const Float2& other) const
{
	Float2 result;
	result.x = x - other.x;
	result.y = y - other.y;
	return result;
}

Float2& Float2::operator*=(const Float2& other)
{
	x *= other.x;
	y *= other.y;
	return *this;
}

Float2 Float2::operator*(const Float2& other) const
{
	Float2 result;
	result.x = x * other.x;
	result.y = y * other.y;
	return result;
}

Float2& Float2::operator*=(float s)
{
	x *= s;
	y *= s;
	return *this;
}

Float2 Float2::operator*(float s) const
{
	Float2 result;
	result.x = x * s;
	result.y = y * s;
	return result;
}

Float2& Float2::operator/=(const Float2& other)
{
	x /= other.x;
	y /= other.y;
	return *this;
}

Float2 Float2::operator/(const Float2& other) const
{
	Float2 result;
	result.x = x / other.x;
	result.y = y / other.y;
	return result;
}

bool Float2::operator==(const Float2& other) const
{
	return x == other.x && y == other.y;
}

bool Float2::operator!=(const Float2& other) const
{
	return x != other.x || y != other.y;
}

Float2 Float2::operator-() const
{
	Float2 result;
	result.x = -x;
	result.y = -y;

	return result;
}

DirectX::XMVECTOR Float2::ToSIMD() const
{
	return DirectX::XMLoadFloat2(reinterpret_cast<const DirectX::XMFLOAT2*>(this));
}

Float2 Float2::Clamp(const Float2& val, const Float2& min, const Float2& max)
{
	Float2 retVal;
	retVal.x = Framework::Clamp(val.x, min.x, max.x);
	retVal.y = Framework::Clamp(val.y, min.y, max.y);
	return retVal;
}

float Float2::Length(const Float2& val)
{
	return std::sqrtf(val.x * val.x + val.y * val.y);
}

// == Float3 ======================================================================================

Float3::Float3()
{
	x = y = z = 0.0f;
}

Float3::Float3(float x_)
{
	x = y = z = x_;
}

Float3::Float3(float x_, float y_, float z_)
{
	x = x_;
	y = y_;
	z = z_;
}

Float3::Float3(Float2 xy, float z_)
{
	x = xy.x;
	y = xy.y;
	z = z_;
}

Float3::Float3(const DirectX::XMFLOAT3& xyz)
{
	x = xyz.x;
	y = xyz.y;
	z = xyz.z;
}

Float3::Float3(DirectX::FXMVECTOR xyz)
{
	DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(this), xyz);
}

float Float3::operator[](unsigned int idx) const
{
	assert(idx < 3);
	return *(&x + idx);
}

Float3& Float3::operator+=(const Float3& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
	return *this;
}

Float3 Float3::operator+(const Float3& other) const
{
	Float3 result;
	result.x = x + other.x;
	result.y = y + other.y;
	result.z = z + other.z;
	return result;
}

Float3& Float3::operator+=(float s)
{
	x += s;
	y += s;
	z += s;
	return *this;
}

Float3 Float3::operator+(float s) const
{
	Float3 result;
	result.x = x + s;
	result.y = y + s;
	result.z = z + s;
	return result;
}

Float3& Float3::operator-=(const Float3& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
	return *this;
}

Float3 Float3::operator-(const Float3& other) const
{
	Float3 result;
	result.x = x - other.x;
	result.y = y - other.y;
	result.z = z - other.z;
	return result;
}

Float3& Float3::operator-=(float s)
{
	x -= s;
	y -= s;
	z -= s;
	return *this;
}

Float3 Float3::operator-(float s) const
{
	Float3 result;
	result.x = x - s;
	result.y = y - s;
	result.z = z - s;
	return result;
}


Float3& Float3::operator*=(const Float3& other)
{
	x *= other.x;
	y *= other.y;
	z *= other.z;
	return *this;
}

Float3 Float3::operator*(const Float3& other) const
{
	Float3 result;
	result.x = x * other.x;
	result.y = y * other.y;
	result.z = z * other.z;
	return result;
}

Float3& Float3::operator*=(float s)
{
	x *= s;
	y *= s;
	z *= s;
	return *this;
}

Float3 Float3::operator*(float s) const
{
	Float3 result;
	result.x = x * s;
	result.y = y * s;
	result.z = z * s;
	return result;
}

Float3& Float3::operator/=(const Float3& other)
{
	x /= other.x;
	y /= other.y;
	z /= other.z;
	return *this;
}

Float3 Float3::operator/(const Float3& other) const
{
	Float3 result;
	result.x = x / other.x;
	result.y = y / other.y;
	result.z = z / other.z;
	return result;
}

Float3& Float3::operator/=(float s)
{
	x /= s;
	y /= s;
	z /= s;
	return *this;
}

Float3 Float3::operator/(float s) const
{
	Float3 result;
	result.x = x / s;
	result.y = y / s;
	result.z = z / s;
	return result;
}

bool Float3::operator==(const Float3& other) const
{
	return x == other.x && y == other.y && z == other.z;
}

bool Float3::operator!=(const Float3& other) const
{
	return x != other.x || y != other.y || z != other.z;
}

Float3 Float3::operator-() const
{
	Float3 result;
	result.x = -x;
	result.y = -y;
	result.z = -z;

	return result;
}

Float3 operator*(float a, const Float3& b)
{
	return Float3(a * b.x, a * b.y, a * b.z);
}

DirectX::XMVECTOR Float3::ToSIMD() const
{
	return DirectX::XMLoadFloat3(reinterpret_cast<const DirectX::XMFLOAT3*>(this));
}

DirectX::XMFLOAT3 Float3::ToXMFLOAT3() const
{
	return DirectX::XMFLOAT3(x, y, z);
}

Float2 Float3::To2D() const
{
	return Float2(x, y);
}

float Float3::Length() const
{
	return Float3::Length(*this);
}

float Float3::Dot(const Float3& a, const Float3& b)
{
	return DirectX::XMVectorGetX(DirectX::XMVector3Dot(a.ToSIMD(), b.ToSIMD()));
}

Float3 Float3::Cross(const Float3& a, const Float3& b)
{
	Float3 result;
	DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&result), DirectX::XMVector3Cross(a.ToSIMD(), b.ToSIMD()));
	return result;
}

Float3 Float3::Normalize(const Float3& a)
{
	Float3 result;
	DirectX::XMStoreFloat3(reinterpret_cast<DirectX::XMFLOAT3*>(&result), DirectX::XMVector3Normalize(a.ToSIMD()));
	return result;
}

Float3 Float3::Transform(const Float3& v, const Float3x3& m)
{
	DirectX::XMVECTOR vec = v.ToSIMD();
	vec = DirectX::XMVector3TransformCoord(vec, m.ToSIMD());
	return Float3(vec);
}

Float3 Float3::Transform(const Float3& v, const Float4x4& m)
{
	DirectX::XMVECTOR vec = v.ToSIMD();
	vec = DirectX::XMVector3TransformCoord(vec, m.ToSIMD());
	return Float3(vec);
}

Float3 Float3::TransformDirection(const Float3& v, const Float4x4& m)
{
	DirectX::XMVECTOR vec = v.ToSIMD();
	vec = DirectX::XMVector3TransformNormal(vec, m.ToSIMD());
	return Float3(vec);
}

Float3 Float3::Transform(const Float3& v, const Quaternion& q)
{
	return Float3::Transform(v, q.ToFloat3x3());
}

Float3 Float3::Clamp(const Float3& val, const Float3& min, const Float3& max)
{
	Float3 retVal;
	retVal.x = Framework::Clamp(val.x, min.x, max.x);
	retVal.y = Framework::Clamp(val.y, min.y, max.y);
	retVal.z = Framework::Clamp(val.z, min.z, max.z);
	return retVal;
}

Float3 Float3::Perpendicular(const Float3& vec)
{
	Assert_(vec.Length() >= 0.00001f);

	Float3 perp;

	float x = std::abs(vec.x);
	float y = std::abs(vec.y);
	float z = std::abs(vec.z);
	float minVal = std::min(x, y);
	minVal = std::min(minVal, z);

	if (minVal == x)
		perp = Float3::Cross(vec, Float3(1.0f, 0.0f, 0.0f));
	else if (minVal == y)
		perp = Float3::Cross(vec, Float3(0.0f, 1.0f, 0.0f));
	else
		perp = Float3::Cross(vec, Float3(0.0f, 0.0f, 1.0f));

	return Float3::Normalize(perp);
}

float Float3::Distance(const Float3& a, const Float3& b)
{
	DirectX::XMVECTOR x = a.ToSIMD();
	DirectX::XMVECTOR y = b.ToSIMD();
	DirectX::XMVECTOR length = DirectX::XMVector3Length(DirectX::XMVectorSubtract(x, y));
	return DirectX::XMVectorGetX(length);
}

float Float3::Length(const Float3& v)
{
	DirectX::XMVECTOR x = v.ToSIMD();
	DirectX::XMVECTOR length = DirectX::XMVector3Length(x);
	return DirectX::XMVectorGetX(length);
}

// == Float4 ======================================================================================

Float4::Float4()
{
	x = y = z = w = 0.0f;
}

Float4::Float4(float x_)
{
	x = y = z = w = x_;
}

Float4::Float4(float x_, float y_, float z_, float w_)
{
	x = x_;
	y = y_;
	z = z_;
	w = w_;
}

Float4::Float4(const Float3& xyz, float w_)
{
	x = xyz.x;
	y = xyz.y;
	z = xyz.z;
	w = w_;
}

Float4::Float4(const DirectX::XMFLOAT4& xyzw)
{
	x = xyzw.x;
	y = xyzw.y;
	z = xyzw.z;
	w = xyzw.w;
}

Float4::Float4(DirectX::FXMVECTOR xyzw)
{
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(this), xyzw);
}

Float4& Float4::operator+=(const Float4& other)
{
	x += other.x;
	y += other.y;
	z += other.z;
	w += other.w;
	return *this;
}

Float4 Float4::operator+(const Float4& other) const
{
	Float4 result;
	result.x = x + other.x;
	result.y = y + other.y;
	result.z = z + other.z;
	result.w = w + other.w;
	return result;
}

Float4& Float4::operator-=(const Float4& other)
{
	x -= other.x;
	y -= other.y;
	z -= other.z;
	w -= other.w;
	return *this;
}

Float4 Float4::operator-(const Float4& other) const
{
	Float4 result;
	result.x = x - other.x;
	result.y = y - other.y;
	result.z = z - other.z;
	result.w = w - other.w;
	return result;
}

Float4& Float4::operator*=(const Float4& other)
{
	x *= other.x;
	y *= other.y;
	z *= other.z;
	w *= other.w;
	return *this;
}

Float4 Float4::operator*(const Float4& other) const
{
	Float4 result;
	result.x = x * other.x;
	result.y = y * other.y;
	result.z = z * other.z;
	result.w = w * other.w;
	return result;
}

Float4& Float4::operator/=(const Float4& other)
{
	x /= other.x;
	y /= other.y;
	z /= other.z;
	w /= other.w;
	return *this;
}

Float4 Float4::operator/(const Float4& other) const
{
	Float4 result;
	result.x = x / other.x;
	result.y = y / other.y;
	result.z = z / other.z;
	result.w = w / other.w;
	return result;
}

bool Float4::operator==(const Float4& other) const
{
	return x == other.x && y == other.y && z == other.z && w == other.w;
}

bool Float4::operator!=(const Float4& other) const
{
	return x != other.x || y != other.y || z != other.z || w != other.w;
}


Float4 Float4::operator-() const
{
	Float4 result;
	result.x = -x;
	result.y = -y;
	result.z = -z;
	result.w = -w;

	return result;
}

DirectX::XMVECTOR Float4::ToSIMD() const
{
	return DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(this));
}

Float3 Float4::To3D() const
{
	return Float3(x, y, z);
}

Float2 Float4::To2D() const
{
	return Float2(x, y);
}

Float4 Float4::Clamp(const Float4& val, const Float4& min, const Float4& max)
{
	Float4 retVal;
	retVal.x = Framework::Clamp(val.x, min.x, max.x);
	retVal.y = Framework::Clamp(val.y, min.y, max.y);
	retVal.z = Framework::Clamp(val.z, min.z, max.z);
	retVal.w = Framework::Clamp(val.w, min.w, max.w);
	return retVal;
}

Float4 Float4::Transform(const Float4& v, const Float4x4& m)
{
	DirectX::XMVECTOR vec = v.ToSIMD();
	vec = DirectX::XMVector4Transform(vec, m.ToSIMD());
	return Float4(vec);
}

// == Quaternion ==================================================================================

Quaternion::Quaternion()
{
	*this = Quaternion::Identity();
}

Quaternion::Quaternion(float x_, float y_, float z_, float w_)
{
	x = x_;
	y = y_;
	z = z_;
	w = w_;
}

Quaternion::Quaternion(const Float3& axis, float angle)
{
	*this = Quaternion::FromAxisAngle(axis, angle);
}

Quaternion::Quaternion(const Float3x3& m)
{
	*this = Quaternion(DirectX::XMQuaternionRotationMatrix(m.ToSIMD()));
}

Quaternion::Quaternion(const DirectX::XMFLOAT4& q)
{
	x = q.x;
	y = q.y;
	z = q.z;
	w = q.w;
}

Quaternion::Quaternion(DirectX::FXMVECTOR q)
{
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(this), q);
}

Quaternion& Quaternion::operator*=(const Quaternion& other)
{
	DirectX::XMVECTOR q = ToSIMD();
	q = DirectX::XMQuaternionMultiply(q, other.ToSIMD());
	DirectX::XMStoreFloat4(reinterpret_cast<DirectX::XMFLOAT4*>(this), q);

	return *this;
}

Quaternion Quaternion::operator*(const Quaternion& other) const
{
	Quaternion q = *this;
	q *= other;
	return q;
}

bool Quaternion::operator==(const Quaternion& other) const
{
	return x == other.x && y == other.y && z == other.z && w == other.w;
}

bool Quaternion::operator!=(const Quaternion& other) const
{
	return x != other.x || y != other.y || z != other.z || w != other.w;
}

Float3x3 Quaternion::ToFloat3x3() const
{
	return Float3x3(DirectX::XMMatrixRotationQuaternion(ToSIMD()));
}

Float4x4 Quaternion::ToFloat4x4() const
{
	return Float4x4(DirectX::XMMatrixRotationQuaternion(ToSIMD()));
}

Quaternion Quaternion::Identity()
{
	return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
}

Quaternion Quaternion::Invert(const Quaternion& q)
{
	return Quaternion(DirectX::XMQuaternionInverse(q.ToSIMD()));
}

Quaternion Quaternion::FromAxisAngle(const Float3& axis, float angle)
{
	DirectX::XMVECTOR q = DirectX::XMQuaternionRotationAxis(axis.ToSIMD(), angle);
	return Quaternion(q);
}

Quaternion Quaternion::FromEuler(float x, float y, float z)
{
	DirectX::XMVECTOR q = DirectX::XMQuaternionRotationRollPitchYaw(x, y, z);
	return Quaternion(q);
}

Quaternion Quaternion::Normalize(const Quaternion& q)
{
	return Quaternion(DirectX::XMQuaternionNormalize(q.ToSIMD()));
}

Float3x3 Quaternion::ToFloat3x3(const Quaternion& q)
{
	return q.ToFloat3x3();
}

Float4x4 Quaternion::ToFloat4x4(const Quaternion& q)
{
	return q.ToFloat4x4();
}

DirectX::XMVECTOR Quaternion::ToSIMD() const
{
	return DirectX::XMLoadFloat4(reinterpret_cast<const DirectX::XMFLOAT4*>(this));
}

DirectX::XMFLOAT4 Quaternion::ToXMFLOAT4() const
{
	return DirectX::XMFLOAT4(x, y, z, w);
}

// == Float3x3 ====================================================================================

Float3x3::Float3x3()
{
	_11 = _22 = _33 = 1.0f;
	_12 = _13 = 0.0f;
	_21 = _23 = 0.0f;
	_31 = _32 = 0.0f;
}

Float3x3::Float3x3(const DirectX::XMFLOAT3X3& m)
{
	*reinterpret_cast<DirectX::XMFLOAT3X3*>(this) = m;
}

Float3x3::Float3x3(DirectX::CXMMATRIX m)
{
	DirectX::XMStoreFloat3x3(reinterpret_cast<DirectX::XMFLOAT3X3*>(this), m);
}

Float3x3::Float3x3(const Float3& r0, const Float3& r1, const Float3& r2)
{
	_11 = r0.x;
	_12 = r0.y;
	_13 = r0.z;

	_21 = r1.x;
	_22 = r1.y;
	_23 = r1.z;

	_31 = r2.x;
	_32 = r2.y;
	_33 = r2.z;
}

Float3x3& Float3x3::operator*=(const Float3x3& other)
{
	DirectX::XMMATRIX result = this->ToSIMD() * other.ToSIMD();
	DirectX::XMStoreFloat3x3(reinterpret_cast<DirectX::XMFLOAT3X3*>(this), result);
	return *this;
}

Float3x3 Float3x3::operator*(const Float3x3& other) const
{
	DirectX::XMMATRIX result = this->ToSIMD() * other.ToSIMD();
	return Float3x3(result);
}

Float3 Float3x3::Up() const
{
	return Float3(_21, _22, _23);
}

Float3 Float3x3::Down() const
{
	return Float3(-_21, -_22, -_23);
}

Float3 Float3x3::Left() const
{
	return Float3(-_11, -_12, -_13);
}

Float3 Float3x3::Right() const
{
	return Float3(_11, _12, _13);
}

Float3 Float3x3::Forward() const
{
	return Float3(_31, _32, _33);
}

Float3 Float3x3::Back() const
{
	return Float3(-_31, -_32, -_33);
}

void Float3x3::SetXBasis(const Float3& x)
{
	_11 = x.x;
	_12 = x.y;
	_13 = x.z;
}

void Float3x3::SetYBasis(const Float3& y)
{
	_21 = y.x;
	_22 = y.y;
	_23 = y.z;
}

void Float3x3::SetZBasis(const Float3& z)
{
	_31 = z.x;
	_32 = z.y;
	_33 = z.z;
}

Float3x3 Float3x3::Transpose(const Float3x3& m)
{
	return Float3x3(XMMatrixTranspose(m.ToSIMD()));
}

Float3x3 Float3x3::Invert(const Float3x3& m)
{
	DirectX::XMVECTOR det;
	return Float3x3(XMMatrixInverse(&det, m.ToSIMD()));
}

Float3x3 Float3x3::ScaleMatrix(float s)
{
	Float3x3 m;
	m._11 = m._22 = m._33 = s;
	return m;
}

Float3x3 Float3x3::ScaleMatrix(const Float3& s)
{
	Float3x3 m;
	m._11 = s.x;
	m._22 = s.y;
	m._33 = s.z;
	return m;
}

Float3x3 Float3x3::RotationAxisAngle(const Float3& axis, float angle)
{
	return Float3x3(DirectX::XMMatrixRotationAxis(axis.ToSIMD(), angle));
}

Float3x3 Float3x3::RotationEuler(float x, float y, float z)
{
	return Float3x3(DirectX::XMMatrixRotationRollPitchYaw(x, y, z));
}

DirectX::XMMATRIX Float3x3::ToSIMD() const
{
	return DirectX::XMLoadFloat3x3(reinterpret_cast<const DirectX::XMFLOAT3X3*>(this));
}

// == Float4x4 ====================================================================================

Float4x4::Float4x4()
{
	_11 = _22 = _33 = _44 = 1.0f;
	_12 = _13 = _14 = 0.0f;
	_21 = _23 = _24 = 0.0f;
	_31 = _32 = _34 = 0.0f;
	_41 = _42 = _43 = 0.0f;
}

Float4x4::Float4x4(const DirectX::XMFLOAT4X4& m)
{
	*reinterpret_cast<DirectX::XMFLOAT4X4*>(this) = m;
}

Float4x4::Float4x4(DirectX::CXMMATRIX m)
{
	DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(this), m);
}

Float4x4::Float4x4(const Float4& r0, const Float4& r1, const Float4& r2, const Float4& r3)
{
	_11 = r0.x;
	_12 = r0.y;
	_13 = r0.z;
	_14 = r0.w;

	_21 = r1.x;
	_22 = r1.y;
	_23 = r1.z;
	_24 = r1.w;

	_31 = r2.x;
	_32 = r2.y;
	_33 = r2.z;
	_34 = r2.w;

	_41 = r3.x;
	_42 = r3.y;
	_43 = r3.z;
	_44 = r3.w;
}

Float4x4& Float4x4::operator*=(const Float4x4& other)
{
	DirectX::XMMATRIX result = this->ToSIMD() * other.ToSIMD();
	DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(this), result);
	return *this;
}

Float4x4 Float4x4::operator*(const Float4x4& other) const
{
	DirectX::XMMATRIX result = this->ToSIMD() * other.ToSIMD();
	return Float4x4(result);
}

Float3 Float4x4::Up() const
{
	return Float3(_21, _22, _23);
}

Float3 Float4x4::Down() const
{
	return Float3(-_21, -_22, -_23);
}

Float3 Float4x4::Left() const
{
	return Float3(-_11, -_12, -_13);
}

Float3 Float4x4::Right() const
{
	return Float3(_11, _12, _13);
}

Float3 Float4x4::Forward() const
{
	return Float3(_31, _32, _33);
}

Float3 Float4x4::Back() const
{
	return Float3(-_31, -_32, -_33);
}

Float3 Float4x4::Translation() const
{
	return Float3(_41, _42, _43);
}

void Float4x4::SetTranslation(const Float3& t)
{
	_41 = t.x;
	_42 = t.y;
	_43 = t.z;
}

void Float4x4::SetXBasis(const Float3& x)
{
	_11 = x.x;
	_12 = x.y;
	_13 = x.z;
}

void Float4x4::SetYBasis(const Float3& y)
{
	_21 = y.x;
	_22 = y.y;
	_23 = y.z;
}

void Float4x4::SetZBasis(const Float3& z)
{
	_31 = z.x;
	_32 = z.y;
	_33 = z.z;
}

void Float4x4::Scale(const Float3& scale)
{
	_11 *= scale.x;
	_12 *= scale.x;
	_13 *= scale.x;

	_21 *= scale.y;
	_22 *= scale.y;
	_23 *= scale.y;

	_31 *= scale.z;
	_32 *= scale.z;
	_33 *= scale.z;
}

Float4x4 Float4x4::Transpose(const Float4x4& m)
{
	return Float4x4(XMMatrixTranspose(m.ToSIMD()));
}

Float4x4 Float4x4::Invert(const Float4x4& m)
{
	DirectX::XMVECTOR det;
	return Float4x4(DirectX::XMMatrixInverse(&det, m.ToSIMD()));
}

Float4x4 Float4x4::RotationAxisAngle(const Float3& axis, float angle)
{
	return Float4x4(DirectX::XMMatrixRotationAxis(axis.ToSIMD(), angle));
}

Float4x4 Float4x4::RotationEuler(float x, float y, float z)
{
	return Float4x4(DirectX::XMMatrixRotationRollPitchYaw(x, y, z));
}

Float4x4 Float4x4::ScaleMatrix(float s)
{
	Float4x4 m;
	m._11 = m._22 = m._33 = s;
	return m;
}

Float4x4 Float4x4::ScaleMatrix(const Float3& s)
{
	Float4x4 m;
	m._11 = s.x;
	m._22 = s.y;
	m._33 = s.z;
	return m;
}

Float4x4 Float4x4::TranslationMatrix(const Float3& t)
{
	Float4x4 m;
	m.SetTranslation(t);
	return m;
}

bool Float4x4::operator==(const Float4x4& other) const
{
	const float* ours = reinterpret_cast<const float*>(this);
	const float* theirs = reinterpret_cast<const float*>(&other);
	for (uint64 i = 0; i < 16; ++i)
		if (ours[i] != theirs[i])
			return false;
	return true;
}

bool Float4x4::operator!=(const Float4x4& other) const
{
	const float* ours = reinterpret_cast<const float*>(this);
	const float* theirs = reinterpret_cast<const float*>(&other);
	for (uint64 i = 0; i < 16; ++i)
		if (ours[i] != theirs[i])
			return true;
	return false;
}

DirectX::XMMATRIX Float4x4::ToSIMD() const
{
	return DirectX::XMLoadFloat4x4(reinterpret_cast<const DirectX::XMFLOAT4X4*>(this));
}

Float3x3 Float4x4::To3x3() const
{
	return Float3x3(Right(), Up(), Forward());
}


// == Uint2 =======================================================================================

Uint2::Uint2() : x(0), y(0)
{
}

Uint2::Uint2(uint32 x_, uint32 y_) : x(x_), y(y_)
{
}

bool Uint2::operator==(Uint2 other) const
{
	return x == other.x && y == other.y;
}

bool Uint2::operator!=(Uint2 other) const
{
	return x != other.x || y != other.y;
}

}