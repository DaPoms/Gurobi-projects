#include "gurobi_c++.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <filesystem>
#include <string>
#include <sstream>
#include <regex>
using namespace std;

/* 
NOTE: While this approach is less efficient than the fileread.cpp version, I decided on 
this approach as it is more readable, and the heuristics will take long anyways, so a few extra 
seconds of processing the data read from the file means nothing in the end
 */
//reads one block of data from MDMKP .txt file (one block = either <=, >= constraints or cost coefficient vals)
/* 
Function name: readAttributeOfMDMKP
Params:
    ifstream& file: The file we want to read from, should be "pointing" to the start of the attribute we want to read 
    vector<vector<long>>& candidateCapacityAtrributes: Where we will store each candidates attribute for each constraint (reminder that there are multiple capacity constraints and demand constraints in each problem)
    bool IsConstraint: if true, will read an additional line that will be the capacity/demand max/min(s) depending on the attribute we are reading. (<= constraint is capacity / max, >= constraint is demand/min)


*/

class MDMKRawProblem // each MDMKRawProblem is actually a set of 6 problems in 1 entity, but processing must be done for that to formm thus the "raw" name
{
    private:
        vector<vector<long>> candidateCapacityAtrributes;
        vector<vector<long>> candidateDemandAtrributes;
        vector<vector<long>> candidateCostAtrributes;
        vector<long> knapsackCapacityVals;
        vector<long> knapsackDemandRequirementVals;
    public:
        vector<vector<long>>& getcandidateCapacityAtrributes() { return candidateCapacityAtrributes; } 
        vector<vector<long>>& getcandidateDemandAtrributes() { return candidateDemandAtrributes; }
        vector<vector<long>>& getCandidateValue() { return candidateCostAtrributes; }
        vector<long>& getknapsackCapacityVals() { return knapsackCapacityVals; }
        vector<long>& getknapsackDemandRequirementVals() { return knapsackDemandRequirementVals; }

};

struct MDMKCandidate
{
    vector<long> capacityVal; // How much capacity this candidate takes up for each capacity constraint
    vector<long> demandVal; //How much demand value this candidate contributes
    long value; // "Cost": how much this item is worth (either +/- depending on the case)
};

struct problemSet
{
    vector < vector <MDMKCandidate> > problemsByCase;
    vector<long> knapsackCapacityVals;
    vector<long> knapsackDemandRequirementVals; 
};


