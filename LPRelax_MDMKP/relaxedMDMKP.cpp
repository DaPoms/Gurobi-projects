#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
using namespace std;

int CURRPROBLEM = 1;
double doubleRelaxVal = 0.01;

vector<vector<int>> isLoosenedCapacity = {};
vector<vector<int>> isLoosenedDemand = {};
//raw MDMK problems are stored as followed:
/* 
    for both vector<vector<long>> candidateCapacityAttributes and candidateDemandAttributes:
        each vector<long> is for the ith demand/capacity attribute, with index 0 of this 
        vector being associated with the first candidate and the rest following in an incrementing pattern
    
    for vector<vector<long>> candidateCostAttributes:
        each vector<long> holds the values of every item for a given case, with each item stored as the first being at index 0. 
        There are 6 vector<long>'s, attributed to the being six MDMKP cases (0-5). The definition of the cases are clarified at 
        separateCandidatesByCases(). Each case has unique cost attribue vals (which is why they are separate)
    
    for vector<long> knapsackCapacityVals and vector<long> knapsackDemandRequirementVals:
        Capacity/demand right coefficients (for demands these are the value we need to be >= to and for capacity its what we need to be <=)
        are stored in these vector<long>'s with the first value being attributed to the first demand/capacity and so on. Every case uses the same demand/capacity values,
        but do note some cases only use part of the demandRequirementVals, not all (clarified at separateCandidatesByCases())
*/
class MDMKRawProblem // each MDMKRawProblem is actually a set of 6 problems in 1 entity, but processing must be done for that to form thus the "raw" name
{
    private:
        vector<vector<long>> candidateCapacityAttributes;
        vector<vector<long>> candidateDemandAttributes;
        vector<vector<long>> candidateCostAttributes;
        vector<long> knapsackCapacityVals;
        vector<long> knapsackDemandRequirementVals;
    public:
        vector<vector<long>>& getcandidateCapacityAttributes() { return candidateCapacityAttributes; } 
        vector<vector<long>>& getcandidateDemandAttributes() { return candidateDemandAttributes; }
        vector<vector<long>>& getCandidateValue() { return candidateCostAttributes; }
        vector<long>& getknapsackCapacityVals() { return knapsackCapacityVals; }
        vector<long>& getknapsackDemandRequirementVals() { return knapsackDemandRequirementVals; }

};

//stores all the attributes of a candidate that can be added to the knapsack. for capacity and demand, the first values are associated with the 1st demands and so on...
struct MDMKCandidate
{
    vector<long> capacityVal; // How much capacity this candidate takes up for each capacity constraint
    vector<long> demandVal; //How much demand value this candidate contributes
    long value; // "Cost": how much this item is worth (either +/- depending on the case)
};

// can either be used to store all cases or only 1. A 1 case version will just use .problemByCase[0] when referencing candidates
// in my code just to more explicitly show that the object is built for 1 case and not all 6
struct problemSet
{
    vector < vector <MDMKCandidate> > problemsByCase;
    vector<long> knapsackCapacityVals;
    vector<long> knapsackDemandRequirementVals; 
};


//Formats MKMDRawProblem into their 6 respective cases, stored in problemsByCase of pproblemSet in accordance to the brunel paper. Here is a definition/order of each case:
/* 
Note that "q" is the number of demand constraints (>=) and "m" is the number of capacity (<=) constraints
These constraints are picked in order as they show up. EX: when q=1, it means only the first demand constraint is considered
    Positive cost/value cases
    Case 1: q = 1
    Case 2: q = m/2
    Case 3: q = m
    Mixed cost/value cases
    Case 4: q = 1
    Case 5: q = m/2
    Case 6: q = m
Note that each problem has 100 candidates to consider
    */
