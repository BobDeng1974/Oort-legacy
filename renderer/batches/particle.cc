#include "renderer/batches/particle.h"

#include <memory>
#include <array>
#include <stdint.h>
#include <boost/foreach.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include "sim/game.h"
#include "sim/ship.h"
#include "sim/bullet.h"
#include "sim/team.h"
#include "gl/check.h"
#include "common/resources.h"

using glm::vec2;
using glm::vec4;
using std::make_shared;
using std::shared_ptr;

namespace Oort {

enum class ParticleType {
	HIT = 0,
	PLASMA = 1,
	ENGINE = 2,
	EXPLOSION = 3,
};

namespace RendererBatches {

ParticleBatch::ParticleBatch(Renderer &renderer)
	: Batch(renderer),
    prog(GL::Program(
      make_shared<GL::VertexShader>(load_resource("shaders/particle.v.glsl")),
      make_shared<GL::FragmentShader>(load_resource("shaders/particle.f.glsl"))))
{
	create_texture();
}

void ParticleBatch::create_texture() {
	tex.bind();
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	const int n = 256;
	std::array<uint8_t, 3*n*n> data;
	for (int y = 0; y < n; y++) {
		for (int x = 0; x < n; x++) {
			int i = n*y+x;
			vec2 point = vec2(float(x)/n, float(y)/n);
			float dist = glm::length(vec2(0.5f,0.5f) - point);
			float alpha = powf(1-glm::clamp(2*dist, 0.0f, 1.0f), 2);
			data[i] = (uint8_t) (alpha*255);
		}
	}
	glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, n, n, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &data[0]);
	glBindTexture(GL_TEXTURE_2D, 0);
	GL::check();
}

void ParticleBatch::render() {
	GL::check();
	glBlendFunc(GL_ONE, GL_ONE);
	prog.use();
	prog.uniform("p_matrix", renderer.p_matrix);
	prog.uniform("current_time", game.time);
	prog.uniform("view_scale", renderer.view_scale);
	prog.uniform("tex", 0);

	auto stride = sizeof(Particle);

	prog.enable_attrib_array("initial_position");
	prog.enable_attrib_array("velocity");
	prog.enable_attrib_array("initial_time");
	prog.enable_attrib_array("lifetime");
	prog.enable_attrib_array("type");
	tex.bind();

	{
		Particle *p = &particles[0];
		prog.attrib_ptr("initial_position", &p->initial_position, stride);
		prog.attrib_ptr("velocity", &p->velocity, stride);
		prog.attrib_ptr("initial_time", &p->initial_time, stride);
		prog.attrib_ptr("lifetime", &p->lifetime, stride);
		prog.attrib_ptr("type", &p->type, stride);
		glDrawArrays(GL_POINTS, 0, particles.size());
	}

	glBindTexture(GL_TEXTURE_2D, 0);
	prog.disable_attrib_array("initial_position");
	prog.disable_attrib_array("velocity");
	prog.disable_attrib_array("initial_time");
	prog.disable_attrib_array("lifetime");
	prog.disable_attrib_array("type");

	glUseProgram(0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL::check();
}

void ParticleBatch::tick() {
	BOOST_FOREACH(auto bullet, game.bullets) {
		if (bullet->get_def().type == GunType::PLASMA) {
			particles.emplace_back(Particle{
				bullet->get_position(),
				bullet->get_velocity()*0.7f,
				game.time, 0.2,
				(float)ParticleType::PLASMA, 0 });
		}
	}
}

}
}
