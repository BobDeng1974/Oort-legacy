// Copyright 2011 Rich Lane
#include "sim/ai.h"

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/normal_distribution.hpp>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

#include "glm/glm.hpp"
#include "glm/gtx/vector_angle.hpp"
#include <Box2D/Box2D.h>

#include "sim/game.h"
#include "sim/ship.h"
#include "sim/bullet.h"
#include "common/log.h"

namespace Oort {

static boost::random::mt19937 prng(42);
static boost::random::normal_distribution<> a_dist(0.0, 10.0);

AI::AI(Ship *ship, AISourceCode &src)
	: ship(ship) {
	G = lua_open();
}

AI::~AI() {
	if (G) {
		lua_close(G);
	}
}

void AI::tick() {
	ship->body->ApplyForceToCenter(b2Vec2(a_dist(prng), a_dist(prng)));
	auto v = ship->body->GetLinearVelocity();
	auto t = ship->body->GetTransform();
	float h = atan2(v.y, v.x);
	ship->body->SetTransform(t.p, h);

	if (ship->game->ticks % 4 == 0) {
		auto bullet = std::make_shared<Bullet>(ship->game, ship->team);
		auto t = ship->body->GetTransform();
		auto h = t.q.GetAngle();
		auto v = ship->body->GetLinearVelocity();
		v += 30.0f*b2Vec2(cos(h), sin(h));
		auto norm_v = v;
		norm_v.Normalize();
		bullet->body->SetTransform(t.p + norm_v, h);
		bullet->body->SetLinearVelocity(v);
		ship->game->bullets.push_back(bullet);
	}
}

}