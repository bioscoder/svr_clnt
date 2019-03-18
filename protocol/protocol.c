#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <errno.h>

#include "protocol.h"

int isHeaderValid(char *input, unsigned int len)
{
	if (len != sizeof(MagicToken))
		return 0;
	
	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	if (mToken == NULL)
	{
		printf("malloc mToken\n");
		return 0;
	}
	if (memmove(mToken, input, len))
	{
		if ((mToken->start_key == MAGIC_START_KEY) && (mToken->end_key == MAGIC_END_KEY))
		{
			free(mToken);
			return 1;
		}
	}
	free(mToken);
	return 0;
}

unsigned int getFilesizeFromHeader(char *input)
{
	unsigned int retval = 0;
	
	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	if (mToken == NULL)
	{
		printf("malloc mToken\n");
		return 0;
	}
	if (memmove(mToken, input, (sizeof(MagicToken))))
		retval = mToken->nextdatasizes;
	free(mToken);
	return retval;
}

unsigned int getCompressionFromHeader(char *input)
{
	unsigned int retval = 0;

	MagicToken *mToken = (MagicToken*)malloc(sizeof(MagicToken));
	if (mToken == NULL)
	{
		printf("malloc mToken\n");
		return 0;
	}
	if (memmove(mToken, input, (sizeof(MagicToken))))
		retval = mToken->compressionType;
	free(mToken);
	return retval;
}
