//This csv2json file_zero can pass the examples with no separators and backslash in them.
//And some field values can be empty but the field names.


#include<iostream>
#include<fstream>
#include<sstream>
#include<string>
#include<vector>
using namespace std;

int main(int argc, char* argv[]) {
	 string ipath, opath;
	if (argc != 3) {
		cout << "arguement error";
		system("pause");
		exit(-1);
	}
	//argv[0] is the path of the path of the .exe file.
	ipath = string(argv[1]);
	opath = string(argv[2]);
	const char separater = ',';
	ifstream csvInput;
	ofstream jsonOutput;
	csvInput.open(ipath, ios::in);
	jsonOutput.open(opath, ios::out | ios::trunc);


	string Oneline;
	string str;
	vector<string> fieldnameArray;
	vector<string>::iterator it1;
	int LineCounter = 0;
	int FieldNumber = 0;
	jsonOutput << "[\n";

	while (getline(csvInput, Oneline))
	{
		Oneline = Oneline.replace(Oneline.find("\r"), 1, "\0");
		vector<string> fieldArray;
		stringstream ss(Oneline);
		LineCounter++;

		if (LineCounter >= 3) {
			jsonOutput << "},\n";
		}
		if (LineCounter >= 2) {
			jsonOutput << "{";
		}
		while (getline(ss, str, separater)) {
			
			if (LineCounter == 1) {
				FieldNumber++;
				fieldnameArray.push_back(str);
			}
			else {
				fieldArray.push_back(str);
			}
		}
		if (LineCounter > 1) {
			int FieldCounter = 0;
			while (FieldCounter < FieldNumber) {
				if (FieldCounter > 0) {
					jsonOutput << separater;
				}
//change the jsonOutput format if we want to change the .json format.
				jsonOutput << "\"" << fieldnameArray[FieldCounter] << "\":\"" << fieldArray[FieldCounter] << "\"";
				FieldCounter++;			
			}

		}
	}
	jsonOutput << "}\n" << "]\n";
	csvInput.close();
	jsonOutput.close();
	return 0;
}