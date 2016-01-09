/*This is not a substitute for a proper HPWH Test Tool, it is merely a short program
 * to aid in the testing of the new HPWH.cc as it is being written.
 * 
 * -NDK
 * 
 * 
 */
#include "HPWH.hh"


#define F_TO_C(T) ((T-32.0)*5.0/9.0)
#define GAL_TO_L(GAL) (GAL*3.78541)

using std::cout;
using std::endl;
using std::string;

void printTankTemps(HPWH& hpwh);
void printHeatSourceInfo(HPWH& hpwh);



int main(void)
{


HPWH hpwh;

//hpwh.HPWHinit_presets(1);

//int HPWH::runOneStep(double inletT_C, double drawVolume_L, 
					//double ambientT_C, double externalT_C,
					//double DRstatus, double minutesPerStep)
//hpwh.runOneStep(0, 120, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 5, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 5, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 5, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 5, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 5, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 10, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);

//hpwh.runOneStep(0, 0, 50, 50, 1, 1);
//printTankTemps(hpwh);
//printHeatSourceInfo(hpwh);



hpwh.HPWHinit_presets(1);
//int HPWH::runOneStep(double inletT_C, double drawVolume_L, 
					//double ambientT_C, double externalT_C,
					//double DRstatus, double minutesPerStep)

          
hpwh.runOneStep(0, 25, 0, 50, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

printHeatSourceInfo(hpwh);
hpwh.runOneStep(0, 25, 0, 50, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

printHeatSourceInfo(hpwh);
hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 25, 0, 50, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 25, 0, 50, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

//number 22 below
hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, -10, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, -10, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, -10, 1, 1);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);

hpwh.runOneStep(0, 0, 0, 50, 1, 2);
printTankTemps(hpwh);
printHeatSourceInfo(hpwh);




//hpwh.HPWHinit_presets(3);
for (int i = 0; i < 1440*365; i++) {
    hpwh.runOneStep(0, 0.2, 0, 50, 1, 1);
}



return 0;

}



void printTankTemps(HPWH& hpwh) {
  cout << std::left;
  for (int i = 0; i < hpwh.getNumNodes(); i++) {
    cout << std::setw(7) << hpwh.getTankNodeTemp(i) << ", ";
  }
  cout << endl;

}

void printHeatSourceInfo(HPWH& hpwh){
  cout << std::left;
  for (int i = 0; i < hpwh.getNumHeatSources(); i++) {
    cout << "heat source " << i << ": " << hpwh.isNthHeatSourceRunning(i) << "\t\t";   
  }
  cout << endl;

  for (int i = 0; i < hpwh.getNumHeatSources(); i++) {
    cout << "input energy kwh: " << std::setw(7) << hpwh.getNthHeatSourceEnergyInput(i) << "\t";   
  }
  cout << endl;

  for (int i = 0; i < hpwh.getNumHeatSources(); i++) {
    cout << "output energy kwh: " << std::setw(7) << hpwh.getNthHeatSourceEnergyOutput(i) << "\t";   
  }
  cout << endl;
  
  for (int i = 0; i < hpwh.getNumHeatSources(); i++) {
    cout << "run time min: " << std::setw(7) << hpwh.getNthHeatSourceRunTime(i) << "\t\t";   
  }
  cout << endl;


  cout << endl << endl;


}