//Formats MKMDProblem into their 6 respective cases, in accordance to the brunel paper. Here is a definition of each case:
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
*/
void separateCandidatesByCases(MDMKRawProblem& problem, problemSet& candidatesByCase) 
{
    candidatesByCase.problemsByCase.resize(6); // new thing learned! Can be used to easily create uninitialized values inside of the vector (or reshape vector to fit this function's usage)
    vector<vector<long>> MDMKCapacityAttributes = problem.getcandidateCapacityAtrributes(); 
    vector<vector<long>> MDMKDemandAttributes = problem.getcandidateDemandAtrributes();
    vector<vector<long>> MDMKValue = problem.getCandidateValue();
    long capacityVarsCount = MDMKCapacityAttributes.size(); //Shortcut to finding the amount of capacity variables (dimensions) for the problem
    long candidateCount = MDMKCapacityAttributes[0].size();//This is a way to find the # of candidates we have without having to pass directly (not too efficient, but less params = simpler). Only works if problem is not empty
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
    /* for(problemSet problem : cases)
        problem.problemsByCase.resize(6); */
    
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


void readAttributeOfMDMKP(ifstream& file, vector<vector<long>>& candidateCoefficientAtrributes, vector<long>& knapsackGoalVals, long candidateCount, long leConstraints, bool isConstraint)
{
    long placeholder; 
    //leConstraints means <= constraints (also known as capacity constraints)
    for(int m{0}; m < leConstraints; m++) //reading for all candidate constraints
    {
        candidateCoefficientAtrributes.push_back(vector<long>());
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
    long testProblemCount, candidateCount, leConstraints; //leConstraints means <= constraints (also known as capacity constraints)
    
    file >> testProblemCount;

    for(int i{0}; i < testProblemCount; i++) //traverses all the problem sets of the file 
    {
        MDMKRawProblem problemSet; //creates a new empty problem set for every instance
        file >> candidateCount >> leConstraints; // These are the "header" variables for the brunel samples, they apply to all cases of a single problem (6 cases)
        vector<vector<long>> candidateCapacityAtrributes; //each vector contains a separate attribute for every candidate 
        vector<long> knapsackCapacityVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateCapacityAtrributes(), problemSet.getknapsackCapacityVals(), candidateCount, leConstraints, true); // reads <= constraint

        vector<vector<long>> candidateDemandAtrributes;
        vector<long> knapsackDemandRequirementVals;
        readAttributeOfMDMKP(file, problemSet.getcandidateDemandAtrributes(), problemSet.getknapsackDemandRequirementVals(), candidateCount, leConstraints, true); // reads >= constraint

        vector<vector<long>> candidateCostAtrributes; //Note that "knapsackCapacityVals" is useless here due to the boolean parameter being = false
        //I repurposed the leconstraints parameter to be the # of cases each model has, which is 6 for Brunel test cases
        readAttributeOfMDMKP(file, problemSet.getCandidateValue(), problemSet.getknapsackCapacityVals(), candidateCount, 6, false); // reads value of each object (Though the Brunel paper calls these "Cost" coefficients). 
        
        MDMKRawProblems.push_back(problemSet);
    }
}

//////////Reading warm start directory/////////////////////////


//////////////////
int extractNumber(const std::string& path, const std::string& pattern) {  //This function is NOT MADE BY ME
    std::regex re(pattern);
    std::smatch match;
    if (std::regex_search(path, match, re) && match.size() > 1) {
        return std::stoi(match[1]);
    }
    return 0;
}
//////////////////////

vector<vector<int>> getWarmSols(string directoryName, int candidateCount)
{
    string read;
    vector<vector<int>> ans;
    vector<filesystem::directory_entry> sortedDirs; // We need to define sorted order (problem 1 -> 15)
    for(const auto& folderInDir : filesystem::recursive_directory_iterator(directoryName))
        if(folderInDir.is_directory() == true)
            sortedDirs.push_back(folderInDir);
    
       
    std::sort(sortedDirs.begin(), sortedDirs.end(), /// NOT MADE BY ME
        [](const filesystem::path& a, const filesystem::path& b) {
            int aB = extractNumber(a.string(), R"(B(\d+)C)");
            int aC = extractNumber(a.string(), R"(B\d+C(\d+))");
            int bB = extractNumber(b.string(), R"(B(\d+)C)");
            int bC = extractNumber(b.string(), R"(B\d+C(\d+))");
            
            if (aB != bB) return aB < bB;
            return aC < bC;
        });

    for(const auto dir : sortedDirs)
    {
        for(const auto& itemInDir : filesystem::recursive_directory_iterator(dir))
        {
            if(itemInDir.is_regular_file() == true && itemInDir.path().stem() == "run_000_best_solution") // sol must only be name this
            {
                vector<int> sol;
                
                getline(ifstream{itemInDir.path()}, read);
                istringstream stringifiedDecisions{read};
                for(int i{0}; i < candidateCount; i++)
                {
                    try
                    {
                        char commaConsume;
                        int decision;
                        stringifiedDecisions >> decision >> commaConsume; //commaConsume is only for consumption, not usage
                        sol.push_back(decision);
                    } catch(exception& e)
                    {
                        continue;
                    }
                    
                }
                ans.push_back(sol);            
            }
        }
    }
    return ans;
}






void runGurobiMDMKPWarm(GRBEnv& env, ofstream& excel, vector<problemSet>& caseNums, int caseCounter, vector<vector<int>> warmSols)
{
    int blockNum{1};
     //// FOR GARCIA ONLY //
        vector<double> caseWarmupTimes = {600.202332, 601.285339, 600.896484, 600.315918, 601.193359, 600.518982, 600.318726, 601.159363, 600.679321, 600.408081, 601.005920, 601.081360, 600.740540, 600.427246, 601.219849 };
        ///////////////////////
    /////////////////////////////////////// Test branch code!!!! (ONCE AGAIN, REMOVE THIS AFTER DONE THE TESTING)
    for(auto caseNum : caseNums)
    {

        //warm start phase//





    ////////Demand relaxation option ////////////
/*         for(long& demandRequirement : caseNum.knapsackDemandRequirementVals) //modifying the demand requirements (loosening requirements)
            demandRequirement *= 0.8; */
    ///////Problem selection/////////////////////

        //code specific for removing cases that did get feasible solutions in previous tests so we can test the core problem approach only on hard problems
/*         if(blockNum < 4)
        {    
            blockNum++;
            continue;
        } */

    //////////////////////////////////////////////



        vector<GRBLinExpr> demandConstr;
        vector<GRBLinExpr> capacityConstr;
        GRBLinExpr objective;

        
        GRBModel model(env);
        
       
        
        model.set(GRB_DoubleParam_MIPGap, 0.0001); //What we deem optimal mipgap to terminate the program  ADD BACK ERIOJERIJGERJOGERJIOGERIOGREJIOGERJIOGERJIORJIOERGJIOERGERJIEGJI
        //model.set(GRB_DoubleParam_TimeLimit, 0.1); // This is used for just getting MIPGAP of warm sol
        model.set(GRB_DoubleParam_TimeLimit, 3600 - caseWarmupTimes[blockNum - 1]); //THIS IS USEDS FOR ACTUAL WARM START
        vector<GRBVar> x; //variable for if we include / not include item in knapsack
//////////////////// objective value definition ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++) 
            x.push_back(model.addVar(0.0, 1.0, 0.0, GRB_BINARY)); 
       
        for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
                objective += caseNum.problemsByCase[0][i].value * x[i]; 
        model.setObjective(objective, GRB_MAXIMIZE);
//////////////////// capacity constraint ///////////////
        for(int i{0}; i < caseNum.knapsackCapacityVals.size(); i++) 
        {
            GRBLinExpr capacityExpr;
           
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
                capacityExpr += caseNum.problemsByCase[0][e].capacityVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
                
            capacityConstr.push_back(capacityExpr);
        }
        for(int i{0}; i < capacityConstr.size(); i++)
            model.addConstr(capacityConstr[i] <= caseNum.knapsackCapacityVals[i] );
       
//////////////////// demand constraint ///////////////
        for(int i{0}; i < caseNum.problemsByCase[0][0].demandVal.size(); i++)  // NEED TO CHECK CASE VAL REIJEIOGJIGJERJIGOERGIOREGJIERGJIOERGJIOERGJIOGJOERGJO
        {
            GRBLinExpr demandExpr;
            for(int e{0}; e < caseNum.problemsByCase[0].size(); e++)
            {
                    demandExpr += caseNum.problemsByCase[0][e].demandVal[i] * x[e]; // REMINDER: FOR NON CASE 1, edit demandVAL[0]
            }
            demandConstr.push_back(demandExpr);
        }
        for(int i{0}; i < demandConstr.size(); i++)
            model.addConstr(demandConstr[i] >= caseNum.knapsackDemandRequirementVals[i] );
      
        


       // /////////////////////Tuning
/*        
       model.set(GRB_IntParam_TuneResults, 2); // Must say 2 as the first is the baseline, 2nd is the best case
        model.set(GRB_DoubleParam_TuneTimeLimit,36000);
        model.tune();
        int resultcount = model.get(GRB_IntAttr_TuneResultCount);
        model.getTuneResult(1); //0 is also the baseline, 1 is for best case
        model.write("tune.prm");
        
        exit(EXIT_SUCCESS);
        */
        //////////////////////////// 
        


        ///// Warm sol insertion ///////////////////
        vector<int> sol = warmSols[blockNum-1];
        for(int i{0}; i < sol.size(); i++)
            x[i].set(GRB_DoubleAttr_Start, sol[i]);
        ////////////////////////////////////////////

        // model.write("testModel.lp"); //Insane new method I learned that helps a lot with debugging, outputs a file that visually shows what the model holds
        model.optimize();

        long long profit{0};

    
        if(model.get(GRB_IntAttr_SolCount) > 0)
        {
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                if( x[i].get(GRB_DoubleAttr_X) >= 0.5) //Turns out x can only be a double, so we must use a bound rather than an exact value
                    profit += caseNum.problemsByCase[0][i].value;
            }        
            excel << "B" << blockNum << "C" << (caseCounter + 1) << "," <<  profit << "," << caseWarmupTimes[blockNum - 1] << ","  << model.get(GRB_DoubleAttr_Runtime)  << "," << model.get(GRB_DoubleAttr_MIPGap) << endl; 
           /*  
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                excel << i << ',';
            }  
            excel << endl;
            for(int i{0}; i < caseNum.problemsByCase[0].size(); i++)
            {
                excel << x[i].get(GRB_DoubleAttr_X) << ',';
            }   
            excel << endl;   THEREWAS A COMMENTBLOCK HEREEEEEE
             */
        }
        else // case of infeasible solution 
        {
            profit = -1;
            excel << "B" << blockNum << "C" << (caseCounter + 1) << "," <<  profit << "," << model.get(GRB_DoubleAttr_Runtime) << endl; 
        }
        blockNum++;



 


/* 
        if (blockNum == 2)
            exit(EXIT_SUCCESS);

 */




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
    ofstream excel("MDMKPct7_Garcia_Gurobi3600s.csv"); //creates file for data to be put in, ios::app allows appending so .open doesn't overwrite
    excel << "Name" << "," << "Obj Fn" << "," << "Runtime" << "," << "MIPGAP" << '\n';

    GRBEnv env = GRBEnv(true); //Heap version (can change dynamically)
    (env).set(GRB_StringParam_WLSAccessID, getenv("GRB_WLSACCESSID"));
    (env).set(GRB_StringParam_WLSSecret, getenv("GRB_WLSSECRET"));
    (env).set(GRB_IntParam_LicenseID, stoi(getenv("GRB_LICENSEID")));
    env.start();

    //reading 
   vector<MDMKRawProblem> MDMKRawProblems;
    readMDMKP("C:/Users/Pomer/Desktop/Gurobi projects/MDMKP/datac7.txt", MDMKRawProblems);
    //readMDMKP("mdmkp_ct8.txt", MDMKRawProblems);
    vector<vector<MDMKCandidate>> candidatesByCase;
    vector<problemSet> problemSets;
    RawProblemsToCases(MDMKRawProblems, problemSets);

/*     for(int i{0}; i <= 5; i++) //extracts cases 1-6 and runs gurobi on them
    {
        vector<problemSet> caseSet;
        formatCase(i, caseSet, problemSets);
        runGurobiMDMKP(env, excel, caseSet, i);
    } */

    int caseNum = 3; //just a fool proof way for me to test specific cases 
    //(I made a mistake once with a more manual setup). Case 3 = case 3, but my functions use arrays that are 0 indexed, so we  need to pass it as case -1

    vector<problemSet> caseSet;
    formatCase(caseNum - 1, caseSet, problemSets);
    vector<vector<int>> warmSols = getWarmSols("C:/Users/Pomer/Desktop/Gurobi projects/Garcia_ct7_600s/case3", caseSet[0].problemsByCase[0].size());
    runGurobiMDMKPWarm(env, excel, caseSet, caseNum - 1, warmSols);

    /* caseNum = 6; //just a fool proof way for me to test specific cases 
    //(I made a mistake once with a more manual setup). Case 3 = case 3, but my functions use arrays that are 0 indexed, so we  need to pass it as case -1
    caseSet.clear();
    formatCase(caseNum - 1, caseSet, problemSets);
    runGurobiMDMKP(env, excel, caseSet, caseNum - 1);
 */
    return 0;
}


// DONE SO FAR: 
