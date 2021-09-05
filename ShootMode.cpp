#include "ShootMode.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>


#include <random>
#include <chrono>

ShootMode::ShootMode() {

	//set up trail as if ball has been here for 'forever':
	ball_trail.clear();
	ball_trail.emplace_back(ball, trail_length);
	ball_trail.emplace_back(ball, 0.0f);


	//----- allocate OpenGL resources -----
	{ //vertex buffer:
		glGenBuffers(1, &vertex_buffer);
		//for now, buffer will be un-filled.

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //vertex array mapping buffer for color_texture_program:
		//ask OpenGL to fill vertex_buffer_for_color_texture_program with the name of an unused vertex array object:
		glGenVertexArrays(1, &vertex_buffer_for_color_texture_program);

		//set vertex_buffer_for_color_texture_program as the current vertex array object:
		glBindVertexArray(vertex_buffer_for_color_texture_program);

		//set vertex_buffer as the source of glVertexAttribPointer() commands:
		glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);

		//set up the vertex array object to describe arrays of ShootMode::Vertex:
		glVertexAttribPointer(
			color_texture_program.Position_vec4, //attribute
			3, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte*)0 + 0 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Position_vec4);
		//[Note that it is okay to bind a vec3 input to a vec4 attribute -- the w component will be filled with 1.0 automatically]

		glVertexAttribPointer(
			color_texture_program.Color_vec4, //attribute
			4, //size
			GL_UNSIGNED_BYTE, //type
			GL_TRUE, //normalized
			sizeof(Vertex), //stride
			(GLbyte*)0 + 4 * 3 //offset
		);
		glEnableVertexAttribArray(color_texture_program.Color_vec4);

		glVertexAttribPointer(
			color_texture_program.TexCoord_vec2, //attribute
			2, //size
			GL_FLOAT, //type
			GL_FALSE, //normalized
			sizeof(Vertex), //stride
			(GLbyte*)0 + 4 * 3 + 4 * 1 //offset
		);
		glEnableVertexAttribArray(color_texture_program.TexCoord_vec2);

		//done referring to vertex_buffer, so unbind it:
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		//done setting up vertex array object, so unbind it:
		glBindVertexArray(0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}

	{ //solid white texture:
		//ask OpenGL to fill white_tex with the name of an unused texture object:
		glGenTextures(1, &white_tex);

		//bind that texture object as a GL_TEXTURE_2D-type texture:
		glBindTexture(GL_TEXTURE_2D, white_tex);

		//upload a 1x1 image of solid white to the texture:
		glm::uvec2 size = glm::uvec2(1, 1);
		std::vector< glm::u8vec4 > data(size.x * size.y, glm::u8vec4(0xff, 0xff, 0xff, 0xff));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data.data());

		//set filtering and wrapping parameters:
		//(it's a bit silly to mipmap a 1x1 texture, but I'm doing it because you may want to use this code to load different sizes of texture)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		//since texture uses a mipmap and we haven't uploaded one, instruct opengl to make one for us:
		glGenerateMipmap(GL_TEXTURE_2D);

		//Okay, texture uploaded, can unbind it:
		glBindTexture(GL_TEXTURE_2D, 0);

		GL_ERRORS(); //PARANOIA: print out any OpenGL errors that may have happened
	}
}

ShootMode::~ShootMode() {

	//----- free OpenGL resources -----
	glDeleteBuffers(1, &vertex_buffer);
	vertex_buffer = 0;

	glDeleteVertexArrays(1, &vertex_buffer_for_color_texture_program);
	vertex_buffer_for_color_texture_program = 0;

	glDeleteTextures(1, &white_tex);
	white_tex = 0;
}

