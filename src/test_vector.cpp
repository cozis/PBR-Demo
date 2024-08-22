
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "vector.h"

#include <stdio.h>

glm::vec3 vec3toglm(Vector3 v) { return glm::vec3(v.x, v.y, v.z); }
glm::vec4 vec4toglm(Vector4 v) { return glm::vec4(v.x, v.y, v.z, v.w); }

glm::mat4 mat4toglm(Matrix4 m)
{
	glm::mat4 r;
	static_assert(sizeof(m) == sizeof(r));
	memcpy(&r, &m, sizeof(r));
	return r;
}

Matrix4 glmtomat4(glm::mat4 m)
{
	Matrix4 r;
	static_assert(sizeof(m) == sizeof(r));
	memcpy(&r, &m, sizeof(r));
	return r;
}

Vector4 glmtovec4(glm::vec4 v)
{
	Vector4 r;
	static_assert(sizeof(v) == sizeof(r));
	memcpy(&r, &v, sizeof(r));
	return r;
}

#include <stdio.h>

bool floateq(float a, float b)
{
	float eps = 0.01;
	float d = a - b;
	return d < eps && d > -eps;
}

bool vec4eq(Vector4 a, glm::vec4 b)
{
	return floateq(a.x, b.x)
		&& floateq(a.y, b.y)
		&& floateq(a.z, b.z)
		&& floateq(a.w, b.w);
}

bool mat4eq(Matrix4 a, Matrix4 b)
{
	return floateq(a.data[0][0], b.data[0][0])
		&& floateq(a.data[0][1], b.data[0][1])
		&& floateq(a.data[0][2], b.data[0][2])
		&& floateq(a.data[0][3], b.data[0][3])
		&& floateq(a.data[1][0], b.data[1][0])
		&& floateq(a.data[1][1], b.data[1][1])
		&& floateq(a.data[1][2], b.data[1][2])
		&& floateq(a.data[1][3], b.data[1][3])
		&& floateq(a.data[2][0], b.data[2][0])
		&& floateq(a.data[2][1], b.data[2][1])
		&& floateq(a.data[2][2], b.data[2][2])
		&& floateq(a.data[2][3], b.data[2][3])
		&& floateq(a.data[3][0], b.data[3][0])
		&& floateq(a.data[3][1], b.data[3][1])
		&& floateq(a.data[3][2], b.data[3][2])
		&& floateq(a.data[3][3], b.data[3][3]);
}

void test_(Vector4 a, glm::vec4 b, const char *file, int line)
{
	if (!vec4eq(a, b)) {
		printf("TEST FAILED (in %s:%d)\n", file, line);
		abort();
	}
}
#define TEST(a, b) test_(a, b, __FILE__, __LINE__)

Vector3 random_vec3(void)
{
	Vector3 v;
	v.x = rand() % 100;
	v.y = rand() % 100;
	v.z = rand() % 100;
	return v;
}

Vector4 random_vec4(void)
{
	Vector4 v;
	v.x = rand() % 100;
	v.y = rand() % 100;
	v.z = rand() % 100;
	v.w = rand() % 100;
	return v;
}

Matrix4 random_mat4(void)
{
	Matrix4 m;
	m.data[0][0] = rand() % 100;
	m.data[0][1] = rand() % 100;
	m.data[0][2] = rand() % 100;
	m.data[0][3] = rand() % 100;
	m.data[1][0] = rand() % 100;
	m.data[1][1] = rand() % 100;
	m.data[1][2] = rand() % 100;
	m.data[1][3] = rand() % 100;
	m.data[2][0] = rand() % 100;
	m.data[2][1] = rand() % 100;
	m.data[2][2] = rand() % 100;
	m.data[2][3] = rand() % 100;
	m.data[3][0] = rand() % 100;
	m.data[3][1] = rand() % 100;
	m.data[3][2] = rand() % 100;
	m.data[3][3] = rand() % 100;
	return m;
}

