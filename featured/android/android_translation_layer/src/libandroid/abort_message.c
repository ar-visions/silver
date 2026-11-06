#include <stdio.h>

/* bionic's: the message a native crash reports */
void android_set_abort_message(const char *msg)
{
	if (msg)
		fprintf(stderr, "abort message: %s\n", msg);
}