glm::mat4 ShootMode::get_rotation_matrix(glm::vec2 const& center, float const& angle)
{
	glm::mat4 rotate_around_origin_mat = glm::mat4(1.0f);
	rotate_around_origin_mat = glm::rotate(rotate_around_origin_mat, angle, glm::vec3(0.0, 0.0, 1.0));

	glm::vec2 rotation_center_2d = center;

	glm::mat4 translate_to_origin_mat = glm::mat4(1.0f);
	translate_to_origin_mat = glm::translate(translate_to_origin_mat, glm::vec3(-rotation_center_2d.x, -rotation_center_2d.y, 0.0f));

	glm::mat4 translate_to_center_mat = glm::mat4(1.0f);
	translate_to_center_mat = glm::translate(translate_to_center_mat, glm::vec3(rotation_center_2d.x, rotation_center_2d.y, 0.0f));

	return translate_to_center_mat * rotate_around_origin_mat * translate_to_origin_mat;
}

bool ShootMode::handle_event(SDL_Event const& evt, glm::uvec2 const& window_size) {

	if (evt.type == SDL_MOUSEMOTION) {
		//convert mouse from window pixels (top-left origin, +y is down) to clip space ([-1,1]x[-1,1], +y is up):
		glm::vec2 clip_mouse = glm::vec2(
			(evt.motion.x + 0.5f) / window_size.x * 2.0f - 1.0f,
			(evt.motion.y + 0.5f) / window_size.y * -2.0f + 1.0f
		);
		//if (pause_flag == 0) left_paddle.y = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y;
		if (pause_flag == 0) {
			float dx = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).x - cannon_base.x;
			float dy = (clip_to_court * glm::vec3(clip_mouse, 1.0f)).y - cannon_base.y;
			float clip_to_cannon_base_angle = glm::atan(dy/dx);
			if(dx > FLT_EPSILON)cannon_angle = clip_to_cannon_base_angle > 0 ? 
						   std::min(clip_to_cannon_base_angle, max_cannon_elevation):
						   std::max(clip_to_cannon_base_angle, -max_cannon_elevation);
		}
	}
	else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_ESCAPE) {
		std::cout << "Pause here " << std::endl;
		if(game_flag == 0)pause_flag = pause_flag == 0 ? 1 : 0;
		return true;
	}
	else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_q) {
		if(game_flag == 0 && pause_flag == 1) Mode::set_current(nullptr);
	}
	else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_y) {
		if (game_flag != 0 || pause_flag == 1) {
			std::cout << "restart game" << std::endl;
			left_score = 0;
			left_health = max_health;
			pause_flag = 0;
			shoot_flag = 0;
			game_flag = 0;
			left_paddle = glm::vec2(0.0f, 0.0f);
			right_paddle = glm::vec2(court_radius.x - 0.5f, 0.0f);
			ball = glm::vec2(-court_radius.x + ball_radius[0] + FLT_EPSILON, 0.0f);
			cannon_angle = 0;
		}
	}
	else if (evt.type == SDL_KEYDOWN && evt.key.keysym.sym == SDLK_n) {
		if (game_flag != 0)Mode::set_current(nullptr);
	}
	else if (evt.type == SDL_MOUSEBUTTONDOWN && evt.button.button == SDL_BUTTON_LEFT) {
		if (shoot_flag == 0 && pause_flag == 0) {
			shoot_flag = 1;
			ball.x = -court_radius.x + barrel_offset + cannon_barrel_length[0] - ball_radius[0];
			ball.y = 0.0f;
			glm::vec4 reset_ball = glm::vec4(ball,0.0f,1.0f);
			reset_ball = get_rotation_matrix(cannon_barrel, cannon_angle) * reset_ball;
			ball.x = reset_ball.x;
			ball.y = reset_ball.y;
			ball_velocity.x = 20.0f * cos(cannon_angle);
			ball_velocity.y = 20.0f * sin(cannon_angle);
		}
	}

	return false;
}

