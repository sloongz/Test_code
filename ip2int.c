#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


unsigned int ipstr2int(char *ipstr)
{
	unsigned int ret = 0;
	char *tmp[4];
	char *p;
	int i, j;
	int sum;
	int len;

	for (i=0; i<4; i++) {
		tmp[i] = calloc(1, sizeof(char)*8);
		p = tmp[i];
		while (*ipstr != '.' && *ipstr != '\0') {
			*p++ = *ipstr++;
		}
		if (*ipstr != '\0') {
			ipstr++;
		}
	}

	printf("%s.%s.%s.%s\n", tmp[0], tmp[1], tmp[2], tmp[3]);

	for (i=0; i<4; i++) {
		sum = 0;
		p = tmp[i];
		len = strlen(tmp[i]);
		for (j=0; j<len; j++) {
			sum = sum*10 + (p[j]-'0');
		}
		ret += sum<<(i*8); 
	}

	for (i=0; i<4; i++) {
		free(tmp[i]);
	}

	return ret;
}

int main(int argc, char **argv)
{
	char *ipstr = "192.168.1.220";
	int ip;
	ip = ipstr2int(ipstr);
	printf("ip:%08x\n", ip);
	ip = inet_addr(ipstr);
	printf("ip:%08x\n", ip);

	return 0;
}
