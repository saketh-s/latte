#include "PolyRep.h"
#include "newIntegration.h"
#include "../timing.h"
#include <iostream>
#include <fstream>
#include <NTL/ZZ.h>

using namespace std;

void timedOut()
{

}

int main(int argc, char *argv[])
{
	if (argc < 4) { cout << "Usage: " << argv[0] << " fileIn fileOut decompose [timeout]" << endl; return 1; }
	bool decomposing = (strcmp(argv[3], "1") == 0);
	bool polynomial = true; //file is assumed to alternate between polynomial and the simplex
	float timeout = -1.0f;
	if (argc == 5) { timeout = strtod(argv[4], NULL); };
	string line;
	monomialSum monomials;
	linFormSum forms;
	ifstream myStream (argv[1]);
	ofstream outStream(argv[2]);
	if (!myStream.is_open()) { cout << "Error opening file " << argv[1] << ", please make sure it is spelled correctly." << endl; return 1; }
	int polyCount = 0;
	int dimension;
	int degree = -1;
	int irregularForms = 0;
	float loadTime, decomposeTime, integrateTime, parseIntegrate;
	loadTime = decomposeTime = integrateTime = parseIntegrate = 0.0f;
	Timer myTimer("Integration timer");
	FormIntegrateConsumer<ZZ> *integrator = new FormIntegrateConsumer<ZZ>();
	string testForms;
	while (!myStream.eof())
	{
		getline(myStream, line, '\n');
		if (!line.empty())
		{
			if (timeout > 0 && integrateTime > timeout)
			{
					delete integrator;
					myStream.close();
					outStream.close();
					FILE* myFile = fopen(argv[2],"w"); //overwriting results file
					fprintf(myFile, "Error");
					fclose(myFile);
										
					cout << "Integration timed out." << endl;
					myFile = fopen("integration/benchmarks.txt","a");
					fprintf(myFile, "%10s", "--");
					fclose(myFile);
					return 1;
			}
			if (polynomial) //reading polynomial
			{
				if (decomposing) //input is sum of monomials that we decompose into sum of linear forms
				{
					myTimer.start();
					loadMonomials(monomials, line);
					myTimer.stop();
					loadTime += myTimer.get_seconds();

					if (monomials.termCount == 0 || monomials.varCount == 0)
					{
						cout << "Error: loaded invalid monomial sum." << endl;
						return 1;
					}

					forms.termCount = 0;
					dimension = forms.varCount = monomials.varCount;
		
					float thisTime = time(NULL);
					//cout << "Decomposing " << printMonomials(monomials);
					myTimer.start();
					for (int i = 0; i < monomials.termCount; i++)
					{
						//cout << ".";
						decompose(monomials, forms, i);
					}
					myTimer.stop();
					decomposeTime += myTimer.get_seconds();
					//cout << endl;
					
					if (forms.termCount == 0)
					{
						cout << "Error: no terms in decomposition to sum of linear forms.";
						return 1;	
					}
					
					testForms = printLinForms(forms);
					outStream << testForms << endl;
					if (degree == -1) //degree is calculated only once
					{
						degree = 0;
						for (int i = 0; i < monomials.varCount; i++)
						{
							degree += monomials.eHead->data[i];
						}
					}
					destroyMonomials(monomials);
				}
				else //input is just linear forms
				{
					myTimer.start();
					loadLinForms(forms, line);
					myTimer.stop();
					loadTime += myTimer.get_seconds();
					if (forms.termCount == 0 || forms.varCount == 0)
					{
						cout << "Error: loaded invalid sum of linear forms.";
						return 1;	
					}
					integrator->setFormSum(line);
				}
				polynomial = false;
				//cout << "Loaded into " << forms.termCount << " linear forms" << endl;
			}
			else //reading simplex
			{
				simplexZZ mySimplex;
				convertToSimplex(mySimplex, line);
				//integrate here
				ZZ numerator, denominator;
				if (decomposing)
				{			
					//check irregularity, timeouts
					integrator->setSimplex(mySimplex);
					myTimer.start();
					parseLinForms(integrator, testForms);
					myTimer.stop();
					/*if (integrator->getDimension() == -1) //we timed out
					{
						timedOut();
						return;
					}*/
					parseIntegrate += myTimer.get_seconds();
					ZZ a, b;
					integrator->getResults(a, b);
					if (IsZero(b))
					{ irregularForms++; }
					//cout << "Irregular: " << printLinForms(forms) << " over " << line << endl;
					myTimer.start();
					integrateList(numerator, denominator, forms, mySimplex);
					myTimer.stop();
					integrateTime += myTimer.get_seconds();
				}
				else
				{
					integrator->setSimplex(mySimplex);
					myTimer.start();
					parseLinForms(integrator, integrator->getFormSum());
					myTimer.stop();
					integrator->getResults(numerator, denominator);
					if (IsZero(denominator)) //irregular
					{	
						irregularForms++;
					}
					integrateTime += myTimer.get_seconds();
				}
				outStream << "[" << numerator << "," << denominator << "]" << endl;
				destroyLinForms(forms);
				polyCount++;
				polynomial = true;
			}
		}
	}
	if (decomposing) { cout << "Dimension " << dimension << ", degree " << degree << ". " << irregularForms << " forms were irregular." << endl; }
	/*cout << "Total time to load " << polyCount << " polynomials: " << loadTime << ", avg. is " << loadTime / polyCount << endl;
	if (decomposing) { cout << "Total time to decompose " << polyCount << " polynomials: " << decomposeTime << ", avg. is " << decomposeTime / polyCount << endl; }
	cout << "Total time to integrate " << polyCount << " polynomials: " << integrateTime << ", avg. is " << integrateTime / polyCount << endl;
	cout << "Total time to integrate " << polyCount << " linear forms: " << parseIntegrate << ", avg. is " << parseIntegrate / polyCount << endl;
	cout << "Total time is " << (decomposing ? loadTime + integrateTime + decomposeTime : loadTime + integrateTime) << ", avg. is " << (decomposing ? loadTime + integrateTime + decomposeTime : loadTime + integrateTime) / polyCount << endl;*/
	FILE* benchmarks = fopen("integration/benchmarks.txt","a");
	fprintf(benchmarks, "%10.2f", (decomposing ? loadTime + integrateTime + decomposeTime : loadTime + integrateTime) / polyCount);
	fclose(benchmarks);
	
	delete integrator;
	myStream.close();
	outStream.close();
	return 0; 
}
