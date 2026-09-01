#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <cmath> //just for abs (even though thats very easy to manually implement)

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/linear_solver/linear_solver.h"
using namespace std;
using namespace operations_research;

/* 
NOTE: While this approach is less efficient than the fileread.cpp version, I decided on 
this approach as it is more readable, and the heuristics will take int anyways, so a few extra 
seconds of processing the data read from the file means nothing in the end
 */
//reads one block of data from MDMKP .txt file (one block = either <=, >= constraints or cost coefficient vals)
/* 
Function name: readAttributeOfMDMKP
Params:
    ifstream& file: The file we want to read from, should be "pointing" to the start of the attribute we want to read 
    vector<vector<int>>& candidateCapacityAtrributes: Where we will store each candidates attribute for each constraint (reminder that there are multiple capacity constraints and demand constraints in each problem)
    bool IsConstraint: if true, will read an additional line that will be the capacity/demand max/min(s) depending on the attribute we are reading. (<= constraint is capacity / max, >= constraint is demand/min)


*/



class MDMKRawProblem // each MDMKRawProblem is actually a set of 6 problems in 1 entity, but processing must be done for that to formm thus the "raw" name
{
    private:
        vector<vector<int>> candidateCapacityAtrributes;
        vector<vector<int>> candidateDemandAtrributes;
        vector<vector<int>> candidateCostAtrributes;
        vector<int> knapsackCapacityVals;
        vector<int> knapsackDemandRequirementVals;
    public:
        vector<vector<int>>& getcandidateCapacityAtrributes() { return candidateCapacityAtrributes; } 
        vector<vector<int>>& getcandidateDemandAtrributes() { return candidateDemandAtrributes; }
        vector<vector<int>>& getCandidateValue() { return candidateCostAtrributes; }
        vector<int>& getknapsackCapacityVals() { return knapsackCapacityVals; }
        vector<int>& getknapsackDemandRequirementVals() { return knapsackDemandRequirementVals; }

};

struct MDMKCandidate
{
    vector<int> capacityVal; // How much capacity this candidate takes up for each capacity constraint
    vector<int> demandVal; //How much demand value this candidate contributes
    int value; // "Cost": how much this item is worth (either +/- depending on the case)
};

struct problemSet
{
    vector < vector <MDMKCandidate> > problemsByCase;
    vector<int> knapsackCapacityVals;
    vector<int> knapsackDemandRequirementVals; 
};


//Formats MKMDProblem into their 6 respective cases, in accordance to the brunel paper. Here is a definition of each case:
//    Note that "q" is the number of demand constraints (>=) and "m" is the number of capacity (<=) constraints
//    These constraints are picked in order as they show up. EX: when q=1, it means only the first demand constraint is considered
//    Positive cost/value cases
//    Case 1: q = 1
//    Case 2: q = m/2
//   Case 3: q = m
//    Mixed cost/value cases
//    Case 4: q = 1
//    Case 5: q = m/2
//    Case 6: q = m

