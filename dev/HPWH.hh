#ifndef HPWH_hh
#define HPWH_hh

#include <string>
#include <cmath>
#include <iostream>
#include <iomanip>

#include <cstdio>
#include <cstdlib>   //for exit
#include <vector>

#define DENSITYWATER_kgperL 0.998
#define CPWATER_kJperkgC 4.181
#define CONDENSITY_SIZE 12  //this must be an integer, and only the value 12
//change at your own risk

class HPWH {
 public:
  HPWH();  //default constructor
	~HPWH(); //destructor - will be defined

  //an enum to specify the various modes for the Demand Response (DR) abilities
  //values may vary - names should be used
  enum DRMODES{
    DR_BLOCK = 0,   //this mode prohibits the elements from engaging and turns
                    //off any currently running
    DR_ALLOW = 1,   //this mode allows the water heater to run normally
    DR_ENGAGE = 2  //this mode forces an element to turn on
    };
  



  
	int HPWHinit_presets(int presetNum);
	/* This function will load in a set of parameters that are hardcoded in this function - 
	 * which particular set of parameters is selected by presetNum.
	 * This is similar to the way the HPWHsim currently operates, as used in SEEM,
	 * but not quite as versatile.
	 * My impression is that this could be a useful input paradigm for CSE
	 * 
	 * The return value is 0 for successful init, something else otherwise
	 */

	int HPWHinit_file(std::string configFile);
	/* This function will load in a set of parameters from a file
	 * The file name is the input - there should be at most one set of parameters per file
	 * This is useful for testing new variations, and for the sort of variability
	 * that we typically do when creating SEEM runs
	 * 
	 * The return value is 0 for successful init, something else otherwise
	 */

	int runOneStep(double inletT_C, double drawVolume_L, 
                  double ambientT_C, double externalT_C,
                  DRMODES DRstatus, double minutesPerStep);
	/* This function will progress the simulation forward in time by one step
	 * all calculated outputs are stored in private variables and accessed through functions
	 * 
	 * The return value is 0 for successful simulation run, something else otherwise
	 */
	 
	int runNSteps(int N,  double *inletT_C, double *drawVolume_L, 
                  double *tankAmbientT_C, double *heatSourceAmbientT_C,
                  DRMODES *DRstatus, double minutesPerStep);
	/* This function will progress the simulation forward in time by N (equal) steps
	 * The calculated values will be summed or averaged, as appropriate, and 
	 * then stored in the usual variables to be accessed through functions
	 * 
	 * The return value is 0 for successful simulation run, something else otherwise
	 */



  int setSetpoint(double newSetpoint /*default units C*/);
  int setSetpoint(double newSetpoint, std::string units);
  //a function to change the setpoint - useful for dynamically setting it
  int resetTankToSetpoint();
  //this function resets the tank temperature profile to be completely at setpoint

  
	int getNumNodes() const;
	//get the number of nodes
  double getTankNodeTemp(int nodeNum /*default units C*/) const;
  double getTankNodeTemp(int nodeNum, std::string units) const;
	//get the temperature of the water at the specified node - with or without units
	double getNthSimTcouple(int N /*default units C*/) const;
  double getNthSimTcouple(int N, std::string units) const;
  //get a temperature from a set of 6 virtual "thermocouples", which are constructed
  //from the node temperature array.  Specify t-couple from 1-6, 1 at the bottom
  //with or without units
	
	int getNumHeatSources() const;
	//get the number of heat sources
	double getNthHeatSourceEnergyInput(int N /*default units kWh*/) const;
	double getNthHeatSourceEnergyInput(int N, std::string units) const;
	//get the energy input to the Nth heat source, with or without units
  //energy used by the heat source is positive - should always be positive
  double getNthHeatSourceEnergyOutput(int N /*default units kWh*/) const;
	double getNthHeatSourceEnergyOutput(int N, std::string units) const;
	//get the energy output from the Nth heat source, with or without units
  //energy put into the water is positive - should always be positive
	double getNthHeatSourceRunTime(int N) const;
	//get the run time for the Nth heat source, in minutes
	//note: they may sum to more than 1 time step for concurrently running heat sources
  bool isNthHeatSourceRunning(int N) const;
  //return true if the Nth heat source is currently engaged