void separateCandidatesByCases(MDMKRawProblem& problem, problemSet& candidatesByCase) 
{
    candidatesByCase.problemsByCase.resize(6); // new thing learned! Can be used to easily create uninitialized values inside of the vector (or reshape vector to fit this function's usage)
    vector<vector<long>> MDMKCapacityAttributes = problem.getcandidateCapacityAttributes(); 
    vector<vector<long>> MDMKDemandAttributes = problem.getcandidateDemandAttributes();
    vector<vector<long>> MDMKValue = problem.getCandidateValue();
    long capacityVarsCount = MDMKCapacityAttributes.size(); //Shortcut to finding the amount of capacity variables (dimensions) for the problem
    long candidateCount = MDMKCapacityAttributes[0].size();//This is a way to find the # of candidates we have without having to pass directly (not too efficient, but less params = simpler). Only works if problem is not empty
    int caseDemand1 = 1; //sort of pointless but slightly more readable as it describes what the value 1 is for (Case 1 of MDMKP problems)
    int caseDemand2 = capacityVarsCount / 2;
    int caseDemand3 = capacityVarsCount; // Note that case 1,2,3 are repeated for case 4,5,6 respectively
    int order[6] = {0,3,1,4,2,5}; //order which we want to get candidates (most efficient order for the cases)
    for(int i{0}; i < candidateCount; i++) // for every candidate...
    {
        MDMKCandidate candidate; 
        for(int c{0}; c < capacityVarsCount; c++) //For every capacity constraint...  //every candidate has the same capacity vals for every problem case
            candidate.capacityVal.push_back(MDMKCapacityAttributes[c][i]);
               
        for(int caseNum{0}; caseNum < 6; caseNum++) //Shapes candidate demand and value for its respective case (note that caseNum = 0 is case 1 and caseNum = 5 is case 6)
        {
            int currentCase = order[caseNum];
            candidate.value = MDMKValue[currentCase][i]; //each case gives a unique cost to the candidate

            for(int currDemand{0}; currDemand < capacityVarsCount; currDemand++) //stops at case 6 (where same # of demands as capacity constraints / q = m)
            {
                candidate.demandVal.push_back(MDMKDemandAttributes[currDemand][i]); //adding on the item's demandAttributes, one at a time
                // WARNING: Very ugly if statement below!
                // I build it like this as its more efficient to start with demand size = 1 cases then add onto it to get to the size = capacity case (30 demands) rather
                // than going out of order and having to remark the candidate object multiple times
                // Effectively, once a case's demand size is met, it adds the candidate to the respective place in .problemsByCase
                if ( (currDemand == caseDemand3 - 1 && (currentCase == 2 || currentCase == 5)) || (currDemand == caseDemand2 - 1 && (currentCase == 1 || currentCase == 4) ) || (currDemand == caseDemand1 - 1 && (currentCase == 0 || currentCase == 3)) ) //these checks are weird but it works!
                    candidatesByCase.problemsByCase[currentCase].push_back(candidate);              
            }     
            candidate.demandVal.clear();
        }
    }
}

void RawProblemsToCases(vector<MDMKRawProblem>& problems, vector<problemSet>& cases)
{
    problemSet candidatesByCase;
    candidatesByCase.problemsByCase.resize(6);    
    int size = problems.size(); // size is the block count (15 for datac7.txt)
    for(int i{0}; i < size; i++) // for each block of problems... (15 total in dataset ct7, with 6 cases in each block)
    {
        separateCandidatesByCases(problems[i], candidatesByCase); //separates the block into the respective 6 case problems
        cases.push_back(candidatesByCase);
        cases[i].knapsackCapacityVals = problems[i].getknapsackCapacityVals(); // copies over knapsack capacity
        cases[i].knapsackDemandRequirementVals = problems[i].getknapsackDemandRequirementVals(); // all cases get the same demand requirementVals as the amount of demands present in candidates inside
                                                                                                 // .problemByCases will tell us how many demand attributes to look into
        candidatesByCase.problemsByCase.clear(); //reset to move onto the next block
    }
}


void readAttributeOfMDMKP(ifstream& file, vector<vector<long>>& candidateCoefficientAtrributes, vector<long>& knapsackGoalVals, long candidateCount, long constraints, bool isConstraint)
{
    long placeholder; 
    //variable constraints can be for <=/>= constraints, and is the amount of constraints there are (the size)
    for(int m{0}; m < constraints; m++) //reading for all candidate constraints
    {
        candidateCoefficientAtrributes.push_back(vector<long>());
        for(int i{0}; i < candidateCount; i++) // reading all <
        {
            file >> placeholder;
            candidateCoefficientAtrributes[m].push_back(placeholder);
        }
    }
    if(isConstraint)
        for(int m{0}; m < constraints; m++) //Reads capacity of knapsack for each capacity constraint
        {
            file >> placeholder;
            knapsackGoalVals.push_back(placeholder);
        }
}


