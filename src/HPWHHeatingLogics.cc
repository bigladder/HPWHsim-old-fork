/*
File of the presets heating logics available HPWHsim
*/

#include "HPWH.hh"

/* State of Charge Based Logic*/
const bool HPWH::SoCBasedHeatingLogic::isValid() {
	bool isValid = true;
	if(decisionPoint < 0) {
		isValid = false;
	}
	return isValid;
}

const double HPWH::SoCBasedHeatingLogic::getComparisonValue() {
	return decisionPoint + hysteresisFraction;
}

const double HPWH::SoCBasedHeatingLogic::getTankValue() {
	double soCFraction;
	if(hpwh->member_inletT_C == HPWH_ABORT && !useCostantMains) {
		soCFraction = HPWH_ABORT;
	} else {
		soCFraction = hpwh->getSoCFraction();
	}
	return soCFraction;
}

const double HPWH::SoCBasedHeatingLogic::getMainsT_C() {
	if(useCostantMains) {
		return constantMains_C;
	} else {
		return hpwh->member_inletT_C;
	}
}

const double HPWH::SoCBasedHeatingLogic::getTempMinUseful_C() {
	return tempMinUseful_C;
}

int HPWH::SoCBasedHeatingLogic::setDecisionPoint(double value) {
	decisionPoint = value;
	return 0;
}

int HPWH::SoCBasedHeatingLogic::setConstantMainsTemperature(double mains_C) {
	constantMains_C = mains_C;
	useCostantMains = true;
	return 0;
}

const double HPWH::SoCBasedHeatingLogic::nodeWeightAvgFract() {
	return getComparisonValue();
}

const double HPWH::SoCBasedHeatingLogic::getFractToMeetComparisonExternal() {
	double deltaSoCFraction = (getComparisonValue() + HPWH::TOL_MINVALUE) - getTankValue();

	// Check how much of a change in the SoC fraction occurs if one full node at set point is added. If this is less than the change needed move on. 
	double fullNodeSoC = 1. / hpwh->getNumNodes();
	if(deltaSoCFraction >= fullNodeSoC) {
		return 1.;
	}

	// Find the last node greater the min use temp
	int calcNode = 0;
	for(int i = hpwh->getNumNodes() - 1; i >= 0; i--) {
		if(hpwh->tankTemps_C[i] < tempMinUseful_C) {
			calcNode = i + 1;
			break;
		}
	}
	if(calcNode == hpwh->getNumNodes()) { // if the whole tank is cold
		return 1.;
	}

	// Find the fraction to heat the calc node to meet the target SoC fraction without heating the node below up to tempMinUseful. 
	double maxSoC = hpwh->getNumNodes() * hpwh->getChargePerNode(getMainsT_C(),tempMinUseful_C,hpwh->setpoint_C);
	double targetTemp = deltaSoCFraction * maxSoC + (hpwh->tankTemps_C[calcNode] - getMainsT_C()) / (tempMinUseful_C - getMainsT_C());
	targetTemp = targetTemp * (tempMinUseful_C - getMainsT_C()) + getMainsT_C();

	//Catch case where node temperature == setpoint
	double fractCalcNode;
	if(hpwh->tankTemps_C[calcNode] >= hpwh->setpoint_C) {
		fractCalcNode = 1;
	} else {
		fractCalcNode = (targetTemp - hpwh->tankTemps_C[calcNode]) / (hpwh->setpoint_C - hpwh->tankTemps_C[calcNode]);
	}

	// If we're at the bottom node there's not another node to heat so case 2 doesn't apply. 
	if(calcNode == 0) {
		return fractCalcNode;
	}

	// Fraction to heat next node, where the step change occurs
	double fractNextNode = (tempMinUseful_C - hpwh->tankTemps_C[calcNode - 1]) / (hpwh->tankTemps_C[calcNode] - hpwh->tankTemps_C[calcNode - 1]);
	fractNextNode += HPWH::TOL_MINVALUE;

	if(hpwh->hpwhVerbosity >= VRB_emetic) {
		double smallestSoCChangeWhenHeatingNextNode = 1. / maxSoC * (1. + fractNextNode * (hpwh->setpoint_C - hpwh->tankTemps_C[calcNode]) /
			(tempMinUseful_C - getMainsT_C()));
		hpwh->msg("fractThisNode %.6f, fractNextNode %.6f,  smallestSoCChangeWithNextNode:  %.6f, deltaSoCFraction: %.6f\n",
			fractCalcNode,fractNextNode,smallestSoCChangeWhenHeatingNextNode,deltaSoCFraction);
	}

	// if the fraction is enough to heat up the next node, do that minimum and handle the heating of the next node next iteration.
	return std::min(fractCalcNode,fractNextNode);
}

