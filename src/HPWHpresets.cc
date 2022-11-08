/*
File Containing all of the presets available in HPWHsim
*/
#include <algorithm>

int HPWH::HPWHinit_resTank() {
	//a default resistance tank, nominal 50 gallons, 0.95 EF, standard double 4.5 kW elements
	return this->HPWHinit_resTank(GAL_TO_L(47.5), 0.95, 4500, 4500);
}
int HPWH::HPWHinit_resTank(double tankVol_L, double energyFactor, double upperPower_W, double lowerPower_W) {

	setAllDefaults(); // reset all defaults if you're re-initilizing
	// sets simHasFailed = true; this gets cleared on successful completion of init
	// return 0 on success, HPWH_ABORT for failure

	//low power element will cause divide by zero/negative UA in EF -> UA conversion
	if (lowerPower_W < 550) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Resistance tank lower element wattage below 550 W.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}
	if (upperPower_W < 0.) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Upper resistance tank wattage below 0 W.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}
	if (energyFactor <= 0.) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Energy Factor less than zero.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}

	//use tank size setting function since it has bounds checking
	tankSizeFixed = false;
	int failure = this->setTankSize(tankVol_L);
	if (failure == HPWH_ABORT) {
		return failure;
	}


	numNodes = 12;
	tankTemps_C = new double[numNodes];
	setpoint_C = F_TO_C(127.0);

	//start tank off at setpoint
	resetTankToSetpoint();

	nextTankTemps_C = new double[numNodes];
	doTempDepression = false;
	tankMixesOnDraw = true;


	HeatSource resistiveElementBottom(this);
	resistiveElementBottom.setupAsResistiveElement(0, lowerPower_W);

	//standard logic conditions
	resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40)));
	resistiveElementBottom.addTurnOnLogic(HPWH::standby(dF_TO_dC(10)));

	if (upperPower_W > 0.) {
		// Only add an upper element when the upperPower_W > 0 otherwise ignore this.
		// If the element is added this can mess with the intended logic.
		HeatSource resistiveElementTop(this);
		resistiveElementTop.setupAsResistiveElement(8, upperPower_W);

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.isVIP = true;

		numHeatSources = 2;
		setOfSources = new HeatSource[numHeatSources];

		// set everything in it's correct place
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;

		setOfSources[0].followedByHeatSource = &setOfSources[1];
	}
	else {
		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];
		setOfSources[0] = resistiveElementBottom;
	}

	// (1/EnFac - 1/RecovEff) / (67.5 * ((24/41094) - 1/(RecovEff * Power_btuperHr))) 
	// Previous comment and the equation source said (1/EnFac + 1/RecovEff) however this was
	// determined to be a typo from the source that was kept in the comment. On 8/12/2021 
	// the comment was changed to reflect the correct mathematical formulation which is performed 
	// below. 
	double recoveryEfficiency = 0.98;
	double numerator = (1.0 / energyFactor) - (1.0 / recoveryEfficiency);
	double temp = 1.0 / (recoveryEfficiency * lowerPower_W*3.41443);
	double denominator = 67.5 * ((24.0 / 41094.0) - temp);
	tankUA_kJperHrC = UAf_TO_UAc(numerator / denominator);

	if (tankUA_kJperHrC < 0.) {
		if (hpwhVerbosity >= VRB_reluctant && tankUA_kJperHrC < -0.1) {
			msg("Computed tankUA_kJperHrC is less than 0, and is reset to 0.");
		}
		tankUA_kJperHrC = 0.0;
	}

	hpwhModel = MODELS_CustomResTank;

	//calculate oft-used derived values
	calcDerivedValues();

	if (checkInputs() == HPWH_ABORT) return HPWH_ABORT;

	isHeating = false;
	for (int i = 0; i < numHeatSources; i++) {
		if (setOfSources[i].isOn) {
			isHeating = true;
		}
		setOfSources[i].sortPerformanceMap();
	}

	if (hpwhVerbosity >= VRB_emetic) {
		for (int i = 0; i < numHeatSources; i++) {
			msg("heat source %d: %p \n", i, &setOfSources[i]);
		}
		msg("\n\n");
	}

	simHasFailed = false;
	return 0;  //successful init returns 0
}


int HPWH::HPWHinit_resTankGeneric(double tankVol_L, double rValue_M2KperW, double upperPower_W, double lowerPower_W) {

	setAllDefaults(); // reset all defaults if you're re-initilizing
	// sets simHasFailed = true; this gets cleared on successful completion of init
	// return 0 on success, HPWH_ABORT for failure

	//low power element will cause divide by zero/negative UA in EF -> UA conversion
	if (lowerPower_W < 0) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Lower resistance tank wattage below 0 W.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}
	if (upperPower_W < 0.) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Upper resistance tank wattage below 0 W.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}
	if (rValue_M2KperW <= 0.) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("R-Value is equal to or below 0.  DOES NOT COMPUTE\n");
		}
		return HPWH_ABORT;
	}

	//set tank size function has bounds checking
	tankSizeFixed = false;
	if (this->setTankSize(tankVol_L) == HPWH_ABORT) {
		return HPWH_ABORT;
	}
	canScale = true;

	numNodes = 12;
	tankTemps_C = new double[numNodes];
	setpoint_C = F_TO_C(127.0);
	resetTankToSetpoint(); //start tank off at setpoint

	nextTankTemps_C = new double[numNodes];
	doTempDepression = false;
	tankMixesOnDraw = true;

	// Count up heat sources
	numHeatSources = 0;
	numHeatSources += upperPower_W > 0. ? 1 : 0;
	numHeatSources += lowerPower_W > 0. ? 1 : 0;
	setOfSources = new HeatSource[numHeatSources];

	// Deal with upper element
	if (upperPower_W > 0.) {
		// Only add an upper element when the upperPower_W > 0 otherwise ignore this.
		// If the element is added this can mess with the intended logic.
		HeatSource resistiveElementTop(this);
		resistiveElementTop.setupAsResistiveElement(8, upperPower_W);
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.isVIP = true;

		// Upper should always be first in setOfSources if it exists.
		setOfSources[0] = resistiveElementTop; 
	}
	
	// Deal with bottom element
	if (lowerPower_W > 0.) {
		HeatSource resistiveElementBottom(this);
		resistiveElementBottom.setupAsResistiveElement(0, lowerPower_W);

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40.)));
		resistiveElementBottom.addTurnOnLogic(HPWH::standby(dF_TO_dC(10.)));

		// set everything in it's correct place
		if (numHeatSources == 1) {// if one only one slot
			setOfSources[0] = resistiveElementBottom;
		}
		else if (numHeatSources == 2) { // if two upper already exists
			setOfSources[1] = resistiveElementBottom;
			setOfSources[0].followedByHeatSource = &setOfSources[1];
		}
	}

	// Calc UA
	double SA_M2 = getTankSurfaceArea(tankVol_L, HPWH::UNITS_L, HPWH::UNITS_M2);
	double tankUA_WperK = SA_M2 / rValue_M2KperW;
	tankUA_kJperHrC = tankUA_WperK * 3.6; // 3.6 = 3600 S/Hr and 1/1000 kJ/J

	if (tankUA_kJperHrC < 0.) {
		if (hpwhVerbosity >= VRB_reluctant && tankUA_kJperHrC < -0.1) {
			msg("Computed tankUA_kJperHrC is less than 0, and is reset to 0.");
		}
		tankUA_kJperHrC = 0.0;
	}

	hpwhModel = HPWH::MODELS_CustomResTankGeneric;

	//calculate oft-used derived values
	calcDerivedValues();

	if (checkInputs() == HPWH_ABORT) return HPWH_ABORT;

	isHeating = false;
	for (int i = 0; i < numHeatSources; i++) {
		if (setOfSources[i].isOn) {
			isHeating = true;
		}
		setOfSources[i].sortPerformanceMap();
	}

	if (hpwhVerbosity >= VRB_emetic) {
		for (int i = 0; i < numHeatSources; i++) {
			msg("heat source %d: %p \n", i, &setOfSources[i]);
		}
		msg("\n\n");
	}

	simHasFailed = false;
	return 0;  //successful init returns 0
}

int HPWH::HPWHinit_genericHPWH(double tankVol_L, double energyFactor, double resUse_C) {

	setAllDefaults(); // reset all defaults if you're re-initilizing
	// sets simHasFailed = true; this gets cleared on successful completion of init
	// return 0 on success, HPWH_ABORT for failure

	//except where noted, these values are taken from MODELS_GE2014STDMode on 5/17/16
	numNodes = 12;
	tankTemps_C = new double[numNodes];
	setpoint_C = F_TO_C(127.0);

	nextTankTemps_C = new double[numNodes];

	//start tank off at setpoint
	resetTankToSetpoint();

	tankSizeFixed = false;

	//custom settings - these are set later
	//tankVolume_L = GAL_TO_L(45);
	//tankUA_kJperHrC = 6.5;

	doTempDepression = false;
	tankMixesOnDraw = true;

	numHeatSources = 3;
	setOfSources = new HeatSource[numHeatSources];

	HeatSource compressor(this);
	HeatSource resistiveElementBottom(this);
	HeatSource resistiveElementTop(this);

	//compressor values
	compressor.isOn = false;
	compressor.isVIP = false;
	compressor.typeOfHeatSource = TYPE_compressor;

	double split = 1.0 / 4.0;
	compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

	compressor.perfMap.reserve(2);

	compressor.perfMap.push_back({
		50, // Temperature (T_F)
		{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
		{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
		});

	compressor.perfMap.push_back({
		70, // Temperature (T_F)
		{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
		{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
		});

	compressor.minT = F_TO_C(45.);
	compressor.maxT = F_TO_C(120.);
	compressor.hysteresis_dC = dF_TO_dC(2);
	compressor.configuration = HeatSource::CONFIG_WRAPPED;
	compressor.maxSetpoint_C = MAXOUTLET_R134A;

	//top resistor values
	resistiveElementTop.setupAsResistiveElement(6, 4500);
	resistiveElementTop.isVIP = true;

	//bottom resistor values
	resistiveElementBottom.setupAsResistiveElement(0, 4000);
	resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

	//logic conditions
	//this is set customly, from input
	//resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(19.6605)));
	resistiveElementTop.addTurnOnLogic(HPWH::topThird(resUse_C));

	resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(86.1111)));

	compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
	compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(12.392)));

	//custom adjustment for poorer performance
	//compressor.addShutOffLogic(HPWH::lowT(F_TO_C(37)));


	//end section of parameters from GE model




	//set tank volume from input
	//use tank size setting function since it has bounds checking
	int failure = this->setTankSize(tankVol_L);
	if (failure == HPWH_ABORT) {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("Failure to set tank size in generic hpwh init.");
		}
		return failure;
	}


	// derive conservative (high) UA from tank volume
	//   curve fit by Jim Lutz, 5-25-2016
	double tankVol_gal = tankVol_L / GAL_TO_L(1.);
	double v1 = 7.5156316175 * pow(tankVol_gal, 0.33) + 5.9995357658;
	tankUA_kJperHrC = 0.0076183819 * v1 * v1;




	//do a linear interpolation to scale COP curve constant, using measured values
	// Chip's attempt 24-May-2014
	double uefSpan = 3.4 - 2.0;

	//force COP to be 70% of GE at UEF 2 and 95% at UEF 3.4
	//use a fudge factor to scale cop and input power in tandem to maintain constant capacity
	double fUEF = (energyFactor - 2.0) / uefSpan;
	double genericFudge = (1. - fUEF)*.7 + fUEF * .95;

	compressor.perfMap[0].COP_coeffs[0] *= genericFudge;
	compressor.perfMap[0].COP_coeffs[1] *= genericFudge;
	compressor.perfMap[0].COP_coeffs[2] *= genericFudge;

	compressor.perfMap[1].COP_coeffs[0] *= genericFudge;
	compressor.perfMap[1].COP_coeffs[1] *= genericFudge;
	compressor.perfMap[1].COP_coeffs[2] *= genericFudge;

	compressor.perfMap[0].inputPower_coeffs[0] /= genericFudge;
	compressor.perfMap[0].inputPower_coeffs[1] /= genericFudge;
	compressor.perfMap[0].inputPower_coeffs[2] /= genericFudge;

	compressor.perfMap[1].inputPower_coeffs[0] /= genericFudge;
	compressor.perfMap[1].inputPower_coeffs[1] /= genericFudge;
	compressor.perfMap[1].inputPower_coeffs[2] /= genericFudge;

	//set everything in its places
	setOfSources[0] = resistiveElementTop;
	setOfSources[1] = resistiveElementBottom;
	setOfSources[2] = compressor;

	//and you have to do this after putting them into setOfSources, otherwise
	//you don't get the right pointers
	setOfSources[2].backupHeatSource = &setOfSources[1];
	setOfSources[1].backupHeatSource = &setOfSources[2];

	setOfSources[0].followedByHeatSource = &setOfSources[1];
	setOfSources[1].followedByHeatSource = &setOfSources[2];



	//standard finishing up init, borrowed from init function

	hpwhModel = MODELS_genericCustomUEF;

	//calculate oft-used derived values
	calcDerivedValues();

	if (checkInputs() == HPWH_ABORT) {
		return HPWH_ABORT;
	}

	isHeating = false;
	for (int i = 0; i < numHeatSources; i++) {
		if (setOfSources[i].isOn) {
			isHeating = true;
		}
		setOfSources[i].sortPerformanceMap();
	}

	if (hpwhVerbosity >= VRB_emetic) {
		for (int i = 0; i < numHeatSources; i++) {
			msg("heat source %d: %p \n", i, &setOfSources[i]);
		}
		msg("\n\n");
	}

	simHasFailed = false;

	return 0;
}