//Param of fileName is the file we want to read from, must be in the same folder as this file (though I could easily change this if needed)
void readMDMKP(string fileName, vector<MDMKRawProblem>& MDMKRawProblems) // reads MDMKP problem text files that are in accordance to the format done by https://people.brunel.ac.uk/~mastjjb/jeb/orlib/mdmkpinfo.html
{
    
    ifstream file{fileName};
    long testProblemCount, candidateCount, leConstraints; //leConstraints means <= constraints (also known as capacity constraints), the file tells us ahead of time how many leConstraints there are
    
    file >> testProblemCount; //this is a header value, the first value in the datac7.txt file. testProblemCount is effectively the # of blocks there are in the text file. Each block has 6 problems in it

    for(int i{0}; i < testProblemCount; i++) //traverses all the problem sets of the file 
    {
        MDMKRawProblem problemSet; //creates a new empty problem set for every instance
        file >> candidateCount >> leConstraints; // These are the "header" variables for the brunel samples, they apply to all cases of a single problem (6 cases)
        vector<vector<long>> candidateCapacityAttributes; //each vector contains a separate attribute for every candidate 
        vector<long> knapsackCapacityVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateCapacityAttributes(), problemSet.getknapsackCapacityVals(), candidateCount, leConstraints, true); // reads <= constraints

        vector<vector<long>> candidateDemandAttributes;
        vector<long> knapsackDemandRequirementVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateDemandAttributes(), problemSet.getknapsackDemandRequirementVals(), candidateCount, leConstraints, true); // reads >= constraints. We just read as much as capacity as the highest case has capacity count == demand count

        vector<vector<long>> candidateCostAttributes; //Note that "knapsackCapacityVals" is useless here due to the boolean parameter being = false
        //I repurposed the leconstraints parameter to be the # of cases each model has, which is 6 for Brunel test cases
        readAttributeOfMDMKP(file, problemSet.getCandidateValue(), problemSet.getknapsackCapacityVals(), candidateCount, 6, false); // reads value of each object (Though the Brunel paper calls these "Cost" coefficients).         
        MDMKRawProblems.push_back(problemSet);
    }
}

///////////////////////////////////

// x stores the answer of how much of each item we include
//only works on one case at a time (just like the rest of the functions essentially)
problemSet extractCore(problemSet& caseProblem, vector<GRBVar>& x) // runs inside of runGurobiMDMKP to return a vector of the core problem, whgere you remove all candidates who were given an x value of 0 1 (included all or none).
{
    problemSet coreProblem; //decided to make a separate problem set to hold the answer as its very likely we'll remove more than we add if we stored the answer in the parameter caseProblem
    vector<MDMKCandidate> emptyCandidates;
    coreProblem.problemsByCase.push_back(emptyCandidates);
    coreProblem.knapsackCapacityVals = caseProblem.knapsackCapacityVals;
    coreProblem.knapsackDemandRequirementVals = caseProblem.knapsackDemandRequirementVals;
    for(int i{0}; i < x.size(); i++) // Note the core is just the candidates that were partially stored in the knapsack when solving MDMKP linearly (allowing fractional insertion) instead of 0-1
    {
        double xVal = x[i].get(GRB_DoubleAttr_X); //x val is how much of the item was added to the knapsack
        if (xVal == 1 || xVal == 0) // xVal == 0 is just another way of skipping the else statement for when x = 0
        {

            if(xVal == 1) //if 1, it means we include it in the 0-1 instance, so the core problem's right coefficients will get shrunk by this candidate
            { 
                for(int c{0}; c < caseProblem.knapsackCapacityVals.size(); c++) // for every capacity constraint... 
                    coreProblem.knapsackCapacityVals[c] -= caseProblem.problemsByCase[0][i].capacityVal[c];
            
                for(int d{0}; d < caseProblem.problemsByCase[0][0].demandVal.size(); d++) // for every demand constraint...
                    coreProblem.knapsackDemandRequirementVals[d] -= caseProblem.problemsByCase[0][i].demandVal[d];
            }   
        }
        else // Case of candidate that was fractionally inserted into the knapsack (a core candidate!)
            coreProblem.problemsByCase[0].push_back(caseProblem.problemsByCase[0][i]); //My syntax is a little fuzzy here so here's clarification: caseProblem is just a single case, which is why theres a 0 for [0][i] when accessing problemsByCase. its just taking the ith item
    }
    return coreProblem;
}