	double getOutletTemp(/*default units C*/) const;
	double getOutletTemp(std::string units) const;
	//a function to get the outlet temperature - returns 0 when no draw occurs
	//the input is a string containing the desired units, F or C
	double getEnergyRemovedFromEnvironment(/*default units kWh*/) const;
	double getEnergyRemovedFromEnvironment(std::string units) const;
	//get the total energy removed from the environment by all heat sources
  //(not net energy - does not include standby)
  //moving heat from the space to the water is the positive direction
	//with or without units - kWh or btu
  double getStandbyLosses(/*default units kWh*/) const;
	double getStandbyLosses(std::string units) const;
	//get the amount of heat lost through the tank
  //moving heat from the water to the space is the positive direction
  //negative should occur seldom
  //with or without units - kWh or btu

 
 private:
  class HeatSource;

	void updateTankTemps(double draw, double inletT, double ambientT, double minutesPerStep);
	bool areAllHeatSourcesOff() const;
	//test if all the heat sources are off
	void turnAllHeatSourcesOff();
	//disengage each heat source
	
	double topThirdAvg_C() const;
	double bottomThirdAvg_C() const;
	double bottomTwelthAvg_C() const;
	//functions to calculate what the temperature in a portion of the tank is


 
	bool isHeating;
	//is the hpwh currently heating or not?
	
	int numHeatSources;
	//how many heat sources this HPWH has
	HeatSource *setOfSources;
	//an array containing the HeatSources, in order of priority
	
	int numNodes;
	//the number of nodes in the tank - must be >= 12, in multiples of 12
	double tankVolume_L;
	//the volume in liters of the tank
	double tankUA_kJperHrC;
	//the UA of the tank, in metric units
	
	double setpoint_C;
	//the setpoint of the tank
	double *tankTemps_C;
	//an array holding the temperature of each node - 0 is the bottom node, numNodes is the top


	//Some outputs
	double outletTemp_C;
	//the temperature of the outlet water - taken from top of tank, 0 if no flow
	double energyRemovedFromEnvironment_kWh;
	//the total energy removed from the environment, to heat the water
	double standbyLosses_kWh;
	//the amount of heat lost to standby
	
	
	//special variables for adding abilities
	bool tankMixesOnDraw;
	// whether or not the bottom third of the tank should mix during draws
	bool doTempDepression;
	// whether the HPWH should use the alternate ambient temperature that  
	// gets depressed when a compressor is running
  // NOTE: this only works for 1 minute steps
  double locationTemperature;
	// this is the special location temperature that stands in for the the 
	// ambient temperature if you are doing temp. depression

};  //end of HPWH class




class HPWH::HeatSource {
 public:
  friend class HPWH;

	HeatSource() {};
	//default constructor, does not create a useful HeatSource
	
	HeatSource(HPWH *parentHPWH);
	//constructor assigns a pointer to the hpwh creating this heat source 

  void setupAsResistiveElement(int node, double Watts);
  //configure the heat source to be a resisive element, positioned at the
  //specified node, with the specified power in watts
  
	bool isEngaged() const;
	//return whether or not the heat source is engaged
	void engageHeatSource(double heatSourceAmbientT_C);
	//turn heat source on, i.e. set isEngaged to TRUE
	void disengageHeatSource();
	//turn heat source off, i.e. set isEngaged to FALSE
	
	bool shouldHeat(double heatSourceAmbientT_C) const;
	//queries the heat source as to whether or not it should turn on
	bool shutsOff(double heatSourceAmbientT_C) const;
	//queries the heat source whether should shut off (typically lowT shutoff)

	void addHeat_temp(double externalT_C, double minutesPerStep);
	void addHeat(double externalT_C, double minutesToRun);
	//adds head to the hpwh - this is the function that interprets the 
	//various configurations (internal/external, resistance/heat pump) to add heat

	void setCondensity(double cnd1, double cnd2, double cnd3, double cnd4, 
                     double cnd5, double cnd6, double cnd7, double cnd8, 
                     double cnd9, double cnd10, double cnd11, double cnd12);
	//a function to set the condensity values, it pretties up the init funcs.
	
	
 private:
	HPWH *hpwh;
	//the creator of the heat source, necessary to access HPWH variables
	
	//these are the heat source state/output variables
	bool isOn;
	//is the heat source running or not	
	
	//some outputs
	double runtime_min;
	//this is the percentage of the step that the heat source was running
	double energyInput_kWh;
	//the energy used by the heat source
	double energyOutput_kWh;
	//the energy put into the water by the heat source

