#include <stdio.h>

#define BIT(x) (1 << (x))
#define CLEARBIT(x) ~(1 << (x))

#define SETBIT(d, x) do {(d) |= BIT(x);} while(0);
#define CLRBIT(d, x) do {(d) &= (~BIT(x));} while(0);

int main()
{
	int x = 1025;
	int y = BIT(6);
	printf("y=%d\n", y);
	SETBIT(x, 6);
	printf("x-set=%d\n", x);
	printf("x=%d\n", x);
	CLRBIT(x, 6);
	printf("x-clear=%d\n", x);
	return 0;
}