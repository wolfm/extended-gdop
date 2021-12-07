int a = 0;

void inc_a() {
	for(int i = 0; i < 3000; ++i) {
		if(i < 1500) {
			a += 2;
		}
		else {
			++a;
		}
	}
}

int main() {
	--a;
	a*=4;
	int b = a << 4;
	inc_a();
	return a;
}
