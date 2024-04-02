#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
int main(int argc, char* argv[])
{
	openlog ("writer", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_USER);
	if (argc != 3)
	{
		syslog (LOG_ERR, "Error: Expecting 2 parameters, but %d passed. Correct Usage: writer writefile writestr", argc - 1);
		exit(1);
	}
	syslog (LOG_DEBUG, "Writing %s to file %s", argv[2], argv[1]);

	FILE *writefilePtr = fopen(argv[1], "w");
	if (writefilePtr == NULL)
	{
		syslog (LOG_ERR, "Error: Failed to open file %s", argv[1]);
		exit(1);
	}
	fputs(argv[2], writefilePtr);
	fclose(writefilePtr);
	closelog ();
	return 0;
}