void ShootMode::update(float elapsed) {

	// if pause, do not update
	if (pause_flag) return;

	unsigned seed = (unsigned)std::chrono::system_clock::now().time_since_epoch().count();
	static std::mt19937 mt(seed); //mersenne twister pseudo-random number generator
	//helper function to reset ball
	auto reset_ball = [&]() {
		shoot_flag = 0;
		ball.x = -court_radius.x + ball_radius[0] + FLT_EPSILON;
		ball.y = 0.0f;
		ball_velocity.x = 0.0f;
		ball_velocity.y = 0.0f;
	};

	//----- paddle update -----
	//paddle ai:
	auto paddle_move = [&](glm::vec2& paddle, float& ai_offset_update, float& ai_offset){
		//speed of paddle doubles every four points:
		float speed_multiplier = 2.0f + 1.0f * left_score / 2.0f;
		//velocity cap
		speed_multiplier = std::min(speed_multiplier, 10.0f);

		ai_offset_update -= elapsed;
		if (ai_offset_update < elapsed) {
			//update again in [2.0,3.5) seconds:
			ai_offset_update = (mt() / float(mt.max())) * 1.5f + 2.0f;
			ai_offset = (mt() / float(mt.max())) * 2.5f - 1.25f;
			std::cout << ai_offset << std::endl;

		}
		if ((ai_offset > 0 && (court_radius.y - paddle_radius.y - paddle.y) < 0.3f) ||
			(ai_offset < 0 && (-court_radius.y + paddle_radius.y - paddle.y) > -0.3f)) {
			ai_offset = -ai_offset;
		}
		paddle.y = paddle.y + speed_multiplier * elapsed * ai_offset;
	};

	paddle_move(left_paddle, left_ai_offset_update, left_ai_offset);
	paddle_move(right_paddle, right_ai_offset_update, right_ai_offset);

	//clamp paddles to court:
	right_paddle.y = std::max(right_paddle.y, -court_radius.y + paddle_radius.y);
	right_paddle.y = std::min(right_paddle.y, court_radius.y - paddle_radius.y);

	left_paddle.y = std::max(left_paddle.y, -court_radius.y + paddle_radius.y);
	left_paddle.y = std::min(left_paddle.y, court_radius.y - paddle_radius.y);

	//----- ball update -----

	ball += elapsed * ball_velocity;

	//---- collision handling ----

	//paddles:
	auto paddle_vs_ball = [this,&reset_ball](glm::vec2 const& paddle, bool obstacle) {
		//compute area of overlap:
		glm::vec2 min = glm::max(paddle - paddle_radius, ball - ball_radius);
		glm::vec2 max = glm::min(paddle + paddle_radius, ball + ball_radius);

		//if no overlap, no collision:
		if (min.x > max.x || min.y > max.y) return;
		if (obstacle) {
			reset_ball();
			left_health -= 1;
		}
		else {
			reset_ball();
			left_score += 1;
		}
	
	};
	paddle_vs_ball(left_paddle, true);
	paddle_vs_ball(right_paddle,false);

	//court walls:

	//upper wall
	if (ball.y > court_radius.y - ball_radius.y) {
		ball.y = court_radius.y - ball_radius.y;
		if (ball_velocity.y > 0.0f) {
			ball_velocity.y = -ball_velocity.y;
		}
	}
	//lower wall
	if (ball.y < -court_radius.y + ball_radius.y) {
		ball.y = -court_radius.y + ball_radius.y;
		if (ball_velocity.y < 0.0f) {
			ball_velocity.y = -ball_velocity.y;
		}
	}
	//right wall
	if (ball.x >= court_radius.x - ball_radius.x) {
		if (ball_velocity.x > FLT_EPSILON) {
			reset_ball();
			left_health -= 1;
		}
	}
	//left wall
	if (ball.x <= -court_radius.x + ball_radius.x) {
		if (ball_velocity.x < -FLT_EPSILON) {
			reset_ball();
			left_health -= 1;
		}
	}
	
	//game status change
	if (left_health <= 0) {
		pause_flag = 1;
		game_flag = 1;

	}
	if (left_score >= max_score) {
		pause_flag = 1;
		game_flag = 2;
	}

	//----- gradient trails -----

	//age up all locations in ball trail:
	for (auto& t : ball_trail) {
		t.z += elapsed;
	}
	//store fresh location at back of ball trail:
	ball_trail.emplace_back(ball, 0.0f);

	//trim any too-old locations from back of trail:
	//NOTE: since trail drawing interpolates between points, only removes back element if second-to-back element is too old:
	while (ball_trail.size() >= 2 && ball_trail[1].z > trail_length) {
		ball_trail.pop_front();
	}
}

