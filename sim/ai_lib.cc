#include "sim/ai_lib.h"
#include "sim/ship.h"
#include "sim/ship_class.h"
#include "sim/math_util.h"
#include "glm/gtx/vector_angle.hpp"

using namespace std;
using namespace glm;

namespace Oort {
namespace AILib {

float sqr(float x) {
	return x*x;
}

float smallest_positive_root_of_quadratic_equation(float a, float b, float c) {
	float z = sqrtf(b*b - 4*a*c);
	float x1 = (b + z)/(2*a);
	float x2 = (b - z)/(2*a);
	if (x1 < 0) {
		return x2;
	} else if (x2 < 0) {
		return x1;
	} else {
		return fminf(x1, x2);
	}
}

int accelerated_goto(float p, float v, float a) {
	auto _a = a;
	auto _b = 2*v;
	auto _c = -p + v*v/(2*a);
	auto t = smallest_positive_root_of_quadratic_equation(_a, _b, _c);
	if (t > 0) {
		return 1;
	} else {
		return -1;
	}
}

void turn_to(Ship &s, float angle) {
	auto h = s.get_heading();
	auto w = s.get_angular_velocity();
	auto diff = angle_diff(h, angle);
	auto f = accelerated_goto(diff, -w, s.klass->max_angular_acc);
	s.acc_angular(f * s.klass->max_angular_acc);
}

void drive_towards(Ship &s, vec2 tp, float speed) {
	auto p = s.get_position();
	auto v = s.get_velocity();
	auto h = s.get_heading();
	auto a = radians(orientedAngle(vec2(1,0), normalize(tp-p)));
	auto rv = rotate(v, degrees(-h));
	s.acc_lateral(-1*rv.y);
	turn_to(s, a);

	auto diff = angle_diff(a,h);
	if (rv.x > speed) {
		s.acc_main(speed-rv.x);
	} else if (fabsf(diff) < M_PI/4) {
		s.acc_main(s.klass->max_main_acc);
	} else if (fabsf(diff) > 3*M_PI/4) {
		s.acc_main(-s.klass->max_main_acc);
	} else {
		s.acc_main(0);
	}
}

// Return the angle to shoot a constant-velocity projectile to hit a moving target
float lead(vec2 p1, vec2 p2, vec2 v1, vec2 v2, float w, float t_max) {
	auto dp = p2 - p1;
	auto dv = v2 - v1;
	auto a = (sqr(dv.x) + sqr(dv.y)) - sqr(w);
	auto b = 2 * (dp.x*dv.x + dp.y*dv.y);
	auto c = sqr(dp.x) + sqr(dp.y);
	auto t = smallest_positive_root_of_quadratic_equation(a, b, c);
	if (t >= 0 && t <= t_max) {
		auto p3 = p2 + dv*t;
		return angle_between(p1, p3);
	} else {
		return std::numeric_limits<float>::quiet_NaN();
	}
}

}
}