int main(void)
{
	for (;;) {

		{
			Matrix4 a = random_mat4();

			glm::mat4 a_ = mat4toglm(a);

			print_matrix(a);
			print_matrix(glmtomat4(a_));

			Matrix4 b;
			bool ok = invert(a, &b);
			if (!ok) {
				printf("Can't invert singular matrix\n");
				abort();
			}

			glm::mat4 b_ = glm::inverse(a_);

			print_matrix(b);
			print_matrix(glmtomat4(b_));

			if (!mat4eq(b, glmtomat4(b_))) {
				printf("invert doesn't work\n");
				abort();
			}
		}

		{
			Matrix4 a = random_mat4();
			Matrix4 b = random_mat4();
			
			glm::mat4 a_ = mat4toglm(a);
			glm::mat4 b_ = mat4toglm(b);

			Matrix4 c = dotm(a, b);
			glm::mat4 c_ = a_ * b_;

			if (!mat4eq(c, glmtomat4(c_))) {
				printf("dotm doesn't work\n");
				abort();
			}
		}

		{
			Matrix4 a = random_mat4();
			Vector4 b = random_vec4();
			
			glm::mat4 a_ = mat4toglm(a);
			glm::vec4 b_ = vec4toglm(b);

			Vector4 c = ldotv(b, a);
			glm::vec4 c_ = b_ * a_;

			Vector4 d = rdotv(a, b);
			glm::vec4 d_ = a_ * b_;

			if (!vec4eq(c, c_)) {
				printf("ldotv doesn't work\n");
				abort();
			}
			if (!vec4eq(d, d_)) {
				printf("rdotv doesn't work\n");
				abort();
			}
		}

		{
			Vector3 eye = random_vec3();
			Vector3 center = random_vec3();
			Vector3 up = random_vec3();

			Matrix4 my_result = lookat_matrix(eye, center, up);

			glm::mat4 glm_result = glm::lookAt(vec3toglm(eye), vec3toglm(center), vec3toglm(up));

			print_matrix(my_result);
			print_matrix(glmtomat4(glm_result));

			if (!mat4eq(my_result, glmtomat4(glm_result))) {
				printf("lookat doesn't work\n");
				abort();
			}
		}

		{
			float left = rand() % 100;
			float right = rand() % 100;
			float bottom = rand() % 100;
			float top = rand() % 100;
			float near = rand() % 100;
			float far = rand() % 100;

			if (abs(far - near) < 0.1)
				far = near + 1;
			if (abs(left - right) < 0.1)
				left = right + 1;
			if (abs(top - bottom) < 0.1)
				top = bottom + 1;

			Matrix4 my_result = ortho_matrix(left, right, bottom, top, near, far);
			glm::mat4 glm_result = glm::ortho(left, right, bottom, top, near, far);

			print_matrix(my_result);
			print_matrix(glmtomat4(glm_result));

			if (!mat4eq(my_result, glmtomat4(glm_result))) {
				printf("ortho doesn't work\n");
				abort();
			}
		}

		{
			Vector3 translate_vector;
			translate_vector.x = rand() % 100;
			translate_vector.y = rand() % 100;
			translate_vector.z = rand() % 100;

			Vector4 starting_vector;
			starting_vector.x = rand() % 100;
			starting_vector.y = rand() % 100;
			starting_vector.z = rand() % 100;
			starting_vector.w = rand() % 100;

			Matrix4 my_m = translate_matrix(translate_vector, 1);
			Vector4 my_final_vector = rdotv(my_m, starting_vector);

			glm::mat4 glm_m = glm::translate(glm::mat4(1), vec3toglm(translate_vector));
			glm::vec4 expected_final_vector = glm_m * vec4toglm(starting_vector);

			TEST(my_final_vector, expected_final_vector);
		}

		{
			Vector3 scale_vector;
			scale_vector.x = rand() % 100;
			scale_vector.y = rand() % 100;
			scale_vector.z = rand() % 100;

			Vector4 starting_vector;
			starting_vector.x = rand() % 100;
			starting_vector.y = rand() % 100;
			starting_vector.z = rand() % 100;
			starting_vector.w = rand() % 100;

			Matrix4 my_m = scale_matrix(scale_vector);
			Vector4 my_final_vector = rdotv(my_m, starting_vector);

			glm::mat4 glm_m(1);
			glm_m = glm::scale(glm_m, vec3toglm(scale_vector));
			glm::vec4 expected_final_vector = glm_m * vec4toglm(starting_vector);

			TEST(my_final_vector, expected_final_vector);
		}

		float angle = 3.14 * (float) (rand() % 5) / 4;

		Vector4 starting_vector;
		starting_vector.x = rand() % 100;
		starting_vector.y = rand() % 100;
		starting_vector.z = rand() % 100;
		starting_vector.w = rand() % 100;

		// --- rotation x --- //
		{
			Matrix4 my_m = rotate_matrix_x(angle);
			Vector4 my_final_vector = rdotv(my_m, starting_vector);

			glm::mat4 glm_m(1);
			glm_m = glm::rotate(glm_m, angle, glm::vec3(1, 0, 0));
			glm::vec4 expected_final_vector = glm_m * vec4toglm(starting_vector);

			TEST(my_final_vector, expected_final_vector);
		}

		// --- rotation y --- //
		{
			Matrix4 my_m = rotate_matrix_y(angle);
			Vector4 my_final_vector = rdotv(my_m, starting_vector);

			glm::mat4 glm_m(1);
			glm_m = glm::rotate(glm_m, angle, glm::vec3(0, 1, 0));
			glm::vec4 expected_final_vector = glm_m * vec4toglm(starting_vector);

			TEST(my_final_vector, expected_final_vector);
		}

		// --- rotation z --- //
		{
			Matrix4 my_m = rotate_matrix_z(angle);
			Vector4 my_final_vector = rdotv(my_m, starting_vector);

			glm::mat4 glm_m(1);
			glm_m = glm::rotate(glm_m, angle, glm::vec3(0, 0, 1));
			glm::vec4 expected_final_vector = glm_m * vec4toglm(starting_vector);

			TEST(my_final_vector, expected_final_vector);
		}
	}

	return 0;
}