/* Temperature Based Heating Logic*/
const bool HPWH::TempBasedHeatingLogic::isValid() {
	bool isValid = true;
	if(!areNodeWeightsValid()) {
		isValid = false;
	}
	return isValid;
}

const bool HPWH::TempBasedHeatingLogic::areNodeWeightsValid() {
	for(auto nodeWeight : nodeWeights) {
		if(nodeWeight.nodeNum > 13 || nodeWeight.nodeNum < 0) {
			return false;
		}
	}
	return true;
}

const double HPWH::TempBasedHeatingLogic::getComparisonValue() {
	double value = decisionPoint;
	if(isAbsolute) {
		return value;
	} else {
		return hpwh->getSetpoint() - value;
	}
}

const double HPWH::TempBasedHeatingLogic::getTankValue() {
	return hpwh->tankAvg_C(nodeWeights);
}

int HPWH::TempBasedHeatingLogic::setDecisionPoint(double value) {
	decisionPoint = value;
	return 0;
}
int HPWH::TempBasedHeatingLogic::setDecisionPoint(double value,bool absolute) {
	isAbsolute = absolute;
	return setDecisionPoint(value);
}

const double HPWH::TempBasedHeatingLogic::nodeWeightAvgFract() {
	double logicNode;
	double calcNodes = 0,totWeight = 0;

	for(auto nodeWeight : nodeWeights) {
		// bottom calc node only
		if(nodeWeight.nodeNum == 0) { // simple equation
			return 1. / (double)hpwh->getNumNodes();
		}
		// top calc node only
		else if(nodeWeight.nodeNum == LOGIC_NODE_SIZE + 1) {
			return 1.;
		} else { // have to tally up the nodes
			calcNodes += nodeWeight.nodeNum * nodeWeight.weight;
			totWeight += nodeWeight.weight;
		}
	}

	logicNode = calcNodes / totWeight;

	return logicNode / static_cast<double>(LOGIC_NODE_SIZE);
}

const double HPWH::TempBasedHeatingLogic::getFractToMeetComparisonExternal() {
	double fracTemp;
	double diff;
	int calcNode = 0;
	int firstNode = -1;
	double sum = 0;
	double totWeight = 0;

	std::vector<double> resampledTankTemps(LOGIC_NODE_SIZE);
	resample(resampledTankTemps,hpwh->tankTemps_C);
	double comparison = getComparisonValue();
	comparison += HPWH::TOL_MINVALUE; // Make this possible so we do slightly over heat

	double nodeDensity = static_cast<double>(hpwh->getNumNodes()) / LOGIC_NODE_SIZE;
	for(auto nodeWeight : nodeWeights) {

		// bottom calc node only
		if(nodeWeight.nodeNum == 0) { // bottom-most tank node only			
			firstNode = calcNode = 0;
			double nodeTemp = hpwh->tankTemps_C.front();
			sum = nodeTemp * nodeWeight.weight;
			totWeight = nodeWeight.weight;
		}
		// top calc node only
		else if(nodeWeight.nodeNum == LOGIC_NODE_SIZE + 1) { // top-most tank node only
			calcNode = firstNode = hpwh->getNumNodes() - 1;
			double nodeTemp = hpwh->tankTemps_C.back();
			sum = nodeTemp * nodeWeight.weight;
			totWeight = nodeWeight.weight;
		} else { // all tank nodes corresponding to logical node
			firstNode = static_cast<int>(nodeDensity * (nodeWeight.nodeNum - 1));
			calcNode = static_cast<int>(nodeDensity * (nodeWeight.nodeNum))- 1;
			double nodeTemp = resampledTankTemps[static_cast<std::size_t>(nodeWeight.nodeNum - 1)];
			sum += nodeTemp * nodeWeight.weight;
			totWeight += nodeWeight.weight;
		}
	}

	if(calcNode == hpwh->getNumNodes() - 1) { // top node calc
		diff = hpwh->getSetpoint() - hpwh->tankTemps_C[firstNode];
	} else {
		diff = hpwh->tankTemps_C[calcNode + 1] - hpwh->tankTemps_C[firstNode];
	}
	// if totWeight * comparison - sum < 0 then the shutoff condition is already true and you shouldn't
	// be here. Will revaluate shut off condition at the end the do while loop of addHeatExternal, in the
	// mean time lets not shift anything around. 
	if(compare(sum,totWeight * comparison)) { // Then should shut off
		fracTemp = 0.; // 0 means shift no nodes
	} else {
		// if the difference in denominator is <= 0 then we aren't adding heat to the nodes we care about, so 
		// shift a whole node.
		// factor of hpwh->nodeDensity included below to reproduce original algorithm
		fracTemp = diff > 0. ? (totWeight * comparison - sum) * nodeDensity / diff : 1.;
	}

	return fracTemp;
}
