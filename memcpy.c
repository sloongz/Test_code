#include <stddef.h>
#include <stdio.h>


void *memcpy(void *dest, const void *src, size_t n)
{
	unsigned char *pd = (unsigned char *)dest;
	unsigned char *ps = (unsigned char *)src;

	while (n--)
	  *pd++ = *ps++;

	return dest;
}

int main()
{
	int i;
	char buf[10];

	for (i=0; i<10; i++) {
		buf[i] = i;
	}

	memcpy(&buf[0], &buf[2], 5);	

	for (i=0; i<10; i++) {
		printf("%d ", buf[i]);
	}
	printf("\n");

	return 0;
}
