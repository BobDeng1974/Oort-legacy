#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <complex.h>
#include <lua.h>
#include <lauxlib.h>

#include "risc.h"
#include "physics.h"
#include "bullet.h"
#include "ship.h"

#define LW_VERBOSE 0

const struct ship_class fighter = {
	.energy_max = 1.0,
	.energy_rate = 0.1,
	.r = 4.0/32.0,
	.hull_max = 1.0,
};

const struct ship_class mothership = {
	.energy_max = 20.0,
	.energy_rate = 1.0,
	.r = 10.0/32.0,
	.hull_max = 30.0,
};

char RKEY_SHIP[1];

GList *all_ships = NULL;

static void lua_registry_set(lua_State *L, void *key, void *value)
{
	lua_pushlightuserdata(L, key);
	lua_pushlightuserdata(L, value);
	lua_settable(L, LUA_REGISTRYINDEX);
}

static void *lua_registry_get(lua_State *L, void *key)
{
	lua_pushlightuserdata(L, key);
	lua_gettable(L, LUA_REGISTRYINDEX);
	void *value = lua_touserdata(L, -1);
	lua_pop(L, 1);
	return value;
}

static struct ship *lua_ship(lua_State *L)
{
	return lua_registry_get(L, RKEY_SHIP);
}

static int api_thrust(lua_State *L)
{
	struct ship *s = lua_ship(L);
	double a = luaL_optnumber(L, 1, 0);
	double f = luaL_optnumber(L, 2, 0);
	s->physics->thrust = f * (cos(a) + sin(a)*I);
	//printf("thrust x=%g y=%g\n", creal(s->physics->thrust), cimag(s->physics->thrust));
	return 0;
}

static int api_yield(lua_State *L)
{
	if (LW_VERBOSE) fprintf(stderr, "api_yield\n");
	return lua_yield(L, 0);
}

static int api_position(lua_State *L)
{
	struct ship *s = lua_ship(L);
	lua_pushnumber(L, creal(s->physics->p));
	lua_pushnumber(L, cimag(s->physics->p));
	return 2;
}

static int api_velocity(lua_State *L)
{
	struct ship *s = lua_ship(L);
	lua_pushnumber(L, creal(s->physics->v));
	lua_pushnumber(L, cimag(s->physics->v));
	return 2;
}

static int api_fire(lua_State *L)
{
	struct ship *s = lua_ship(L);

	if (s->last_shot_tick > ticks - 8) {
		return 0;
	}

	double a = luaL_optnumber(L, 1, 0);
	double v = 10.0;
	struct bullet *b = bullet_create();
	b->team = s->team;
	b->physics->p = s->physics->p;
	b->physics->v = s->physics->v + v * (cos(a) + sin(a)*I);

	s->last_shot_tick = ticks;
	return 0;
}

static int api_sensor_contacts(lua_State *L)
{
	int i = 1;
	lua_newtable(L);
	GList *e;
	for (e = g_list_first(all_ships); e; e = g_list_next(e)) {
		struct ship *s = e->data;
		lua_pushnumber(L, i++);
		lua_newtable(L);

		lua_pushstring(L, "team");
		lua_pushstring(L, s->team->name);
		lua_settable(L, -3);

		lua_pushstring(L, "x"); // index 3
		lua_pushnumber(L, creal(s->physics->p)); // index 4
		lua_settable(L, -3);

		lua_pushstring(L, "y"); // index 3
		lua_pushnumber(L, creal(s->physics->p)); // index 4
		lua_settable(L, -3);

		lua_settable(L, -3);
	}
	return 1;
}

static lua_State *ai_create(char *filename)
{
	lua_State *G, *L;

	G = luaL_newstate();
	luaL_openlibs(G);
	lua_register(G, "thrust", api_thrust);
	lua_register(G, "yield", api_yield);
	lua_register(G, "position", api_position);
	lua_register(G, "velocity", api_velocity);
	lua_register(G, "fire", api_fire);
	lua_register(G, "sensor_contacts", api_sensor_contacts);

	L = lua_newthread(G);

	if (luaL_loadfile(L, filename)) {
		fprintf(stderr, "Couldn't load file %s: %s\n", filename, lua_tostring(L, -1));
		// XXX free
		return NULL;
	}

	return L;
}

static void count_hook(lua_State *L, lua_Debug *ar)
{
	if (LW_VERBOSE) fprintf(stderr, "count hook fired\n");
	lua_yield(L, 0);
}

int ship_ai_run(struct ship *s, int len)
{
	int result;
	double returned;
	lua_State *L = s->lua;

	lua_sethook(L, count_hook, LUA_MASKCOUNT, len);

	result = lua_resume(L, 0);
	if (result == LUA_YIELD) {
		if (LW_VERBOSE) fprintf(stderr, "script yielded\n");
		return 1;
	} else if (result == 0) {
		/* Get the returned value at the top of the stack (index -1) */
		returned = lua_tonumber(L, -1);
		if (LW_VERBOSE) fprintf(stderr, "script returned: %.0f\n", returned);
		lua_pop(L, 1);	/* Take the returned value out of the stack */
		return 0;
	} else {
		fprintf(stderr, "script error: %s\n", lua_tostring(L, -1));
		return 0;
	}
}

void ship_tick_one(struct ship *s, void *unused)
{
	if (ticks % 16 == 0) {
		s->tail[s->tail_head++] = s->physics->p;
		if (s->tail_head == TAIL_SEGMENTS) s->tail_head = 0;
	}

	if (!s->ai_dead) {
		int ret = ship_ai_run(s, 100);
		if (!ret) s->ai_dead = 1;
	}
}

void ship_tick(double t)
{
	g_list_foreach(all_ships, (GFunc)ship_tick_one, NULL);
}

struct ship *ship_create(char *filename, const struct ship_class *class)
{
	struct ship *s = g_slice_new0(struct ship);

	s->lua = ai_create(filename);
	if (!s->lua) {
		fprintf(stderr, "failed to create AI\n");
		return NULL;
	}

	lua_registry_set(s->lua, RKEY_SHIP, s);

	s->class = class;

	s->physics = physics_create();
	s->physics->r = s->class->r;

	s->dead = 0;
	s->ai_dead = 0;
	s->hull = s->class->hull_max;

	int i;
	for (i = 0; i < TAIL_SEGMENTS; i++) {
		s->tail[i] = NAN;
	}

	s->tail_head = 0;

	all_ships = g_list_append(all_ships, s);

	return s;
}

void ship_destroy(struct ship *s)
{
	all_ships = g_list_remove(all_ships, s);
	physics_destroy(s->physics);
	g_slice_free(struct ship, s);
}

void ship_purge(void)
{
	GList *e, *e2;
	for (e = g_list_first(all_ships); e; e = e2) {
		struct ship *s = e->data;
		e2 = g_list_next(e);
		if (s->dead) {
			ship_destroy(s);
		}
	}
}