vector<double> GRBToDoubleDecisionValues(vector<GRBVar>& x) //converts the read only GRB_DoubleAttr_X values into changeable ints 
//(Note that the GRB_DoubleAttr_X are the solutions i.e. which candidates are needed in the knapsack that gurobi found for the MDMKP)
{
    vector<double> xVals;
    for(int i{0}; i < x.size(); i++)
        xVals.push_back(x[i].get(GRB_DoubleAttr_X));
    return xVals;
}

bool isFeasible(problemSet& caseProblem, vector<double>& xVals) //checks if solution is feasible, assumes that all the items with a decision variable = 1 are in the solution
{

    //Note I only did 2 sets of nested for loops for future proofing, as there is a case with 15 demand but 30 capacity constraints
    long target{-1};
    vector<long> currKnapsackCapacityVals(caseProblem.knapsackCapacityVals.size(), 0);
    vector<long> currKnapsackDemandVals(caseProblem.knapsackDemandRequirementVals.size(), 0);
    for(int i{0}; i < caseProblem.problemsByCase[0].size(); i++) //for every candidate...
    {
        if(xVals[i] == 1) //if candidate is considered part of the solution...
        {
            //for loop for summing <= constraints
            for(int c{0}; c < caseProblem.knapsackCapacityVals.size(); c++) // for every capacity constraint... 
                currKnapsackCapacityVals[c] += caseProblem.problemsByCase[0][i].capacityVal[c];
            //for loop for summing >= constraints
            for(int d{0}; d < caseProblem.problemsByCase[0][0].demandVal.size(); d++) // for every demand constraint...
                currKnapsackDemandVals[d] += caseProblem.problemsByCase[0][i].demandVal[d];
        }
    }
     //compares sums to what the actual limitations are
    for(int c{0}; c < caseProblem.knapsackCapacityVals.size(); c++)
            if(currKnapsackCapacityVals[c] > caseProblem.knapsackCapacityVals[c]) return false;
    for(int d{0}; d < caseProblem.problemsByCase[0][0].demandVal.size(); d++)
        if(currKnapsackDemandVals[d] < caseProblem.knapsackDemandRequirementVals[d]) return false;
    return true;
}


//returns true if item can be added without oveflowing any capacity constraints
bool isGreedyAddAllowed(problemSet& coreProblem, vector<long>& currCapacityVals, int index) //index is the item to be added
{
    for(int c{0}; c < currCapacityVals.size(); c++) //for every capacity constraint...
    {
        if(currCapacityVals[c] + coreProblem.problemsByCase[0][index].capacityVal[c] > coreProblem.knapsackCapacityVals[c])
            return false; // we return false if this item will cause the knapsack to exceed in ANY capacity constraint
    }
    return true;
}

 void greedyCoreSolver(problemSet& coreProblem, vector<double>& xVals) //attempts to solve greedily (selects for highest xVal values). Only stores answer by setting xVals we added to 1, which is then interpreted by a separate function
 {
    int len = coreProblem.knapsackCapacityVals.size();
    vector<long> currCapacityVals(len , 0); // what the current core problem knapsack capacity values are (starts with 0 for all as bag is empty)
    vector<int> bestXValIndices;
    //first we sort by obj value / weight

    for(int i{0}; i < xVals.size(); i++)
        bestXValIndices.push_back(i);

    //sorts a vector of indexes to be where the most picked XVals in the core problem are in the passed xVals vector, with this index also aligning with the candidates location in the coreProblem vector
    sort(bestXValIndices.begin(), bestXValIndices.end(), [&xVals](int i, int j)
    {
        return (xVals[i] > xVals[j]);
    }
    );

    for(int i{0}; i < bestXValIndices.size(); i++) //adds greedily, but has to check that all capacity constraints would not go over if we added this item.
    // I Could make this a while loop for a performance increase, but cores are very small in our samples and thus the performance hit of running even when the bag is full is not an issue
    {
        int target = bestXValIndices[i];
        if(isGreedyAddAllowed(coreProblem, currCapacityVals, target))
        {
            for(int c{0}; c < currCapacityVals.size(); c++)
                currCapacityVals[c] += coreProblem.problemsByCase[0][target].capacityVal[c];
            xVals[target] = 1.0; //item is now included into the solution
        }
    }
 }