void separateCandidatesByCases(MDMKRawProblem& problem, problemSet& candidatesByCase) 
{
    candidatesByCase.problemsByCase.resize(6); // new thing learned! Can be used to easily create uninitialized values inside of the vector (or reshape vector to fit this function's usage)
    vector<vector<int>> MDMKCapacityAttributes = problem.getcandidateCapacityAtrributes(); 
    vector<vector<int>> MDMKDemandAttributes = problem.getcandidateDemandAtrributes();
    vector<vector<int>> MDMKValue = problem.getCandidateValue();
    int capacityVarsCount = MDMKCapacityAttributes.size(); //Shortcut to finding the amount of capacity variables (dimensions) for the problem
    int candidateCount = MDMKCapacityAttributes[0].size();//This is a way to find the # of candidates we have without having to pass directly (not too efficient, but less params = simpler). Only works if problem is not empty
    int caseDemand1 = 1; //sort of pointless but slightly more readable as it describes what the value 1 is for (Case 1 of MDMKP problems)
    int caseDemand2 = capacityVarsCount / 2;
    int caseDemand3 = capacityVarsCount; // Note that case 1,2,3 are repeated for case 4,5,6 respectively
     int order[6] = {0,3,1,4,2,5}; //order which we want to get candidates (most efficient order for the cases)
    for(int i{0}; i < candidateCount; i++)
    {
        MDMKCandidate candidate;
        for(int c{0}; c < capacityVarsCount; c++)
            candidate.capacityVal.push_back(MDMKCapacityAttributes[c][i]);
        
       
        for(int caseNum{0}; caseNum < 6; caseNum++) //Changes candidate to each case (note that caseNum = 0 is case 1 and caseNum = 5 is case 6)
        {
            int currentCase = order[caseNum];
            candidate.value = MDMKValue[currentCase][i];

            for(int currDemand{0}; currDemand < capacityVarsCount; currDemand++) //stops at case 6 (where same # of demands as capacity constraints)
            {
                // WARNING: Very ugly if statement below!
                candidate.demandVal.push_back(MDMKDemandAttributes[currDemand][i]);

                if ( (currDemand == caseDemand3 - 1 && (currentCase == 2 || currentCase == 5)) || (currDemand == caseDemand2 - 1 && (currentCase == 1 || currentCase == 4) ) || (currDemand == caseDemand1 - 1 && (currentCase == 0 || currentCase == 3)) ) //these checks are really inefficient but they work!
                {
                    candidatesByCase.problemsByCase[currentCase].push_back(candidate);
                    
                }      
            }
            
            candidate.demandVal.clear();///
        }
        
    }
}

void RawProblemsToCases(vector<MDMKRawProblem>& problems, vector<problemSet>& cases)
{
    problemSet candidatesByCase;
    candidatesByCase.problemsByCase.resize(6);
    //cases.resize(0);
    // for(problemSet problem : cases)
     //   problem.problemsByCase.resize(6); 
    
    int size = problems.size();
    //for(MDMKRawProblem problem : problems)
    for(int i{0}; i < size; i++)
    {
        
        separateCandidatesByCases(problems[i], candidatesByCase);
        cases.push_back(candidatesByCase);
        cases[i].knapsackCapacityVals = problems[i].getknapsackCapacityVals();
        cases[i].knapsackDemandRequirementVals = problems[i].getknapsackDemandRequirementVals();
        candidatesByCase.problemsByCase.clear();
    }
}


void readAttributeOfMDMKP(ifstream& file, vector<vector<int>>& candidateCoefficientAtrributes, vector<int>& knapsackGoalVals, int candidateCount, int leConstraints, bool isConstraint)
{
    int placeholder; 
    //leConstraints means <= constraints (also known as capacity constraints)
    for(int m{0}; m < leConstraints; m++) //reading for all candidate constraints
    {
        candidateCoefficientAtrributes.push_back(vector<int>());
        for(int i{0}; i < candidateCount; i++) // reading all <
        {
            file >> placeholder;
            candidateCoefficientAtrributes[m].push_back(placeholder);
        }
    }
    if(isConstraint)
        for(int m{0}; m < leConstraints; m++) //Reads capacity of knapsack for each capacity constraint
        {
            file >> placeholder;
            knapsackGoalVals.push_back(placeholder);
        }
}


