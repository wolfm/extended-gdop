#include <iostream>
#include <cmath>
#include <string>
using namespace std;

int usbcMem(int numBowlers);
int gasCost(int miles, int numBowlers);
int practiceCost(int numBowlers);

int main() {
	int numBowlers = 0;
	int sum = 0;
	string fall1;
	cout << "How many bowlers do you have?" << endl;
	cin >> numBowlers;
	numBowlers += 2; //this is to account for those who plan to join but don't									
	cout << "Do you need USBC membership? Y/N" << endl; 
	cin >> fall1;										
	if (fall1 == "Y" || fall1 == "y")
		sum = sum + usbcMem(numBowlers);
	sum+=practiceCost(numBowlers);						//gets practice cost
	
	char tourney = 'Y';
	char hotel = 'o';
	int miles = 0;
	int tourneyCost = 0;

	//cycles through all tournaments, adding to the running total
	while (tourney == 'Y' || tourney == 'y') {
		//adds tournament cost
		cout << "Enter the price of the tournament" << endl;
		cin >> tourneyCost;
		sum += tourneyCost;
		cout << endl;
		
		//calculates hotel cost
		cout << "Do you need a hotel? Y/N" << endl;
		cin >> hotel;
		if (hotel == 'Y' || hotel == 'y') {
			int numDays;
			cout << "How many days is the tournament?" << endl;
			cin >> numDays;
			if (numDays == 2) sum += 400; //estimated cost for 2 rooms at $100
			else sum += 200; //estimated cost for 2 rooms at $100 
		}
		cout << endl;								  

		cout << "Enter the number of miles from Ann Arbor to the tournament" << endl;
		cin >> miles;
		sum = sum + (gasCost(miles, numBowlers) * 3);
		cout << endl;
		
		cout << "Is there another tourney after this one? Y/N" << endl;
		cin >> tourney;
		cout << endl;
	}
	
	cout << "Your total cost for this year will be: $" << sum << endl;
	cout << "This is $" << ceil(sum / ((double)numBowlers)) << " per person without jersey costs" << endl;
	cout << "Have a good season!";

	return 0;
}

//adds normal usbc cost + price of additional bowlers
int usbcMem(int numBowlers) {	
	int sum = 360;
	
	if (numBowlers > 8) {
		sum = sum + ((numBowlers - 8) * 35);
		return sum;
	}

	return sum;
}

//this function assumes that three cars are taken and each runs at 20 mpg
int gasCost(int miles, int numBowlers) {
	double numGallons = miles / 20.0;
	
	//only use the top if we think we need 3 ppl to drive. otherwise just use 2
	//if ((numBowlers / 3.0) > 2.0) return ceil(numGallons * 6);
	return ceil(numGallons * 5);
}

int practiceCost(int numBowlers) {
	int numPractices = 0;
	cout << "How many practices will you have this semester? " << endl;
	cin >> numPractices;

	return numPractices * numBowlers * 5;
}