/* 
 //My thought for this is what if we shorten the work gurobi has to do by first doing the relaxed version,
 // and then using the smaller core formed from this as another input for the unrelaxed problem

 //Only difference to runGurobiMDMKP is there is no file output and this only does one problemSet at a time, not a vector of problemSets
vector<double> gurobiOnCore(problemSet& coreProblem, GRBEnv& env, ofstream& excel)
{
        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;
        GRBModel model(env);
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program 
        model.set(GRB_DoubleParam_TimeLimit, 600); //600 
        vector<GRBVar> x; //variable for if we include / not include item in knapsack
//////////////////// objective value definition ///////////////
        for(int i{0}; i < coreProblem.problemsByCase[0].size(); i++) 
            x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY)); //works due to unique behavior of GRBVars, effectively the GRBVar's will take on the associated model's changes
       
        for(int i{0}; i < coreProblem.problemsByCase[0].size(); i++)
                objective += coreProblem.problemsByCase[0][i].value * x[i]; 
        model.setObjective(objective, GRB_MAXIMIZE);
//////////////////// capacity constraint ///////////////
        for(int i{0}; i < coreProblem.knapsackCapacityVals.size(); i++) 
        {
            GRBLinExpr capacityExpr;
            for(int e{0}; e < coreProblem.problemsByCase[0].size(); e++)
                capacityExpr += coreProblem.problemsByCase[0][e].capacityVal[i] * x[e];
            capacityConstr.push_back(capacityExpr);
        }
        for(int i{0}; i < capacityConstr.size(); i++)
            model.addConstr(capacityConstr[i] <= coreProblem.knapsackCapacityVals[i] );
       
//////////////////// demand constraint ///////////////
        for(int i{0}; i < coreProblem.problemsByCase[0][0].demandVal.size(); i++) //fixed to fit actual demand amount for the given case
        {
            GRBLinExpr demandExpr;
            for(int e{0}; e < coreProblem.problemsByCase[0].size(); e++)
                demandExpr += coreProblem.problemsByCase[0][e].demandVal[i] * x[e];  
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= coreProblem.knapsackDemandRequirementVals[i] );
        // model.set(GRB_IntParam_OutputFlag, 0);
        model.optimize();

         //model.write("testModel.lp");
        if(model.get(GRB_IntAttr_SolCount) == 0)
        {
            excel << "failed\n";
            return vector<double>(); //case of a failed solution
        }
        excel << "passed";  
        return GRBToDoubleDecisionValues(x); //returns core solution if found
}
 */



void loosenCapAndDemand(vector<int>& isLoosenedCapacity, vector<int>& isLoosenedDemand, problemSet& caseNum)
{
    int i{0};
    for(long& capacityRHS : caseNum.knapsackCapacityVals) //modifying the demand requirements (loosening requirements)
    {
        if(isLoosenedCapacity[i])
            capacityRHS *= 1 + doubleRelaxVal; // demand decrease, capacity should increase. Was tested in increments of 10%
        i++;
    }

    i = 0;
    for(long& demandRHS : caseNum.knapsackDemandRequirementVals) //modifying the demand requirements (loosening requirements)
    {
        if(isLoosenedDemand[i])
            demandRHS *= 1 - doubleRelaxVal;
        i++;
    }
}