//Param of fileName is the file we want to read from, must be in the same folder as this file (though I could easily change this if needed)
void readMDMKP(string fileName, vector<MDMKRawProblem>& MDMKRawProblems) // read MDMKP problem text files in accordance to the format done by https://people.brunel.ac.uk/~mastjjb/jeb/orlib/mdmkpinfo.html
{
    
    ifstream file{fileName};
    int testProblemCount, candidateCount, leConstraints; //leConstraints means <= constraints (also known as capacity constraints)
    
    file >> testProblemCount;

    for(int i{0}; i < testProblemCount; i++) //traverses all the problem sets of the file 
    {
        MDMKRawProblem problemSet; //creates a new empty problem set for every instance
        file >> candidateCount >> leConstraints; // These are the "header" variables for the brunel samples, they apply to all cases of a single problem (6 cases)
        vector<vector<int>> candidateCapacityAtrributes; //each vector contains a separate attribute for every candidate 
        vector<int> knapsackCapacityVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateCapacityAtrributes(), problemSet.getknapsackCapacityVals(), candidateCount, leConstraints, true); // reads <= constraint

        vector<vector<int>> candidateDemandAtrributes;
        vector<int> knapsackDemandRequirementVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateDemandAtrributes(), problemSet.getknapsackDemandRequirementVals(), candidateCount, leConstraints, true); // reads >= constraint

        vector<vector<int>> candidateCostAtrributes; //Note that "knapsackCapacityVals" is useless here due to the boolean parameter being = false
        //I repurposed the leconstraints parameter to be the # of cases each model has, which is 6 for Brunel test cases
        readAttributeOfMDMKP(file, problemSet.getCandidateValue(), problemSet.getknapsackCapacityVals(), candidateCount, 6, false); // reads value of each object (Though the Brunel paper calls these "Cost" coefficients). 
        
        MDMKRawProblems.push_back(problemSet);
    }
}

///////////////////////////////////


void runSolverMDMKP(string solverName, int timeLimitInSeconds, ofstream& excel, vector<problemSet>& caseNums, int caseCounter)
{
    


    int blockNum{1};
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto caseNum : caseNums)
    {
        std::unique_ptr<MPSolver> solver(MPSolver::CreateSolver(solverName));
 //        for(int& demandRequirement : caseNum.knapsackDemandRequirementVals) //modifying the demand requirements (loosening requirements)
   //         demandRequirement *= 0.8; 
    /////////////////////////




        //code specific for removing cases that did get feasible solutions in previous tests so we can test the core problem approach only on hard problems
       /*  
        if(blockNum != 10)
        {    
            blockNum++;
            continue;
        }
        */
        //vector<GRBLinExpr> demandConstr;
        //vector<GRBLinExpr> capacityConstr;
        MPObjective* objective = solver->MutableObjective();

        vector<MPVariable*> x; //variable for if we include / not include item in knapsack

//////////////////// objective value definition ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++) 
            x.push_back(solver->MakeIntVar(0.0, 1.0, "x" + to_string(i))); 
       
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
                objective->SetCoefficient(x[i], caseNum.problemsByCase[0][i].value); 
        objective->SetMaximization();
//////////////////// capacity constraint ///////////////
        for(int i{0}; i < caseNum.knapsackCapacityVals.size(); i++) 
        {
            MPConstraint* capacityExpr = solver->MakeRowConstraint(0, caseNum.knapsackCapacityVals[i]);  
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
                capacityExpr->SetCoefficient(x[e], caseNum.problemsByCase[0][e].capacityVal[i]); // REMINDER: FOR NON CASE 1, edit demandVAL[0]
        }       
//////////////////// demand constraint ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0][0].demandVal.size(); i++)  // NEED TO CHECK CASE VAL REIJEIOGJIGJERJIGOERGIOREGJIERGJIOERGJIOERGJIOGJOERGJO
        {
            MPConstraint* demandConstr = solver->MakeRowConstraint(caseNum.knapsackDemandRequirementVals[i], solver->infinity());  
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
                demandConstr->SetCoefficient(x[e], caseNum.problemsByCase[0][e].demandVal[i]); // REMINDER: FOR NON CASE 1, edit demandVAL[0
        }
////////////////////////////////////////////////////
        solver->set_time_limit(timeLimitInSeconds * 1000); // this solver param is done in milliseconds
        //solver->SetSolverSpecificParametersAsString("MIPGAP = 0.0001"); //Gurobi
       // solver->SetSolverSpecificParametersAsString("relative_gap_limit: 0.0001"); //CP-SAT
        //CBC (uses Cbc 2.10.12)
        MPSolverParameters params; // CPLEX must use parameters object
        params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.0001); // FOR CPLEX + CBC
        //


         // allowableGap 0.0001 CBC, MIPGAP = 0.0001 GUROBI, limits/gap = 0.0001 SCIP. MUST CHANGE WHEN GOING BETWEEN SOLVERS
        //solver->EnableOutput();
        const MPSolver::ResultStatus result = solver->Solve(params); //For CPLEX version
        //const MPSolver::ResultStatus result = solver->Solve(); //For CPLEX version
  //       std::cout << "Vars: " << model.get(GRB_IntAttr_NumVars) << endl;
  //      std::cout << "Constraints: " << model.get(GRB_IntAttr_NumConstrs); 

    
        
        if(result == MPSolver::FEASIBLE)
        {   
            excel << "B" << blockNum << "C" << caseCounter << "," <<  objective->Value() << "," << ((float)solver->wall_time()) / 1000 << "," << (std::abs(objective->BestBound() - objective->Value())) / objective->Value()  << endl; //obj will always be positive 
           // For showing decision variables
           
            //  for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
           // {
            //    excel << i << ',';
           // }  
            //excel << endl;
           // for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
           // {
           //     excel << x[i].get(GRB_DoubleAttr_X) << ',';
           // }   
           // excel << endl;  
        }
        else // case of infeasible solution 
        {
            excel << "B" << blockNum << "C" << caseCounter << "," <<  -1 << "," << ((float)solver->wall_time()) / 1000 << endl; 
        }
        

        // For printing display + capaacity constraints
 //        excel << "B" + to_string(blockNum) << endl;
   //     excel << "Capacity,";
     //   for(int i{0}; i < caseNum.knapsackCapacityVals.size(); i++)
       //    excel << caseNum.knapsackCapacityVals[i] << ",";
        //excel << endl;
        //excel << "Demand,";
        //for(int i{0}; i < caseNum.knapsackDemandRequirementVals.size(); i++)
         //   excel << caseNum.knapsackDemandRequirementVals[i] << ",";
        //excel << endl; 
        blockNum++;

        //exit(EXIT_SUCCESS);

    }
}

