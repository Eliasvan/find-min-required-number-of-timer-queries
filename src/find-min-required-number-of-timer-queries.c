#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <GL/freeglut.h>
#include <GL/glext.h>

#define TIMERQUERIES_NUM_MAX 255
#define TIMERQUERIES_LOG_TIME 0

static PFNGLGETSTRINGIPROC glGetStringi = NULL;
static PFNGLGETQUERYIVPROC glGetQueryiv = NULL;
static PFNGLGENQUERIESPROC glGenQueries = NULL;
static PFNGLQUERYCOUNTERPROC glQueryCounter = NULL;
static PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv = NULL;
static PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v = NULL;
static GLint timerqueries_numbits = 0;
static GLuint timerqueries[TIMERQUERIES_NUM_MAX];
static uint8_t timerqueries_num = 2, idxinit = 0, idxback = 0, idxfront = 1;
static uint64_t nrframes = 0;

static void init()
{
	// Identify this GPU device along with its driver.
	printf("GL_RENDERER           = %s\n", (char *) glGetString(GL_RENDERER));
	printf("GL_VERSION            = %s\n", (char *) glGetString(GL_VERSION));
	printf("GL_VENDOR             = %s\n", (char *) glGetString(GL_VENDOR));

#	define INIT_GLEXT_AND_EXIT_ON_NULL(function_type, function_name) \
	do { \
		function_name = (function_type)glutGetProcAddress(#function_name); \
		if (!function_name) { \
			printf("Error: function pointer to %s is NULL\n", #function_name); \
			exit(1); \
		} \
	} while (0)

	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLGETSTRINGIPROC, glGetStringi);

	const GLubyte *extension;
	int i = 0;
	while ((extension = glGetStringi(GL_EXTENSIONS, i++)) && strcmp((const char *)extension, "GL_ARB_timer_query"));
	if (!extension) {
		printf("Error: extension GL_ARB_timer_query is not available\n");
		exit(1);
	}

	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLGETQUERYIVPROC, glGetQueryiv);
	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLGENQUERIESPROC, glGenQueries);
	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLQUERYCOUNTERPROC, glQueryCounter);
	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLGETQUERYOBJECTIVPROC, glGetQueryObjectiv);
	INIT_GLEXT_AND_EXIT_ON_NULL(PFNGLGETQUERYOBJECTUI64VPROC, glGetQueryObjectui64v);

#	undef INIT_GLEXT_AND_EXIT_ON_NULL

	glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &timerqueries_numbits);
	printf("GL_QUERY_COUNTER_BITS = %d\n", timerqueries_numbits);

	glGenQueries(timerqueries_num, timerqueries);
	printf("\nInitial number of timer queries: %d\n", timerqueries_num);
}

static void write_and_read_timerqueries() {
	// Schedule a record of the GPU timestamp after the GPU commands submitted for this frame (i.e. at idx back).
	glQueryCounter(timerqueries[idxback], GL_TIMESTAMP);
	++nrframes;

	if (idxinit < timerqueries_num - 1) {
		// Don't query any result yet, since not all timer queries are scheduled yet.
		++idxinit;
	} else {
		idxinit = timerqueries_num;

		// Verify assumption "result is available"; if not true, querying the result would stall the pipeline.
		GLint available = 0;
		glGetQueryObjectiv(timerqueries[idxfront], GL_QUERY_RESULT_AVAILABLE, &available);
		const uint64_t frame_at_idxfront = nrframes - timerqueries_num;

		if (available) {
			// Query the oldest result (i.e. at idx front).
			GLuint64 newtime;
			glGetQueryObjectui64v(timerqueries[idxfront], GL_QUERY_RESULT, &newtime);

#			if TIMERQUERIES_LOG_TIME
			static bool firstdone = false;
			static GLuint64 totaltime = 0;
			static GLuint64 oldtime;
			if (firstdone) {
				// Calculate delta and consider timer query bits.
				const GLuint64 nsec = (newtime - oldtime) &
						(timerqueries_numbits < 64 ? ((GLuint64) 1 << timerqueries_numbits) - 1 : (GLuint64) -1);
				printf("\tFrametime (frame %" PRIu64 ") %" PRIu64 " ns\n", frame_at_idxfront, nsec);

				totaltime += nsec;
				printf("\tFrametime (frame %" PRIu64 ") cumsum %" PRIu64 " ns\n", frame_at_idxfront, totaltime);
			}
			firstdone = true;
			oldtime = newtime;
#			endif
		} else {
			// We'll need to increment the number of timer queries to prevent stalls.
			if (timerqueries_num >= TIMERQUERIES_NUM_MAX) {
				printf("Error: buffer resize needed at frame %" PRIu64 ", but would exceed maximum allowed number of timer queries (%d).\n",
						frame_at_idxfront, TIMERQUERIES_NUM_MAX);
				exit(1);
			}

			// Make an 'empty hole' at the next idx back, by moving the elements to the right one position right.
			for (uint8_t idx = timerqueries_num - 1; idxback < idx; --idx) {
				timerqueries[idx + 1] = timerqueries[idx];
			}
			++timerqueries_num;

			// Generate a timer query in the 'empty hole'.
			glGenQueries(1, &timerqueries[idxback + 1]);
			printf("Buffer resize was needed at frame %" PRIu64 ", updated number of timer queries: %d\n",
					frame_at_idxfront, timerqueries_num);
		}
	}

	// 'Swap'/rotate idx pointers to timer queries.
	idxback = (idxback + 1) % timerqueries_num;
	idxfront = (idxback + 1) % timerqueries_num;
}

static void draw()
{
	// TODO: replace this with something that asks a lot of GPU time, (but almost no CPU time).
	glClearColor(0, 1, 0, 1);    // green
	glClear(GL_COLOR_BUFFER_BIT);

	write_and_read_timerqueries();
	glutSwapBuffers();
}

static void key_pressed(unsigned char key, int x __attribute__((unused)), int y __attribute__((unused)))
{
	switch (key) {
	case 27:    // escape
		exit(0);
	}
}

int main(int argc, char **argv)
{
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(640, 480);
	glutCreateWindow("find-min-required-number-of-timer-queries (Press ESC to end test)");
	glutKeyboardFunc(key_pressed);
	glutDisplayFunc(draw);
	glutIdleFunc(glutPostRedisplay);
	init();

	glutMainLoop();

	return 0;
}