void ShootMode::draw(glm::uvec2 const& drawable_size) {
	//some nice colors from the course web page:
#define HEX_TO_U8VEC4( HX ) (glm::u8vec4( (HX >> 24) & 0xff, (HX >> 16) & 0xff, (HX >> 8) & 0xff, (HX) & 0xff ))
	const glm::u8vec4 bg_color = HEX_TO_U8VEC4(0x193b59ff);
	const glm::u8vec4 fg_color = HEX_TO_U8VEC4(0xf2d2b6ff);
	const glm::u8vec4 health_color = HEX_TO_U8VEC4(0xf50303ff);
	const glm::u8vec4 shadow_color = HEX_TO_U8VEC4(0xf2ad94ff);
	const glm::u8vec4 health_shadow = HEX_TO_U8VEC4(0x920a1eff);
	const std::vector< glm::u8vec4 > trail_colors = {
		HEX_TO_U8VEC4(0xf2ad9488),
		HEX_TO_U8VEC4(0xf2897288),
		HEX_TO_U8VEC4(0xbacac088),
	};
#undef HEX_TO_U8VEC4

	//other useful drawing constants:
	const float wall_radius = 0.05f;
	const float shadow_offset = 0.07f;
	const float padding = 0.14f; //padding between outside of walls and edge of window

	//---- compute vertices to draw ----

	//vertices will be accumulated into this list and then uploaded+drawn at the end of this function:
	std::vector< Vertex > vertices;

	//inline helper function for rectangle drawing:
	auto draw_rectangle = [&vertices](glm::vec2 const& center, glm::vec2 const& radius, glm::u8vec4 const& color) {
		//draw rectangle as two CCW-oriented triangles:
		vertices.emplace_back(glm::vec3(center.x - radius.x, center.y - radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x + radius.x, center.y - radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x + radius.x, center.y + radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(center.x - radius.x, center.y - radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x + radius.x, center.y + radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(center.x - radius.x, center.y + radius.y, 0.0f), color, glm::vec2(0.5f, 0.5f));
	};

	auto draw_circle = [&vertices](glm::vec2 const& center, float const& radius, glm::u8vec4 const& color) {
		auto degree = [&](int a) {return double(a) * M_PI / 180.0; };
		for (int a = 0; a < 360; a += 1) {
			vertices.emplace_back(glm::vec3(center, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a)), center.y + radius * sin(degree(a)), 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a + 1)), center.y + radius * sin(degree(a + 1)), 0.0f), color, glm::vec2(0.5f, 0.5f));
		}
	};

	//draw the base of cannon
	auto draw_cannon_base = [&vertices](glm::vec2 const& center, float const& radius, glm::u8vec4 const& color) {
		auto degree = [&](int a) {return double(a) * M_PI / 180.0; };
		for (int a = 0; a < 90; a += 1) {
			vertices.emplace_back(glm::vec3(center, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a)), center.y + radius * sin(degree(a)), 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a+1)), center.y + radius * sin(degree(a+1)), 0.0f), color, glm::vec2(0.5f, 0.5f));
		}
		for (int a = 0; a < 90; a += 1) {
			vertices.emplace_back(glm::vec3(center, 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a)), center.y - radius * sin(degree(a)), 0.0f), color, glm::vec2(0.5f, 0.5f));
			vertices.emplace_back(glm::vec3(center.x + radius * cos(degree(a+1)), center.y - radius * sin(degree(a+1)), 0.0f), color, glm::vec2(0.5f, 0.5f));
		}
	};

	//draw cannon barrel 
	//inspired by https://github.com/bobowitz/15-666-boat-game
	auto draw_cannon_barrel = [&](glm::vec2 const& center, glm::vec2 const& length, glm::vec2 const& radius, float const& angle, glm::u8vec4 const& color) {
		
		//integrate the transform matrices
		glm::mat4 rotate_around_center_mat = get_rotation_matrix(center,angle);
		
		//draw the rotated barrel
		glm::vec4 bot_left = rotate_around_center_mat * glm::vec4(center.x + barrel_offset, -radius[0], 0.0f, 1.0f);
		glm::vec4 bot_right = rotate_around_center_mat * glm::vec4(center.x + barrel_offset + length[0], -radius[0], 0.0f, 1.0f);
		glm::vec4 top_right = rotate_around_center_mat * glm::vec4(center.x + barrel_offset + length[0], radius[0], 0.0f, 1.0f);
		glm::vec4 top_left = rotate_around_center_mat * glm::vec4(center.x + barrel_offset, radius[0], 0.0f, 1.0f);

		vertices.emplace_back(glm::vec3(bot_left), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(bot_right), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(top_right), color, glm::vec2(0.5f, 0.5f));

		vertices.emplace_back(glm::vec3(bot_left), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(top_right), color, glm::vec2(0.5f, 0.5f));
		vertices.emplace_back(glm::vec3(top_left), color, glm::vec2(0.5f, 0.5f));
	};

	//shadows for everything (except the trail):

	glm::vec2 s = glm::vec2(0.0f, -shadow_offset);

	draw_rectangle(glm::vec2(-court_radius.x - wall_radius, 0.0f) + s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2(court_radius.x + wall_radius, 0.0f) + s, glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), shadow_color);
	draw_rectangle(glm::vec2(0.0f, -court_radius.y - wall_radius) + s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(glm::vec2(0.0f, court_radius.y + wall_radius) + s, glm::vec2(court_radius.x, wall_radius), shadow_color);
	draw_rectangle(left_paddle + s, paddle_radius, shadow_color);
	draw_rectangle(right_paddle + s, paddle_radius, health_shadow);
	draw_circle(ball + s, ball_radius[0], shadow_color);

	//ball's trail:
	if (ball_trail.size() >= 2) {
		//start ti at second element so there is always something before it to interpolate from:
		std::deque< glm::vec3 >::iterator ti = ball_trail.begin() + 1;
		//draw trail from oldest-to-newest:
		constexpr uint32_t STEPS = 20;
		//draw from [STEPS, ..., 1]:
		for (uint32_t step = STEPS; step > 0; --step) {
			//time at which to draw the trail element:
			float t = step / float(STEPS) * trail_length;
			//advance ti until 'just before' t:
			while (ti != ball_trail.end() && ti->z > t) ++ti;
			//if we ran out of recorded tail, stop drawing:
			if (ti == ball_trail.end()) break;
			//interpolate between previous and current trail point to the correct time:
			glm::vec3 a = *(ti - 1);
			glm::vec3 b = *(ti);
			glm::vec2 at = (t - a.z) / (b.z - a.z) * (glm::vec2(b) - glm::vec2(a)) + glm::vec2(a);

			//look up color using linear interpolation:
			//compute (continuous) index:
			float c = (step - 1) / float(STEPS - 1) * trail_colors.size();
			//split into an integer and fractional portion:
			int32_t ci = int32_t(std::floor(c));
			float cf = c - ci;
			//clamp to allowable range (shouldn't ever be needed but good to think about for general interpolation):
			if (ci < 0) {
				ci = 0;
				cf = 0.0f;
			}
			if (ci > int32_t(trail_colors.size()) - 2) {
				ci = int32_t(trail_colors.size()) - 2;
				cf = 1.0f;
			}
			//do the interpolation (casting to floating point vectors because glm::mix doesn't have an overload for u8 vectors):
			glm::u8vec4 color = glm::u8vec4(
				glm::mix(glm::vec4(trail_colors[ci]), glm::vec4(trail_colors[ci + 1]), cf)
			);

			//draw:
			draw_circle(at, ball_radius[0], color);
		}
	}

	//solid objects:

	//walls:
	draw_rectangle(glm::vec2(-court_radius.x - wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2(court_radius.x + wall_radius, 0.0f), glm::vec2(wall_radius, court_radius.y + 2.0f * wall_radius), fg_color);
	draw_rectangle(glm::vec2(0.0f, -court_radius.y - wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);
	draw_rectangle(glm::vec2(0.0f, court_radius.y + wall_radius), glm::vec2(court_radius.x, wall_radius), fg_color);

	//paddles:
	draw_rectangle(left_paddle, paddle_radius, fg_color);
	draw_rectangle(right_paddle, paddle_radius, health_color);

	//cannon base:
	draw_cannon_base(cannon_base, cannon_base_radius[0], fg_color);
	draw_cannon_barrel(cannon_barrel, cannon_barrel_length, cannon_barrel_radius,cannon_angle, fg_color);


	//ball:
	draw_circle(ball, ball_radius[0], fg_color);

	//scores:
	glm::vec2 score_radius = glm::vec2(0.1f, 0.1f);
	{
		uint32_t i = 0;
		for (; i < max_health; ++i) {
			if(i < left_health)draw_rectangle(glm::vec2(-court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, health_color);
		}
		for (; i < left_score + max_health; ++i) {
			draw_rectangle(glm::vec2(-court_radius.x + (2.0f + 3.0f * i) * score_radius.x, court_radius.y + 2.0f * wall_radius + 2.0f * score_radius.y), score_radius, fg_color);
		}
	}



	//------ compute court-to-window transform ------

	//compute area that should be visible:
	glm::vec2 scene_min = glm::vec2(
		-court_radius.x - 2.0f * wall_radius - padding,
		-court_radius.y - 2.0f * wall_radius - padding
	);
	glm::vec2 scene_max = glm::vec2(
		court_radius.x + 2.0f * wall_radius + padding,
		court_radius.y + 2.0f * wall_radius + 3.0f * score_radius.y + padding
	);

	//compute window aspect ratio:
	float aspect = drawable_size.x / float(drawable_size.y);
	//we'll scale the x coordinate by 1.0 / aspect to make sure things stay square.

	//compute scale factor for court given that...
	float scale = std::min(
		(2.0f * aspect) / (scene_max.x - scene_min.x), //... x must fit in [-aspect,aspect] ...
		(2.0f) / (scene_max.y - scene_min.y) //... y must fit in [-1,1].
	);

	glm::vec2 center = 0.5f * (scene_max + scene_min);

	//build matrix that scales and translates appropriately:
	glm::mat4 court_to_clip = glm::mat4(
		glm::vec4(scale / aspect, 0.0f, 0.0f, 0.0f),
		glm::vec4(0.0f, scale, 0.0f, 0.0f),
		glm::vec4(0.0f, 0.0f, 1.0f, 0.0f),
		glm::vec4(-center.x * (scale / aspect), -center.y * scale, 0.0f, 1.0f)
	);
	//NOTE: glm matrices are specified in *Column-Major* order,
	// so each line above is specifying a *column* of the matrix(!)

	//also build the matrix that takes clip coordinates to court coordinates (used for mouse handling):
	clip_to_court = glm::mat3x2(
		glm::vec2(aspect / scale, 0.0f),
		glm::vec2(0.0f, 1.0f / scale),
		glm::vec2(center.x, center.y)
	);

	//---- actual drawing ----

	//clear the color buffer:
	glClearColor(bg_color.r / 255.0f, bg_color.g / 255.0f, bg_color.b / 255.0f, bg_color.a / 255.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	//use alpha blending:
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	//don't use the depth test:
	glDisable(GL_DEPTH_TEST);

	//upload vertices to vertex_buffer:
	glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer); //set vertex_buffer as current
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STREAM_DRAW); //upload vertices array
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	//set color_texture_program as current program:
	glUseProgram(color_texture_program.program);

	//upload OBJECT_TO_CLIP to the proper uniform location:
	glUniformMatrix4fv(color_texture_program.OBJECT_TO_CLIP_mat4, 1, GL_FALSE, glm::value_ptr(court_to_clip));

	//use the mapping vertex_buffer_for_color_texture_program to fetch vertex data:
	glBindVertexArray(vertex_buffer_for_color_texture_program);

	//bind the solid white texture to location zero so things will be drawn just with their colors:
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, white_tex);

	//run the OpenGL pipeline:
	glDrawArrays(GL_TRIANGLES, 0, GLsizei(vertices.size()));

	//unbind the solid white texture:
	glBindTexture(GL_TEXTURE_2D, 0);

	//reset vertex array to none:
	glBindVertexArray(0);

	//reset current program to none:
	glUseProgram(0);


	GL_ERRORS(); //PARANOIA: print errors just in case we did something wrong.

}


