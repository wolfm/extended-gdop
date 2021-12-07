int a = 0;

void decrement_a() {
	--a;
}

int main() {
	++a;
	decrement_a();
	return a;
}