//note: by design, caseProblems should only hold problemSets of the same case, so you need a for loop with formatByCase() being used to evaluate all cases
void runGurobiMDMKP(GRBEnv& env, ofstream& excel, vector<problemSet>& caseProblems, int caseCounter)
{
    int blockNum{1};
    for(problemSet caseProblem : caseProblems)
    {
        if(blockNum != CURRPROBLEM)
        {    
            blockNum++;
            continue;
        } 

        for(int i{0}; i < 6; i++)
        {
            if(isLoosenedCapacity.size() != 0 && isLoosenedDemand.size() != 0) // Implements building previous problem 
            {
                for(int i{0}; i < isLoosenedCapacity.size(); i++)
                    loosenCapAndDemand(isLoosenedCapacity[i], isLoosenedDemand[i], caseProblem);
            }


            //code specific for removing cases that did get feasible solutions in previous tests so we can test the core problem approach only on hard problems
        
            vector<GRBLinExpr> demandConstr;
            vector<GRBLinExpr> capacityConstr;
            GRBLinExpr objective;
            GRBModel model(env);
            
            model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program 
            model.set(GRB_DoubleParam_TimeLimit, 600); //600 
            vector<GRBVar> x; //variable for if we include / not include item in knapsack
    //////////////////// objective value definition ///////////////
            for(int i{0}; i < caseProblem.problemsByCase[0].size(); i++) 
                x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_CONTINUOUS)); 
        
            for(int i{0}; i < caseProblem.problemsByCase[0].size(); i++)
                    objective += caseProblem.problemsByCase[0][i].value * x[i]; 
            model.setObjective(objective, GRB_MAXIMIZE);
    //////////////////// capacity constraint ///////////////
            for(int i{0}; i < caseProblem.knapsackCapacityVals.size(); i++) 
            {
                GRBLinExpr capacityExpr;
                for(int e{0}; e < caseProblem.problemsByCase[0].size(); e++)
                    capacityExpr += caseProblem.problemsByCase[0][e].capacityVal[i] * x[e];
                capacityConstr.push_back(capacityExpr);
            }
            for(int i{0}; i < capacityConstr.size(); i++)
                model.addConstr(capacityConstr[i] <= caseProblem.knapsackCapacityVals[i] );
        
    //////////////////// demand constraint ///////////////
            for(int i{0}; i < caseProblem.problemsByCase[0][0].demandVal.size(); i++) //fixed one small error of including extra demands, as each problem case varies on demand amount
            {
                GRBLinExpr demandExpr;
                for(int e{0}; e < caseProblem.problemsByCase[0].size(); e++)
                    demandExpr += caseProblem.problemsByCase[0][e].demandVal[i] * x[e];  
                demandConstr.push_back(demandExpr);
            }
            for(int i{0}; i < demandConstr.size(); i++)
                model.addConstr(demandConstr[i] >= caseProblem.knapsackDemandRequirementVals[i] );
        
            //model.set(GRB_IntParam_OutputFlag, 0); // allows suppressing of gurobi terminal output (useful for isolating messages on core approach with gurobi)
            model.optimize();
        
            long long profit{0};

            ///// Shadow values
            //isLoosenedCapacity.clear();
            //isLoosenedDemand.clear();
            vector<int> newLoosenedCapacity;
            vector<int> newLoosenedDemand;
            auto constrs = model.getConstrs();
            excel << "CURR RELAX: " + (to_string(doubleRelaxVal * isLoosenedCapacity.size())) << ",";
                for(int i{0}; i < caseProblem.knapsackCapacityVals.size(); i++)
                    excel << i << ",";
                excel << endl;

    /*             for(int i{0}; i < caseProblem.knapsackCapacityVals.size(); i++) // prints capacity RHS
                    excel << constrs[i].get(GRB_DoubleAttr_RHS) << ",";
                excel << endl; */
                excel << "Capacity" << ",";
                for(int i{0}; i < caseProblem.knapsackCapacityVals.size(); i++) // prints capacity
                {
                    double d = constrs[i].get(GRB_DoubleAttr_Pi);
                    //excel << d << ",";
                    if(d != 0)
                    {
                        excel << 1 << ",";
                        newLoosenedCapacity.push_back(1);
                    }
                    else
                    {
                        excel << 0 << ",";
                        newLoosenedCapacity.push_back(0);
                    }
                }
                excel << endl;

        /*       for(int i{static_cast<int>(caseProblem.knapsackCapacityVals.size())}; i < caseProblem.knapsackDemandRequirementVals.size() + caseProblem.knapsackCapacityVals.size(); i++) // prints capacity
                    excel << constrs[i].get(GRB_DoubleAttr_RHS) << ",";  // prints demand RHS
                excel << endl; */
                excel << "Demand" << ",";
                for(int i{static_cast<int>(caseProblem.knapsackCapacityVals.size())}; i < caseProblem.knapsackDemandRequirementVals.size() + caseProblem.knapsackCapacityVals.size(); i++) //print demand shadow vals
                {
                    double d = constrs[i].get(GRB_DoubleAttr_Pi);
                    //excel << d << ",";
                    if(d != 0)
                    {
                        excel << 1 << ",";
                        newLoosenedDemand.push_back(1);
                    }
                    else
                    {
                        excel << 0 << ",";
                        newLoosenedDemand.push_back(0);
                    }
                }
                excel << endl << endl;
            ///
            isLoosenedCapacity.push_back(newLoosenedCapacity);
            isLoosenedDemand.push_back(newLoosenedDemand);
            model.write("model.lp");
            //doubleRelaxVal+=0.01;
                /* if(blockNum == CURRPROBLEM) //DELETE, this is code for solution extraction
                exit(EXIT_SUCCESS);  */
            
        }
        blockNum++;
    // blockNum++;
    }
}

