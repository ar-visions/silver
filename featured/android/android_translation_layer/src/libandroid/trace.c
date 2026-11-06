#include <stdbool.h>

bool ATrace_isEnabled()
{
	return false;
}

void ATrace_beginSection(const char *sectionName)
{
}

void ATrace_endSection()
{
}

void ATrace_beginAsyncSection(const char *sectionName, int cookie)
{
}

void ATrace_endAsyncSection(const char *sectionName, int cookie)
{
}

void ATrace_setCounter(const char *counterName, long counterValue)
{
}