	//these are the heat source property variables
	bool isVIP;
	//is this heat source a high priority heat source? (e.g. upper resisitor)
	HeatSource* backupHeatSource;
	//a pointer to the heat source which serves as backup to this one
  //should be NULL if no backup exists
	HeatSource* companionHeatSource;
	//a pointer to the heat source which will run concurrently with this one
  //it still will only turn on if shutsOff is false
	
	double condensity[CONDENSITY_SIZE];
	//The condensity function is always composed of 12 nodes.  
	//It represents the location within the tank where heat will be distributed,
	//and it also is used to calculate the condenser temperature for inputPower/COP calcs.
	//It is conceptually linked to the way condenser coils are wrapped around 
	//(or within) the tank, however a resistance heat source can also be simulated
	//by specifying the entire condensity in one node.
	
	double T1_F, T2_F;
	//the two temperatures where the input and COP curves are defined
	double inputPower_T1_constant_W, inputPower_T2_constant_W;
	double inputPower_T1_linear_WperF, inputPower_T2_linear_WperF;
	double inputPower_T1_quadratic_WperF2, inputPower_T2_quadratic_WperF2;
	//these are the coefficients for the quadratic function 
	//defining the input power as a function of the condenser temperature
	double COP_T1_constant, COP_T2_constant;
	double COP_T1_linear, COP_T2_linear;
	double COP_T1_quadratic, COP_T2_quadratic;
	//these are the coefficients for the quadratic function 
	//defining the COP as a function of the condenser temperature


	//the heating logic instructions come in pairs - a string to select
	//which logic function to use, and a double to give the setpoint
	//for that function
	struct heatingLogicPair{
		std::string selector;
		double decisionPoint_C;
		//and a constructor to allow creating anonymous structs for easy assignment
		heatingLogicPair(std::string x, double y) : selector(x), decisionPoint_C(y) {};
		};
	
	//a vector to hold the set of logical choices for turning this element on
	std::vector<heatingLogicPair> turnOnLogicSet;
	//a vector to hold the set of logical choices that can cause an element to turn off
	std::vector<heatingLogicPair> shutOffLogicSet;


	double hysteresis;
	//a hysteresis term that prevents short cycling due to heat pump self-interaction

	bool depressesTemperature;
	// heat pumps can depress the temperature of their space in certain instances - 
	// whether or not this occurs is a bool in HPWH, but a heat source must 
	// know if it is capable of contributing to this effect or not
  // NOTE: this only works for 1 minute steps

	std::string configuration; // submerged, wrapped, external



  //some private functions, mostly used for heating the water with the addHeat function

 	double addHeatAboveNode(double cap_kJ, int node, double minutesToRun);
  //adds heat to the set of nodes that are at the same temperature, above the
  //specified node number
  double addHeatExternal(double externalT_C, double minutesToRun, double &cap_BTUperHr, double &input_BTUperHr, double &cop);
  // Add heat from a source outside of the tank. Assume the condensity is where
  // the water is drawn from and hot water is put at the top of the tank.
  

	// I wrote some methods to help with the add heat interface - MJL
  void getCapacity(double externalT_C, double condenserTemp_C, double &input_BTUperHr, double &cap_BTUperHr, double &cop);
  void calcHeatDist(std::vector<double> &heatDistribution);

	int lowestNode();
  //returns the number of the first non-zero condensity entry
	double getCondenserTemp();
  //returns the temperature of the condensor - it's a weighted average of the
  //tank temperature, using the condensity as weights
  
  // A few helper functions
  double expitFunc(double x, double offset);
  void normalize(std::vector<double> &distribution, int n);

};  //end of HeatSource class


//a few extra functions for unit converesion
inline double dF_TO_dC(double temperature) { return (temperature*5.0/9.0); }
inline double F_TO_C(double temperature) { return ((temperature - 32.0)*5.0/9.0); }
inline double C_TO_F(double temperature) { return (((9.0/5.0)*temperature) + 32.0); }
inline double KWH_TO_BTU(double kwh) { return (3412.14 * kwh); }
inline double KWH_TO_KJ(double kwh) { return (kwh * 3600.0); }
inline double BTU_TO_KWH(double btu) { return (btu / 3412.14); }
inline double KJ_TO_KWH(double kj) { return (kj/3600.0); }
inline double BTU_TO_KJ(double btu) { return (btu * 1.055); }
inline double GAL_TO_L(double gallons) { return (gallons * 3.78541); }


#endif
