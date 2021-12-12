#include <stdio.h> 
int a = 0;

void inc_a() {
	int b = 0;
	for(int i = 2999; i < 3000; ++i) {
		if(i < 1500) {
			a = 2;
			a = 4;
			b++;
			// b = a << 2;
			// b = a++;
		}
		else {
			int d = 3;
			//b++;
			printf("hello\n");
			printf("hello\n");
			printf("hello\n");
			printf("hello\n");
			printf("hello\n");
			printf("hello\n");
			int c = 5;
			int g = 0;
			int f = 10;
			c = 3;
			d = 10;
			//b = 4;
			// ++a;
			// --b;
			// b = a++;
			// b--;
		}
	}
}

int main() {
	--a;
	int b = a << 4;
	a*=4;
	inc_a();
	return a;
}