//Stores all the problems for 1 of the 6 cases in the caseSet vector
void formatCase(int caseNum, vector<problemSet>& caseSet, vector<problemSet>& problemSets) //caseSet should be empty! Holds the answer
{
    for(int i{0}; i < problemSets.size(); i++)
    {
        problemSet caseProblem;
        caseProblem.problemsByCase.push_back(problemSets[i].problemsByCase[caseNum]);
        caseProblem.knapsackCapacityVals = problemSets[i].knapsackCapacityVals;
        caseProblem.knapsackDemandRequirementVals = problemSets[i].knapsackDemandRequirementVals;
        caseSet.push_back(caseProblem);
    }
}

int main()
{
    // This gives the LP relaxation OF the #P, and this is used to tell us what the next increment needs to change
    string filename = "MDMKPct7LP_RELAX_SHADOWVALS_B" + to_string(CURRPROBLEM) + "C6_ALLP's.csv";
    ofstream excel(filename); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << '\n';

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
     env.start();

    //reading 
    vector<MDMKRawProblem> MDMKRawProblems;
    readMDMKP("datac7.txt", MDMKRawProblems);
    vector<problemSet> problemSets; //will store the sorted problems of each block (each block contains all 6 cases), blocks are defined by the .txt file we read from (read https://people.brunel.ac.uk/~mastjjb/jeb/orlib/mdmkpinfo.html to learn more about what a block is)
    RawProblemsToCases(MDMKRawProblems, problemSets); // this just stores problems in a more understandable state that allows direct usage in solving algorithms
                                                      // rawProblem's problemSet objects contains a .problemByCase of 6 elements, one for each case 

/*     for(int i{0}; i <= 5; i++) //extracts cases 1-6 and runs gurobi on them
    {
        vector<problemSet> caseSet;
        formatCase(i, caseSet, problemSets); // stores the ith case from every block in caseSet (15 blocks total), 
                                             // but note that i is one behind what you expect. EX: case 1 is i = 0, case 2 is i = 1, and so on.
        runGurobiMDMKP(env, excel, caseSet, i);
    } */
   int caseNum = 6;
 // In this program my goal is to test the LP relaxation approach on the hardest cases, case 3 and 6, and to see if its a valid or invalid approach for a sample size of n = 100
    vector<problemSet> caseSet; // case 3 
    formatCase(caseNum - 1, caseSet, problemSets); //yes, an input of 2 means case 3.
    runGurobiMDMKP(env, excel, caseSet, caseNum - 1);
 
    return 0;
}




 /*     Used for 99p LP relax of 100
            vector<int> isLoosenedCapacity = {1,0,1,1,0,1,1,0,1,0,0,1,0,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1};
            vector<int> isLoosenedDemand   = {1,1,0,1,0,1,1,1,0,0,1,0,1,1,1,0,0,0,0,0,1,0,1,1,1,1,1,1,0,1};
        */
        // Used for 98p (we use the output of this problem with 98p relax to get 98p results)
        /*         
        vector<int> isLoosenedCapacity = {1,1,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,0,1,1,0,1,1,0,1,1,1,1,1,1};
        vector<int> isLoosenedDemand   = {0,0,0,1,0,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,1,1,1,1,1,0,0};
        */

        // Used for 97p, input in this program to get in return the 96p (3 percent) relax
    /*     
        vector<int> isLoosenedCapacity = {1,1,1,1,0,1,1,0,1,1,0,0,0,1,1,0,0,0,1,1,1,0,1,1,1,1,1,0,1,0};
        vector<int> isLoosenedDemand   = {0,0,0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1};
    */
        
        // used for 96p, gives LP relax of 97p
        /* 
        vector<int> isLoosenedCapacity = {1,0,0,0,0,0,0,0,1,0,1,1,0,0,1,1,0,0,0,1,0,1,1,0,1,0,0,1,1,1};
        vector<int> isLoosenedDemand   = {1,0,0,0,0,1,0,0,0,0,1,1,1,0,1,0,0,0,0,0,0,1,0,1,1,1,1,1,1,0};
    */
    //Used for 95p, LP relax of 96p
    /*     vector<int> isLoosenedCapacity = {1,0,1,1,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,1,1,0,0,1};
        vector<int> isLoosenedDemand   = {0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0,1,1,0,0,1,1,0,0,0,0,1}; 
    */