int HPWH::HPWHinit_presets(MODELS presetNum) {

	setAllDefaults(); // reset all defaults if you're re-initilizing
	// sets simHasFailed = true; this gets cleared on successful completion of init
	// return 0 on success, HPWH_ABORT for failure

	//resistive with no UA losses for testing
	if (presetNum == MODELS_restankNoUA) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankSizeFixed = false;
		tankVolume_L = GAL_TO_L(50);
		tankUA_kJperHrC = 0; //0 to turn off


		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 2;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementTop.setupAsResistiveElement(8, 4500);

		//standard logic conditions
		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40)));
		resistiveElementBottom.addTurnOnLogic(HPWH::standby(dF_TO_dC(10)));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.isVIP = true;

		//assign heat sources into array in order of priority
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;

		setOfSources[0].followedByHeatSource = &setOfSources[1];
	}

	//resistive tank with massive UA loss for testing
	else if (presetNum == MODELS_restankHugeUA) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = 50;

		tankSizeFixed = false;
		tankVolume_L = 120;
		tankUA_kJperHrC = 500; //0 to turn off

		doTempDepression = false;
		tankMixesOnDraw = false;


		numHeatSources = 2;
		setOfSources = new HeatSource[numHeatSources];

		//set up a resistive element at the bottom, 4500 kW
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementTop.setupAsResistiveElement(9, 4500);

		//standard logic conditions
		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(20));
		resistiveElementBottom.addTurnOnLogic(HPWH::standby(15));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(20));
		resistiveElementTop.isVIP = true;


		//assign heat sources into array in order of priority
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;

		setOfSources[0].followedByHeatSource = &setOfSources[1];

	}

	//realistic resistive tank
	else if (presetNum == MODELS_restankRealistic) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankSizeFixed = false;
		tankVolume_L = GAL_TO_L(50);
		tankUA_kJperHrC = 10; //0 to turn off

		doTempDepression = false;
		//should eventually put tankmixes to true when testing progresses
		tankMixesOnDraw = false;

		numHeatSources = 2;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementTop.setupAsResistiveElement(9, 4500);

		//standard logic conditions
		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(20));
		resistiveElementBottom.addTurnOnLogic(HPWH::standby(15));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(20));
		resistiveElementTop.isVIP = true;

		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;

		setOfSources[0].followedByHeatSource = &setOfSources[1];
	}

	else if (presetNum == MODELS_StorageTank) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = 52;

		setpoint_C = 800;

		tankSizeFixed = false;
		tankVolume_L = GAL_TO_L(80);
		tankUA_kJperHrC = 10; //0 to turn off

		doTempDepression = false;
		tankMixesOnDraw = false;

		////////////////////////////////////////////////////
		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];
		
		HeatSource extra(this);
		
		//compressor values
		extra.isOn = false;
		extra.isVIP = false;
		extra.typeOfHeatSource = TYPE_extra;
		extra.configuration = HeatSource::CONFIG_WRAPPED;
		
		extra.addTurnOnLogic(HPWH::topThird_absolute(1));

		//initial guess, will get reset based on the input heat vector
		extra.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		setOfSources[0] = extra;
	}

	//basic compressor tank for testing
	else if (presetNum == MODELS_basicIntegrated) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = 50;

		tankSizeFixed = false;
		tankVolume_L = 120;
		tankUA_kJperHrC = 10; //0 to turn off
		//tankUA_kJperHrC = 0; //0 to turn off

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);
		HeatSource compressor(this);

		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementTop.setupAsResistiveElement(9, 4500);

		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);

		//standard logic conditions
		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(20));
		resistiveElementBottom.addTurnOnLogic(HPWH::standby(15));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(20));
		resistiveElementTop.isVIP = true;


		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double oneSixth = 1.0 / 6.0;
		compressor.setCondensity(oneSixth, oneSixth, oneSixth, oneSixth, oneSixth, oneSixth, 0, 0, 0, 0, 0, 0);

		//GE tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{0.290 * 1000, 0.00159 * 1000, 0.00000107 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.49, -0.0187, -0.0000133} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{0.375 * 1000, 0.00121 * 1000, 0.00000216 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{5.60, -0.0252, 0.00000254} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = 0;
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(4);
		compressor.configuration = HeatSource::CONFIG_WRAPPED; //wrapped around tank
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.addTurnOnLogic(HPWH::bottomThird(20));
		compressor.addTurnOnLogic(HPWH::standby(15));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = compressor;
		setOfSources[2] = resistiveElementBottom;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}

	//simple external style for testing
	else if (presetNum == MODELS_externalTest) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = 50;

		tankSizeFixed = false;
		tankVolume_L = 120;
		//tankUA_kJperHrC = 10; //0 to turn off
		tankUA_kJperHrC = 0; //0 to turn off

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);

		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		//GE tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{0.290 * 1000, 0.00159 * 1000, 0.00000107 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.49, -0.0187, -0.0000133} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{0.375 * 1000, 0.00121 * 1000, 0.00000216 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{5.60, -0.0252, 0.00000254} // COP Coefficients (COP_coeffs)
			});

		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = 0;  //no hysteresis
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.addTurnOnLogic(HPWH::bottomThird(20));
		compressor.addTurnOnLogic(HPWH::standby(15));

		//lowT cutoff
		compressor.addShutOffLogic(HPWH::bottomNodeMaxTemp(20));


		//set everything in its places
		setOfSources[0] = compressor;
	}
	//voltex 60 gallon
	else if (presetNum == MODELS_AOSmithPHPT60) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 215.8;
		tankUA_kJperHrC = 7.31;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 5.0;
		compressor.setCondensity(split, split, split, split, split, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{0.467 * 1000, 0.00281 * 1000, 0.0000072 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.86, -0.0222, -0.00001} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{0.541 * 1000, 0.00147 * 1000, 0.0000176 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{6.58, -0.0392, 0.0000407} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(45.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(4);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4250);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 2000);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);

		//logic conditions
		double compStart = dF_TO_dC(43.6);
		double standbyT = dF_TO_dC(23.8);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(compStart));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(25.0)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = compressor;
		setOfSources[2] = resistiveElementBottom;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_AOSmithPHPT80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 283.9;
		tankUA_kJperHrC = 8.8;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 5.0;
		compressor.setCondensity(split, split, split, split, split, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{0.467 * 1000, 0.00281 * 1000, 0.0000072 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.86, -0.0222, -0.00001} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{0.541 * 1000, 0.00147 * 1000, 0.0000176 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{6.58, -0.0392, 0.0000407} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(45.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(4);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4250);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 2000);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);


		//logic conditions
		double compStart = dF_TO_dC(43.6);
		double standbyT = dF_TO_dC(23.8);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(compStart));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(25.0)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = compressor;
		setOfSources[2] = resistiveElementBottom;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_GE2012) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 172;
		tankUA_kJperHrC = 6.8;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 5.0;
		compressor.setCondensity(split, split, split, split, split, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{0.3 * 1000, 0.00159 * 1000, 0.00000107 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.7, -0.0210, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{0.378 * 1000, 0.00121 * 1000, 0.00000216 * 1000}, // Input Power Coefficients (inputPower_coeffs)
			{4.8, -0.0167, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(45.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(4);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4200);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4200);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);


		//logic conditions
		//    double compStart = dF_TO_dC(24.4);
		double compStart = dF_TO_dC(40.0);
		double standbyT = dF_TO_dC(5.2);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));
		// compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(66)));
		compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(65)));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(compStart));
		//resistiveElementBottom.addShutOffLogic(HPWH::lowTreheat(lowTcutoff));
		//GE element never turns off?

		// resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(31.0)));
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(28.0)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = compressor;
		setOfSources[2] = resistiveElementBottom;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	// If a Colmac single pass preset cold weather or not
	else if (MODELS_ColmacCxV_5_SP <= presetNum && presetNum <= MODELS_ColmacCxA_30_SP) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7; 

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.perfMap.reserve(1);
		compressor.hysteresis_dC = 0;

		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		//logic conditions
		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(15), this));

		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node", nodeWeights1, dF_TO_dC(15.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//Defrost Derate 
		compressor.setupDefrostMap();

		if (presetNum == MODELS_ColmacCxV_5_SP) {
			setTankSize_adjustUA(200., UNITS_GAL);
			//logic conditions
			compressor.minT = F_TO_C(-4.0);
			compressor.maxSetpoint_C = MAXOUTLET_R410A;

			compressor.perfMap.push_back({
				100, // Temperature (T_F)
				
				{ 4.9621645063, -0.0096084144, -0.0095647009, -0.0115911960, -0.0000788517, 0.0000886176,
				0.0001114142, 0.0001832377, -0.0000451308, 0.0000411975, 0.0000003535}, // Input Power Coefficients (inputPower_coeffs)

				{ 3.8189922420, 0.0569412237, -0.0320101962, -0.0012859036, 0.0000576439, 0.0001101241, 
				-0.0000352368, -0.0002630301, -0.0000509365, 0.0000369655, -0.0000000606} // COP Coefficients (COP_coeffs)
				});
		}
		else {
			//logic conditions
			compressor.minT = F_TO_C(40.);
			compressor.maxSetpoint_C = MAXOUTLET_R134A;

			if (presetNum == MODELS_ColmacCxA_10_SP) {
				setTankSize_adjustUA(500., UNITS_GAL);

				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 5.9786974243, 0.0194445115, -0.0077802278, 0.0053809029, -0.0000334832, 0.0001864310, 0.0001190540,
					0.0000040405, -0.0002538279, -0.0000477652, 0.0000014101}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 3.6128563086, 0.0527064498, -0.0278198945, -0.0070529748, 0.0000934705, 0.0000781711, -0.0000359215,
					-0.0002223206, 0.0000359239, 0.0000727189, -0.0000005037} // COP Coefficients (COP_coeffs)
				});

			}
			else if (presetNum == MODELS_ColmacCxA_15_SP) {
				setTankSize_adjustUA(600., UNITS_GAL);

				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 15.5869846555, -0.0044503761, -0.0577941202, -0.0286911185, -0.0000803325, 0.0003399817,
					0.0002009576, 0.0002494761, -0.0000595773, 0.0001401800, 0.0000004312}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 1.6643120405, 0.0515623393, -0.0110239930, 0.0041514430, 0.0000481544, 0.0000493424,
					-0.0000262721, -0.0002356218, -0.0000989625, -0.0000070572, 0.0000004108} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_20_SP) {
				setTankSize_adjustUA(800., UNITS_GAL);

				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 23.0746692231, 0.0248584608, -0.1417927282, -0.0253733303, -0.0004882754, 0.0006508079, 
					0.0002139934, 0.0005552752, -0.0002026772, 0.0000607338, 0.0000021571}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 1.7692660120, 0.0525134783, -0.0081102040, -0.0008715405, 0.0001274956, 0.0000369489, 
					-0.0000293775, -0.0002778086, -0.0000095067, 0.0000381186, -0.0000003135} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_25_SP) {
				setTankSize_adjustUA(1000., UNITS_GAL);

				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 20.4185336541, -0.0236920615, -0.0736219119, -0.0260385082, -0.0005048074, 0.0004940510,
					0.0002632660, 0.0009820050, -0.0000223587, 0.0000885101, 0.0000005649}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 0.8942843854, 0.0677641611, -0.0001582927, 0.0048083998, 0.0001196407, 0.0000334921, 
					-0.0000378740, -0.0004146401, -0.0001213363, -0.0000031856, 0.0000006306} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_30_SP) {
				setTankSize_adjustUA(1200., UNITS_GAL);

				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 11.3687485772, -0.0207292362, 0.0496254077, -0.0038394967, -0.0005991041, 0.0001304318,
					0.0003099774, 0.0012092717, -0.0001455509, -0.0000893889, 0.0000018221}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 4.4170108542, 0.0596384263, -0.0416104579, -0.0017199887, 0.0000774664, 0.0001521934,
					-0.0000251665, -0.0003289731, -0.0000801823, 0.0000325972, 0.0000002705} // COP Coefficients (COP_coeffs)
					});
			}
		} //End if MODELS_ColmacCxV_5_SP

		//set everything in its places
		setOfSources[0] = compressor;
	}
	

	// if colmac multipass
	else if (MODELS_ColmacCxV_5_MP <= presetNum && presetNum <= MODELS_ColmacCxA_30_MP) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.perfMap.reserve(1);
		compressor.hysteresis_dC = 0;
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = (int)(numNodes / 3.) - 1;

		//logic conditions
		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(5.), this));

		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(4);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights1, dF_TO_dC(0.), this, false, std::greater<double>()));

		compressor.depressesTemperature = false;  //no temp depression

		//Defrost Derate 
		compressor.setupDefrostMap();

		if (presetNum == MODELS_ColmacCxV_5_MP) {
			setTankSize_adjustUA(200., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(9.); //https://colmacwaterheat.com/wp-content/uploads/2020/10/Technical-Datasheet-Air-Source.pdf

			//logic conditions
			compressor.minT = F_TO_C(-4.0);
			compressor.maxT = F_TO_C(105.);
			compressor.maxSetpoint_C = MAXOUTLET_R410A;
			compressor.perfMap.push_back({
				100, // Temperature (T_F)
				
				{ 5.8438525529, 0.0003288231, -0.0494255840, -0.0000386642, 0.0004385362, 0.0000647268}, // Input Power Coefficients (inputPower_coeffs)
				
				{ 0.6679056901, 0.0499777846, 0.0251828292, 0.0000699764, -0.0001552229, -0.0002911167} // COP Coefficients (COP_coeffs)
				});
		}
		else {
			//logic conditions
			compressor.minT = F_TO_C(40.);
			compressor.maxT = F_TO_C(105.);
			compressor.maxSetpoint_C = MAXOUTLET_R134A;

			if (presetNum == MODELS_ColmacCxA_10_MP) {
				setTankSize_adjustUA(500., UNITS_GAL);
				compressor.mpFlowRate_LPS = GPM_TO_LPS(18.);
				compressor.perfMap.push_back({
					100, // Temperature (T_F) 
					
					{ 8.6918824405, 0.0136666667, -0.0548348214, -0.0000208333, 0.0005301339, -0.0000250000}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 0.6944181117, 0.0445926666, 0.0213188804, 0.0001172913, -0.0001387694, -0.0002365885} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_15_MP) {
				setTankSize_adjustUA(600., UNITS_GAL);
				compressor.mpFlowRate_LPS = GPM_TO_LPS(26.);
				compressor.perfMap.push_back({ 
					100, // Temperature (T_F)
					
					{ 12.4908723958, 0.0073988095, -0.0411417411, 0.0000000000, 0.0005789621, 0.0000696429}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 1.2846349520, 0.0334658309, 0.0019121906, 0.0002840970, 0.0000497136, -0.0004401737} // COP Coefficients (COP_coeffs)

					});

			}
			else if (presetNum == MODELS_ColmacCxA_20_MP) {
				setTankSize_adjustUA(800., UNITS_GAL);
				compressor.mpFlowRate_LPS = GPM_TO_LPS(36.); //https://colmacwaterheat.com/wp-content/uploads/2020/10/Technical-Datasheet-Air-Source.pdf

				compressor.perfMap.push_back({ 
					100, // Temperature (T_F)
					
					{ 14.4893345424, 0.0355357143, -0.0476593192, -0.0002916667, 0.0006120954, 0.0003607143}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 1.2421582831, 0.0450256569, 0.0051234755, 0.0001271296, -0.0000299981, -0.0002910606} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_25_MP) {
				setTankSize_adjustUA(1000., UNITS_GAL);
				compressor.mpFlowRate_LPS = GPM_TO_LPS(32.);
				compressor.perfMap.push_back({ 
					100, // Temperature (T_F)
				
					{ 14.5805808222, 0.0081934524, -0.0216169085, -0.0001979167, 0.0007376535, 0.0004955357}, // Input Power Coefficients (inputPower_coeffs)
																											  
					{ 2.0013175767, 0.0576617432, -0.0130480870, 0.0000856818, 0.0000610760, -0.0003684106} // COP Coefficients (COP_coeffs)
					});

			}
			else if (presetNum == MODELS_ColmacCxA_30_MP) {
				setTankSize_adjustUA(1200., UNITS_GAL);
				compressor.mpFlowRate_LPS = GPM_TO_LPS(41.);
				compressor.perfMap.push_back({
					100, // Temperature (T_F)
					
					{ 14.5824911644, 0.0072083333, -0.0278055246, -0.0002916667, 0.0008841378, 0.0008125000}, // Input Power Coefficients (inputPower_coeffs)
					
					{ 2.6996807527, 0.0617507969, -0.0220966420, 0.0000336149, 0.0000890989, -0.0003682431} // COP Coefficients (COP_coeffs)
					});
			}
		}

		//set everything in its places
		setOfSources[0] = compressor;
	}
	// If Nyle single pass preset
	else if (MODELS_NyleC25A_SP <= presetNum && presetNum <= MODELS_NyleC250A_C_SP) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7; 
		
		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		//HeatSource resistiveElement(this);

		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.extrapolationMethod = EXTRAP_NEAREST;
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.perfMap.reserve(1);
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		//logic conditions
		if (MODELS_NyleC25A_SP <= presetNum && presetNum <= MODELS_NyleC250A_SP) {// If not cold weather package
			compressor.minT = F_TO_C(40.); // Min air temperature sans Cold Weather Package
		}
		else {
			compressor.minT = F_TO_C(35.);// Min air temperature WITH Cold Weather Package
		}
		compressor.maxT = F_TO_C(120.0); // Max air temperature
		compressor.hysteresis_dC = 0;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		// Defines the maximum outlet temperature at the a low air temperature
		compressor.maxOut_at_LowT.outT_C = F_TO_C(140.);  
		compressor.maxOut_at_LowT.airT_C = F_TO_C(40.);

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(15), this, false));

		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node", nodeWeights1, dF_TO_dC(15.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression
		
		//Defrost Derate 
		compressor.setupDefrostMap();

		//Perfmaps for each compressor size
		if (presetNum == MODELS_NyleC25A_SP) {
			setTankSize_adjustUA(200., UNITS_GAL);
			compressor.perfMap.push_back({
					90, // Temperature (T_F)

					{4.060120364 ,-0.020584279,-0.024201054,-0.007023945,0.000017461,0.000110366,0.000060338,
					0.000120015,0.000111068,0.000138907,-0.000001569}, // Input Power Coefficients (inputPower_coeffs)

					{0.462979529,0.065656840,0.001077377,0.003428059,0.000243692,0.000021522,0.000005143,
					-0.000384778,-0.000404744,-0.000036277, 0.000001900 } // COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_NyleC60A_SP || presetNum == MODELS_NyleC60A_C_SP) {
			setTankSize_adjustUA(300., UNITS_GAL);
			compressor.perfMap.push_back({
					90, // Temperature (T_F)

					{-0.1180905709,0.0045354306,0.0314990479,-0.0406839757,0.0002355294,0.0000818684,0.0001943834,
					-0.0002160871,0.0003053633,0.0003612413,-0.0000035912}, // Input Power Coefficients (inputPower_coeffs)

					{6.8205043418,0.0860385185,-0.0748330699,-0.0172447955,0.0000510842,0.0002187441,-0.0000321036,
					-0.0003311463,-0.0002154270,0.0001307922,0.0000005568} // COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_NyleC90A_SP || presetNum == MODELS_NyleC90A_C_SP) {
			setTankSize_adjustUA(400., UNITS_GAL);
			compressor.perfMap.push_back({
					90, // Temperature (T_F)

					{13.27612215047,-0.01014009337,-0.13401028549,-0.02325705976,-0.00032515646,0.00040270625,
					0.00001988733,0.00069451670,0.00069067890,0.00071091372,-0.00000854352}, // Input Power Coefficients (inputPower_coeffs)

					{1.49112327987,0.06616282153,0.00715307252,-0.01269458185,0.00031448571,0.00001765313,
					0.00006002498,-0.00045661397,-0.00034003896,-0.00004327766,0.00000176015} // COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_NyleC125A_SP || presetNum == MODELS_NyleC125A_C_SP) {
			setTankSize_adjustUA(500., UNITS_GAL);
			compressor.perfMap.push_back({
					90, // Temperature (T_F)

					{-3.558277209, -0.038590968,	0.136307181, -0.016945699,0.000983753, -5.18201E-05,	
					0.000476904, -0.000514211, -0.000359172,	0.000266509, -1.58646E-07}, // Input Power Coefficients (inputPower_coeffs)

					{4.889555031,	0.117102769, -0.060005795, -0.011871234, -1.79926E-05,	0.000207293, 
					-1.4452E-05, -0.000492486, -0.000376814,	7.85911E-05,	1.47884E-06}// COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_NyleC185A_SP || presetNum == MODELS_NyleC185A_C_SP) {
			setTankSize_adjustUA(800., UNITS_GAL);
			compressor.perfMap.push_back({
					90, // Temperature (T_F)

					{18.58007733, -0.215324777, -0.089782421,	0.01503161,	0.000332503,	0.000274216,
					2.70498E-05,	0.001387914,	0.000449199,	0.000829578, -5.28641E-06}, // Input Power Coefficients (inputPower_coeffs)

					{-0.629432348,	0.181466663,	0.00044047,	0.012104957, -6.61515E-05,	9.29975E-05,	
						9.78042E-05, -0.000872708,-0.001013945, -0.00021852,	5.55444E-06}// COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_NyleC250A_SP || presetNum == MODELS_NyleC250A_C_SP) {
			setTankSize_adjustUA(800., UNITS_GAL);

			compressor.perfMap.push_back({
				90, // Temperature (T_F)

				{-13.89057656,0.025902417,0.304250541,0.061695153,-0.001474249,-0.001126845,-0.000220192,
				0.001241026,0.000571009,-0.000479282,9.04063E-06},// Input Power Coefficients (inputPower_coeffs)

				{7.443904067,0.185978755,-0.098481635,-0.002500073,0.000127658,0.000444321,0.000139547,
				-0.001000195,-0.001140199,-8.77557E-05,4.87405E-06} // COP Coefficients (COP_coeffs)
				});
		}

		//set everything in its places
		setOfSources[0] = compressor;
	}

	// If Nyle multipass presets
	else if (MODELS_NyleC60A_MP <= presetNum && presetNum <= MODELS_NyleC250A_C_MP) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.extrapolationMethod = EXTRAP_NEAREST;
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.hysteresis_dC = 0;
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = (int)(numNodes / 3.) - 1;

		//logic conditions//logic conditions
		if (MODELS_NyleC60A_MP <= presetNum && presetNum <= MODELS_NyleC250A_MP) {// If not cold weather package
			compressor.minT = F_TO_C(40.); // Min air temperature sans Cold Weather Package
		}
		else {
			compressor.minT = F_TO_C(35.);// Min air temperature WITH Cold Weather Package
		}
		compressor.maxT = F_TO_C(130.0); // Max air temperature
		compressor.maxSetpoint_C = F_TO_C(160.);

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(5.), this));

		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(4);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights1, dF_TO_dC(0.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//Defrost Derate 
		compressor.setupDefrostMap();

		// Performance grid
		compressor.perfGrid.reserve(2);
		compressor.perfGridValues.reserve(2);

		// Nyle MP models are all on the same grid axes
		compressor.perfGrid.push_back({ 40., 60., 80., 90. }); // Grid Axis 1 Tair (F)
		compressor.perfGrid.push_back({ 40., 60., 80., 100., 130., 150. }); // Grid Axis 2 Tin (F)

		if (presetNum == MODELS_NyleC60A_MP || presetNum == MODELS_NyleC60A_C_MP) {
			setTankSize_adjustUA(360., UNITS_GAL); 
			compressor.mpFlowRate_LPS = GPM_TO_LPS(13.);
			if (presetNum == MODELS_NyleC60A_C_MP) {
				compressor.resDefrost = {
								4.5,  // inputPwr_kW;
								5.0,  // constTempLift_dF;
								40.0  // onBelowT_F
							 };
			}
			// Grid values in long format, table 1, input power (W)
			compressor.perfGridValues.push_back({ 3.64, 4.11, 4.86, 5.97, 8.68, 9.95, 3.72, 4.27, 4.99, 6.03, 8.55, 10.02, 3.98, 4.53,
				5.24, 6.24, 8.54, 9.55, 4.45, 4.68, 5.37, 6.34, 8.59, 9.55
				});
			// Grid values in long format, table 2, COP
			compressor.perfGridValues.push_back({ 3.362637363, 2.917274939, 2.407407407, 1.907872697, 1.296082949, 1.095477387, 4.438172043,
				3.772833724, 3.132264529, 2.505804312, 1.678362573, 1.386227545, 5.467336683, 4.708609272, 3.921755725, 3.169871795, 2.165105386,
				1.860732984, 5.512359551, 5.153846154, 4.290502793, 3.417981073, 2.272409779, 1.927748691 
				});
		}
		else if (presetNum == MODELS_NyleC90A_MP || presetNum == MODELS_NyleC90A_C_MP) {
			setTankSize_adjustUA(480., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(20.);
			if (presetNum == MODELS_NyleC90A_C_MP) {
				compressor.resDefrost = {
								5.4,  // inputPwr_kW;
								5.0,  // constTempLift_dF;
								40.0  // onBelowT_F
							};
			}
			// Grid values in long format, table 1, input power (W)
			compressor.perfGridValues.push_back({ 4.41, 6.04, 7.24, 9.14, 12.23, 14.73, 4.78, 6.61, 7.74, 9.40, 12.47, 14.75,
				5.51, 6.66, 8.44, 9.95, 13.06, 15.35, 6.78, 7.79, 8.81, 10.01, 11.91, 13.35 
				});
			// Grid values in long format, table 2, COP
			compressor.perfGridValues.push_back({ 4.79138322, 3.473509934, 2.801104972, 2.177242888, 1.569910057, 1.272233537, 6.071129707, 
				4.264750378, 3.536175711, 2.827659574, 2.036086608, 1.666440678, 7.150635209, 5.659159159, 4.305687204, 3.493467337, 
				2.487748851, 2.018241042, 6.750737463, 5.604621309, 4.734392736, 3.94005994, 3.04534005, 2.558801498
				});

		}
		else if (presetNum == MODELS_NyleC125A_MP || presetNum == MODELS_NyleC125A_C_MP) {
			setTankSize_adjustUA(600., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(28.);
			if (presetNum == MODELS_NyleC125A_C_MP) {
				compressor.resDefrost = {
								9.0,  // inputPwr_kW;
								5.0,  // constTempLift_dF;
								40.0  // onBelowT_F
							};
			}
			// Grid values in long format, table 1, input power (W)
			compressor.perfGridValues.push_back({ 6.4, 7.72, 9.65, 12.54, 20.54, 24.69, 6.89, 8.28, 10.13, 12.85, 19.75, 24.39, 
				7.69, 9.07, 10.87, 13.44, 19.68, 22.35, 8.58, 9.5, 11.27, 13.69, 19.72, 22.4
				});
			// Grid values in long format, table 2, COP
			compressor.perfGridValues.push_back({ 4.2390625, 3.465025907, 2.718134715, 2.060606061, 1.247809153, 1.016605913, 
				5.374455733, 4.352657005, 3.453109576, 2.645136187, 1.66278481, 1.307093071, 6.503250975, 5.276736494, 4.229070837, 
				3.27827381, 2.113821138, 1.770469799, 6.657342657, 5.749473684, 4.612244898, 3.542731921, 2.221095335, 1.816964286
				});
		}
		else if (presetNum == MODELS_NyleC185A_MP || presetNum == MODELS_NyleC185A_C_MP) {
			setTankSize_adjustUA(960., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(40.);
			if (presetNum == MODELS_NyleC185A_C_MP) {
				compressor.resDefrost = {
								7.2,  // inputPwr_kW;
								5.0,  // constTempLift_dF;
								40.0  // onBelowTemp_F
							};
			}
			// Grid values in long format, table 1, input power (W)
			compressor.perfGridValues.push_back({ 7.57, 11.66, 14.05, 18.3, 25.04, 30.48, 6.99, 10.46, 14.28, 18.19, 26.24, 32.32,
				7.87, 12.04, 15.02, 18.81, 25.99, 31.26, 8.15, 12.46, 15.17, 18.95, 26.23, 31.62
				});
			// Grid values in long format, table 2, COP
			compressor.perfGridValues.push_back({ 5.531043593, 3.556603774, 2.918149466, 2.214754098, 1.590255591, 1.291010499, 
				8.010014306, 5.258126195, 3.778711485, 2.916437603, 1.964176829, 1.56404703, 9.65819568, 6.200166113, 4.792276964, 
				3.705475811, 2.561369758, 2.05950096, 10.26993865, 6.350722311, 5.04218853, 3.841688654, 2.574151735, 2.025616698
				});
		}
		else if (presetNum == MODELS_NyleC250A_MP || presetNum == MODELS_NyleC250A_C_MP) {
			setTankSize_adjustUA(960., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(50.);
			if (presetNum == MODELS_NyleC250A_C_MP) {
				compressor.resDefrost = {
								18.0,  // inputPwr_kW;
								5.0,  // constTempLift_dF;
								40.0  // onBelowT_F
							};
			}
			// Grid values in long format, table 1, input power (W)
			compressor.perfGridValues.push_back({ 10.89, 12.23, 13.55, 14.58, 15.74, 16.72, 11.46, 13.76, 15.97, 17.79, 
				20.56, 22.50, 10.36, 14.66, 18.07, 21.23, 25.81, 29.01, 8.67, 15.05, 18.76, 21.87, 26.63, 30.02 
				});

			// Grid values in long format, table 2, COP
			compressor.perfGridValues.push_back({ 5.81818181, 4.50040883, 3.69667896, 3.12414266, 2.38500635, 1.93540669, 
				7.24520069, 5.50145348, 4.39323732, 3.67734682, 2.73249027, 2.23911111, 10.6196911, 7.05320600, 5.41228555, 
				4.28638718, 3.04804339, 2.46053085, 14.7831603, 7.77903268, 5.71801705, 4.40237768, 2.92489673, 2.21419054 
				});

		}

		// Set up regular grid interpolator. 
		compressor.perfRGI = new Btwxt::RegularGridInterpolator(compressor.perfGrid, compressor.perfGridValues);
		compressor.useBtwxtGrid = true;

		//set everything in its places
		setOfSources[0] = compressor;
	}
	// if rheem multipass
	else if (MODELS_RHEEM_HPHD60HNU_201_MP <= presetNum && presetNum <= MODELS_RHEEM_HPHD135VNU_483_MP) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.perfMap.reserve(1);
		compressor.hysteresis_dC = 0;
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = (int)(numNodes / 3.) - 1;

		//logic conditions
		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(5.), this));

		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(4);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights1, dF_TO_dC(0.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//Defrost Derate 
		compressor.setupDefrostMap();

		//logic conditions
		compressor.minT = F_TO_C(45.);
		compressor.maxT = F_TO_C(110.);
		compressor.maxSetpoint_C = MAXOUTLET_R134A; // data says 150...

		if (presetNum == MODELS_RHEEM_HPHD60HNU_201_MP || presetNum == MODELS_RHEEM_HPHD60VNU_201_MP) {
			setTankSize_adjustUA(250., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(17.4);
			compressor.perfMap.push_back({
				110, // Temperature (T_F)

				{ 1.8558438453, 0.0120796155, -0.0135443327, 0.0000059621, 0.0003010506, -0.0000463525}, // Input Power Coefficients (inputPower_coeffs)
				
				{ 3.6840046360, 0.0995685071, -0.0398107723, -0.0001903160, 0.0000980361, -0.0003469814} // COP Coefficients (COP_coeffs)
				});
		}
		else if (presetNum == MODELS_RHEEM_HPHD135HNU_483_MP || presetNum == MODELS_RHEEM_HPHD135VNU_483_MP) {
			setTankSize_adjustUA(500., UNITS_GAL);
			compressor.mpFlowRate_LPS = GPM_TO_LPS(34.87);
			compressor.perfMap.push_back({
				110, // Temperature (T_F)
				
				{ 5.1838201136, 0.0247312962, -0.0120766440, 0.0000493862, 0.0005422089, -0.0001385078}, // Input Power Coefficients (inputPower_coeffs)
				
				{ 5.0207181209, 0.0442525790, -0.0418284882, 0.0000793531, 0.0001132421, -0.0002491563} // COP Coefficients (COP_coeffs)
				});
		}

		//set everything in its places
		setOfSources[0] = compressor;
	}

	else if (presetNum == MODELS_MITSUBISHI_QAHV_N136TAU_HPB_SP) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = 65;

		tankVolume_L = GAL_TO_L(500);
		tankUA_kJperHrC = 12;
		tankSizeFixed = false;

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);

		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.minT = F_TO_C(-13.);
		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		// What to do about these?!
		compressor.hysteresis_dC = 4; 
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.maxSetpoint_C = F_TO_C(176.1);


		// Turn on
		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("eighth node absolute", nodeWeights, F_TO_C(110.), this, true));

		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node", nodeWeights1, dF_TO_dC(15.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		// Performance grid: externalT_F, Tout_F, condenserTemp_F
		compressor.perfGrid.reserve(2);
		compressor.perfGridValues.reserve(2);
		compressor.perfGrid.push_back({ -13, -11.2, -7.6, -4, -0.4, 3.2, 6.8, 10.4, 14, 17.6, 21.2, 24.8, 28.4, 32, 35.6, 39.2, 
			42.8, 46.4, 50, 53.6, 57.2, 60.8, 64.4, 68, 71.6, 75.2, 78.8, 82.4, 86, 89.6, 93.2, 96.8, 100.4, 104}); // Grid Axis 1 Tair (F)
		compressor.perfGrid.push_back({ 140., 158., 176. }); // Grid Axis 2 Tout (F)
		compressor.perfGrid.push_back({ 41, 48.2, 62.6, 75.2, 84.2}); // Grid Axis 3 Tin (F)

		// Grid values in long format, table 1, input power (Btu/hr)
		compressor.perfGridValues.push_back({ 56518.565328, 57130.739544, 57094.73612, 57166.756616, 57238.777112, 56518.565328, 57130.739544, 57094.73612, 57166.756616, 57238.777112, 
			58061.348896, 58360.24692, 58591.39286, 58870.368216, 59093.547136, 56626.5960719999, 57202.763452, 57310.794196, 57310.794196, 57490.848848, 56626.5960719999, 57202.763452,
			57310.794196, 57310.794196, 57490.848848, 58280.539188, 58519.65556, 58786.67868, 59093.547136, 59372.5190799999, 56950.695128, 57850.95474, 57814.947904, 57814.947904, 
			57922.978648, 56950.695128, 57850.95474, 57814.947904, 57814.947904, 57922.978648, 58679.067612, 58977.969048, 59372.5190799999, 59651.494436, 59930.46638, 57418.831764, 
			58175.053796, 58283.08454, 58247.07088, 58427.12212, 57418.831764, 58175.053796, 58283.08454, 58247.07088, 58427.12212, 59137.384512, 59376.504296, 59902.566456, 60265.23476,
			60516.313604, 57778.930832, 58643.190432, 58823.23826, 58643.190432, 58967.282664, 57778.930832, 58643.190432, 58823.23826, 58643.190432, 58967.282664, 59515.99368, 59954.377676,
			60321.031196, 60934.77152, 61213.743464, 58103.029888, 59075.313408, 59219.357812, 59291.374896, 59579.45688, 58103.029888, 59075.313408, 59219.357812, 59291.374896, 59579.45688, 
			60014.159328, 60273.205192, 60934.77152, 61436.922384, 61799.587276, 58535.152864, 59399.412464, 59795.525192, 59903.555936, 60299.672076, 58535.152864, 59399.412464, 59795.525192, 
			59903.555936, 60299.672076, 60512.321564, 60751.448172, 61409.02246, 62022.769608, 62329.641476, 58931.275828, 59651.4842, 60011.58668, 60263.66524, 60551.747224, 58931.275828, 
			59651.4842, 60011.58668, 60263.66524, 60551.747224, 60910.860224, 61369.1703, 62022.769608, 62580.713496, 62803.895828, 59219.357812, 60155.634496, 60443.71648, 60659.777968, 
			61163.92144, 59219.357812, 60155.634496, 60443.71648, 60659.777968, 61163.92144, 61329.321552, 61687.997816, 62441.224112, 63166.56072, 63585.015224, 59651.4842, 60479.726728, 
			61019.88386, 61163.92144, 61920.146884, 59651.4842, 60479.726728, 61019.88386, 61163.92144, 61920.146884, 61707.923896, 62186.166876, 63027.071336, 63724.504608, 64170.862448, 
			59723.504696, 60839.83262, 61379.993164, 61524.030744, 62352.273272, 59723.504696, 60839.83262, 61379.993164, 61524.030744, 62352.273272, 61966.96976, 62445.21274, 63361.839716, 
			64142.969348, 64505.63424, 59759.511532, 60695.788216, 61524.030744, 61884.136636, 62712.382576, 59759.511532, 60695.788216, 61524.030744, 61884.136636, 62712.382576, 62166.240796,
			62744.114176, 63668.714996, 64282.451908, 64589.323776, 59759.511532, 61019.88386, 61848.1298, 62064.191288, 62964.4509, 59759.511532, 61019.88386, 61848.1298, 62064.191288, 
			62964.4509, 62425.28666, 63003.16004, 63975.58004, 64533.523928, 64617.2237, 59471.429548, 61055.894108, 61956.160544, 62460.304016, 63216.526048, 59471.429548, 61055.894108, 
			61956.160544, 62460.304016, 63216.526048, 62624.550872, 63202.424252, 64310.351832, 64756.70626, 64728.806336, 61632.061488, 63036.474808, 63828.7105, 64296.847136, 65053.069168,
			61632.061488, 63036.474808, 63828.7105, 64296.847136, 65053.069168, 63561.10734, 64099.125148, 65258.8605359999, 65593.625504, 65621.525428, 51189.004268, 52917.506408, 54826.070024,
			57094.73612, 59327.388556, 51189.004268, 52917.506408, 54826.070024, 57094.73612, 59327.388556, 55391.172288, 58240.683616, 62106.459144, 65733.114888, 65705.214964, 41286.100768, 
			42726.524336, 45931.460292, 49784.590948, 53457.6669519999, 41286.100768, 42726.524336, 45931.460292, 49784.590948, 53457.6669519999, 49433.093532, 51067.083272, 57921.8561, 
			61604.3082799999, 64477.734316, 35632.444064, 37108.87788, 41394.134924, 45139.228012, 49568.526048, 35632.444064, 37108.87788, 41394.134924, 45139.228012, 49568.526048, 47540.06134,
			47221.233824, 56080.631716, 59539.904976, 61883.276812, 34192.023908, 35596.430404, 39809.6669519999, 43410.72246, 47659.969256, 34192.023908, 35596.430404, 39809.6669519999, 
			43410.72246, 47659.969256, 47559.98742, 47061.821772, 54490.482764, 57893.959588, 60153.648712, 32895.641332, 34192.023908, 38225.202392, 41646.20666, 45751.409052, 32895.641332, 
			34192.023908, 38225.202392, 41646.20666, 45751.409052, 47659.621232, 46842.624656, 52956.13366, 56220.1211, 58312.420916, 31563.25192, 32715.58668, 36532.707088, 39881.687448, 
			43914.869344, 31563.25192, 32715.58668, 36532.707088, 39881.687448, 43914.869344, 47719.402884, 46742.994256, 51393.877808, 54518.382688, 56554.886068, 30194.85226, 31347.18702, 
			35092.286932, 38117.171648, 42006.312552, 30194.85226, 31347.18702, 35092.286932, 38117.171648, 42006.312552, 47799.1072039999, 46324.532928, 49692.139396, 52732.951328, 54769.45812,
			30446.923996, 31383.197268, 34300.054652, 37144.884716, 40781.953884, 30446.923996, 31383.197268, 34300.054652, 37144.884716, 40781.953884, 45547.391924, 44212.307032, 47320.867636, 
			50389.57608, 52202.90054, 30698.995732, 31311.176772, 33723.88386, 36316.6456, 39701.636208, 30698.995732, 31311.176772, 33723.88386, 36316.6456, 39701.636208, 43295.680056, 
			41980.517832, 45089.078436, 47795.121988, 49720.03932, 30987.081128, 31311.176772, 33183.726728, 35344.358668, 38477.27754, 30987.081128, 31311.176772, 33183.726728, 35344.358668, 
			38477.27754, 41562.059916, 40486.017476, 43415.236536, 46037.590552, 47795.121988, 31203.146028, 31311.176772, 32571.545688, 34552.126388, 37360.949616, 31203.146028, 31311.176772, 
			32571.545688, 34552.126388, 37360.949616, 40486.017476, 39310.3446359999, 42159.859376, 44782.20998, 46511.844904, 31167.132368, 31311.176772, 32355.4842, 34156.010248, 36712.758328,
			31167.132368, 31311.176772, 32355.4842, 34156.010248, 36712.758328, 39449.830608, 38353.862088, 41127.657724, 43666.31538, 45284.360844, 31059.101624, 31131.12212, 32319.473952,
			34156.010248, 36748.771988, 31059.101624, 31131.12212, 32319.473952, 34156.010248, 36748.771988, 38592.985284, 37457.161192, 40151.249096, 42801.496212, 44419.541676, 30879.050384,
			31023.091376, 32319.473952, 34192.023908, 36820.789072, 30879.050384, 31023.091376, 32319.473952, 34192.023908, 36820.789072, 37756.062628, 36680.0236, 39258.536828, 41797.194484,
			43387.343436, 30807.029888, 30951.07088, 32427.504696, 34156.010248, 36748.771988, 30807.029888, 30951.07088, 32427.504696, 34156.010248, 36748.771988, 37795.9182, 36779.657412, 
			39258.536828, 41769.29456, 43331.547, 30626.976942, 30735.007686, 32319.473952, 34156.010248, 36748.771988, 30626.976942, 30735.007686, 32319.473952, 34156.010248, 36748.771988, 
			37815.84428, 36779.657412, 39286.431634, 41825.090996, 43359.446924, 30446.923996, 30518.944492, 32355.4842, 34264.040992, 36820.789072, 30446.923996, 30518.944492, 32355.4842, 
			34264.040992, 36820.789072, 37775.988708, 36759.731332, 39286.431634, 41825.090996, 43359.446924, 30230.859096, 30410.913748, 32319.473952, 34192.023908, 36748.771988, 30230.859096, 
			30410.913748, 32319.473952, 34192.023908, 36748.771988, 37775.988708, 36759.731332, 39342.226364, 41825.090996, 43303.650488, 30194.85226, 30194.85226, 32391.491036, 34156.010248,
			36748.771988, 30194.85226, 30194.85226, 32391.491036, 34156.010248, 36748.771988, 37756.062628, 36779.657412, 39342.226364, 41825.090996, 43359.446924
			});

		// Grid values in long format, table 2, COP
		compressor.perfGridValues.push_back({ 1.177126, 1.1393, 1.091664, 1.033858, 0.981755, 1.177126, 1.1393, 1.091664, 1.033858, 0.981755, 1.134534, 1.106615, 1.04928, 0.989101, 0.946182, 1.228935, 
			1.190326, 1.136507, 1.075244, 1.023802, 1.228935, 1.190326, 1.136507, 1.075244, 1.023802, 1.174944, 1.147101, 1.087318, 1.026909, 0.980265, 1.324165, 1.27451, 1.218468, 1.156182, 1.103823,
			1.324165, 1.27451, 1.218468, 1.156182, 1.103823, 1.267595, 1.236929, 1.165864, 1.102888, 1.052601, 1.415804, 1.365212, 1.301359, 1.238022, 1.180586, 1.415804, 1.365212, 1.301359, 1.238022,
			1.180586, 1.358408, 1.324943, 1.249272, 1.179609, 1.128155, 1.510855, 1.453792, 1.381237, 1.31747, 1.253435, 1.510855, 1.453792, 1.381237, 1.31747, 1.253435, 1.446639, 1.404653, 1.331368, 
			1.249056, 1.195511, 1.60469, 1.540079, 1.462451, 1.39417, 1.326987, 1.60469, 1.540079, 1.462451, 1.39417, 1.326987, 1.529924, 1.492107, 1.404944, 1.325122, 1.265433, 1.690249, 1.630494,
			1.539446, 1.464082, 1.395939, 1.690249, 1.630494, 1.539446, 1.464082, 1.395939, 1.610302, 1.56761, 1.479273, 1.393118, 1.33076, 1.776046, 1.715967, 1.624662, 1.540783, 1.469819, 1.776046,
			1.715967, 1.624662, 1.540783, 1.469819, 1.693465, 1.646725, 1.550545, 1.459601, 1.400666, 1.865309, 1.797965, 1.703902, 1.612645, 1.539888, 1.865309, 1.797965, 1.703902, 1.612645, 1.539888,
			1.776866, 1.728095, 1.626829, 1.531743, 1.461555, 1.956837, 1.881215, 1.777073, 1.690021, 1.603082, 1.956837, 1.881215, 1.777073, 1.690021, 1.603082, 1.857512, 1.807899, 1.699347, 1.599321,
			1.526899, 2.038891, 1.956496, 1.848049, 1.757975, 1.668207, 2.038891, 1.956496, 1.848049, 1.757975, 1.668207, 1.934721, 1.878022, 1.764777, 1.657606, 1.583414, 2.110576, 2.028182, 1.922739,
			1.818155, 1.725237, 2.110576, 2.028182, 1.922739, 1.818155, 1.725237, 1.997516, 1.937435, 1.824625, 1.712596, 1.630169, 2.185899, 2.091178, 1.989083, 1.883087, 1.786388, 2.185899, 2.091178,
			1.989083, 1.883087, 1.786388, 2.06464, 2.003084, 1.88041, 1.763428, 1.68041, 2.29337, 2.155411, 2.063354, 1.942635, 1.844204, 2.29337, 2.155411, 2.063354, 1.942635, 1.844204, 2.125448, 2.061323,
			1.933955, 1.810339, 1.720612, 2.213555, 2.111682, 2.027503, 1.91515, 1.819817, 2.213555, 2.111682, 2.027503, 1.91515, 1.819817, 2.126499, 2.064584, 1.935343, 1.818713, 1.728239, 2.665142,
			2.578088, 2.488506, 2.388206, 2.29651, 2.665142, 2.578088, 2.488506, 2.388206, 2.29651, 2.46407, 2.344111, 2.198428, 2.057188, 1.96338, 3.304405, 3.191319, 2.971384, 2.741049, 2.550691, 3.304405,
			3.191319, 2.971384, 2.741049, 2.550691, 2.763177, 2.673398, 2.356773, 2.216348, 2.116712, 3.827691, 3.676371, 3.297085, 3.022338, 2.752997, 3.827691, 3.676371, 3.297085, 3.022338, 2.752997,
			2.871739, 2.890389, 2.434648, 2.292726, 2.205906, 3.989995, 3.834598, 3.428313, 3.14185, 2.862486, 3.989995, 3.834598, 3.428313, 3.14185, 2.862486, 2.871269, 2.900921, 2.506208, 2.358391, 
			2.270261, 4.147236, 3.992101, 3.570419, 3.274967, 2.982684, 4.147236, 3.992101, 3.570419, 3.274967, 2.982684, 2.865266, 2.913751, 2.578296, 2.428607, 2.341945, 4.322305, 4.172262, 3.73583, 
			3.419865, 3.107421, 4.322305, 4.172262, 3.73583, 3.419865, 3.107421, 2.861677, 2.919962, 2.65667, 2.504413, 2.414725, 4.518187, 4.354394, 3.889174, 3.578177, 3.248607, 4.518187, 4.354394, 3.889174,
			3.578177, 3.248607, 2.856905, 2.946338, 2.747649, 2.589208, 2.493442, 4.481963, 4.349398, 3.979002, 3.671837, 3.346137, 4.481963, 4.349398, 3.979002, 3.671837, 3.346137, 2.998141, 3.087098, 2.885335,
			2.709619, 2.616032, 4.445162, 4.359402, 4.046983, 3.755577, 3.437188, 4.445162, 4.359402, 4.046983, 3.755577, 3.437188, 3.154067, 3.251216, 3.028152, 2.856705, 2.746669, 4.402673, 4.359402, 4.112859,
			3.858889, 3.546561, 4.402673, 4.359402, 4.112859, 3.858889, 3.546561, 3.285629, 3.371232, 3.1449, 2.965763, 2.857289, 4.373341, 4.359402, 4.19016, 3.947368, 3.65253, 4.373341, 4.359402, 4.19016, 
			3.947368, 3.65253, 3.372955, 3.472057, 3.238544, 3.048902, 2.936122, 4.378394, 4.359402, 4.218141, 3.993147, 3.717018, 4.378394, 4.359402, 4.218141, 3.993147, 3.717018, 3.461548, 3.558644, 3.319824,
			3.126817, 3.015709, 4.391304, 4.384616, 4.222841, 3.993147, 3.713376, 4.391304, 4.384616, 4.222841, 3.993147, 3.713376, 3.538402, 3.643836, 3.400556, 3.189995, 3.074423, 4.419242, 4.399884, 4.222841,
			3.988941, 3.706113, 4.419242, 4.399884, 4.222841, 3.988941, 3.706113, 3.616836, 3.721038, 3.477882, 3.266644, 3.147565, 4.429573, 4.410122, 4.208773, 3.993147, 3.713376, 4.429573, 4.410122, 4.208773,
			3.993147, 3.713376, 3.613022, 3.710957, 3.477882, 3.268826, 3.151618, 4.45679, 4.441125, 4.222841, 3.993147, 3.713376, 4.45679, 4.441125, 4.222841, 3.993147, 3.713376, 3.611119, 3.710957, 3.475413,
			3.264466, 3.14959, 4.483146, 4.472566, 4.218141, 3.980557, 3.706113, 4.483146, 4.472566, 4.218141, 3.980557, 3.706113, 3.614928, 3.712969, 3.475413, 3.264466, 3.14959, 4.515188, 4.488454, 4.222841,
			3.988941, 3.713376, 4.515188, 4.488454, 4.222841, 3.988941, 3.713376, 3.614928, 3.712969, 3.470484, 3.264466, 3.153648, 4.522957, 4.520572, 4.213452, 3.993147, 3.713376, 4.522957, 4.520572, 4.213452,
			3.993147, 3.713376, 3.616836, 3.710957, 3.470484, 3.264466, 3.14959
			});

		// Set up regular grid interpolator. 
		Btwxt::GriddedData gridded_data(compressor.perfGrid, compressor.perfGridValues);
		gridded_data.set_axis_extrap_method(2, Btwxt::Method::LINEAR); //Linearly extrapolate on Tin (F)
		compressor.perfRGI = new Btwxt::RegularGridInterpolator(gridded_data);
		compressor.useBtwxtGrid = true;

		//set everything in its places
		setOfSources[0] = compressor;

	}

	else if (presetNum == MODELS_Sanden80 || presetNum == MODELS_Sanden_GS3_45HPA_US_SP || presetNum == MODELS_Sanden120) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = 65;
		setpointFixed = true;

		if (presetNum == MODELS_Sanden120) {
			tankVolume_L = GAL_TO_L(119);
			tankUA_kJperHrC = 9;
		}
		else {
			tankVolume_L = 315;
			tankUA_kJperHrC = 7;
			if (presetNum == MODELS_Sanden_GS3_45HPA_US_SP) {
				tankSizeFixed = false;
			}
		}

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);

		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.minT = F_TO_C(-25.);
		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		compressor.perfMap.reserve(5);

		compressor.perfMap.push_back({
			17, // Temperature (T_F)
			{1650, 5.5, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{3.2, -0.015, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			35, // Temperature (T_F)
			{1100, 4.0, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{3.7, -0.015, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{880, 3.1, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.25, -0.025, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{740, 4.0, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{6.2, -0.03, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{790, 2, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.15, -0.04, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.hysteresis_dC = 4;
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.maxSetpoint_C = MAXOUTLET_R744;

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(8);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("eighth node absolute", nodeWeights, F_TO_C(113), this, true));
		if (presetNum == MODELS_Sanden80 || presetNum == MODELS_Sanden120) {
			compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(8.2639)));
			// Adds a bonus standby logic so the external heater does not cycle, recommended for any external heater with standby
			std::vector<NodeWeight> nodeWeightStandby;
			nodeWeightStandby.emplace_back(0);
			compressor.standbyLogic = std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node absolute", nodeWeightStandby, F_TO_C(113), this, true, std::greater<double>());
		}
		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node absolute", nodeWeights1, F_TO_C(135), this, true, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//set everything in its places
		setOfSources[0] = compressor;
	}
	else if (presetNum == MODELS_Sanden40) {
		numNodes = 96;
		tankTemps_C = new double[numNodes];
		setpoint_C = 65;
		setpointFixed = true;

		tankVolume_L = 160;
		//tankUA_kJperHrC = 10; //0 to turn off
		tankUA_kJperHrC = 5;

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);

		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.minT = F_TO_C(-25.);
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(5);

		compressor.perfMap.push_back({
			17, // Temperature (T_F)
			{1650, 5.5, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{3.2, -0.015, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			35, // Temperature (T_F)
			{1100, 4.0, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{3.7, -0.015, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{880, 3.1, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.25, -0.025, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{740, 4.0, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{6.2, -0.03, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{790, 2, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.15, -0.04, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.hysteresis_dC = 4;
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.maxSetpoint_C = MAXOUTLET_R744;

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		std::vector<NodeWeight> nodeWeightStandby;
		nodeWeightStandby.emplace_back(0);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node absolute", nodeWeights, F_TO_C(113), this, true));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(8.2639)));
		compressor.standbyLogic = std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node absolute", nodeWeightStandby, F_TO_C(113), this, true, std::greater<double>());

		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom twelth absolute", nodeWeights1, F_TO_C(135), this, true, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//set everything in its places
		setOfSources[0] = compressor;
	}
	else if (presetNum == MODELS_AOSmithHPTU50 || presetNum == MODELS_RheemHBDR2250 || presetNum == MODELS_RheemHBDR4550) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 171;
		tankUA_kJperHrC = 6;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 5.0;
		compressor.setCondensity(split, split, split, split, split, 0, 0, 0, 0, 0, 0, 0);

		// performance map
		compressor.perfMap.reserve(3);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{170, 2.02, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.93, -0.027, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{144.5, 2.42, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.67, -0.037, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{94.1, 3.15, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{11.1, -0.056, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(42.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		if (presetNum == MODELS_RheemHBDR2250) {
			resistiveElementTop.setupAsResistiveElement(8, 2250);
		}
		else {
			resistiveElementTop.setupAsResistiveElement(8, 4500);
		}
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		if (presetNum == MODELS_RheemHBDR2250) {
			resistiveElementBottom.setupAsResistiveElement(0, 2250);
		}
		else {
			resistiveElementBottom.setupAsResistiveElement(0, 4500);
		}
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		double compStart = dF_TO_dC(35);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(11); nodeWeights.emplace_back(12);
		resistiveElementTop.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("top sixth absolute", nodeWeights, F_TO_C(105), this, true));
		//		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(28)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];


	}
	else if (presetNum == MODELS_AOSmithHPTU66 || presetNum == MODELS_RheemHBDR2265 || presetNum == MODELS_RheemHBDR4565) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		if (presetNum == MODELS_AOSmithHPTU66) {
			tankVolume_L = 244.6;
		}
		else {
			tankVolume_L = 221.4;
		}
		tankUA_kJperHrC = 8;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		// performance map
		compressor.perfMap.reserve(3);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{170, 2.02, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.93, -0.027, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{144.5, 2.42, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.67, -0.037, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{94.1, 3.15, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{11.1, -0.056, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(42.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		if (presetNum == MODELS_RheemHBDR2265) {
			resistiveElementTop.setupAsResistiveElement(8, 2250);
		}
		else {
			resistiveElementTop.setupAsResistiveElement(8, 4500);
		}
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		if (presetNum == MODELS_RheemHBDR2265) {
			resistiveElementBottom.setupAsResistiveElement(0, 2250);
		}
		else {
			resistiveElementBottom.setupAsResistiveElement(0, 4500);
		}
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		double compStart = dF_TO_dC(35);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(11); nodeWeights.emplace_back(12);
		resistiveElementTop.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("top sixth absolute", nodeWeights, F_TO_C(105), this, true));
		//		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(31)));


				//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_AOSmithHPTU80 || presetNum == MODELS_RheemHBDR2280 || presetNum == MODELS_RheemHBDR4580) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 299.5;
		tankUA_kJperHrC = 9;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		double split = 1.0 / 3.0;
		compressor.setCondensity(split, split, split, 0, 0, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(3);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{170, 2.02, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.93, -0.027, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{144.5, 2.42, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.67, -0.037, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{94.1, 3.15, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{11.1, -0.056, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(42.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.hysteresis_dC = dF_TO_dC(1);

		//top resistor values
		if (presetNum == MODELS_RheemHBDR2280) {
			resistiveElementTop.setupAsResistiveElement(8, 2250);
		}
		else {
			resistiveElementTop.setupAsResistiveElement(8, 4500);
		}
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		if (presetNum == MODELS_RheemHBDR2280) {
			resistiveElementBottom.setupAsResistiveElement(0, 2250);
		}
		else {
			resistiveElementBottom.setupAsResistiveElement(0, 4500);
		}
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		double compStart = dF_TO_dC(35);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));

		std::vector<NodeWeight> nodeWeights;
		//		nodeWeights.emplace_back(9); nodeWeights.emplace_back(10);
		nodeWeights.emplace_back(11); nodeWeights.emplace_back(12);
		resistiveElementTop.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("top sixth absolute", nodeWeights, F_TO_C(105), this, true));
		//		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(35)));


				//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_AOSmithHPTU80_DR) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = 283.9;
		tankUA_kJperHrC = 9;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{142.6, 2.152, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{6.989258, -0.038320, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{120.14, 2.513, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{8.188, -0.0432, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(42.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		double compStart = dF_TO_dC(34.1636);
		double standbyT = dF_TO_dC(7.1528);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80.108)));

		// resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(39.9691)));
		resistiveElementTop.addTurnOnLogic(HPWH::topThird_absolute(F_TO_C(87)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_AOSmithCAHP120) {
	numNodes = 24;
	tankTemps_C = new double[numNodes];
	setpoint_C = F_TO_C(150.0);

	tankVolume_L = GAL_TO_L(111.76); // AOSmith docs say 111.76
	tankUA_kJperHrC = UAf_TO_UAc(3.94);

	doTempDepression = false;
	tankMixesOnDraw = false;

	numHeatSources = 3;
	setOfSources = new HeatSource[numHeatSources];

	HeatSource compressor(this);
	HeatSource resistiveElementBottom(this);
	HeatSource resistiveElementTop(this);

	//compressor values
	compressor.isOn = false;
	compressor.isVIP = false;
	compressor.typeOfHeatSource = TYPE_compressor;
	compressor.setCondensity(0.3, 0.3, 0.2, 0.1, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);

	//From CAHP 120 COP Tests
	compressor.perfMap.reserve(3);
	
	// Tuned on the multiple K167 tests
	compressor.perfMap.push_back({
		50., // Temperature (T_F)
		{2010.49966, -4.20966, 0.085395}, // Input Power Coefficients (inputPower_coeffs)
		{5.91, -0.026299, 0.0} // COP Coefficients (COP_coeffs)
		});

	compressor.perfMap.push_back({
		67.5, // Temperature (T_F)
		{2171.012, -6.936571, 0.1094962}, // Input Power Coefficients (inputPower_coeffs)
		{7.26272, -0.034135, 0.0} // COP Coefficients (COP_coeffs)
		});

	compressor.perfMap.push_back({
		95., // Temperature (T_F)
		{2276.0625, -7.106608, 0.119911}, // Input Power Coefficients (inputPower_coeffs)
		{8.821262, -0.042059, 0.0} // COP Coefficients (COP_coeffs)
		});

	compressor.minT = F_TO_C(47.0); //Product documentation says 45F doesn't look like it in CMP-T test//
	compressor.maxT = F_TO_C(110.0);
	compressor.hysteresis_dC = dF_TO_dC(2);
	compressor.configuration = HeatSource::CONFIG_WRAPPED;
	compressor.maxSetpoint_C = MAXOUTLET_R134A;

	//top resistor values
	double wattRE = 6000;//5650.; 
	resistiveElementTop.setupAsResistiveElement(7, wattRE);
	resistiveElementTop.isVIP = true; // VIP is the only source that turns on independently when something else is already heating.

	//bottom resistor values
	resistiveElementBottom.setupAsResistiveElement(0, wattRE);
	resistiveElementBottom.hysteresis_dC = dF_TO_dC(2); 
	resistiveElementBottom.setCondensity(0.2, 0.8, 0., 0., 0., 0., 0., 0., 0., 0., 0., 0.); // Based of CMP test

	//logic conditions for 	
	double compStart = dF_TO_dC(5.25); 
	double standbyT = dF_TO_dC(5.); //Given CMP_T test 
	compressor.addTurnOnLogic(HPWH::secondSixth(compStart));
	compressor.addTurnOnLogic(HPWH::standby(standbyT));

	double resistanceStart = 12.;
	resistiveElementTop.addTurnOnLogic(HPWH::topThird(resistanceStart));
	resistiveElementBottom.addTurnOnLogic(HPWH::topThird(resistanceStart));

	resistiveElementTop.addShutOffLogic(HPWH::fifthSixthMaxTemp(F_TO_C(117.)));
	resistiveElementBottom.addShutOffLogic(HPWH::secondSixthMaxTemp(F_TO_C(109.)));

	//set everything in its places
	setOfSources[0] = resistiveElementTop;
	setOfSources[1] = resistiveElementBottom;
	setOfSources[2] = compressor;

	//and you have to do this after putting them into setOfSources, otherwise
	//you don't get the right pointers
	setOfSources[2].backupHeatSource = &setOfSources[1];
	setOfSources[1].backupHeatSource = &setOfSources[2];

	setOfSources[0].followedByHeatSource = &setOfSources[1];
	setOfSources[1].followedByHeatSource = &setOfSources[2];
	//setOfSources[2].followedByHeatSource = &setOfSources[1];;

	setOfSources[0].companionHeatSource = &setOfSources[1];
	setOfSources[1].companionHeatSource = &setOfSources[2];

	}
		else if (presetNum == MODELS_GE2014STDMode) {
			numNodes = 12;
			tankTemps_C = new double[numNodes];
			setpoint_C = F_TO_C(127.0);

			tankVolume_L = GAL_TO_L(45);
			tankUA_kJperHrC = 6.5;

			doTempDepression = false;
			tankMixesOnDraw = true;

			numHeatSources = 3;
			setOfSources = new HeatSource[numHeatSources];

			HeatSource compressor(this);
			HeatSource resistiveElementBottom(this);
			HeatSource resistiveElementTop(this);

			//compressor values
			compressor.isOn = false;
			compressor.isVIP = false;
			compressor.typeOfHeatSource = TYPE_compressor;

			double split = 1.0 / 4.0;
			compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

			compressor.perfMap.reserve(2);

			compressor.perfMap.push_back({
				50, // Temperature (T_F)
				{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
				{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
				});

			compressor.perfMap.push_back({
				70, // Temperature (T_F)
				{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
				{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
				});

			compressor.minT = F_TO_C(37.0);
			compressor.maxT = F_TO_C(120.);
			compressor.hysteresis_dC = dF_TO_dC(2);
			compressor.configuration = HeatSource::CONFIG_WRAPPED;
			compressor.maxSetpoint_C = MAXOUTLET_R134A;

			//top resistor values
			resistiveElementTop.setupAsResistiveElement(6, 4500);
			resistiveElementTop.isVIP = true;

			//bottom resistor values
			resistiveElementBottom.setupAsResistiveElement(0, 4000);
			resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
			resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

			//logic conditions
			resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(19.6605)));

			resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(86.1111)));

			compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
			compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(12.392)));
			//    compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(65)));

			//set everything in its places
			setOfSources[0] = resistiveElementTop;
			setOfSources[1] = resistiveElementBottom;
			setOfSources[2] = compressor;

			//and you have to do this after putting them into setOfSources, otherwise
			//you don't get the right pointers
			setOfSources[2].backupHeatSource = &setOfSources[1];
			setOfSources[1].backupHeatSource = &setOfSources[2];

			setOfSources[0].followedByHeatSource = &setOfSources[1];
			setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_GE2014STDMode_80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(75.4);
		tankUA_kJperHrC = 10.;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(19.6605)));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(86.1111)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(12.392)));
		compressor.minT = F_TO_C(37);
		//    compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(65)));
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_GE2014) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(45);
		tankUA_kJperHrC = 6.5;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.addShutOffLogic(HPWH::topNodeMaxTemp(F_TO_C(116.6358)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(11.0648)));
		// compressor.addShutOffLogic(HPWH::largerDraw(F_TO_C(62.4074)));

		resistiveElementBottom.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(60)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;


		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_GE2014_80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(75.4);
		tankUA_kJperHrC = 10.;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.addShutOffLogic(HPWH::topNodeMaxTemp(F_TO_C(116.6358)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(11.0648)));
		// compressor.addShutOffLogic(HPWH::largerDraw(F_TO_C(62.4074)));

		resistiveElementBottom.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(60)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;


		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_GE2014_80DR) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(75.4);
		tankUA_kJperHrC = 10.;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		// resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.addTurnOnLogic(HPWH::topThird_absolute(F_TO_C(87)));
		// resistiveElementTop.addShutOffLogic(HPWH::topNodeMaxTemp(F_TO_C(116.6358)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(11.0648)));
		// compressor.addShutOffLogic(HPWH::largerDraw(F_TO_C(62.4074)));

		resistiveElementBottom.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(60)));
		// resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;


		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	// PRESET USING GE2014 DATA 
	else if (presetNum == MODELS_BWC2020_65) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(64);
		tankUA_kJperHrC = 7.6;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.addShutOffLogic(HPWH::topNodeMaxTemp(F_TO_C(116.6358)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(11.0648)));
		// compressor.addShutOffLogic(HPWH::largerDraw(F_TO_C(62.4074)));

		resistiveElementBottom.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(60)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;


		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

	}
	// If Rheem Premium
	else if (MODELS_Rheem2020Prem40 <= presetNum && presetNum <= MODELS_Rheem2020Prem80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		if (presetNum == MODELS_Rheem2020Prem40) {
			tankVolume_L = GAL_TO_L(36.1);
			tankUA_kJperHrC = 9.5;
		}
		else if (presetNum == MODELS_Rheem2020Prem50) {
			tankVolume_L = GAL_TO_L(45.1);
			tankUA_kJperHrC = 8.55;
		}
		else if (presetNum == MODELS_Rheem2020Prem65) {
			tankVolume_L = GAL_TO_L(58.5);
			tankUA_kJperHrC = 10.64;
		}
		else if (presetNum == MODELS_Rheem2020Prem80) {
			tankVolume_L = GAL_TO_L(72.0);
			tankUA_kJperHrC = 10.83;
		}

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		compressor.setCondensity(0.2, 0.2, 0.2, 0.2, 0.2, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{250, -1.0883, 0.0176}, // Input Power Coefficients (inputPower_coeffs)
			{6.7, -0.0087, -0.0002} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{275.0, -0.6631, 0.01571}, // Input Power Coefficients (inputPower_coeffs)
			{7.0, -0.0168, -0.0001} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.hysteresis_dC = dF_TO_dC(1);
		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);

		//logic conditions
		double compStart = dF_TO_dC(32);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));
		resistiveElementTop.addTurnOnLogic(HPWH::topSixth(dF_TO_dC(20.4167)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[1].backupHeatSource = &setOfSources[2];
		setOfSources[2].backupHeatSource = &setOfSources[1];

		setOfSources[0].followedByHeatSource = &setOfSources[2];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];
	}

	// If Rheem Build
	else if (MODELS_Rheem2020Build40 <= presetNum && presetNum <= MODELS_Rheem2020Build80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		if (presetNum == MODELS_Rheem2020Build40) {
			tankVolume_L = GAL_TO_L(36.1);
			tankUA_kJperHrC = 9.5;
		}
		else if (presetNum == MODELS_Rheem2020Build50) {
			tankVolume_L = GAL_TO_L(45.1);
			tankUA_kJperHrC = 8.55;
		}
		else if (presetNum == MODELS_Rheem2020Build65) {
			tankVolume_L = GAL_TO_L(58.5);
			tankUA_kJperHrC = 10.64;
		}
		else if (presetNum == MODELS_Rheem2020Build80) {
			tankVolume_L = GAL_TO_L(72.0);
			tankUA_kJperHrC = 10.83;
		}

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		compressor.setCondensity(0.2, 0.2, 0.2, 0.2, 0.2, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);
		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{220.0, 0.8743, 0.00454}, // Input Power Coefficients (inputPower_coeffs)
			{ 7.96064, -0.0448, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{275.0, -0.6631, 0.01571}, // Input Power Coefficients (inputPower_coeffs)
			{8.45936, -0.04539, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.hysteresis_dC = dF_TO_dC(1);
		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(4);

		//logic conditions
		double compStart = dF_TO_dC(30);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));
		resistiveElementTop.addTurnOnLogic(HPWH::topSixth(dF_TO_dC(20.4167)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[1].backupHeatSource = &setOfSources[2];
		setOfSources[2].backupHeatSource = &setOfSources[1];

		setOfSources[0].followedByHeatSource = &setOfSources[2];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];
	}		
	else if (MODELS_RheemPlugInShared40 <= presetNum && presetNum <= MODELS_RheemPlugInShared80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];

		if (presetNum == MODELS_RheemPlugInShared40) {
			tankVolume_L = GAL_TO_L(36.0);
			tankUA_kJperHrC = 9.5;
			setpoint_C = F_TO_C(140.0);
		}
		else if (presetNum == MODELS_RheemPlugInShared50) {
			tankVolume_L = GAL_TO_L(45.0);
			tankUA_kJperHrC = 8.55;
			setpoint_C = F_TO_C(140.0);
		}
		else if (presetNum == MODELS_RheemPlugInShared65) {
			tankVolume_L = GAL_TO_L(58.5);
			tankUA_kJperHrC = 10.64;
			setpoint_C = F_TO_C(127.0);
		}
		else if (presetNum == MODELS_RheemPlugInShared80) {
			tankVolume_L = GAL_TO_L(72.0);
			tankUA_kJperHrC = 10.83;
			setpoint_C = F_TO_C(127.0);
		}

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];
		HeatSource compressor(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;		
		compressor.setCondensity(0.2, 0.2, 0.2, 0.2, 0.2, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{250, -1.0883, 0.0176}, // Input Power Coefficients (inputPower_coeffs)
			{6.7, -0.0087, -0.0002} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{275.0, -0.6631, 0.01571}, // Input Power Coefficients (inputPower_coeffs)
			{7.0, -0.0168, -0.0001} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.hysteresis_dC = dF_TO_dC(1);
		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//logic conditions
		double compStart = dF_TO_dC(32);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));
		//set everything in its places
		setOfSources[0] = compressor;
	}
	else if (presetNum == MODELS_RheemPlugInDedicated40 || presetNum == MODELS_RheemPlugInDedicated50) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);
		if (presetNum == MODELS_RheemPlugInDedicated40) {
			tankVolume_L = GAL_TO_L(36);
			tankUA_kJperHrC = 5.5;
		}
		else if (presetNum == MODELS_RheemPlugInDedicated50) {
			tankVolume_L = GAL_TO_L(45);
			tankUA_kJperHrC = 6.33;
		}
		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 1;
		setOfSources = new HeatSource[numHeatSources];
		HeatSource compressor(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0., 0., 0., 0.);

		compressor.perfMap.reserve(2);
		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{528.91, 4.8988, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{ 4.3943, -0.012443, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			95, // Temperature (T_F)
			{494.03, 7.7266, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.48189, -0.01604, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.hysteresis_dC = dF_TO_dC(1);
		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;


		//logic conditions
		double compStart = dF_TO_dC(20);
		double standbyT = dF_TO_dC(9);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));
		//set everything in its places
		setOfSources[0] = compressor;
	}
	else if (presetNum == MODELS_RheemHB50) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(45);
		tankUA_kJperHrC = 7;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			47, // Temperature (T_F)
			{280, 4.97342, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.634009, -0.029485, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{280, 5.35992, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{6.3, -0.03, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.hysteresis_dC = dF_TO_dC(1);
		compressor.minT = F_TO_C(40.0);
		compressor.maxT = F_TO_C(120.0);
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.configuration = HPWH::HeatSource::CONFIG_WRAPPED;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4200);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 2250);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		double compStart = dF_TO_dC(38);
		double standbyT = dF_TO_dC(13.2639);
		compressor.addTurnOnLogic(HPWH::bottomThird(compStart));
		compressor.addTurnOnLogic(HPWH::standby(standbyT));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(76.7747)));

		resistiveElementTop.addTurnOnLogic(HPWH::topSixth(dF_TO_dC(20.4167)));


		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_Stiebel220E) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127);

		tankVolume_L = GAL_TO_L(56);
		//tankUA_kJperHrC = 10; //0 to turn off
		tankUA_kJperHrC = 9;

		doTempDepression = false;
		tankMixesOnDraw = false;

		numHeatSources = 2;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElement(this);

		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		resistiveElement.setupAsResistiveElement(0, 1500);
		resistiveElement.hysteresis_dC = dF_TO_dC(0);

		compressor.setCondensity(0, 0.12, 0.22, 0.22, 0.22, 0.22, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{295.55337, 2.28518, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.744118, -0.025946, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{282.2126, 2.82001, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{8.012112, -0.039394, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(32.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = 0;  //no hysteresis
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		compressor.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(6.5509)));
		compressor.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));

		compressor.depressesTemperature = false;  //no temp depression

		//set everything in its places
		setOfSources[0] = compressor;
		setOfSources[1] = resistiveElement;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[0].backupHeatSource = &setOfSources[1];

	}
	else if (presetNum == MODELS_Generic1) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(50);
		tankUA_kJperHrC = 9;
		doTempDepression = false;
		tankMixesOnDraw = true;
		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{472.58616, 2.09340, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{2.942642, -0.0125954, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{439.5615, 2.62997, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{3.95076, -0.01638033, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(45.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(8, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40.0)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(10)));
		compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(65)));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(80)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(110)));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(35)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_Generic2) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(50);
		tankUA_kJperHrC = 7.5;
		doTempDepression = false;
		tankMixesOnDraw = true;
		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{272.58616, 2.09340, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{4.042642, -0.0205954, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{239.5615, 2.62997, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.25076, -0.02638033, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(40.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(10)));
		compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(60)));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(80)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(100)));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(40)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_Generic3) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(50);
		tankUA_kJperHrC = 5;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{172.58616, 2.09340, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.242642, -0.0285954, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			67, // Temperature (T_F)
			{139.5615, 2.62997, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{6.75076, -0.03638033, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(35.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4500);
		resistiveElementBottom.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(40)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(10)));
		compressor.addShutOffLogic(HPWH::largeDraw(F_TO_C(55)));

		resistiveElementBottom.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(60)));

		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(40)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = compressor;
		setOfSources[2] = resistiveElementBottom;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (presetNum == MODELS_UEF2generic) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		tankVolume_L = GAL_TO_L(45);
		tankUA_kJperHrC = 6.5;

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{4.29, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			{5.61, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(37.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(18.6605)));

		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(86.1111)));

		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(12.392)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

	}
	else if (MODELS_AWHSTier3Generic40 <= presetNum && presetNum <= MODELS_AWHSTier3Generic80) {
		numNodes = 12;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(127.0);

		if (presetNum == MODELS_AWHSTier3Generic40) {
			tankVolume_L = GAL_TO_L(36.1);
			tankUA_kJperHrC = 5;
		}
		else if (presetNum == MODELS_AWHSTier3Generic50) {
			tankVolume_L = GAL_TO_L(45);
			tankUA_kJperHrC = 6.5;
		}
		else if (presetNum == MODELS_AWHSTier3Generic65) {
			tankVolume_L = GAL_TO_L(64);
			tankUA_kJperHrC = 7.6;
		}
		else if (presetNum == MODELS_AWHSTier3Generic80) {
			tankVolume_L = GAL_TO_L(75.4);
			tankUA_kJperHrC = 10.;
		} 
		else {
			if (hpwhVerbosity >= VRB_reluctant) {
				msg("Incorrect model specification.  \n");
			}
			return HPWH_ABORT;
		}

		doTempDepression = false;
		tankMixesOnDraw = true;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);

		//compressor values
		compressor.isOn = false;
		compressor.isVIP = false;
		compressor.typeOfHeatSource = TYPE_compressor;

		double split = 1.0 / 4.0;
		compressor.setCondensity(split, split, split, split, 0, 0, 0, 0, 0, 0, 0, 0);

		//voltex60 tier 1 values
		compressor.perfMap.reserve(2);

		compressor.perfMap.push_back({
			50, // Temperature (T_F)
			{187.064124, 1.939747, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			//{5.4977772, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			{5.22288834, -0.0243008, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.perfMap.push_back({
			70, // Temperature (T_F)
			{148.0418, 2.553291, 0.0}, // Input Power Coefficients (inputPower_coeffs)
			//{7.207307, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			{6.84694165, -0.0335265, 0.0} // COP Coefficients (COP_coeffs)
			});

		compressor.minT = F_TO_C(42.0);
		compressor.maxT = F_TO_C(120.);
		compressor.hysteresis_dC = dF_TO_dC(2);
		compressor.configuration = HeatSource::CONFIG_WRAPPED;
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		//top resistor values
		resistiveElementTop.setupAsResistiveElement(6, 4500);
		resistiveElementTop.isVIP = true;

		//bottom resistor values
		resistiveElementBottom.setupAsResistiveElement(0, 4000);
		resistiveElementBottom.setCondensity(0, 0.2, 0.8, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		resistiveElementBottom.hysteresis_dC = dF_TO_dC(2);

		//logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(20)));
		resistiveElementTop.addShutOffLogic(HPWH::topNodeMaxTemp(F_TO_C(116.6358)));
		compressor.addTurnOnLogic(HPWH::bottomThird(dF_TO_dC(33.6883)));
		compressor.addTurnOnLogic(HPWH::standby(dF_TO_dC(11.0648)));
		resistiveElementBottom.addTurnOnLogic(HPWH::thirdSixth(dF_TO_dC(60)));
		resistiveElementBottom.addShutOffLogic(HPWH::bottomTwelthMaxTemp(F_TO_C(80)));

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];
	}
	// If a the model is the TamOMatic, HotTam, Generic... This model is scalable. 
	else if (presetNum == MODELS_TamScalable_SP) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;
		canScale = true; // a fully scallable model

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; 
		tankUA_kJperHrC = 7; 
		setTankSize_adjustUA(600., UNITS_GAL);

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.isMultipass = false;
		compressor.perfMap.reserve(1);
		compressor.hysteresis_dC = 0;

		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = numNodes - 1;

		//Defrost Derate 
		compressor.setupDefrostMap();

		//Perfmap for input power and COP made from data for poor preforming modeled to be scalled for this model
		std::vector<double> inputPwr_coeffs = { 13.6, 0.00995, -0.0342, -0.014, -0.000110, 0.00026, 0.000232, 0.000195, -0.00034, 5.30E-06, 2.3600E-06};
		std::vector<double> COP_coeffs = { 1.945, 0.0412, -0.0112, -0.00161, 0.0000492, 0.0000348, -0.0000323, -0.000166, 0.0000112, 0.0000392, -3.52E-07};

		compressor.perfMap.push_back({
			105, // Temperature (T_F)
			inputPwr_coeffs, // Input Power Coefficients (inputPower_coeffs
			COP_coeffs // COP Coefficients (COP_coeffs)
		});

		//logic conditions
		compressor.minT = F_TO_C(40.);
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(15), this));

		//lowT cutoff
		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(1);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("bottom node", nodeWeights1, dF_TO_dC(15.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression


		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);
		resistiveElementBottom.setupAsResistiveElement(0, 30000);
		resistiveElementTop.setupAsResistiveElement(9, 30000);

		//top resistor values
		//standard logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(15)));
		resistiveElementTop.isVIP = true;

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];
	}
	else if (presetNum == MODELS_Scalable_MP) {
		numNodes = 24;
		tankTemps_C = new double[numNodes];
		setpoint_C = F_TO_C(135.0);
		tankSizeFixed = false;
		canScale = true; // a fully scallable model

		doTempDepression = false;
		tankMixesOnDraw = false;

		tankVolume_L = 315; // Gets adjust per model but ratio between vol and UA is important 
		tankUA_kJperHrC = 7;

		numHeatSources = 3;
		setOfSources = new HeatSource[numHeatSources];

		HeatSource compressor(this);
		compressor.isOn = false;
		compressor.isVIP = true;
		compressor.typeOfHeatSource = TYPE_compressor;
		compressor.setCondensity(0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0);
		compressor.configuration = HeatSource::CONFIG_EXTERNAL;
		compressor.perfMap.reserve(1);
		compressor.hysteresis_dC = 0;
		compressor.externalOutletHeight = 0;
		compressor.externalInletHeight = (int)(numNodes / 3.) - 1;

		//logic conditions
		std::vector<NodeWeight> nodeWeights;
		nodeWeights.emplace_back(4);
		compressor.addTurnOnLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights, dF_TO_dC(5.), this, false));

		std::vector<NodeWeight> nodeWeights1;
		nodeWeights1.emplace_back(4);
		compressor.addShutOffLogic(std::make_shared<HPWH::TempBasedHeatingLogic>("fourth node", nodeWeights1, dF_TO_dC(0.), this, false, std::greater<double>()));
		compressor.depressesTemperature = false;  //no temp depression

		//Defrost Derate 
		compressor.setupDefrostMap();

		//logic conditions
		compressor.minT = F_TO_C(40.);
		compressor.maxT = F_TO_C(105.);
		compressor.maxSetpoint_C = MAXOUTLET_R134A;

		setTankSize_adjustUA(600., UNITS_GAL);
		compressor.mpFlowRate_LPS = GPM_TO_LPS(25.);
		compressor.perfMap.push_back({
			100, // Temperature (T_F)

			{ 12.4, 0.00739, -0.0410, 0.0, 0.000578, 0.0000696}, // Input Power Coefficients (inputPower_coeffs)

			{ 1.20, 0.0333, 0.00191, 0.000283, 0.0000496, -0.000440} // COP Coefficients (COP_coeffs)

			});

		HeatSource resistiveElementBottom(this);
		HeatSource resistiveElementTop(this);
		resistiveElementBottom.setupAsResistiveElement(0, 30000);
		resistiveElementTop.setupAsResistiveElement(9, 30000);

		//top resistor values
		//standard logic conditions
		resistiveElementTop.addTurnOnLogic(HPWH::topThird(dF_TO_dC(30)));
		resistiveElementTop.isVIP = true;

		//set everything in its places
		setOfSources[0] = resistiveElementTop;
		setOfSources[1] = resistiveElementBottom;
		setOfSources[2] = compressor;

		//and you have to do this after putting them into setOfSources, otherwise
		//you don't get the right pointers
		setOfSources[2].backupHeatSource = &setOfSources[1];
		setOfSources[1].backupHeatSource = &setOfSources[2];

		setOfSources[0].followedByHeatSource = &setOfSources[1];
		setOfSources[1].followedByHeatSource = &setOfSources[2];

		setOfSources[0].companionHeatSource = &setOfSources[2];
	}

	else {
		if (hpwhVerbosity >= VRB_reluctant) {
			msg("You have tried to select a preset model which does not exist.  \n");
		}
		return HPWH_ABORT;
	}

	//start tank off at setpoint
	resetTankToSetpoint();

	// initialize nextTankTemps_C 
	nextTankTemps_C = new double[numNodes];

	hpwhModel = presetNum;

	//calculate oft-used derived values
	calcDerivedValues();

	if (checkInputs() == HPWH_ABORT) {
		return HPWH_ABORT;
	}

	isHeating = false;
	for (int i = 0; i < numHeatSources; i++) {
		if (setOfSources[i].isOn) {
			isHeating = true;
		}
		setOfSources[i].sortPerformanceMap();
	}

	if (hpwhVerbosity >= VRB_emetic) {
		for (int i = 0; i < numHeatSources; i++) {
			msg("heat source %d: %p \n", i, &setOfSources[i]);
		}
		msg("\n\n");
	}

	simHasFailed = false;
	return 0;  //successful init returns 0
}  //end HPWHinit_presets