//Stores all the problems for 1 of the 6 cases in the caseSet vector
void formatCase(int caseNum, vector<problemSet>& caseSet, vector<problemSet>& problemSets) //caseSet should be empty! Holds the answer
{
    for(int i{0}; i < problemSets.size(); i++)
    {
        problemSet caseProblem;
        caseProblem.problemsByCase.push_back(problemSets[i].problemsByCase[caseNum - 1]);
        caseProblem.knapsackCapacityVals = problemSets[i].knapsackCapacityVals;
        caseProblem.knapsackDemandRequirementVals = problemSets[i].knapsackDemandRequirementVals;
        caseSet.push_back(caseProblem);
    }
}


int main()
{
    

    //GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    //(env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    //(env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    //(env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    //env.start();

    //reading 
   vector<MDMKRawProblem> MDMKRawProblems;
    readMDMKP("C:/Users/Pomer/Desktop/Gurobi projects/MDMKP/datac7.txt", MDMKRawProblems);
    //readMDMKP("mdmkp_ct8.txt", MDMKRawProblems);
    vector<problemSet> problemSets;
    RawProblemsToCases(MDMKRawProblems, problemSets);

    //for(int i{0}; i <= 5; i++) //extracts cases 1-6 and runs gurobi on them
    //{
     //   vector<problemSet> caseSet;
      //  formatCase(i, caseSet, problemSets);
       // runGurobiMDMKP(env, excel, caseSet, i);
    //} 

    int caseNum = 6; 
    int timeInSecondsPerProblem = 3600;
    string solverName = "CBC";

    ofstream excel(solverName + "_MDMKP_Case" + to_string(caseNum) +  "_" + to_string(timeInSecondsPerProblem) + "s.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';


    vector<problemSet> caseSet;
    formatCase(caseNum, caseSet, problemSets);
    cout << "Using " << solverName << endl <<
    "Started solving" << endl;
    runSolverMDMKP(solverName, timeInSecondsPerProblem, excel, caseSet, caseNum);
    cout << "Finished solving" << endl;
    return 0;
}

