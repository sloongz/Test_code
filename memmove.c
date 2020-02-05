#include <stdio.h>
#include <stddef.h>


void *memmove(void *s1, const void *s2, size_t n)
{
	unsigned char *pd = (unsigned char *)s1;
	unsigned char *ps = (unsigned char *)s2;

	if ((unsigned long)s1 > (unsigned long)s2) {
		pd += n;
		ps += n;
		while (n--)
		  *pd-- = *ps--;
	} else {
		while (n--)
		  *pd++ = *ps++;
	}
}

int main()
{
	char buf[10];
	int i;

	for (i = 0; i< 10; i++) {
		buf[i] = i;
	}

	memmove(&buf[0], &buf[2], 7);
	for (i=0; i<10; i++) {
		printf("%d ", buf[i]);
	}
	printf("\n");

	for (i = 0; i< 10; i++) {
		buf[i] = i;
	}
	memmove(&buf[2], &buf[0], 8);
	for (i=0; i<10; i++) {
		printf("%d ", buf[i]);
	}
	printf("\n");

	return 0;
}
