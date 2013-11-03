#include <unistd.h>

int main()
{
	while (1) {
		int s;
		wait(&s);
	}
}
